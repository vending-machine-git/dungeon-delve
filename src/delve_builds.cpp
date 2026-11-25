#include "main.hpp"
#include "game.hpp"
#include "entity.hpp"
#include "stat.hpp"
#include "player.hpp"
#include "net.hpp"
#include "raid.hpp"
#include "delve_builds.hpp"
#include "delve_passives.hpp"
#include "delve_effects.hpp"
#include "delve_hud.hpp"
#include "collision.hpp"
#include "prng.hpp"
#include "draw.hpp"
#include "files.hpp"
#include "interface/interface.hpp"
#include "magic/magic.hpp"
#include "mod_tools.hpp"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#ifdef BARONY_SMOKE_TESTS
#include "monster.hpp"
#include "ui/Frame.hpp"
#include "ui/Field.hpp"
#endif

namespace DelveBuilds {
namespace {
const std::array<Option,260> catalogue{{
#include "delve_builds_catalogue.inc"
}};
constexpr Build empty{{-1,-1,-1,-1}};
constexpr int firstAbilitySpell = 600;
std::array<std::array<Build,26>,MAXPLAYERS> selections;
bool loaded = false;
bool migratedSelections = false;
struct Session {
    Build build = empty;
    int characterClass = -1;
    bool ready = false, applied = false, abilitiesPrepared = false;
    std::array<unsigned int,2> cooldown{};
    unsigned int recovery = 0, requestSequence = 0, acceptedSequence = 0, serial = 0, receivedSerial = 0;
};
std::array<Session,MAXPLAYERS> sessions;
unsigned int lastTick = 0, lastSend = 0;
#ifdef BARONY_SMOKE_TESTS
bool auditing=false;
#endif
bool playerValid(int p) { return p >= 0 && p < MAXPLAYERS; }
int abilitySpellId(int characterClass, int index) { return firstAbilitySpell + characterClass * 4 + index - 6; }
void registerAbilitySpells() {
    for (int characterClass = 0; characterClass < 26; ++characterClass) {
        for (int index = 6; index < 10; ++index) {
            int spellId = abilitySpellId(characterClass, index);
            const auto& entry = option(characterClass, index);
            std::string internalName = "spell_delve_" + std::to_string(spellId);
            if (allGameSpells.find(spellId) == allGameSpells.end()) {
                auto spell = spellConstructor(spellId, 0, internalName.c_str(), {});
                if (!spell) continue;
                spell->mana = entry.ability.manaCost;
                spell->sustain = false;
                spell->hide_from_ui = true;
            }
            auto& info = ItemTooltips.spellItems[spellId];
            info = {};
            info.id = spellId;
            info.internalName = internalName;
            info.name = entry.name;
            info.name_lowercase = entry.name;
            info.mana = entry.ability.manaCost;
            info.difficulty = 0;
        }
    }
}
void prepareAbilities(int player) {
    if (!playerValid(player) || !players[player] || !players[player]->isLocalPlayer() || !stats[player]) return;
    auto& session = sessions[player];
    if (!session.ready || !session.applied || session.abilitiesPrepared) return;
    registerAbilitySpells();
    auto& magic = players[player]->magic;
    if (magic.selectedSpell() && isAbilitySpell(magic.selectedSpell()->ID)) magic.equipSpell(nullptr);
    for (auto& spell : magic.selected_spell_alternate) {
        if (spell && isAbilitySpell(spell->ID)) spell = nullptr;
    }
    if (isAbilitySpell(magic.selected_spell_last_appearance)) magic.selected_spell_last_appearance = -1;
    for (auto node = magic.spellList.first; node;) {
        auto next = node->next;
        if (isAbilitySpell(static_cast<spell_t*>(node->element)->ID)) {
            magic.resetQuickCastSpell();
            list_RemoveNode(node);
        }
        node = next;
    }
    for (auto node = stats[player]->inventory.first; node;) {
        auto next = node->next;
        auto item = static_cast<Item*>(node->element);
        if (item->type == SPELL_ITEM && isAbilitySpell(item->appearance)) {
            for (auto& slot : players[player]->hotbar.slots()) if (slot.item == item->uid) slot.item = 0;
            for (auto& alternate : players[player]->hotbar.slotsAlternate()) {
                for (auto& slot : alternate) if (slot.item == item->uid) slot.item = 0;
            }
            list_RemoveNode(node);
        }
        node = next;
    }
    session.abilitiesPrepared = true;
}
std::filesystem::path selectionPath() { return std::filesystem::path(outputdir)/"delve-builds.json"; }
void load() {
    if (loaded) return; loaded = true;
    for (auto& p : selections) p.fill(empty);
    std::ifstream f(selectionPath()); std::string text((std::istreambuf_iterator<char>(f)),{});
    if (text.size()>65536) return;
    rapidjson::Document d; d.Parse(text.c_str());
    if (d.HasParseError() || !d.IsObject() || !d.HasMember("version") || !d["version"].IsInt() || (d["version"].GetInt()!=1 && d["version"].GetInt()!=2) || !d.HasMember("classes") || !d["classes"].IsArray()) return;
    const auto& classes=d["classes"];
    for (unsigned c=0;c<classes.Size() && c<26;++c) {
        if (!classes[c].IsArray() || classes[c].Size()!=4) continue;
        Build b=empty; bool valid=true;
        for (int i=0;i<4;++i) { if(!classes[c][i].IsInt()) {valid=false;break;} b[i]=classes[c][i].GetInt(); }
        if (valid && validBuild(c,b)) {
            if (d["version"].GetInt() == 1) { b[0] = b[1] = -1; migratedSelections = true; }
            for (auto& playerSelections : selections) playerSelections[c] = b;
        }
    }
}
bool save(int p) {
    rapidjson::Document d;d.SetObject();d.AddMember("version",2,d.GetAllocator());
    rapidjson::Value classes(rapidjson::kArrayType);
    for(const auto& b:selections[p]) { rapidjson::Value row(rapidjson::kArrayType);for(int n:b)row.PushBack(n,d.GetAllocator());classes.PushBack(row,d.GetAllocator()); }
    d.AddMember("classes",classes,d.GetAllocator());
    rapidjson::StringBuffer buf;rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buf);d.Accept(writer);
    auto temp=selectionPath();temp += ".tmp";
    { std::ofstream f(temp,std::ios::trunc);f<<buf.GetString()<<'\n';if(!f.good())return false; }
    // Keep the preceding valid selection if writing fails. Files live only in this runtime profile.
    std::error_code ec; auto backup=selectionPath();backup += ".previous";
    std::filesystem::remove(backup,ec);ec.clear();
    if(std::filesystem::exists(selectionPath())) {std::filesystem::rename(selectionPath(),backup,ec);if(ec)return false;}
    std::filesystem::rename(temp,selectionPath(),ec);
    if(ec) {std::error_code restore;std::filesystem::rename(backup,selectionPath(),restore);return false;}
    return true;
}
bool fromHost() {return net_packet->address.host==net_server.host && net_packet->address.port==net_server.port;}
int packetPlayer() {
    if(net_packet->len<9 || SDLNet_Read32(net_packet->data+4)!=uniqueGameKey)return -1;
    int p=net_packet->data[8];
    if(p<1 || p>=MAXPLAYERS || client_disconnected[p])return -1;
    const auto& a=net_clients[p-1];return net_packet->address.host==a.host && net_packet->address.port==a.port?p:-1;
}
void encodeBuild(unsigned char* data,const Build& b) {for(int i=0;i<4;++i)data[i]=b[i]+1;}
Build decodeBuild(const unsigned char* data) {Build b;for(int i=0;i<4;++i)b[i]=int(data[i])-1;return b;}
void sendState(int p) {
#ifdef BARONY_SMOKE_TESTS
    if(auditing)return;
#endif
    if(multiplayer!=SERVER || p<1 || !sessions[p].ready || client_disconnected[p])return;
    auto& s=sessions[p];std::memcpy(net_packet->data,"DBST",4);SDLNet_Write32(uniqueGameKey,net_packet->data+4);
    net_packet->data[8]=p;net_packet->data[9]=s.characterClass;encodeBuild(net_packet->data+10,s.build);
    SDLNet_Write16(s.cooldown[0],net_packet->data+14);SDLNet_Write16(s.cooldown[1],net_packet->data+16);SDLNet_Write16(s.recovery,net_packet->data+18);
    SDLNet_Write32(++s.serial,net_packet->data+20);SDLNet_Write32(s.acceptedSequence,net_packet->data+24);
    net_packet->len=28;net_packet->address=net_clients[p-1];sendPacketSafe(net_sock,-1,net_packet,p-1);
}
void sendBuild() {
    auto& s=sessions[clientnum];std::memcpy(net_packet->data,"DBLD",4);SDLNet_Write32(uniqueGameKey,net_packet->data+4);
    net_packet->data[8]=clientnum;net_packet->data[9]=s.characterClass;encodeBuild(net_packet->data+10,s.build);
    net_packet->len=14;net_packet->address=net_server;sendPacketSafe(net_sock,-1,net_packet,0);
#ifdef BARONY_SMOKE_TESTS
    if(std::getenv("BARONY_BALANCE_BUILDS"))printlog("[DELVE-BUILD-TEST] build_sent key=%u class=%d",uniqueGameKey,s.characterClass);
#endif
}
void apply(int p) {
    auto& s=sessions[p];if(s.applied || !s.ready || !stats[p] || !validBuild(s.characterClass,s.build))return;
    s.applied=true;auto st=stats[p];
    for(int slot=0;slot<2;++slot) if(s.build[slot]>=0) {
        const auto& o=option(s.characterClass,s.build[slot]);
        st->MAXHP=std::max(1,st->MAXHP+o.stats[0]);
        if (st->HP > 0) st->HP=std::clamp(st->HP+o.stats[0],1,st->MAXHP);
        st->MAXMP=std::max(0,st->MAXMP+o.stats[1]);st->MP=std::clamp(st->MP+o.stats[1],0,st->MAXMP);
        st->STR+=o.stats[2];st->DEX+=o.stats[3];st->CON+=o.stats[4];st->INT+=o.stats[5];st->PER+=o.stats[6];st->CHR+=o.stats[7];
        for(const auto& prof:o.proficiencies)if(prof.id>=0) {
            int old=st->getProficiency(prof.id);st->setProficiency(prof.id,std::max(old,std::min(99,old+prof.amount)));
        }
    }
    st->OLDHP=st->HP;
    printlog("[DELVE-BUILD] applied slot=%d class=%d choices=%d,%d,%d,%d",p,s.characterClass,s.build[0],s.build[1],s.build[2],s.build[3]);
}
bool alivePlaying(int p) {
    return playerValid(p) && Raid::active() && !intro && !loading && !fadeout && !gamePaused && !Raid::matchComplete() &&
        !client_disconnected[p] && Raid::outcome(p)==Raid::Outcome::Playing && players[p] && players[p]->entity && stats[p] && stats[p]->HP>0;
}
bool controllable(int p) {
    if(!alivePlaying(p) || !players[p]->bControlEnabled)return false;
    auto st=stats[p];return !st->getEffectActive(EFF_ASLEEP) && !st->getEffectActive(EFF_PARALYZED) && !st->getEffectActive(EFF_STUNNED) && !st->getEffectActive(EFF_FEAR) && !st->getEffectActive(EFF_PACIFY);
}
void cleanse(Entity& e) {
    for(int effect:{EFF_POISONED,EFF_BLEEDING,EFF_SLOW,EFF_WEBBED,EFF_CONFUSED,EFF_FEAR,EFF_ASLEEP,EFF_PARALYZED})e.setEffect(effect,false,0,true);
}
}
const Option& option(int c,int i) {return catalogue[std::clamp(c,0,25)*10+std::clamp(i,0,9)];}
bool isAbilitySpell(int spellId) { return spellId >= firstAbilitySpell && spellId < firstAbilitySpell + 104; }
bool skillPenaltiesDisabled(int spellId) { return Raid::active() || isAbilitySpell(spellId); }
const Option* spellOption(int spellId) {
    if (!isAbilitySpell(spellId)) return nullptr;
    int offset = spellId - firstAbilitySpell;
    return &option(offset / 4, 6 + offset % 4);
}
int spellIcon(int spellId) {
    auto entry = spellOption(spellId);
    if (!entry) return spellId;
    const auto& ability = entry->ability;
    if (ability.heal || ability.cleanse) return SPELL_HEALING;
    if (ability.damage) return ability.physical ? SPELL_STRIKE : SPELL_MAGICMISSILE;
    if (ability.control >= 0) return SPELL_FEAR;
    for (auto effect : ability.self) {
        if (effect.id == EFF_INVISIBLE) return SPELL_INVISIBILITY;
        if (effect.id == EFF_FAST) return SPELL_SPEED;
    }
    return SPELL_REFLECT_MAGIC;
}
int abilityIcon(int player, int slot) {
    if (!playerValid(player) || slot < 0 || slot > 1 || !sessions[player].applied
        || sessions[player].build[slot + 2] < 6) return SPELL_FORCEBOLT;
    return spellIcon(abilitySpellId(sessions[player].characterClass, sessions[player].build[slot + 2]));
}
bool castAbilitySpell(int player, int spellId) {
    if (!isAbilitySpell(spellId)) return false;
    if (player != clientnum || !controllable(player)) return true;
    const auto& session = sessions[player];
    for (int slot = 0; slot < 2; ++slot) {
        if (session.build[slot + 2] >= 6 && abilitySpellId(session.characterClass, session.build[slot + 2]) == spellId) {
            if (session.cooldown[slot] || session.recovery) {
                messagePlayer(player, MESSAGE_MISC, "%s is not ready yet.", option(session.characterClass, session.build[slot + 2]).name);
            } else requestActivation(slot);
            break;
        }
    }
    return true;
}
bool validBuild(int c,const Build& b) {
    if(c<0 || c>=26)return false;
    for(int i=0;i<4;++i)if(b[i]!=-1 && (b[i]<0 || b[i]>9 || ((i<2)!=(b[i]<6))))return false;
    return (b[0]<0 || b[0]!=b[1]) && (b[2]<0 || b[2]!=b[3]);
}
Build selection(int p,int c) {load();return playerValid(p) && c>=0 && c<26?selections[p][c]:empty;}
bool setSelection(int p,int c,const Build& b,bool persist) {
    load();if(!playerValid(p) || !validBuild(c,b) || (Raid::active() && !intro))return false;
    selections[p][c]=b;if(persist && !save(p)) {messagePlayer(p,MESSAGE_MISC,"Could not save this build. It is selected for this session.");printlog("[DELVE-BUILD] selection save failed");}
    return true;
}
Build activeSelection(int player) { return playerValid(player) && sessions[player].ready && sessions[player].applied ? sessions[player].build : empty; }
int activeClass(int player) { return playerValid(player) ? sessions[player].characterClass : -1; }
void reduceCooldown(int player, int abilityIndex, unsigned int amount) {
    if (multiplayer != SERVER || !playerValid(player)) return;
    auto& session = sessions[player];
    for (int slot = 0; slot < 2; ++slot) if (session.build[slot + 2] == abilityIndex)
        session.cooldown[slot] -= std::min(session.cooldown[slot], amount);
}
void reset() {sessions.fill(Session{});lastTick=ticks;lastSend=ticks;DelvePassives::reset();DelveEffects::reset();}
void deployed(int p) {
    if(!Raid::active() || !playerValid(p))return;
    // Remote builds arrive from their owner and are applied only after native kit staging.
    if(multiplayer==SERVER && !players[p]->isLocalPlayer()) {equipmentReady(p);return;}
    auto& s=sessions[p];s.build=selection(p,client_classes[p]);
    if (migratedSelections && players[p]->isLocalPlayer()) {
        messagePlayer(p, MESSAGE_MISC, "Passives redesigned: old stat-perk slots were cleared. Active ability selections were kept.");
        if (save(p)) migratedSelections = false;
    }
#ifdef BARONY_SMOKE_TESTS
    if(std::getenv("BARONY_BALANCE_BUILDS") || std::getenv("BARONY_SMOKE_EQUIP_BUILD"))s.build={{0,1,6,7}};
#endif
    s.characterClass=client_classes[p];s.ready=true;apply(p);
    if(multiplayer==CLIENT)sendBuild();
}
void equipmentReady(int p) {
    if(multiplayer==SERVER && playerValid(p) && sessions[p].ready && Raid::hasLoadout(p)) {apply(p);sendState(p);}
}
void redeploy(int player) {
    if (!playerValid(player)) return;
    DelveEffects::resetPlayer(player);
    DelvePassives::resetPlayer(player);
    sessions[player].applied = false;
    sessions[player].abilitiesPrepared = false;
    sessions[player].cooldown = {};
    sessions[player].recovery = 0;
    deployed(player);
}
bool activate(int p,int slot) {
    if(multiplayer!=SERVER || slot<0 || slot>1 || !controllable(p))return false;
    auto& s=sessions[p];if(!s.ready || !s.applied || s.cooldown[slot] || s.recovery || s.build[slot+2]<6)return false;
    const auto& o=option(s.characterClass,s.build[slot+2]);const auto& a=o.ability;auto st=stats[p];auto e=players[p]->entity;
    if(st->MP<a.manaCost || st->HP<=a.hpCost) {messagePlayer(p,MESSAGE_MISC,"%s: insufficient HP or MP.",o.name);return false;}
    st->HP-=a.hpCost;e->modMP(-a.manaCost);s.cooldown[slot]=a.cooldown*TICKS_PER_SECOND;s.recovery=TICKS_PER_SECOND/2;
    DelvePassives::Scope passiveAction(e, DelvePassives::Channel::Ability,
        abilitySpellId(s.characterClass, s.build[slot + 2]), 0, a.manaCost, a.physical);
    passiveAction.commit();
    if(a.cleanse)cleanse(*e);
    if(a.heal)e->modHP(a.heal);
    if(a.mana)e->modMP(a.mana);
    for(const auto& effect:a.self)if(effect.id>=0)e->setEffect(effect.id,true,effect.ticks,true);
    DelveEffects::activated(p, s.build[slot + 2]);
    int hits=0;
    if(a.range>0) for(auto node=map.entities->first;node;) {
        auto next=node->next;auto target=static_cast<Entity*>(node->element);node=next;
        if(target==e || (target->behavior!=&actMonster && target->behavior!=&actPlayer) || !target->getStats() || target->getStats()->HP<=0 || !e->checkEnemy(target))continue;
        if(target->behavior==&actPlayer && (!alivePlaying(target->skill[2]) || Raid::protectedPlayer(target->skill[2])))continue;
        double dx=target->x-e->x,dy=target->y-e->y,distance=std::hypot(dx,dy);if(distance>a.range)continue;
        double angle=std::atan2(dy,dx),diff=std::remainder(angle-e->yaw,2*PI);
        if(a.arc<360 && std::abs(diff)>a.arc*PI/360.0)continue;
        auto savedHit=hit;
        lineTraceTarget(e,e->x,e->y,angle,distance+4,0,false,target);
        bool visible=hit.entity==target;hit=savedHit;if(!visible)continue;
        auto ts=target->getStats();
        // Reflection blocks instant magic hits; no hidden, unreflectable damage bypass.
        if(!a.physical && a.damage && target->getReflection()>0)continue;
        int damage=0;
        if(a.damage) {
            int power=std::clamp(a.physical?statGetSTR(st,e):statGetINT(st,e),0,24)/4;
            int base=a.damage+std::clamp(st->LVL-1,0,6)+power;
            if(a.physical) base=std::max(1,base-std::max(0,AC(ts))/2);
            else base=std::max(1,base/(1+std::max(0,Entity::getMagicResistance(ts))));
            double race=Entity::getDamageTableMultiplier(target,*ts,a.physical?DAMAGE_TABLE_SWORD:DAMAGE_TABLE_MAGIC,nullptr,nullptr,e);
            damage=std::max(1,int(base*race));int before=ts->HP;
            target->modHP(-damage);damage=std::clamp(before-ts->HP,0,std::max(0,before));
            e->killedByMonsterObituary(target,!a.physical);
            target->setObituary("was defeated by a Dungeon Delve ability.");
            if(ts->HP<=0) {
                ts->killer_uid=e->getUID();
                e->awardXP(target,true,true);
                Raid::recordKill(*e,*target);
            }
            if(a.leech)e->modHP(damage*a.leech/100);
        }
        bool controlled = a.control >= 0 && ts->HP > 0 && Raid::applyControl(*target,e,a.control,a.controlTicks,true);
        if (damage > 0 || controlled) DelveEffects::impact(p, s.build[slot + 2], *target, controlled);
        if(target->behavior==&actMonster && ts->HP>0)target->monsterAcquireAttackTarget(*e,MONSTER_STATE_PATH);
        if(++hits>=4)break;
    }
    messagePlayer(p,MESSAGE_MISC,"%s activated.",o.name);
    printlog("[DELVE-BUILD] activated slot=%d ability=%s hits=%d",p,o.name,hits);
    serverUpdatePlayerStats();sendState(p);return true;
}
void requestActivation(int slot) {
    if(slot<0 || slot>1 || !controllable(clientnum)) {
#ifdef BARONY_SMOKE_TESTS
        if(std::getenv("BARONY_BALANCE_BUILDS"))printlog("[DELVE-BUILD-TEST] blocked local=%d active=%d intro=%d loading=%d fade=%d pause=%d complete=%d disconnected=%d outcome=%d hp=%d ctrl=%d sleep=%d para=%d stun=%d fear=%d pacify=%d",clientnum,Raid::active(),intro,loading,fadeout,gamePaused,Raid::matchComplete(),client_disconnected[clientnum],int(Raid::outcome(clientnum)),stats[clientnum]->HP,players[clientnum]->bControlEnabled,stats[clientnum]->getEffectActive(EFF_ASLEEP),stats[clientnum]->getEffectActive(EFF_PARALYZED),stats[clientnum]->getEffectActive(EFF_STUNNED),stats[clientnum]->getEffectActive(EFF_FEAR),stats[clientnum]->getEffectActive(EFF_PACIFY));
#endif
        return;
    }
    if(multiplayer==SERVER) {activate(clientnum,slot);return;}
    auto& s=sessions[clientnum];if(!s.applied || s.cooldown[slot] || s.recovery || s.build[slot+2]<6)return;
    std::memcpy(net_packet->data,"DACT",4);SDLNet_Write32(uniqueGameKey,net_packet->data+4);
    net_packet->data[8]=clientnum;net_packet->data[9]=slot;SDLNet_Write32(++s.requestSequence,net_packet->data+10);
    net_packet->len=14;net_packet->address=net_server;sendPacketSafe(net_sock,-1,net_packet,0);
}
void receiveBuild() {
#ifdef BARONY_SMOKE_TESTS
    if(std::getenv("BARONY_BALANCE_BUILDS") && !auditing)printlog("[DELVE-BUILD-TEST] build_packet length=%d player=%d class=%d key=%u expected=%u",net_packet->len,int(net_packet->data[8]),int(net_packet->data[9]),SDLNet_Read32(net_packet->data+4),uniqueGameKey);
#endif
    if(!Raid::active() || multiplayer!=SERVER || net_packet->len!=14)return;
    int p=packetPlayer();if(p<0)return;int c=net_packet->data[9];Build b=decodeBuild(net_packet->data+10);
    if(c!=client_classes[p] || !validBuild(c,b))return;
    auto& s=sessions[p];
    if(s.ready) {if(s.characterClass==c && s.build==b)sendState(p);return;}
    s.ready=true;s.characterClass=c;s.build=b;equipmentReady(p);
}
void receiveActivation() {
    if(!Raid::active() || multiplayer!=SERVER || net_packet->len!=14)return;
    int p=packetPlayer();if(p<0)return;int slot=net_packet->data[9];unsigned int seq=SDLNet_Read32(net_packet->data+10);
    auto& s=sessions[p];if(!s.ready || !seq || seq<=s.acceptedSequence)return;
    s.acceptedSequence=seq;activate(p,slot);sendState(p);
}
void receiveState() {
    if(!Raid::active() || multiplayer!=CLIENT || net_packet->len!=28 || !fromHost() || SDLNet_Read32(net_packet->data+4)!=uniqueGameKey || net_packet->data[8]!=clientnum)return;
    auto& s=sessions[clientnum];unsigned int serial=SDLNet_Read32(net_packet->data+20);
    if(serial<=s.receivedSerial || net_packet->data[9]!=s.characterClass || decodeBuild(net_packet->data+10)!=s.build)return;
    s.receivedSerial=serial;s.cooldown[0]=SDLNet_Read16(net_packet->data+14);s.cooldown[1]=SDLNet_Read16(net_packet->data+16);s.recovery=SDLNet_Read16(net_packet->data+18);s.acceptedSequence=SDLNet_Read32(net_packet->data+24);
}
void tick() {
    unsigned int delta=ticks-lastTick;lastTick=ticks;
    if(!Raid::active() || intro || loading || fadeout || gamePaused || Raid::matchComplete()) { DelveEffects::tick(0); return; }
    prepareAbilities(clientnum);
    delta=std::min<unsigned int>(delta,TICKS_PER_SECOND);
    DelvePassives::tick(delta);
    for(auto& s:sessions) {for(auto& cd:s.cooldown)cd-=std::min(cd,delta);s.recovery-=std::min(s.recovery,delta);}
    DelveEffects::tick(delta);
    if(ticks-lastSend>=TICKS_PER_SECOND) {
        lastSend=ticks;
        if(multiplayer==CLIENT && sessions[clientnum].ready && !sessions[clientnum].receivedSerial)sendBuild();
        if(multiplayer==SERVER)for(int p=1;p<MAXPLAYERS;++p)sendState(p);
    }
    if(controllable(clientnum) && players[clientnum]->shootmode && !command && !Input::inputs[clientnum].isDisabled()) {
        for(int i=0;i<2;++i)if(Input::inputs[clientnum].consumeBinaryToggle(i==0?"Delve Ability 1":"Delve Ability 2"))requestActivation(i);
    }
}
unsigned int cooldownRemaining(int player, int slot) {
    if (!Raid::active() || !playerValid(player) || slot < 0 || slot >= 2) return 0;
    const auto& session = sessions[player];
    if (!session.applied || session.build[slot + 2] < 6) return 0;
    return session.cooldown[slot];
}
void drawHUD(int player) {
    if (!Raid::active() || player != clientnum || intro || loading || fadeout || gamePaused
        || Raid::matchComplete() || Raid::outcome(player) != Raid::Outcome::Playing || Raid::protectedPlayer(player)
        || !players[player] || !players[player]->shootmode || players[player]->gui_mode != GUI_MODE_NONE
        || !stats[player] || stats[player]->HP <= 0 || !sessions[player].applied) return;
    const DelveHUD::Canvas hud;
    DelveEffects::drawHUD(player);
    const auto& session = sessions[player];
    for (int slot = 0; slot < 2; ++slot) {
        const auto perkIndex = session.build[slot];
        if (perkIndex >= 0 && perkIndex < 6) {
            const auto& entry = option(session.characterClass, perkIndex);
            const auto& definition = DelvePassives::rule(session.characterClass, perkIndex);
            hud.cooldown({entry.name, DelvePassives::cooldownRemaining(player, slot),
                static_cast<unsigned int>(definition.cooldown), slot, true});
        }
    }
}
#ifdef BARONY_SMOKE_TESTS
void smokeCooldownHUD(int phase) {
    auto& session = sessions[clientnum];
    session.cooldown = {};
    for (int slot = 0; slot < 2; ++slot) {
        const auto index = session.build[slot + 2];
        if (phase < 2 && index >= 6)
            session.cooldown[slot] = option(session.characterClass, index).ability.cooldown * TICKS_PER_SECOND / (phase + 1);
    }
    DelvePassives::smokeCooldownHUD(phase);
}
void smokeEquip(int player, int characterClass, int firstPerk, int secondPerk) {
    auto& session = sessions[player];
    session = Session{}; session.characterClass = characterClass;
    session.ready = session.applied = true;
    session.build = {{firstPerk, secondPerk, 6, 7}};
}
bool smokeReady() {return sessions[0].ready && sessions[0].applied && sessions[0].abilitiesPrepared && sessions[1].ready && sessions[1].applied;}
bool smokePresentAbility(int characterClass, int index) {
    if (multiplayer != SERVER || !DelveEffects::validProfile(characterClass, index) || index < 6) return false;
    for (int player = 0; player < 2; ++player) {
        for (int effect = 0; effect < NUMEFFECTS; ++effect) players[player]->entity->setEffect(effect, false, 0, true);
        stats[player]->HP = 900; stats[player]->MAXHP = 1000; stats[player]->MP = stats[player]->MAXMP = 1000;
        players[player]->entity->flags[BURNING] = false;
        DelvePassives::resetPlayer(player); DelveEffects::resetPlayer(player);
    }
    auto& session = sessions[0]; session = Session{};
    session.characterClass = client_classes[0] = characterClass;
    session.build = {{-1, -1, index, index == 6 ? 7 : 6}};
    session.ready = session.applied = session.abilitiesPrepared = true;
    Raid::smokeResetControl();
    return activate(0, 0);
}
bool smokeNetworkConfirmed() {const auto& s=sessions[multiplayer==CLIENT?clientnum:1];return s.acceptedSequence>0 && s.cooldown[0]>0;}
bool smokeAudit(Entity& host,Entity& guest) {
    auditing=true;
    DelveEffects::smokeQuiet(true);
    std::ofstream out("build-audit.jsonl",std::ios::trunc);bool all=true;int count=0;
    std::ofstream balance("skill-balance.jsonl",std::ios::trunc);
    auto check=[&](const std::string& name,bool ok) {out<<"{\"name\":"<<'"'<<name<<"\",\"pass\":"<<(ok?"true":"false")<<"}\n";all&=ok;++count;if(!ok)printlog("[DELVE-BUILD-TEST] FAIL %s",name.c_str());};
    struct Snapshot {
        Stat* s;std::array<Sint32*,13> fields;std::array<Sint32,13> values{};std::array<int,NUMPROFICIENCIES> profs{};
        Snapshot(Stat* st):s(st),fields{{&st->HP,&st->MAXHP,&st->MP,&st->MAXMP,&st->STR,&st->DEX,&st->CON,&st->INT,&st->PER,&st->CHR,&st->EXP,&st->LVL,&st->OLDHP}} {for(int i=0;i<13;++i)values[i]=*fields[i];for(int i=0;i<NUMPROFICIENCIES;++i)profs[i]=s->getProficiency(i);}
        void restore() {for(int i=0;i<13;++i)*fields[i]=values[i];for(int i=0;i<NUMPROFICIENCIES;++i)s->setProficiencyUnsafe(i,profs[i]);}
    } originalHost(stats[0]),originalGuest(stats[1]);
    auto originalSessions=sessions;auto originalSelections=selections;int originalClass=client_classes[0];double guestX=guest.x,guestY=guest.y;guest.x=host.x+24;guest.y=host.y;
    auto clearEffects=[](Entity& e) {for(int i=0;i<NUMEFFECTS;++i)e.setEffect(i,false,0,false);e.flags[BURNING]=false;};
    auto originalHostility = ShopkeeperPlayerHostility;
    auto originalType = stats[0]->type;
    auto originalRace = stats[0]->playerRace;
    auto originalMask = stats[0]->mask;
    stats[0]->mask = nullptr;
    auto merchant = summonMonster(SHOPKEEPER, host.x + 32, host.y);
    check("shopkeeper_fixture_spawned", merchant && merchant->getStats());
    if (merchant && merchant->getStats()) {
        for (int race : {RACE_HUMAN, RACE_SKELETON, RACE_VAMPIRE, RACE_SUCCUBUS, RACE_GOATMAN, RACE_AUTOMATON,
            RACE_INCUBUS, RACE_GOBLIN, RACE_INSECTOID, RACE_GNOME, RACE_GREMLIN, RACE_DRYAD, RACE_MYCONID, RACE_SALAMANDER}) {
            ShopkeeperPlayerHostility.reset();
            stats[0]->playerRace = race; stats[0]->type = getMonsterFromPlayerRace(race);
            check("shopkeeper_all_races_welcome_" + std::to_string(race),
                ShopkeeperPlayerHostility.playerRaceCheckHostility(0, stats[0]->type)
                && !merchant->checkEnemy(&host) && !host.checkEnemy(merchant)
                && merchant->checkFriend(&host) && host.checkFriend(merchant));
        }
        for (Monster form : {RAT, SPIDER, TROLL, CREATURE_IMP}) {
            ShopkeeperPlayerHostility.reset(); stats[0]->type = form;
            check("shopkeeper_shapeshift_welcome_" + std::to_string(form), !merchant->checkEnemy(&host) && !host.checkEnemy(merchant));
        }
        ShopkeeperPlayerHostility.onShopkeeperHit(merchant, merchant->getStats(), &host);
        check("shopkeeper_reacts_to_actual_aggression", ShopkeeperPlayerHostility.isPlayerEnemy(0)
            && merchant->checkEnemy(&host) && host.checkEnemy(merchant));
        list_RemoveNode(merchant->mynode);
    }
    ShopkeeperPlayerHostility = originalHostility;
    stats[0]->type = originalType; stats[0]->playerRace = originalRace; stats[0]->mask = originalMask;
    DelveHUD::Cooldown indicator{"Test", 300, 600, 0, false};
    check("cooldown_indicator_half_progress", indicator.visible() && indicator.fraction() == .5f);
    indicator.remaining = 0;
    check("cooldown_indicator_ready_hidden", !indicator.visible() && indicator.fraction() == 0.f);
    indicator.remaining = 1200;
    check("cooldown_indicator_progress_clamped", indicator.fraction() == 1.f);
    indicator.total = 0;
    check("cooldown_indicator_no_recovery_hidden", !indicator.visible());
    indicator.total = 600; indicator.name = nullptr;
    check("cooldown_indicator_unequipped_hidden", !indicator.visible());
    registerAbilitySpells();
    auto savedHotbar = players[0]->hotbar.slots();
    const auto inventorySize = list_Size(&stats[0]->inventory);
    int inventoryAbilities = 0;
    for (auto node = stats[0]->inventory.first; node; node = node->next) {
        const auto item = static_cast<Item*>(node->element);
        inventoryAbilities += item->type == SPELL_ITEM && isAbilitySpell(item->appearance);
    }
    check("hud_only_no_inventory_abilities", inventoryAbilities == 0);
    auto savedSpell = players[0]->magic.selectedSpell();
    sessions[0].abilitiesPrepared = false;
    prepareAbilities(0);
    bool sameHotbar = true;
    for (size_t slot = 0; slot < savedHotbar.size(); ++slot) sameHotbar &= savedHotbar[slot].item == players[0]->hotbar.slots()[slot].item;
    check("hud_only_preserves_inventory_hotbar", sameHotbar && list_Size(&stats[0]->inventory) == inventorySize);
    check("hud_only_preserves_native_spell", savedSpell == players[0]->magic.selectedSpell());
    const int legacyId = abilitySpellId(originalClass, 6);
    addSpell(legacyId, 0, true);
    bool legacyCreated = false;
    for (auto node = stats[0]->inventory.first; node; node = node->next) {
        auto item = static_cast<Item*>(node->element);
        if (item->type != SPELL_ITEM || item->appearance != legacyId) continue;
        useItem(item, 0);
        players[0]->hotbar.slots()[0].item = item->uid;
        players[0]->magic.selected_spell_alternate[0] = players[0]->magic.selectedSpell();
        legacyCreated = true;
        break;
    }
    check("hud_only_legacy_fixture", legacyCreated);
    sessions[0].abilitiesPrepared = false;
    prepareAbilities(0);
    check("hud_only_removes_legacy_items", list_Size(&stats[0]->inventory) == inventorySize);
    check("hud_only_clears_legacy_selection", !players[0]->magic.selectedSpell()
        && !players[0]->magic.selected_spell_alternate[0] && !players[0]->hotbar.slots()[0].item);
    players[0]->magic.equipSpell(savedSpell);
    players[0]->hotbar.slots() = savedHotbar;
    check("spell_vanilla_passthrough", !castAbilitySpell(0, SPELL_FORCEBOLT));
    check("spell_unowned_rejected", castAbilitySpell(0, abilitySpellId((originalClass + 1) % 26, 6)) && sessions[0].cooldown == originalSessions[0].cooldown);
    check("xp_zero",Raid::experienceGain(0)==0);check("xp_negative",Raid::experienceGain(-2)==-2);
    check("xp_skill_two_point_five",Raid::experienceGain(2)==5);check("xp_combat_two_point_five_rounded",Raid::experienceGain(17)==43);check("xp_cache_two_point_five",Raid::experienceGain(30)==75);
    check("duplicate_perks_rejected",!validBuild(0,{{0,0,6,7}}));check("duplicate_abilities_rejected",!validBuild(0,{{0,1,6,6}}));
    check("cross_slot_rejected",!validBuild(0,{{6,1,0,7}}));check("unknown_class_rejected",!validBuild(26,empty));check("unknown_option_rejected",!validBuild(0,{{0,1,6,10}}));
    check("mid_match_edit_rejected",!setSelection(0,0,empty,false));
    check("per_class_selection_isolated",selection(0,0)==originalSelections[0][0]);
    for(int c=0;c<26;++c) {
        client_classes[0]=c;
        for(int i=0;i<6;++i) {
            originalHost.restore();const auto& o=option(c,i);sessions[0]=Session{};
            check("penalty_free_perk_" + std::to_string(c) + "_" + std::to_string(i),
                std::all_of(o.stats.begin(), o.stats.end(), [](int value) { return value >= 0; }));
            auto& s=sessions[0];s.characterClass=c;s.ready=true;s.build={{i,-1,-1,-1}};apply(0);
            bool ok=stats[0]->MAXHP==std::max(1,originalHost.values[1]+o.stats[0]) && stats[0]->MAXMP==std::max(0,originalHost.values[3]+o.stats[1]);
            for(int n=0;n<6;++n)ok &= *originalHost.fields[4+n]==originalHost.values[4+n]+o.stats[2+n];
            for(auto prof:o.proficiencies)if(prof.id>=0)ok &= stats[0]->getProficiency(prof.id)==std::max(originalHost.profs[prof.id],std::min(99,originalHost.profs[prof.id]+prof.amount));
            Snapshot once(stats[0]);apply(0);for(int n=0;n<13;++n)ok &= *once.fields[n]==once.values[n];
            check(std::string("perk_")+std::to_string(c)+"_"+std::to_string(i),ok);
        }
        for(int i=6;i<10;++i) {
            originalHost.restore();originalGuest.restore();clearEffects(host);clearEffects(guest);Raid::smokeResetControl();
            sessions[0]=Session{};auto& s=sessions[0];s.characterClass=c;s.ready=s.applied=true;s.build={{-1,-1,i,-1}};
            stats[0]->HP=250;stats[0]->MAXHP=500;stats[0]->MP=250;stats[0]->MAXMP=500;
            stats[1]->HP=stats[1]->MAXHP=1000;
            const auto& a=option(c,i).ability;int hp=stats[0]->HP,mp=stats[0]->MP,ghp=stats[1]->HP;
            check("penalty_free_ability_" + std::to_string(c) + "_" + std::to_string(i), a.hpCost == 0);
            int spellId = abilitySpellId(c, i);
            auto spell = getSpellFromID(spellId);
            check("spell_definition_" + std::to_string(c) + "_" + std::to_string(i),
                spell && spell->mana == a.manaCost && std::string(spell->getSpellName()) == option(c,i).name
                && ItemTooltips.getSpellNodeFromSpellID(spellId) && spell->hide_from_ui);
            Item tooltipItem{};
            tooltipItem.type = SPELL_ITEM; tooltipItem.appearance = spellId; tooltipItem.identified = true;
            const bool previousShootmode = players[0]->shootmode;
            players[0]->shootmode = false;
            players[0]->hud.updateFrameTooltip(&tooltipItem, 300, 100, Field::LEFT);
            players[0]->shootmode = previousShootmode;
            auto tooltip = players[0]->inventoryUI.tooltipFrame;
            auto attributes = tooltip ? tooltip->findFrame("inventory mouse tooltip attributes frame") : nullptr;
            auto description = attributes ? attributes->findField("inventory mouse tooltip attributes text") : nullptr;
            bool descriptionFits = description && !description->isDisabled();
            if (descriptionFits) {
                auto compact = [](std::string value) {
                    value.erase(std::remove_if(value.begin(), value.end(), [](char character) {
                        return character == ' ' || character == '\n' || character == '\r';
                    }), value.end());
                    return value;
                };
                descriptionFits = compact(description->getText()) == compact(option(c,i).description);
                auto rendered = Text::get(description->getLongestLine().c_str(), description->getFont(), description->getTextColor(), description->getOutlineColor());
                descriptionFits &= rendered && rendered->getWidth() <= description->getSize().w;
                int lineHeight = 0;
                Font::get(description->getFont())->sizeText("_", nullptr, &lineHeight);
                descriptionFits &= description->getSize().y + description->getNumTextLines() * lineHeight <= attributes->getSize().h;
                descriptionFits &= description->getSize().x + description->getSize().w <= attributes->getSize().w;
                if (!descriptionFits) printlog("[DELVE-TOOLTIP-BOUNDS] spell=%d equal=%d rendered=%ux%u field=%d,%d,%d,%d parent=%d,%d lines=%d lineheight=%d", spellId, compact(description->getText()) == compact(option(c,i).description), rendered ? rendered->getWidth() : 0, rendered ? rendered->getHeight() : 0, description->getSize().x, description->getSize().y, description->getSize().w, description->getSize().h, attributes->getSize().w, attributes->getSize().h, description->getNumTextLines(), lineHeight);
            }
            check("tooltip_description_" + std::to_string(c) + "_" + std::to_string(i), descriptionFits);
            if (!descriptionFits) printlog("[DELVE-TOOLTIP-TEST] spell=%d field=%d disabled=%d text=%s", spellId, description != nullptr, description ? description->isDisabled() : -1, description ? description->getText() : "missing");
            const auto effectCount = DelveEffects::smokeCount(0, i);
            if(a.control>=0)for(auto node=map.entities->first;node;node=node->next) {
                auto target=static_cast<Entity*>(node->element);
                if(target!=&host && target->getStats())target->setEffect(a.control,false,0,false);
            }
            castSpellInit(host.getUID(), spell, false, false);
            check("skill_effect_cast_" + std::to_string(c) + "_" + std::to_string(i), DelveEffects::smokeCount(0, i) > effectCount);
            const auto activeVisual = DelveEffects::view(0, i);
            unsigned int expectedVisual = 0;
            for (const auto& effect : a.self) if (effect.id >= 0) expectedVisual = std::max(expectedVisual, static_cast<unsigned int>(std::max(0, stats[0]->EFFECTS_TIMERS[effect.id])));
            if(a.control>=0)for(auto node=map.entities->first;node;node=node->next) {
                auto target=static_cast<Entity*>(node->element);
                if(target!=&host && target->getStats() && target->getStats()->getEffectActive(a.control))
                    expectedVisual=std::max(expectedVisual,static_cast<unsigned int>(std::max(0,target->getStats()->EFFECTS_TIMERS[a.control])));
            }
            check("skill_effect_duration_" + std::to_string(c) + "_" + std::to_string(i), activeVisual.remaining == expectedVisual);
            bool ok=s.cooldown[0] > 0;
            check("cooldown_hud_ability_" + std::to_string(c) + "_" + std::to_string(i), cooldownRemaining(0, 0) == s.cooldown[0]);
            ok &= s.cooldown[0]==unsigned(a.cooldown*TICKS_PER_SECOND) && s.recovery==unsigned(TICKS_PER_SECOND/2);
            ok &= stats[0]->MP==mp-a.manaCost+a.mana;
            if(!a.leech)ok &= stats[0]->HP==hp-a.hpCost+a.heal;
            for(auto effect:a.self)if(effect.id>=0)ok &= stats[0]->getEffectActive(effect.id) && stats[0]->EFFECTS_TIMERS[effect.id]==effect.ticks;
            if(a.damage)ok &= stats[1]->HP<ghp;
            if(a.control>=0)ok &= stats[1]->getEffectActive(a.control)>0;
            const auto acceptedEffects = DelveEffects::smokeCount(0, i);
            int after=stats[0]->HP;ok &= !activate(0,0) && stats[0]->HP==after;
            check("skill_effect_rejected_cast_inert_" + std::to_string(c) + "_" + std::to_string(i), DelveEffects::smokeCount(0, i) == acceptedEffects);
            check(std::string("ability_")+std::to_string(c)+"_"+std::to_string(i),ok);
            balance << "{\"class\":" << c << ",\"index\":" << i
                << ",\"cooldown_seconds\":" << a.cooldown << ",\"mana_cost\":" << a.manaCost
                << ",\"damage\":" << ghp - stats[1]->HP << ",\"healing\":" << stats[0]->HP - hp
                << ",\"mana_restored\":" << a.mana << ",\"control_ticks\":"
                << (a.control >= 0 ? stats[1]->EFFECTS_TIMERS[a.control] : 0) << "}\n";
            if(a.leech) {
                s.cooldown[0]=s.recovery=0;
                clearEffects(host);clearEffects(guest);Raid::smokeResetControl();
                stats[0]->HP=250;stats[0]->MP=250;stats[1]->HP=900;
                guest.setEffect(EFF_DELAY_PAIN,true,100,false);
                bool accepted=activate(0,0);
                check("leech_requires_actual_health_loss_"+std::to_string(c)+"_"+std::to_string(i),
                    accepted && stats[1]->HP==900 && stats[0]->HP==250+a.heal);
                clearEffects(host);clearEffects(guest);
                s.cooldown[0]=s.recovery=0;
                stats[0]->HP=250;stats[0]->MP=250;stats[1]->HP=900;
                guest.setEffect(EFF_DELAY_PAIN,Uint8(249),100,false);
                accepted=activate(0,0);
                int actualLoss=900-stats[1]->HP;
                check("leech_matches_mitigated_health_loss_"+std::to_string(c)+"_"+std::to_string(i),
                    accepted && actualLoss>0 && stats[0]->HP==250+a.heal+actualLoss*a.leech/100);
                clearEffects(host);clearEffects(guest);
            }
            s.cooldown[0]=s.recovery=0;
            clearEffects(host);clearEffects(guest);Raid::smokeResetControl();
            stats[0]->HP=stats[0]->MAXHP;stats[0]->MP=stats[0]->MAXMP;stats[1]->HP=1000;
            check("skill_resources_capped_"+std::to_string(c)+"_"+std::to_string(i),
                activate(0,0) && stats[0]->HP<=stats[0]->MAXHP && stats[0]->MP<=stats[0]->MAXMP);
            s.cooldown[0]=s.recovery=0;
            clearEffects(host);clearEffects(guest);Raid::smokeResetControl();
            stats[0]->HP=250;stats[0]->MP=a.manaCost;stats[1]->HP=1000;
            check("skill_exact_mana_budget_"+std::to_string(c)+"_"+std::to_string(i),
                activate(0,0) && stats[0]->MP==a.mana);
            s.cooldown[0]=s.recovery=0;
            if(a.manaCost){stats[0]->MP=0;check("mana_cost_"+std::to_string(c)+"_"+std::to_string(i),!activate(0,0) && s.cooldown[0]==0);}
            if(a.hpCost){stats[0]->HP=a.hpCost;check("hp_cost_"+std::to_string(c)+"_"+std::to_string(i),!activate(0,0) && stats[0]->HP==a.hpCost && s.cooldown[0]==0);}
        }
    }
    originalHost.restore();originalGuest.restore();clearEffects(host);clearEffects(guest);sessions=originalSessions;client_classes[0]=originalClass;selections=originalSelections;
    stats[0]->INT = 0;
    for (int skill : {PRO_SORCERY, PRO_MYSTICISM, PRO_THAUMATURGY}) {
        stats[0]->setProficiencyUnsafe(skill, 0);
        check("no_beginner_penalty_" + std::to_string(skill), !isSpellcasterBeginner(0, &host, skill));
    }
    Item difficultBook{};
    difficultBook.type = SPELLBOOK_FIREBALL;
    difficultBook.status = SERVICABLE;
    check("no_spellbook_skill_penalty", !isSpellcasterBeginnerFromSpellbook(0, &host, stats[0], getSpellFromID(SPELL_FIREBALL), &difficultBook));
    check("no_spell_tooltip_penalty", skillPenaltiesDisabled(SPELL_FORCEBOLT) && skillPenaltiesDisabled(firstAbilitySpell));
    bool nativeCastsExact = true;
    for (int attempt = 0; attempt < 32; ++attempt) {
        stats[0]->setProficiencyUnsafe(PRO_SORCERY, 0);
        stats[0]->HP = stats[0]->MAXHP = 500;
        stats[0]->MP = stats[0]->MAXMP = 500;
        auto spell = getSpellFromID(SPELL_FORCEBOLT);
        int cost = getCostOfSpell(spell, &host);
        auto projectile = castSpell(host.getUID(), spell, false, false, false);
        nativeCastsExact &= projectile && stats[0]->MP == 500 - cost && stats[0]->HP == 500;
        if (projectile) list_RemoveNode(projectile->mynode);
    }
    check("low_skill_native_casts_no_fizzle_or_extra_mp", nativeCastsExact);
    originalHost.restore();
    // Native proficiency XP uses the real function, not the multiplier helper.
    stats[0]->setProficiencyUnsafe(PRO_APPRAISAL,0);int xp=stats[0]->EXP;
    bool increased=host.increaseSkill(PRO_APPRAISAL,false);check("native_proficiency_xp_two_point_five",increased && stats[0]->EXP-xp==5);
    originalHost.restore();
    stats[0]->LVL=stats[1]->LVL=1;stats[0]->EXP=0;
    auto rngPrediction=local_rng;int normalCombat=10+rngPrediction.rand()%10;
    host.awardXP(&guest,false,false);
    check("native_combat_exactly_two_point_five_once",stats[0]->EXP==(normalCombat*5+1)/2);
    originalHost.restore();originalGuest.restore();
    // A ray with an opponent outside its arc must not hit them.
    sessions[0]=Session{};sessions[0].ready=sessions[0].applied=true;sessions[0].characterClass=CLASS_WIZARD;sessions[0].build={{-1,-1,6,-1}};
    stats[0]->MP=500;stats[0]->MAXMP=500;double yaw=host.yaw;host.yaw+=PI;int health=stats[1]->HP;
    check("facing_rejects_targets",activate(0,0) && stats[1]->HP==health);host.yaw=yaw;
    sessions[0].cooldown[0]=sessions[0].recovery=0;stats[0]->MP=500;
    guest.setEffect(EFF_MAGICREFLECT,true,100,false);health=stats[1]->HP;
    check("native_reflection_blocks_magic_hit",activate(0,0) && stats[1]->HP==health);guest.setEffect(EFF_MAGICREFLECT,false,0,false);
    sessions[0].cooldown[0]=sessions[0].recovery=0;stats[0]->MP=500;
    host.setEffect(EFF_PARALYZED,true,100,false);check("control_locks_activation",!activate(0,0));host.setEffect(EFF_PARALYZED,false,0,false);
    sessions[0].cooldown[0]=sessions[0].recovery=0;stats[0]->MP=500;
    double nearX=guest.x;guest.x=host.x+170;health=stats[1]->HP;
    check("range_rejects_targets",activate(0,0) && stats[1]->HP==health);guest.x=nearX;
    sessions[0].cooldown[0]=sessions[0].recovery=0;stats[0]->MP=500;
    int wallX=int(host.x/16)+1,wallY=int(host.y/16);
    int tileAt=OBSTACLELAYER+wallY*MAPLAYERS+wallX*MAPLAYERS*map.height;
    int oldTile=map.tiles[tileAt];map.tiles[tileAt]=1;health=stats[1]->HP;
    check("wall_blocks_direct_attack",activate(0,0) && stats[1]->HP==health);map.tiles[tileAt]=oldTile;
    sessions[0].cooldown[0]=sessions[0].recovery=0;bool pause=gamePaused;gamePaused=true;
    check("pause_blocks_activation",!activate(0,0));gamePaused=pause;
    int livingHP=stats[0]->HP;stats[0]->HP=0;check("death_blocks_activation",!activate(0,0));stats[0]->HP=livingHP;
    // Packet handlers authenticate the endpoint, session, class, shape, categories and initial lock.
    auto savedAddress=net_packet->address;int savedLength=net_packet->len;
    std::vector<unsigned char> savedBytes(net_packet->data,net_packet->data+savedLength);
    auto makePacket=[&](const char* cmd,int length) {std::memcpy(net_packet->data,cmd,4);SDLNet_Write32(uniqueGameKey,net_packet->data+4);net_packet->data[8]=1;net_packet->address=net_clients[0];net_packet->len=length;};
    sessions[1]=Session{};makePacket("DBLD",14);net_packet->data[9]=client_classes[1];encodeBuild(net_packet->data+10,{{0,1,6,7}});
    net_packet->address.port^=1;receiveBuild();check("spoofed_endpoint_rejected",!sessions[1].ready);net_packet->address.port^=1;
    SDLNet_Write32(uniqueGameKey+1,net_packet->data+4);receiveBuild();check("stale_session_rejected",!sessions[1].ready);SDLNet_Write32(uniqueGameKey,net_packet->data+4);
    net_packet->len=13;receiveBuild();check("truncated_build_rejected",!sessions[1].ready);net_packet->len=14;
    net_packet->data[9]=26;receiveBuild();check("wrong_class_rejected",!sessions[1].ready);net_packet->data[9]=client_classes[1];
    encodeBuild(net_packet->data+10,{{0,0,6,7}});receiveBuild();check("duplicate_packet_rejected",!sessions[1].ready);
    encodeBuild(net_packet->data+10,{{0,1,6,7}});receiveBuild();check("valid_build_staged",sessions[1].ready && sessions[1].applied);
    auto locked=sessions[1].build;makePacket("DBLD",14);net_packet->data[9]=client_classes[1];encodeBuild(net_packet->data+10,{{2,3,8,9}});receiveBuild();check("mid_match_packet_swap_rejected",sessions[1].build==locked);
    makePacket("DACT",14);net_packet->data[9]=2;SDLNet_Write32(1,net_packet->data+10);receiveActivation();check("invalid_ability_slot_rejected",sessions[1].cooldown[0]==0 && sessions[1].cooldown[1]==0);
    makePacket("DACT",14);net_packet->data[9]=0;SDLNet_Write32(1,net_packet->data+10);receiveActivation();check("replayed_command_rejected",sessions[1].cooldown[0]==0);
    std::copy(savedBytes.begin(),savedBytes.end(),net_packet->data);net_packet->len=savedLength;net_packet->address=savedAddress;
    DelvePassives::smokeAudit(host, guest, check);
    sessions=originalSessions;originalHost.restore();originalGuest.restore();clearEffects(host);clearEffects(guest);Raid::smokeResetControl();
    DelveEffects::smokeAudit(host, guest, check);
    DelveEffects::smokeQuiet(false);
    guest.x=guestX;guest.y=guestY;serverUpdateEffects(1);serverUpdatePlayerStats();auditing=false;
    printlog("[DELVE-BUILD-TEST] %d checks %s",count,all?"PASS":"FAIL");
    return all;
}
#endif

}
