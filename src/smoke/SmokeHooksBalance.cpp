#include "SmokeHooksCommon.hpp"
#include "../main.hpp"
#include "../game.hpp"
#include "../entity.hpp"
#include "../stat.hpp"
#include "../items.hpp"
#include "../player.hpp"
#include "../net.hpp"
#include "../raid.hpp"
#include "../delve_builds.hpp"
#include "../delve_passives.hpp"
#include "../delve_effects.hpp"
#include "../delve_rarity.hpp"
#include "../monster.hpp"
#include "../ui/Frame.hpp"
#include "../ui/GameUI.hpp"
#include <filesystem>
#include "../collision.hpp"
#include "../magic/magic.hpp"
#include "../rapidjson/document.h"
#include "../rapidjson/writer.h"
#include "../rapidjson/stringbuffer.h"
#include <fstream>
#include <vector>
#include <array>
#include <cmath>
#include <sstream>

namespace Raid { bool smokeWorldReady(); void smokeNerfKit(int player); }
namespace SmokeTestHooks::Balance {
namespace {
using D=rapidjson::Document;
const int races[]={RACE_HUMAN,RACE_SKELETON,RACE_VAMPIRE,RACE_SUCCUBUS,RACE_GOATMAN,RACE_AUTOMATON,RACE_INCUBUS,RACE_GOBLIN,RACE_INSECTOID,RACE_GNOME,RACE_GREMLIN,RACE_DRYAD,RACE_MYCONID,RACE_SALAMANDER};
const std::vector<int>& gallerySkills() {
    static const auto skills = []() {
        std::vector<int> result;
        std::istringstream stream(SmokeHooksCommon::parseEnvString("BARONY_EFFECTS_SELECTION", ""));
        std::string entry;
        while (std::getline(stream,entry,',')) { const int ordinal = std::atoi(entry.c_str()); if (ordinal >= 0 && ordinal < 104) result.push_back(ordinal); }
        if (result.empty()) for (int ordinal = 0; ordinal < 104; ++ordinal) result.push_back(ordinal);
        return result;
    }();
    return skills;
}
double galleryPitch() {
    return std::clamp(std::atof(SmokeHooksCommon::parseEnvString("BARONY_EFFECTS_PITCH", "0.1").c_str()),-1.1,1.1);
}
bool galleryFloor(int tileX, int tileY) {
    if (tileX < 1 || tileY < 1 || tileX >= map.width-1 || tileY >= map.height-1) return false;
    const int offset = MAPLAYERS*(tileY+tileX*map.height), floor = map.tiles[offset];
    return floor && !map.tiles[offset+OBSTACLELAYER] && !swimmingtiles[floor] && !lavatiles[floor];
}
void number(D& d,const char* key,double value) { d.AddMember(rapidjson::Value(key,d.GetAllocator()),rapidjson::Value(value),d.GetAllocator()); }
void string(D& d,const char* key,const char* value) { d.AddMember(rapidjson::Value(key,d.GetAllocator()),rapidjson::Value(value,d.GetAllocator()),d.GetAllocator()); }
void emit(D& d) {
    rapidjson::StringBuffer b;rapidjson::Writer<rapidjson::StringBuffer> w(b);d.Accept(w);
    std::ofstream("balance.jsonl",std::ios::app)<<b.GetString()<<'\n';
}
void fresh(int c,int r,int appearance=0) {
    auto h=players[0]->entity;
    players[0]->magic.clearSelectedSpells();
    list_FreeAll(&players[0]->magic.spellList);
    stats[0]->clearStats();client_classes[0]=c;
    stats[0]->playerRace=r;stats[0]->stat_appearance=appearance;
    h->effectShapeshift=NOTHING;h->effectPolymorph=NOTHING;
    h->flags[BURNING]=false;
    initClass(0);::Raid::smokeNerfKit(0);
    stats[0]->type=appearance ? HUMAN : ::getMonsterFromPlayerRace(r);
}
void dump(int c,int r,const char* kind) {
    D d;d.SetObject();string(d,"kind",kind);number(d,"class",c);number(d,"race",r);
    auto s=stats[0];auto h=players[0]->entity;
    number(d,"hp",s->MAXHP);number(d,"mp",s->MAXMP);number(d,"starting_mp",s->MP);
    number(d,"str",statGetSTR(s,h));number(d,"dex",statGetDEX(s,h));number(d,"con",statGetCON(s,h));
    number(d,"int",statGetINT(s,h));number(d,"per",statGetPER(s,h));number(d,"chr",statGetCHR(s,h));
    number(d,"ac",AC(s));number(d,"attack",Entity::getAttack(h,s,true));number(d,"gold",s->GOLD);
    number(d,"reflection",h->getReflection());number(d,"magic_resistance",Entity::getMagicResistance(s));
    rapidjson::Value skills(rapidjson::kArrayType),equipment(rapidjson::kArrayType),inventory(rapidjson::kArrayType),spells(rapidjson::kArrayType),multipliers(rapidjson::kArrayType),pvpMultipliers(rapidjson::kArrayType);
    for(int i=0;i<NUMPROFICIENCIES;++i) skills.PushBack(s->getProficiency(i),d.GetAllocator());
    const char* slots[]={"weapon","shield","helmet","breastplate","gloves","shoes","cloak","amulet","ring","mask"};
    Item* pieces[]={s->weapon,s->shield,s->helmet,s->breastplate,s->gloves,s->shoes,s->cloak,s->amulet,s->ring,s->mask};
    for(int i=0;i<10;++i)if(pieces[i]) {
        rapidjson::Value item(rapidjson::kObjectType);item.AddMember("slot",rapidjson::Value(slots[i],d.GetAllocator()),d.GetAllocator());
        item.AddMember("name",rapidjson::Value(items[pieces[i]->type].getIdentifiedName(),d.GetAllocator()),d.GetAllocator());
        item.AddMember("type",pieces[i]->type,d.GetAllocator());item.AddMember("status",pieces[i]->status,d.GetAllocator());equipment.PushBack(item,d.GetAllocator());
    }
    for(auto n=s->inventory.first;n;n=n->next) {
        auto p=static_cast<Item*>(n->element);if(itemCategory(p)==SPELL_CAT)continue;
        rapidjson::Value item(rapidjson::kObjectType);
        item.AddMember("name",rapidjson::Value(items[p->type].getIdentifiedName(),d.GetAllocator()),d.GetAllocator());
        item.AddMember("type",p->type,d.GetAllocator());item.AddMember("count",p->count,d.GetAllocator());item.AddMember("status",p->status,d.GetAllocator());
        item.AddMember("category",itemCategory(p),d.GetAllocator());inventory.PushBack(item,d.GetAllocator());
    }
    for(auto n=players[0]->magic.spellList.first;n;n=n->next) {
        auto p=static_cast<spell_t*>(n->element);rapidjson::Value spell(rapidjson::kObjectType);
        spell.AddMember("id",p->ID,d.GetAllocator());spell.AddMember("name",rapidjson::Value(p->spell_internal_name,d.GetAllocator()),d.GetAllocator());
        spell.AddMember("mana",getCostOfSpell(p,h),d.GetAllocator());spells.PushBack(spell,d.GetAllocator());
    }
    for(int i=0;i<7;++i)multipliers.PushBack(Entity::getDamageTableMultiplier(h,*s,static_cast<DamageTableType>(i)),d.GetAllocator());
    for(int i=0;i<7;++i)pvpMultipliers.PushBack(Entity::getDamageTableMultiplier(h,*s,static_cast<DamageTableType>(i),nullptr,nullptr,players[1]->entity),d.GetAllocator());
    d.AddMember("skills",skills,d.GetAllocator());d.AddMember("equipment",equipment,d.GetAllocator());d.AddMember("inventory",inventory,d.GetAllocator());d.AddMember("spells",spells,d.GetAllocator());d.AddMember("incoming_multipliers",multipliers,d.GetAllocator());
    d.AddMember("pvp_incoming_multipliers",pvpMultipliers,d.GetAllocator());
    number(d,"asleep",s->getEffectActive(EFF_ASLEEP));emit(d);
}
struct Probe { int c;const char* name;int spell=0;int item=-1;int race=RACE_HUMAN;bool reflect=false;bool rear=false;int meleeWeapon=-1; };
const std::vector<Probe> probes={
    {CLASS_BARBARIAN,"barbarian_melee"},{CLASS_WARRIOR,"warrior_melee"},{CLASS_MONK,"monk_melee"},{CLASS_NINJA,"ninja_melee"},{CLASS_PALADIN,"paladin_melee"},{CLASS_BREWER,"brewer_melee"},
    {CLASS_NINJA,"ninja_rear",0,-1,RACE_HUMAN,false,true},
    {CLASS_PUNISHER,"punisher_starter_axe",0,-1,RACE_HUMAN,false,false,IRON_AXE},
    {CLASS_SAPPER,"sapper_melee"},{CLASS_BARD,"bard_melee"},{CLASS_HERMIT,"hermit_melee"},
    {CLASS_NINJA,"ninja_vs_skeleton",0,-1,RACE_SKELETON},
    {CLASS_WARRIOR,"warrior_vs_skeleton",0,-1,RACE_SKELETON},
    {CLASS_MONK,"monk_vs_skeleton",0,-1,RACE_SKELETON},
    {CLASS_WIZARD,"wizard_fireball",SPELL_FIREBALL},{CLASS_WIZARD,"wizard_cold",SPELL_COLD},
    {CLASS_ARCANIST,"arcanist_fire_staff",0,MAGICSTAFF_FIRE},{CLASS_CONJURER,"conjurer_lightning_staff",0,MAGICSTAFF_LIGHTNING},
    {CLASS_MESMER,"mesmer_cold_staff",0,MAGICSTAFF_COLD},{CLASS_MESMER,"mesmer_charm_staff",0,MAGICSTAFF_CHARM},
    {CLASS_SHAMAN,"shaman_poison_staff",0,MAGICSTAFF_POISON},{CLASS_SEXTON,"sexton_sleep",SPELL_SLEEP},
    {CLASS_NINJA,"ninja_paralysis_potion",0,POTION_PARALYSIS},{CLASS_BREWER,"brewer_acid",0,POTION_ACID},
    {CLASS_HUNTER,"hunter_longbow",0,LONGBOW},{CLASS_ARCANIST,"arcanist_crossbow",0,CROSSBOW},{CLASS_ROGUE,"rogue_shortbow",0,SHORTBOW},
    {CLASS_WIZARD,"fireball_vs_reflection",SPELL_FIREBALL,-1,RACE_HUMAN,true},
    {CLASS_ARCANIST,"fire_staff_vs_reflection",0,MAGICSTAFF_FIRE,RACE_HUMAN,true},
    {CLASS_HUNTER,"hunter_vs_skeleton",0,LONGBOW,RACE_SKELETON},
    {CLASS_WIZARD,"fireball_vs_skeleton",SPELL_FIREBALL,-1,RACE_SKELETON},
    {CLASS_WIZARD,"fireball_vs_insectoid",SPELL_FIREBALL,-1,RACE_INSECTOID},
    {CLASS_WIZARD,"fireball_vs_salamander",SPELL_FIREBALL,-1,RACE_SALAMANDER},
    {CLASS_SEXTON,"sleep_vs_skeleton",SPELL_SLEEP,-1,RACE_SKELETON},
    {CLASS_BREWER,"brewer_second_acid",0,POTION_ACID},
    {CLASS_SHAMAN,"shaman_web",SPELL_SPRAY_WEB},
    {CLASS_SHAMAN,"shaman_fear",SPELL_FEAR},
    {CLASS_JOKER,"joker_confuse",SPELL_CONFUSE}
};
int phase=0,index=0,lx=0,ly=0,preMana=0,spentMana=0,firstDamage=0;
Uint32 at=0;std::array<int,8> peak{};
Uint32 nextProbeAt=0;
const int effects[]={EFF_SLOW,EFF_ASLEEP,EFF_PARALYZED,EFF_CONFUSED,EFF_POISONED,EFF_BLEEDING,EFF_WEBBED,EFF_FEAR};
void target(const Probe& p) {
    auto h=players[0]->entity,g=players[1]->entity;
    g->flags[BURNING]=false;
    stats[1]->clearStats();stats[1]->playerRace=p.race;stats[1]->stat_appearance=0;stats[1]->type=::getMonsterFromPlayerRace(p.race);
    stats[1]->HP=stats[1]->MAXHP=1000;stats[1]->MP=stats[1]->MAXMP=1000;
    if(p.reflect)stats[1]->amulet=newItem(AMULET_MAGICREFLECTION,EXCELLENT,0,1,0,true,nullptr);
    h->teleporterMove(lx,ly,2);g->teleporterMove(lx+2,ly,2);h->yaw=0;g->yaw=p.rear ? 0:PI;
    h->vel_x=h->vel_y=g->vel_x=g->vel_y=0;
}
void check(const char* name,bool pass) {
    D d;d.SetObject();string(d,"kind","contract");string(d,"name",name);number(d,"pass",pass);emit(d);
    printlog("[BALANCE] check %s %s",pass ? "PASS":"FAIL",name);
}
void finish() {std::ofstream("balance-complete")<<"complete\n";printlog("[BALANCE] complete");phase=99;}
Entity* find(void (*behavior)(Entity*)) {
    for(auto n=map.entities->first;n;n=n->next){auto e=static_cast<Entity*>(n->element);if(e->behavior==behavior && (behavior!=&actMonster || (e->getStats() && (e->getStats()->type==RAT || e->getStats()->type==GOBLIN || e->getStats()->type==SKELETON))))return e;}
    return nullptr;
}
void use(int p,Entity& exit) {
    auto player=players[p]->entity;if(!player)return;
    const int x=static_cast<int>(exit.x)/16,y=static_cast<int>(exit.y)/16;
    const int sides[][2]={{p ? -1:1,0},{0,1},{0,-1},{p ? 1:-1,0}};
    for(auto& side:sides) {
        int tx=x+side[0],ty=y+side[1];
        if(tx<0 || ty<0 || tx>=map.width || ty>=map.height)continue;
        int offset=MAPLAYERS*(ty+tx*map.height),floor=map.tiles[offset];
        if(map.tiles[offset+OBSTACLELAYER] || swimmingtiles[floor] || lavatiles[floor] || checkObstacle(tx*16+8,ty*16+8,player,&exit))continue;
        if(player->teleporterMove(tx,ty,2))break;
    }
    if(p==0){selectedEntity[0]=&exit;inrange[0]=true;::Raid::useExit(exit);}
    else {
        const auto sync=SmokeHooksCommon::parseEnvString("BARONY_SMOKE_RAID_SYNC","");
        std::ofstream(sync+"/target-1")<<currentlevel<<' '<<exit.getUID()<<'\n';
    }
}
}
void normalTick() {
    if(intro || loading || !players[clientnum]->entity)return;
    if(multiplayer==CLIENT) {
        static bool logged=false;if(!logged){logged=true;printlog("[BALANCE] peer_identity race=%d",stats[clientnum]->playerRace);}return;
    }
    static bool done=false;
    if(done || multiplayer!=SERVER || !players[1]->entity || ticks<TICKS_PER_SECOND*2)return;
    done=true;std::ofstream("balance.jsonl",std::ios::trunc);
    auto h=players[0]->entity,g=players[1]->entity;
    D d;d.SetObject();string(d,"kind","normal_scope");number(d,"raid_active",::Raid::active());number(d,"hp",stats[0]->MAXHP);
    number(d,"reflection",h->getReflection());number(d,"guest_race",stats[1]->playerRace);
    number(d,"normal_xp_multiplier",::Raid::experienceGain(2));
    bool nativePrices=true;
    for(auto type:{SCROLL_ENCHANTWEAPON,SCROLL_ENCHANTARMOR}) {
        auto scroll=newItem(type,EXCELLENT,0,1,0,true,nullptr);
        nativePrices &= scroll->getGoldValue()==items[type].gold_value;
        free(scroll);
    }
    number(d,"normal_blessing_prices_retained",nativePrices);
    bool rarityInert=true;
    for (auto type : {IRON_SWORD,STEEL_HELM,RING_PROTECTION,SPELLBOOK_FIREBALL,GEM_DIAMOND,GEM_JEWEL,POTION_HEALING,POTION_RESTOREMAGIC,POTION_ACID,POTION_SPEED,TOOL_BEARTRAP}) {
        auto item=newItem(type,EXCELLENT,0,1,123,true,nullptr);
        const auto value=item->getGoldValue(), attack=item->weaponGetAttack();
        const auto heal=item->potionGetEffectHealth(nullptr,nullptr), damage=item->potionGetEffectDamage(nullptr,nullptr);
        const auto duration=item->potionGetEffectDurationMinimum(nullptr,nullptr);
        DelveRarity::assign(*item,DelveRarity::Tier::Legendary);
        rarityInert &= DelveRarity::tier(*item)==DelveRarity::Tier::Grey && DelveRarity::summary(*item).empty()
            && DelveRarity::scalePotency(*item,100)==100 && item->getGoldValue()==value && item->weaponGetAttack()==attack
            && item->potionGetEffectHealth(nullptr,nullptr)==heal && item->potionGetEffectDamage(nullptr,nullptr)==damage
            && item->potionGetEffectDurationMinimum(nullptr,nullptr)==duration;
        for (auto roll : DelveRarity::affixes(*item)) rarityInert &= roll.kind == DelveAffixes::Kind::Count;
        free(item);
    }
    for (int type = 0; type < NUMITEMS; ++type) {
        if (items[type].category != GEM) continue;
        auto gem = newItem(static_cast<ItemType>(type),BROKEN,0,1,123,true,nullptr);
        for (int quality = BROKEN; quality <= EXCELLENT; ++quality) {
            gem->status = static_cast<Status>(quality);
            const int nativeValue = type == GEM_JEWEL ? (quality == BROKEN ? 0 : 250 << (quality - 1)) : items[type].gold_value;
            rarityInert &= gem->getGoldValue() == nativeValue && DelveRarity::summary(*gem).empty();
        }
        free(gem);
    }
    number(d,"normal_gem_potion_trap_rarity_inert",rarityInert);
    Frame blessedSlot("normal blessed audit"); blessedSlot.setOwner(0); blessedSlot.setSize({0, 0, 48, 48});
    createPlayerInventorySlotFrameElements(&blessedSlot);
    auto blessedItem = newItem(IRON_SWORD, EXCELLENT, 2, 1, 0, true, nullptr);
    updateSlotFrameFromItem(&blessedSlot, blessedItem);
    number(d, "normal_blessed_background_retained", !blessedSlot.findFrame("beatitude status frame")->isDisabled());
    free(blessedItem);
    bool equipmentQualityInert = true, blessedAffixesInert = true;
    for (int type = 0; type < RMF_FIRST_ITEM; ++type) {
        auto item = newItem(static_cast<ItemType>(type),DECREPIT,-2,1,123,true,nullptr);
        if (DelveRarity::equipment(*item)) {
            DelveRarity::assign(*item,DelveRarity::Tier::Legendary,73);
            for (int blessing : {-3, 0, 1, 5}) {
                item->beatitude = blessing;
                for (auto roll : DelveRarity::affixes(*item)) blessedAffixesInert &= roll.kind == DelveAffixes::Kind::Count;
                blessedAffixesInert &= DelveRarity::summary(*item).empty();
            }
            item->beatitude = -2;
            for (int quality = BROKEN; quality <= EXCELLENT; ++quality) {
                item->status = static_cast<Status>(quality);
                DelveRarity::assign(*item,DelveRarity::Tier::Legendary,73);
                equipmentQualityInert &= item->status == static_cast<Status>(quality) && item->beatitude == -2;
            }
        }
        free(item);
    }
    number(d,"normal_equipment_roll_quality_inert",equipmentQualityInert);
    number(d,"normal_blessed_equipment_affixes_inert",blessedAffixesInert);
    number(d,"normal_equipment_affixes_inert",rarityInert);
    const bool originalInvisible=g->flags[INVISIBLE];
    g->flags[INVISIBLE]=true;
    number(d,"normal_concealment_inert",!::Raid::concealedFromObserver(*g,0));
    g->flags[INVISIBLE]=originalInvisible;
    stats[0]->setProficiencyUnsafe(PRO_APPRAISAL,0);int xp=stats[0]->EXP;h->increaseSkill(PRO_APPRAISAL,false);number(d,"normal_skill_xp",stats[0]->EXP-xp);
    number(d,"delve_ability_rejected",!DelveBuilds::activate(0,0));
    int originalSorcery = stats[0]->getProficiency(PRO_SORCERY);
    int originalINT = stats[0]->INT;
    stats[0]->setProficiencyUnsafe(PRO_SORCERY, 0);stats[0]->INT = 0;
    number(d,"normal_skill_penalty_retained",isSpellcasterBeginner(0,h,PRO_SORCERY));
    number(d,"normal_tooltip_penalty_retained",!DelveBuilds::skillPenaltiesDisabled(SPELL_FORCEBOLT));
    number(d,"normal_passives_inert",DelvePassives::speedMultiplier(0)==1.0 && DelvePassives::modifyHealth(*h,-10)==-10);
    auto originalMask = stats[0]->mask;
    stats[0]->mask = nullptr;
    number(d,"normal_shopkeeper_race_rules_retained", !ShopkeeperPlayerHostility.playerRaceCheckHostility(0, SKELETON)
        && ShopkeeperPlayerHostility.playerRaceCheckHostility(0, HUMAN));
    stats[0]->mask = originalMask;
    auto unidentified = newItem(IRON_SWORD, SERVICABLE, 0, 1, 123, false, nullptr);
    number(d,"normal_appraisal_retained", !unidentified->identified);
    unidentified->identified=true;
    auto& groundTooltip=players[0]->worldUI.worldTooltipItem;
    groundTooltip.blitItemWorldTooltip(unidentified);
    number(d,"normal_ground_tooltip_retained",groundTooltip.statSummary.empty());
    free(unidentified);
    number(d,"normal_cooldown_hud_inert", !DelveBuilds::cooldownRemaining(0,0) && !DelvePassives::cooldownRemaining(0,0));
    number(d,"normal_xp_multiplier_inert", ::Raid::experienceGain(17)==17);
    DelveEffects::activated(0, 6); DelveEffects::passive(0, 0, 100, 1, 0, 100, true);
    number(d,"normal_skill_feedback_inert", !DelveEffects::view(0,6).remaining && !DelveEffects::view(0,6).flash && !DelveEffects::view(0,0).charges);
    stats[0]->setProficiencyUnsafe(PRO_SORCERY,originalSorcery);stats[0]->INT=originalINT;
    number(d,"native_sword",Entity::getDamageTableMultiplier(g,*stats[1],DAMAGE_TABLE_SWORD,nullptr,nullptr,h));
    ::Raid::applyControl(*g,h,EFF_PARALYZED,420,false);number(d,"paralysis_ticks",stats[1]->EFFECTS_TIMERS[EFF_PARALYZED]);
    ::Raid::applyControl(*g,h,EFF_FEAR,400,false);number(d,"fear_ticks",stats[1]->EFFECTS_TIMERS[EFF_FEAR]);
    ::Raid::applyControl(*g,h,EFF_WEBBED,400,false);number(d,"web_ticks",stats[1]->EFFECTS_TIMERS[EFF_WEBBED]);
    ::Raid::applyControl(*g,h,EFF_CONFUSED,600,false,Uint8(1));number(d,"confusion_ticks",stats[1]->EFFECTS_TIMERS[EFF_CONFUSED]);emit(d);finish();
}
void tick() {
    const bool builds=SmokeHooksCommon::parseEnvString("BARONY_BALANCE_BUILDS","")=="1";
    if (builds && SmokeHooksCommon::parseEnvString("BARONY_EFFECTS_CAPTURE", "") == "1") {
        playerSettings[0].mkb_world_tooltips_enabled = false;
        if (players[clientnum]) players[clientnum]->worldUI.disable();
    }
    if(builds && multiplayer==CLIENT) {
        if (SmokeHooksCommon::parseEnvString("BARONY_EFFECTS_CAPTURE", "") == "1") { players[clientnum]->worldUI.disable(); disable_messages=true; }
        if (SmokeHooksCommon::parseEnvString("BARONY_EFFECTS_CAPTURE", "") == "1" && std::filesystem::exists("../effects-view-ready")
            && players[0] && players[0]->entity && players[clientnum] && players[clientnum]->entity) {
            auto local = players[clientnum]->entity; auto host = players[0]->entity;
            local->yaw = std::atan2(host->y - local->y, host->x - local->x); local->pitch = galleryPitch();
        }
        static bool sent=false,acked=false;
        if(!acked && ticks%TICKS_PER_SECOND==0 && std::filesystem::exists("../build-network-ready")) {
            DelveBuilds::requestActivation(0);
            if (!sent) printlog("[DELVE-BUILD-TEST] client_activation_requested via=ability");
            sent = true;
        }
        if(sent && !acked && DelveBuilds::smokeNetworkConfirmed()) {acked=true;std::ofstream("../build-network-ack")<<"ack\n";printlog("[DELVE-BUILD-TEST] client_ack");}
        static bool passiveAcknowledged = false;
        if (!passiveAcknowledged && std::filesystem::exists("../passive-network-ready") && DelvePassives::smokeNetworkConfirmed()) {
            passiveAcknowledged = true;
            std::ofstream("../passive-network-ack") << "ack\n";
            printlog("[DELVE-PASSIVE-TEST] guest_state_ack");
        }
        static bool effectsAcknowledged = false;
        if (!effectsAcknowledged && DelveEffects::smokeNetworkConfirmed()) {
            effectsAcknowledged = true; std::ofstream("../effects-network-ack") << "ack\n";
            printlog("[DELVE-EFFECT-TEST] guest_event_and_state_ack");
        }
    }
    const bool resurrectionOnly=SmokeHooksCommon::parseEnvString("BARONY_BALANCE_RESURRECTION_ONLY","")=="1";
    const bool verify=SmokeHooksCommon::parseEnvString("BARONY_BALANCE_VERIFY_FIXES","")=="1";
    const bool contracts=SmokeHooksCommon::parseEnvString("BARONY_BALANCE_CONTRACTS","")=="1";
    const bool matrix=SmokeHooksCommon::parseEnvString("BARONY_BALANCE_MATRIX","")=="1";
    const auto sampleSet=SmokeHooksCommon::parseEnvString("BARONY_BALANCE_SAMPLES","");
    const int idleSeconds=SmokeHooksCommon::parseEnvInt("BARONY_BALANCE_IDLE_SECONDS",0,0,300);
    const bool collapse=SmokeHooksCommon::parseEnvString("BARONY_BALANCE_CAUSE","")=="collapse";
    const int mana=SmokeHooksCommon::parseEnvInt("BARONY_BALANCE_MANA",75,0,100);
    // Match the server's measurement health buffer on the owning client too.
    // Real revival, contract and normal-campaign checks retain native health.
    if(multiplayer==CLIENT && !resurrectionOnly && !contracts && players[clientnum]->entity)
        stats[clientnum]->HP=stats[clientnum]->MAXHP=1000;
    if(multiplayer==CLIENT && resurrectionOnly && players[clientnum]->entity) {
        static bool reported=false;
        if(!reported){reported=true;printlog("[BALANCE] peer_identity race=%d",stats[clientnum]->playerRace);}
    }
    if(multiplayer!=SERVER || !::Raid::smokeWorldReady() || (!players[0]->entity && phase<24) || (!players[1]->entity && phase<7) || !::Raid::hasLoadout(1))return;
    if(!(verify && phase>=22 && phase<40))::Raid::setSmokeElapsed(0);
    for(auto n=map.entities->first;n;n=n->next) {
        auto e=static_cast<Entity*>(n->element);
        if(e->behavior==&actMonster && e->getStats() &&
            (!e->getStats()->getEffectActive(EFF_ASLEEP) || e->getStats()->EFFECTS_TIMERS[EFF_ASLEEP]<1000))
            e->setEffect(EFF_ASLEEP,true,100000,true);
    }
    if(builds && phase==0 && !DelveBuilds::smokeReady())return;
    if(phase==0) {
        std::ofstream("balance.jsonl",std::ios::trunc);
        std::ifstream f("delve-generated-floor1.json");std::string j((std::istreambuf_iterator<char>(f)),{});D d;d.Parse(j.c_str());
        if(d.HasParseError() || !d.HasMember("combat_lane"))return;
        lx=d["combat_lane"]["x"].GetInt();ly=d["combat_lane"]["y"].GetInt();
        if(builds) {
            target({CLASS_WARRIOR,"build_target"});
            check("build_catalogue_native",DelveBuilds::smokeAudit(*players[0]->entity,*players[1]->entity));
            std::ofstream("../build-network-ready")<<"ready\n";phase=70;at=ticks;return;
        }
        if(contracts){phase=40;at=ticks;return;}
        if(matrix){phase=50;index=0;}
        else if(idleSeconds){at=ticks;phase=60;}
        else if(!sampleSet.empty()){phase=3;index=sampleSet=="projectiles" ? 14:0;}
        if(resurrectionOnly) {
            D row;row.SetObject();string(row,"kind","identity");number(row,"guest_race",stats[1]->playerRace);number(row,"guest_type",stats[1]->type);emit(row);
            auto h=players[0]->entity,g=players[1]->entity;
            h->teleporterMove(lx,ly,2);g->teleporterMove(lx+2,ly,2);h->yaw=0;g->yaw=PI;
            stats[1]->MAXMP=std::max(75,mana);g->setMP(mana);g->setHP(1);
            if(collapse) {
                ::Raid::spawnStormWave(0);
                Entity* storm=nullptr;
                for(auto node=map.entities->first;node;node=node->next) {
                    auto entity=static_cast<Entity*>(node->element);
                    if(::Raid::stormBoulder(*entity)){storm=entity;break;}
                }
                check("storm_revival_fixture_available",storm!=nullptr);
                if(storm) {
                    storm->x=g->x;storm->y=g->y;storm->vel_x=storm->vel_y=0;
                    ::Raid::tickStormBoulder(*storm);list_RemoveNode(storm->mynode);
                    check("storm_native_crush_applied",stats[1]->HP<=0);
                }
            } else h->attack(1,MAXCHARGE,g);
            at=ticks;phase=7;return;
        }
        if(!matrix && !idleSeconds && sampleSet.empty()){phase=1;index=0;}
    }
    if(phase==60) {
        if(ticks-at<idleSeconds*TICKS_PER_SECOND)return;
        check("long_idle_direct_ip_session",!client_disconnected[1] && ::Raid::outcome(1)==::Raid::Outcome::Playing);
        finish();return;
    }
    if(phase==50) {
        constexpr int raceCount=sizeof(races)/sizeof(races[0]);
        fresh((index/raceCount)%NUMCLASSES,races[index%raceCount],index/(raceCount*NUMCLASSES));
        at=ticks;phase=51;return;
    }
    if(phase==51) {
        if(ticks-at<3)return;
        constexpr int raceCount=sizeof(races)/sizeof(races[0]);
        dump((index/raceCount)%NUMCLASSES,races[index%raceCount],index<raceCount*NUMCLASSES ? "matrix_innate":"matrix_cosmetic");
        ++index;if(index==raceCount*NUMCLASSES*2)finish();else phase=50;
        return;
    }
    if(phase==1) {
        fresh(index<NUMCLASSES ? index:CLASS_BARBARIAN,index<NUMCLASSES ? RACE_HUMAN:races[index-NUMCLASSES]);
        players[0]->entity->teleporterMove(lx,ly,2);at=ticks;phase=2;return;
    }
    if(phase==2) {
        if(ticks-at<TICKS_PER_SECOND)return;
        dump(index<NUMCLASSES ? index:CLASS_BARBARIAN,index<NUMCLASSES ? RACE_HUMAN:races[index-NUMCLASSES],index<NUMCLASSES ? "class":"race");
        printlog("[BALANCE] catalog=%d",index);++index;
        if(index<NUMCLASSES+sizeof(races)/sizeof(races[0]))phase=1;else {index=0;phase=3;}
        return;
    }
    if(phase==3) {
        if(sampleSet=="melee" && index==14){finish();return;}
        if(ticks<nextProbeAt)return;
        for(auto n=map.entities->first;n;) {
            auto next=n->next;auto e=static_cast<Entity*>(n->element);
            if(e->behavior==&actMagicMissile || e->behavior==&actArrow || e->behavior==&actThrown)list_RemoveNode(n);
            n=next;
        }
        if(index>=probes.size()) {
            // Revival is tested separately with a genuinely selected Skeleton
            // on both peers; changing only host-side dummy race is insufficient.
            finish();return;
        }
        auto& p=probes[index];fresh(p.c,RACE_HUMAN);target(p);at=ticks;phase=4;return;
    }
    if(phase==4) {
        if(ticks-at<TICKS_PER_SECOND/4)return;
        auto& p=probes[index];auto h=players[0]->entity,g=players[1]->entity;
        if(!p.spell && p.item<0) {
            D d;d.SetObject();string(d,"kind","melee");string(d,"name",p.name);number(d,"class",p.c);rapidjson::Value samples(rapidjson::kArrayType);
            for(int trial=0;trial<16;++trial) {
                fresh(p.c,RACE_HUMAN);target(p);
                int wanted=p.meleeWeapon>=0 ? p.meleeWeapon : p.c==CLASS_BREWER ? IRON_AXE:-1;
                if(wanted>=0)for(auto n=stats[0]->inventory.first;n;n=n->next) {
                    auto item=static_cast<Item*>(n->element);
                    if(item->type==wanted && item!=stats[0]->weapon){equipItem(item,&stats[0]->weapon,0,false);break;}
                }
                h->attack(1,MAXCHARGE,g);
                samples.PushBack(1000-stats[1]->HP,d.GetAllocator());
            }
            d.AddMember("damage",samples,d.GetAllocator());emit(d);printlog("[BALANCE] measured=%s",p.name);++index;phase=3;return;
        }
        if(p.item>=0) {
            Item* chosen=nullptr;for(auto n=stats[0]->inventory.first;n;n=n->next)if(static_cast<Item*>(n->element)->type==p.item){chosen=static_cast<Item*>(n->element);break;}
            if(!chosen) {printlog("[BALANCE] missing starter item=%d case=%s",p.item,p.name);phase=8;return;}
            if(chosen!=stats[0]->weapon)equipItem(chosen,&stats[0]->weapon,0,false);
        }
        // Prevent a reflected projectile from killing the fixture caster.
        stats[0]->HP=stats[0]->MAXHP=1000;preMana=stats[0]->MP;peak.fill(0);
        if(p.spell) {
            // These are handler/cap checks, separate from the native starter
            // damage samples: remove novice RNG for the later-form controls.
            if(p.spell==SPELL_SPRAY_WEB || p.spell==SPELL_FEAR)stats[0]->setProficiency(getSpellFromID(p.spell)->skillID,100);
            castSpell(h->getUID(),getSpellFromID(p.spell),false,false);
        } else h->attack(1,MAXCHARGE,nullptr);
        spentMana=preMana-stats[0]->MP;firstDamage=0;
        at=ticks;phase=5;return;
    }
    if(phase==5) {
        if(!firstDamage && stats[1]->HP<1000)firstDamage=1000-stats[1]->HP;
        for(int i=0;i<8;++i)if(stats[1]->getEffectActive(effects[i]))peak[i]=std::max(peak[i],stats[1]->EFFECTS_TIMERS[effects[i]]);
        if(ticks-at<TICKS_PER_SECOND*3)return;
        auto& p=probes[index];D d;d.SetObject();string(d,"kind","projectile");string(d,"name",p.name);number(d,"class",p.c);number(d,"target_race",p.race);
        number(d,"target_damage",1000-stats[1]->HP);number(d,"caster_damage",1000-stats[0]->HP);number(d,"mana_spent",spentMana);
        number(d,"impact_damage",firstDamage);
        number(d,"trained_control_probe",p.spell==SPELL_SPRAY_WEB || p.spell==SPELL_FEAR);
        const char* keys[]={"slow_ticks","sleep_ticks","paralysis_ticks","confusion_ticks","poison_ticks","bleed_ticks","web_ticks","fear_ticks"};
        for(int i=0;i<8;++i)number(d,keys[i],peak[i]);emit(d);printlog("[BALANCE] measured=%s damage=%d",p.name,1000-stats[1]->HP);
        if(peak[1] || peak[2] || peak[3] || peak[6] || peak[7])nextProbeAt=ticks+TICKS_PER_SECOND*3;
        ++index;phase=3;return;
    }
    if(phase==7 && ticks-at>=TICKS_PER_SECOND*(collapse ? 3:1)) {
        D d;d.SetObject();string(d,"kind","resurrection");number(d,"hp",stats[1]->HP);number(d,"mp",stats[1]->MP);
        number(d,"entity",players[1]->entity!=nullptr);number(d,"raid_outcome",static_cast<int>(::Raid::outcome(1)));emit(d);
        if(!verify || !resurrectionOnly){finish();return;}
        check("death_resolved",mana>=75 ? stats[1]->HP>0 && ::Raid::outcome(1)==::Raid::Outcome::Playing : stats[1]->HP<=0 && ::Raid::outcome(1)==::Raid::Outcome::Slain);
        phase=mana>=75 ? 20:30;at=ticks;return;
    }
    if(phase==20) {if(auto ladder=find(&actLadder))use(1,*ladder);phase=21;return;}
    if(phase==21) {
        if(::Raid::outcome(1)==::Raid::Outcome::Descended) {
            check("revived_player_descended",stats[1]->HP>0 && ::Raid::protectedPlayer(1));
            ::Raid::setSmokeElapsed(420);
            if(auto portal=find(&actPortal))use(0,*portal);phase=22;
        } else if(auto ladder=find(&actLadder))use(1,*ladder);
        return;
    }
    if(phase==22 && currentlevel==2 && players[1]->entity) {
        check("revived_player_next_floor",stats[1]->HP>0 && ::Raid::outcome(1)==::Raid::Outcome::Playing);
        ::Raid::setSmokeElapsed(420);phase=23;
    }
    if(phase==23) {
        if(currentlevel==2) {
            ::Raid::setSmokeElapsed(420);
            if(::Raid::outcome(1)==::Raid::Outcome::Playing) {
                if(auto ladder=find(&actLadder))use(1,*ladder);
            } else if(::Raid::outcome(0)==::Raid::Outcome::Playing) {
                if(auto portal=find(&actPortal))use(0,*portal);
            }
        } else if(currentlevel==3) {
            ::Raid::setSmokeElapsed(420);
            if(::Raid::isExtracted(1)) {check("revived_player_won_final_portal",true);phase=24;}
            else if(auto portal=find(&actPortal))use(1,*portal);
        }
        return;
    }
    if(phase==30) {
        if(::Raid::matchComplete()){finish();return;}
        for(int player=0;player<2;++player)if(stats[player])stats[player]->HP=0;
        return;
    }
    if(phase==24 && ::Raid::matchComplete()) {finish();return;}
    if(phase==40 && ticks-at>TICKS_PER_SECOND*6) {
        auto h=players[0]->entity,g=players[1]->entity;
        g->setEffect(EFF_PARALYZED,false,0,false);g->setEffect(EFF_ASLEEP,false,0,false);
        check("pvp_paralysis_cap",::Raid::applyControl(*g,h,EFF_PARALYZED,600,false) && stats[1]->EFFECTS_TIMERS[EFF_PARALYZED]==75);
        check("shared_control_no_chain",!::Raid::applyControl(*g,h,EFF_ASLEEP,600,false));
        g->setEffect(EFF_PARALYZED,false,0,false);
        check("cure_does_not_reset_recovery",!::Raid::applyControl(*g,h,EFF_ASLEEP,600,false));
        if(auto npc=find(&actMonster)) {
            npc->setEffect(EFF_ASLEEP,false,0,false);
            check("pve_sleep_unchanged",::Raid::applyControl(*npc,h,EFF_ASLEEP,600,false) && npc->getStats()->EFFECTS_TIMERS[EFF_ASLEEP]==600);
            check("monster_paralysis_unchanged",::Raid::applyControl(*g,npc,EFF_PARALYZED,420,false) && stats[1]->EFFECTS_TIMERS[EFF_PARALYZED]==420);
            g->setEffect(EFF_PARALYZED,false,0,false);
            stats[1]->type=SKELETON;
            check("pvp_sword_race_bound",std::abs(Entity::getDamageTableMultiplier(g,*stats[1],DAMAGE_TABLE_SWORD,nullptr,nullptr,h)-0.8)<0.001);
            check("pvp_mace_race_bound",std::abs(Entity::getDamageTableMultiplier(g,*stats[1],DAMAGE_TABLE_MACE,nullptr,nullptr,h)-1.2)<0.001);
            check("pve_race_unchanged",std::abs(Entity::getDamageTableMultiplier(g,*stats[1],DAMAGE_TABLE_SWORD,nullptr,nullptr,npc)-0.5)<0.001);
            npc->getStats()->leader_uid=h->getUID();npc->monsterAllyIndex=0;
            check("owned_minion_uses_pvp_rules",std::abs(Entity::getDamageTableMultiplier(g,*stats[1],DAMAGE_TABLE_SWORD,nullptr,nullptr,npc)-0.8)<0.001);
            npc->getStats()->leader_uid=0;npc->monsterAllyIndex=-1;
            stats[1]->ring=newItem(RING_MAGICRESISTANCE,SERVICABLE,0,1,0,true,nullptr);
            check("equipment_resistance_preserved",Entity::getDamageTableMultiplier(g,*stats[1],DAMAGE_TABLE_MAGIC,nullptr,nullptr,h)<0.8);
            g->setEffect(EFF_SHADOW_TAGGED,true,100,false);
            check("shadow_tag_override_preserved",std::abs(Entity::getDamageTableMultiplier(g,*stats[1],DAMAGE_TABLE_SWORD,nullptr,nullptr,h)-1.0)<0.001);
            g->setEffect(EFF_SHADOW_TAGGED,false,0,false);stats[1]->type=HUMAN;
        }
        check("self_control_unchanged",::Raid::applyControl(*h,h,EFF_PARALYZED,420,false) && stats[0]->EFFECTS_TIMERS[EFF_PARALYZED]==420);
        h->setEffect(EFF_PARALYZED,false,0,false);at=ticks;phase=41;return;
    }
    if(phase==41 && ticks-at>TICKS_PER_SECOND*4) {
        auto h=players[0]->entity,g=players[1]->entity;
        check("recovery_expires",::Raid::applyControl(*g,h,EFF_ASLEEP,600,false));
        check("pvp_sleep_cap",stats[1]->EFFECTS_TIMERS[EFF_ASLEEP]==150);
        check("sleep_blocks_paralysis_chain",!::Raid::applyControl(*g,h,EFF_PARALYZED,600,false));
        g->setEffect(EFF_ASLEEP,false,0,false);at=ticks;phase=42;return;
    }
    if(phase==42 && ticks-at>TICKS_PER_SECOND*6) {
        auto h=players[0]->entity,g=players[1]->entity;
        h->teleporterMove(lx,ly,2);g->teleporterMove(lx+2,ly,2);
        auto trap=newEntity(667,1,map.entities,nullptr);trap->behavior=&actBeartrap;trap->parent=h->getUID();
        trap->x=g->x;trap->y=g->y;trap->skill[11]=SERVICABLE;trap->skill[17]=0;
        g->setEffect(EFF_PARALYZED,false,0,false);actBeartrap(trap);
        check("native_player_trap_cap",stats[1]->getEffectActive(EFF_PARALYZED) && stats[1]->EFFECTS_TIMERS[EFF_PARALYZED]<=75);
        at=ticks;phase=43;return;
    }
    if(phase>=43 && phase<=45 && ticks-at>TICKS_PER_SECOND*6) {
        auto h=players[0]->entity,g=players[1]->entity;
        const int added[]={EFF_FEAR,EFF_WEBBED,EFF_CONFUSED};
        const char* names[]={"pvp_fear_cap","pvp_web_cap","pvp_confusion_cap"};
        const int effect=added[phase-43],cap=effect==EFF_CONFUSED ? 150:100;
        g->setEffect(EFF_PARALYZED,false,0,false);g->setEffect(EFF_ASLEEP,false,0,false);
        check(names[phase-43],::Raid::applyControl(*g,h,effect,600,false,Uint8(1)) && stats[1]->EFFECTS_TIMERS[effect]==cap);
        check("additional_control_shares_recovery",!::Raid::applyControl(*g,h,EFF_PARALYZED,600,false));
        g->setEffect(effect,false,0,false);
        check("additional_control_cure_keeps_recovery",!::Raid::applyControl(*g,h,EFF_ASLEEP,600,false));
        if(auto npc=find(&actMonster)) {
            check("additional_pve_control_native",::Raid::applyControl(*npc,h,effect,600,false) && npc->getStats()->EFFECTS_TIMERS[effect]==600);
            npc->setEffect(effect,false,0,false);
            npc->getStats()->leader_uid=h->getUID();npc->monsterAllyIndex=0;
            check("minion_control_shares_recovery",!::Raid::applyControl(*g,npc,effect,600,false));
            npc->getStats()->leader_uid=0;npc->monsterAllyIndex=-1;
        }
        at=ticks;++phase;return;
    }
    if(phase==70 && std::filesystem::exists("../build-network-ack")) {
        check("live_peer_activation_and_ack",DelveBuilds::smokeNetworkConfirmed());
        DelvePassives::blocked(*players[1]->entity, *players[0]->entity, 5);
        std::ofstream("../passive-network-ready") << "ready\n";
        phase = 71; return;
    }
    if (phase == 71 && std::filesystem::exists("../passive-network-ack") && std::filesystem::exists("../effects-network-ack")) {
        if (SmokeHooksCommon::parseEnvString("BARONY_EFFECTS_RECORD", "") == "1" && !std::filesystem::exists("../effects-recording")) {
            std::ofstream("../effects-record-ready") << "ready\n"; return;
        }
        check("live_peer_passive_replication", true); check("live_peer_skill_feedback_replication", true);
        if (SmokeHooksCommon::parseEnvString("BARONY_EFFECTS_CAPTURE", "") == "1") {
            disable_messages=true;
            bool found = false;
            for (int tileX = 4; tileX < map.width-4 && !found; ++tileX) for (int tileY = 4; tileY < map.height-4 && !found; ++tileY) {
                bool clear = true;
                for (int offsetX = -2; offsetX <= 3; ++offsetX) for (int offsetY = -2; offsetY <= 2; ++offsetY)
                    clear &= galleryFloor(tileX+offsetX,tileY+offsetY);
                if (clear) { lx=tileX; ly=tileY; found=true; }
            }
            phase = 72; index = gallerySkills().front();
        }
        else finish();
        return;
    }
    if (phase == 72) {
        fresh(index/4,RACE_HUMAN);
        stats[0]->mask = nullptr;
        players[0]->worldUI.disable();
        target({CLASS_WARRIOR, "effects_target"});
        check(("effects_gallery_activation_" + std::to_string(index)).c_str(), DelveBuilds::smokePresentAbility(index / 4, 6 + index % 4));
        at = ticks; phase = 73; return;
    }
    if (phase == 73) {
        players[0]->entity->yaw = 0; players[1]->entity->yaw = PI;
        players[0]->entity->pitch = players[1]->entity->pitch = galleryPitch();
        players[0]->entity->vel_x = players[0]->entity->vel_y = players[1]->entity->vel_x = players[1]->entity->vel_y = 0;
        const bool motion = SmokeHooksCommon::parseEnvString("BARONY_EFFECTS_MOTION", "") == "1";
        const bool settled = SmokeHooksCommon::parseEnvString("BARONY_EFFECTS_SETTLED", "") == "1";
        static Uint32 lastReplay = 0;
        if (!settled && ticks != lastReplay && (ticks - at) % (motion ? 50 : 8) == 0) { lastReplay=ticks; DelveEffects::smokeShowcase(index / 4, 6 + index % 4); }
        if (ticks - at >= (settled ? 50 : motion ? 150 : 8)) std::ofstream("../effects-view-ready") << index << '\n';
        if (std::filesystem::exists("../effects-captured-" + std::to_string(index))) {
            auto next = std::find(gallerySkills().begin(),gallerySkills().end(),index);
            if (next == gallerySkills().end() || ++next == gallerySkills().end()) finish();
            else { index=*next; phase=72; }
        }
        return;
    }
    if(phase==46 && ticks-at>TICKS_PER_SECOND*6) {
        fresh(CLASS_WIZARD,RACE_HUMAN);target({CLASS_WIZARD,"starter_fireball",SPELL_FIREBALL});
        stats[1]->HP=stats[1]->MAXHP=50;
        castSpell(players[0]->entity->getUID(),getSpellFromID(SPELL_FIREBALL),false,false);
        at=ticks;phase=47;return;
    }
    if(phase==47 && ticks-at>TICKS_PER_SECOND*3) {
        check("fresh_starter_survives_fireball_three_seconds",stats[1]->HP>0 && stats[1]->HP<50 && ::Raid::outcome(1)==::Raid::Outcome::Playing);
        D d;d.SetObject();string(d,"kind","contract");string(d,"name","starter_fireball_remaining_hp");number(d,"remaining_hp",stats[1]->HP);
        number(d,"pass",stats[1]->HP>0 && stats[1]->HP<50);emit(d);finish();return;
    }
}
}
