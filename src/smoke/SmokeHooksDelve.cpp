#include "SmokeHooksCommon.hpp"
#include "../entity.hpp"
#include "../game.hpp"
#include "../items.hpp"
#include "../net.hpp"
#include "../player.hpp"
#include "../raid.hpp"
#include "../raid_battle_rules.hpp"
#include "../shops.hpp"
#include "../delve_rarity.hpp"
#include "../delve_passives.hpp"
#include "../gear_loadout.hpp"
#include "../scores.hpp"
#include "../files.hpp"
#include "../ui/GameUI.hpp"
#include "../ui/Frame.hpp"
#include "../ui/Field.hpp"
#include "../ui/Text.hpp"
#include "../ui/Button.hpp"
#include "../delve_builds.hpp"
#include "../interface/interface.hpp"
#include "../collision.hpp"
#include "../stat.hpp"
#include "../magic/magic.hpp"
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cstring>
#include "../rapidjson/document.h"

namespace Raid { bool smokeWorldReady(); bool smokePopulationAudit(); bool smokeKioskAudit(); void smokeResultsNames(); }
namespace MainMenu { bool smokeActivateRestart(bool deathScreen); }
namespace SmokeTestHooks::Balance { void tick(); void normalTick(); }
namespace SmokeTestHooks::Delve {
namespace {
using O = ::Raid::Outcome;
const std::string scenario = SmokeHooksCommon::parseEnvString("BARONY_SMOKE_DELVE", "");
const std::string sync = SmokeHooksCommon::parseEnvString("BARONY_SMOKE_RAID_SYNC", "");
const int peers = SmokeHooksCommon::parseEnvInt("BARONY_SMOKE_PLAYERS", 2, 1, 4);
int observedFloor = 0, phase = 0;
Uint32 began = 0, clickAt = 0, portalUID = 0, lootUID = 0;
int tourView = 0; Uint32 tourAt = 0;
std::array<Uint32,MAXPLAYERS> pendingTargets{},moveAt{};
bool done = false, committed = false, retry = false, duplicate = false;
bool outpostChecked = false;
bool populationChecked = false, populationAuditChecked = false;
int previousHP = 0;
Uint32 restartSession = 0;
int restartRound = 0;
int restartAction = 0;
bool layoutPoint(const char* key,int ordinal,int& x,int& y,double& yaw) {
    std::ifstream input("delve-generated-floor"+std::to_string(currentlevel)+".json");
    std::string json((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    rapidjson::Document d;d.Parse(json.c_str());
    if(d.HasParseError() || !d.IsObject() || !d.HasMember(key))return false;
    auto* p=&d[key];
    if(p->IsArray()) { if(ordinal<0 || ordinal>=p->Size())return false;p=&(*p)[ordinal]; }
    if(!p->IsObject() || !p->HasMember("x") || !p->HasMember("y"))return false;
    x=(*p)["x"].GetInt();y=(*p)["y"].GetInt();yaw=p->HasMember("yaw") ? (*p)["yaw"].GetDouble():0;
    return true;
}
void check(bool ok, const char* label) { printlog("[SMOKE][DELVE] check %s %s",ok ? "PASS" : "FAIL",label); }
void checkHoverStats(int player) {
    auto& tooltip=players[player]->worldUI.worldTooltipItem;
    const ItemType cases[]={MAGICSTAFF_COLD,IRON_SWORD,STEEL_BREASTPIECE,RING_STRENGTH,POTION_HEALING,SPELLBOOK_COLD,FOOD_BREAD,TOOL_BEARTRAP};
    for (int index=0;index<8;++index) {
        auto item=newItem(cases[index],EXCELLENT,0,1,123,true,nullptr);
        auto surface=tooltip.blitItemWorldTooltip(item);
        const auto summary=tooltip.statSummary;
        check(surface && !summary.empty(),("hover_stats_category_"+std::to_string(index)).c_str());
        check(summary.find("%d")==std::string::npos && summary.find("%s")==std::string::npos
            && summary.find("%+d")==std::string::npos,"hover_stats_formatted_values");
        if (surface) {
            const auto name="hover-stats-"+std::to_string(index)+".png";
            check(IMG_SavePNG(surface,name.c_str())==0,"hover_stats_capture");
        }
        printlog("[SMOKE][DELVE] hover_stats player=%d type=%d text=%s",player,int(item->type),summary.c_str());
        const int identifiedHeight=surface ? surface->h:0;
        item->identified=false;
        auto unknown=tooltip.blitItemWorldTooltip(item);
        check(tooltip.statSummary.empty() && unknown && unknown->h<identifiedHeight,"hover_stats_unknown_hidden");
        item->identified=true;
        surface=tooltip.blitItemWorldTooltip(item);
        check(surface && tooltip.statSummary==summary,"hover_stats_restored_after_identification");
        if (index==1) {
            item->beatitude=3;
            tooltip.blitItemWorldTooltip(item);
            check(tooltip.statSummary!=summary,"hover_stats_item_change_refreshes");
        }
        free(item);
    }
    auto neutral=newItem(POTION_HEALING,EXCELLENT,0,1,123,true,nullptr);
    tooltip.blitItemWorldTooltip(neutral);
    const auto before=tooltip.statSummary;
    const auto originalCON=stats[player]->CON;
    stats[player]->CON+=10;
    tooltip.blitItemWorldTooltip(neutral);
    check(tooltip.statSummary!=before,"hover_stats_player_change_refreshes");
    stats[player]->CON=originalCON;
    free(neutral);
    check(true,player==0 ? "hover_stats_host_complete":"hover_stats_guest_complete");
}
Entity* find(void (*behavior)(Entity*), int ordinal = 0) {
    for (auto node = map.entities->first; node; node = node->next) {
        auto e = static_cast<Entity*>(node->element);
        if (e->behavior == behavior && ordinal-- == 0) return e;
    }
    return nullptr;
}
void publish(int p, Entity* target) {
    pendingTargets[p]=target ? target->getUID():0;moveAt[p]=ticks+TICKS_PER_SECOND/2;
    std::ofstream f(sync+"/target-"+std::to_string(p));
    f << currentlevel << ' ' << (target ? target->getUID() : 0) << '\n';
    printlog("[SMOKE][DELVE] target slot=%d uid=%u x=%d y=%d",p,target ? target->getUID():0,target ? (int)target->x/16:0,target ? (int)target->y/16:0);
}
void move(int p, Entity& target) {
    if (!players[p] || !players[p]->entity) return;
    const int x=static_cast<int>(target.x)/16,y=static_cast<int>(target.y)/16;
    const int sides[][2]={{1,0},{-1,0},{0,1},{0,-1}};
    for (auto& side:sides) {
        int tx=x+side[0],ty=y+side[1];
        if(tx<0 || ty<0 || tx>=map.width || ty>=map.height)continue;
        int offset=MAPLAYERS*(ty+tx*map.height),floor=map.tiles[offset];
        if(map.tiles[offset+OBSTACLELAYER] || swimmingtiles[floor] || lavatiles[floor])continue;
        if(checkObstacle(tx*16+8,ty*16+8,players[p]->entity,&target))continue;
        if(players[p]->entity->teleporterMove(tx,ty,2))return;
    }
    check(false,"no_interaction_approach");
}
void use(int p, Entity& target) {
    move(p,target);
    if (p == 0) { selectedEntity[0] = &target; inrange[0] = true; ::Raid::useExit(target); }
    else publish(p,&target);
}
bool allLoaded() {
    for (int p=1;p<peers;++p) if (client_disconnected[p] || !players[p]->entity || !::Raid::hasLoadout(p)) return false;
    return true;
}
#include "DelveAffixAudit.inc"
#include "DelveSpellbookAudit.inc"
#include "DelveRarityAudit.inc"
#include "DelveBattleAudit.inc"
#include "DelveShopAudit.inc"
#include "DelveBoulderAudit.inc"
void checkConcealment() {
    for (int slot=0;slot<peers;++slot) {
        auto root=players[slot]->entity;
        if (!root) return;
        check(!::Raid::concealedFromObserver(*root,slot),"invisibility_preserves_owner");
        for (int observer=0;observer<peers;++observer) {
            if (observer==slot) continue;
            
            check(::Raid::concealedFromObserver(*root,observer),"invisibility_hides_remote_body");
            for (auto node=root->children.first;node;node=node->next) {
                auto part=static_cast<Entity*>(node->element);
                if(!part || part->flags[OVERDRAW])continue;
                check(::Raid::concealedFromObserver(*part,observer),"invisibility_hides_remote_equipment");
            }
        }
    }
}
void host() {
    if (scenario == "storm-wipe") { boulderWipe(); return; }
    if (scenario == "shops") { shopNetworkAudit(); return; }
    if (scenario == "battle-royale" || scenario == "battle-wipe" || scenario == "battle-host-win") { battleHost(); return; }
    if (scenario == "revenants") { ::Raid::smokeReturnLives(); return; }
    if (scenario == "rarity") { rarityHost(); return; }
    // The rendering tour leaves peers idle for a minute. Protect only this fixture,
    // before readiness checks; normal matches and ordinary smoke runs are untouched.
    const bool touring=SmokeHooksCommon::parseEnvString("BARONY_SMOKE_DELVE_TOUR", "") == "1" && (observedFloor!=currentlevel || tourView<4);
    if(scenario=="population" || scenario=="concealment")for(int slot=0;slot<peers;++slot)
        if(stats[slot] && stats[slot]->HP>0 && ::Raid::outcome(slot)==O::Playing)stats[slot]->HP=stats[slot]->MAXHP;
    if(touring)for(int p=0;p<peers;++p)if(stats[p] && ::Raid::outcome(p)==O::Playing)stats[p]->HP=stats[p]->MAXHP;
    for(int p=1;p<peers;++p)if(pendingTargets[p] && ticks>=moveAt[p]) {
        if(::Raid::outcome(p)!=O::Playing) {pendingTargets[p]=0;continue;}
        if(auto target=uidToEntity(pendingTargets[p])) {
            // Fixture-created loot must reach the owned peer before requesting native pickup.
            // Retry only this pending item, at the existing half-second fixture cadence.
            if(target->behavior==&actItem)sendEntityUDP(target,p,true);
            move(p,*target);
        } else pendingTargets[p]=0;
        moveAt[p]=ticks+TICKS_PER_SECOND/2;
    }
    const bool combatEnded = scenario == "combat" && phase >= 2;
    if ((!allLoaded() && !combatEnded) || !::Raid::smokeWorldReady() || !find(&actItem)) return;
    for (auto node=map.entities->first;node;node=node->next) {
        auto e=static_cast<Entity*>(node->element);
        if (e->behavior == &actMonster && e->getStats() &&
            (!e->getStats()->getEffectActive(EFF_ASLEEP) || e->getStats()->EFFECTS_TIMERS[EFF_ASLEEP]<1000))
            e->setEffect(EFF_ASLEEP,true,100000,true);
    }
    if (observedFloor != currentlevel) {
        tourView=0; tourAt=0;pendingTargets.fill(0);moveAt.fill(0);outpostChecked=false;populationChecked=false;
        observedFloor = currentlevel; began = ticks; phase = 0; committed = retry = duplicate = false;
        int chests=0, drops=0, monsters=0, shrines=0, stairs=0, portals=0;
        for (auto node=map.entities->first;node;node=node->next) {
            auto e=static_cast<Entity*>(node->element);
            chests += e->behavior == &actChest; drops += e->behavior == &actItem;
            monsters += e->behavior == &actMonster; shrines += e->behavior == &actFountain;
            stairs += e->behavior == &actLadder; portals += e->behavior == &actPortal;
        }
        printlog("[SMOKE][DELVE] world floor=%d chests=%d drops=%d monsters=%d shrines=%d stairs=%d portals=%d",currentlevel,chests,drops,monsters,shrines,stairs,portals);
        for (int p=0;p<peers;++p) if (players[p]->entity)
            printlog("[SMOKE][DELVE] spawn floor=%d slot=%d x=%d y=%d gold=%d class=%d weapon_status=%d race=%d hp=%d maxhp=%d",currentlevel,p,(int)players[p]->entity->x/16,(int)players[p]->entity->y/16,stats[p]->GOLD,client_classes[p],stats[p]->weapon ? (int)stats[p]->weapon->status : -1,stats[p]->playerRace,stats[p]->HP,stats[p]->MAXHP);
        if (currentlevel==1 && players[0]->entity && players[1]->entity)
            printlog("[SMOKE][DELVE] hostility enemy=%d friend=%d friendlyfire=%d",players[0]->entity->checkEnemy(players[1]->entity),players[0]->entity->checkFriend(players[1]->entity),(svFlags & SV_FLAG_FRIENDLYFIRE)!=0);
        // Freeze fixture monsters, preserving their real initialized inventory/stats for checks.
        for (auto node=map.entities->first;node;node=node->next) {
            auto e=static_cast<Entity*>(node->element);
            if (e->behavior == &actMonster && e->getStats() && e->getStats()->type != SHOPKEEPER) e->setEffect(EFF_ASLEEP,true,100000,true);
        }
    }
    if(scenario != "results" && scenario != "cooldowns" && scenario != "abilities" && scenario != "appraisal" && scenario != "appraisal-disabled" && !populationChecked && ticks-began>TICKS_PER_SECOND) {
        if(!populationAuditChecked) {
            check(::Raid::smokePopulationAudit(),"population_3000_seeded_rosters");
            populationAuditChecked=true;
        }
        std::ifstream input("delve-generated-floor"+std::to_string(currentlevel)+".json");
        std::string json((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
        rapidjson::Document layout;layout.Parse(json.c_str());
        bool valid=!layout.HasParseError() && layout.HasMember("encounters");
        std::array<int,NUMMONSTERS> expected{},actual{};
        if(valid)for(auto& encounter:layout["encounters"].GetArray()) {
            if(!encounter.HasMember("monster") || encounter["monster"].GetInt()<=NOTHING || encounter["monster"].GetInt()>=NUMMONSTERS) {valid=false;break;}
            ++expected[encounter["monster"].GetInt()];
        }
        const Monster bosses[]={SKELETON,GOBLIN,TROLL};
        ++expected[bosses[currentlevel-1]];
        bool ordinary=true,physical=true,starterThreat=true;
        int guardians=0;
        const char* guardianNames[]={"Outpost Warden","Jungle Warden","Sanctum Guardian"};
        const int guardianHP[]={60,90,140},guardianLevels[]={3,6,12};
        const int attackCeilings[]={18,24,28};
        for(auto node=map.entities->first;node;node=node->next) {
            auto monster=static_cast<Entity*>(node->element);auto stats=monster->getStats();
            if(monster->behavior!=&actMonster || !stats || stats->type==SHOPKEEPER)continue;
            ++actual[stats->type];
            ordinary &= stats->MISC_FLAGS[STAT_FLAG_DISABLE_MINIBOSS] && stats->getAttribute("special_npc")=="";
            if(stats->type==SKELETON || stats->type==GOBLIN)
                physical &= stats->weapon && (isMeleeWeapon(*stats->weapon) || isRangedWeapon(*stats->weapon));
            if(stats->type==GOBLIN)physical &= stats->weapon && isMeleeWeapon(*stats->weapon);
            const int attack=Entity::getAttack(monster,stats,false);
            starterThreat &= attack<=attackCeilings[currentlevel-1];
            printlog("[SMOKE][DELVE] enemy_stats floor=%d type=%d hp=%d attack=%d level=%d",
                currentlevel,stats->type,stats->MAXHP,attack,stats->LVL);
            if(!std::strcmp(stats->name,guardianNames[currentlevel-1])) {
                ++guardians;
                check(stats->type==bosses[currentlevel-1] && stats->HP==guardianHP[currentlevel-1] &&
                    stats->MAXHP==guardianHP[currentlevel-1] && stats->LVL==guardianLevels[currentlevel-1],
                    "population_low_level_guardian_stats");
                printlog("[SMOKE][DELVE] guardian floor=%d type=%d hp=%d level=%d",currentlevel,stats->type,stats->MAXHP,stats->LVL);
            }
        }
        for(int mob=0;mob<NUMMONSTERS;++mob)if(expected[mob] || actual[mob])
            printlog("[SMOKE][DELVE] roster floor=%d type=%d expected=%d actual=%d",currentlevel,mob,expected[mob],actual[mob]);
        check(valid && expected==actual && ordinary,"population_matches_authored_roster");
        check(physical,"population_physical_guards_on_every_floor");
        check(starterThreat,"population_starter_appropriate_attack_ceiling");
        check(guardians==1,"population_exactly_one_guardian");
        check(::Raid::smokeKioskAudit(),"kiosks_geometry_stock_trade_and_mutual_neutrality");
        printlog("[SMOKE][DELVE] population_checked floor=%d rats=%d bats=%d skeletons=%d",currentlevel,actual[RAT],actual[BAT_SMALL],actual[SKELETON]);
        check(actual[RAT]>0 && actual[BAT_SMALL]>0 && actual[SKELETON]>0,"low_tier_enemies_on_every_floor");
        populationChecked=true;
    }
    if(currentlevel==1 && !outpostChecked && ticks-began>TICKS_PER_SECOND) {
        int guards=0,casters=0,variants=0,goblins=0,nonmeleeGoblins=0;
        for(auto node=map.entities->first;node;node=node->next) {
            auto guard=static_cast<Entity*>(node->element);
            auto stats=guard->getStats();
            if(guard->behavior!=&actMonster || !stats || (stats->type!=GOBLIN && stats->type!=SKELETON))continue;
            ++guards;
            if(stats->type==GOBLIN) {
                ++goblins;
                if(!stats->weapon || !isMeleeWeapon(*stats->weapon))++nonmeleeGoblins;
            }
            if(stats->weapon && (itemCategory(stats->weapon)==MAGICSTAFF || itemCategory(stats->weapon)==SPELLBOOK))++casters;
            if(!stats->MISC_FLAGS[STAT_FLAG_DISABLE_MINIBOSS] || stats->getAttribute("special_npc")!="")++variants;
        }
        printlog("[SMOKE][DELVE] outpost_loadouts guards=%d casters=%d variants=%d",guards,casters,variants);
        check(guards>0 && casters==0 && variants==0,"outpost_physical_guards_no_random_minibosses");
        printlog("[SMOKE][DELVE] outpost_goblins total=%d nonmelee=%d",goblins,nonmeleeGoblins);
        check(goblins==0 && nonmeleeGoblins==0,"outpost_has_no_goblins");
        outpostChecked=true;
    }
    if (scenario == "concealment" && currentlevel==1) {
        if (phase==0) {
            for(int slot=0;slot<peers;++slot)players[slot]->entity->setEffect(EFF_INVISIBLE,true,60*TICKS_PER_SECOND,true);
            phase=1;
        }
        if (phase==1) {
            if(ticks-began<3*TICKS_PER_SECOND) return;
            checkConcealment();
            for(auto type:{SCROLL_ENCHANTWEAPON,SCROLL_ENCHANTARMOR}) {
                auto scroll=newItem(type,EXCELLENT,0,1,0,true,nullptr);
                check(scroll->getGoldValue()==std::max(1,items[type].gold_value/4),"blessing_scroll_quarter_value");
                free(scroll);
            }
            std::ofstream(sync+"/concealment-hidden") << "ready";
            phase=2;
        }
        if (phase==2) {
            for(int slot=1;slot<peers;++slot)if(!std::filesystem::exists(sync+"/concealment-guest-"+std::to_string(slot))) return;
            for(int slot=0;slot<peers;++slot)players[slot]->entity->setEffect(EFF_INVISIBLE,false,0,true);
            phase=3;
        }
        if (phase==3) {
            if(ticks-began<6*TICKS_PER_SECOND) return;
            for(int slot=0;slot<peers;++slot)
                check(!::Raid::concealedFromObserver(*players[slot]->entity,(slot+1)%peers),"invisibility_restores_visibility");
            std::ofstream(sync+"/concealment-visible") << "ready";
            phase=4;
        }
        if (phase==4) {
            for(int slot=1;slot<peers;++slot)if(!std::filesystem::exists(sync+"/concealment-guest-visible-"+std::to_string(slot))) return;
        }
    }
    if ((scenario == "population" || scenario == "concealment") && !touring) {
        if (!populationChecked || !outpostChecked && currentlevel == 1) return;
        if (ticks - began < 3 * TICKS_PER_SECOND) return;
        for (int slot = 0; slot < peers; ++slot) stats[slot]->HP = 0;
        return;
    }
    if (scenario == "results") {
        if (ticks - began < 3 * TICKS_PER_SECOND) return;
        ::Raid::smokeResultsNames();
        for (int slot = 0; slot < peers; ++slot) stats[slot]->HP = 0;
        return;
    }
    if (scenario == "abilities") {
        if (ticks - began < 2 * TICKS_PER_SECOND) return;
        static bool equipped = false;
        static int stage = 0;
        if (!equipped) {
            ::DelveBuilds::smokeEquip(0, CLASS_WARRIOR, 0, 1);
            stats[0]->MAXMP = stats[0]->MP = 100;
            check(std::string(Input::inputs[0].binding("Delve Ability 1")) == "Q"
                && std::string(Input::inputs[0].binding("Delve Ability 2")) == "E", "legacy_hotkeys_migrated_host");
            equipped = true;
        }
        ::Raid::setSmokeElapsed(0);
        players[0]->shootmode = stage != 5;
        players[0]->gui_mode = stage == 5 ? GUI_MODE_INVENTORY : GUI_MODE_NONE;
        int next = -1;
        std::ifstream(sync + "/ability-command") >> next;
        if (stage == 0 && ::DelveBuilds::cooldownRemaining(0, 0)) {
            check(true, "hud_q_key_activates_host"); stage = 1;
        } else if (stage == 1 && ::DelveBuilds::cooldownRemaining(0, 1)) {
            check(true, "hud_e_key_activates_host"); stage = 2;
        } else if (stage == 2 && ::DelveBuilds::cooldownRemaining(1, 0)) {
            check(true, "hud_q_key_activates_guest_packet"); stage = 3;
        } else if (stage == 3 && ::DelveBuilds::cooldownRemaining(1, 1)) {
            check(true, "hud_e_key_activates_guest_packet"); stage = 4;
        } else if (stage == 4 && next == 5) stage = 5;
        else if (stage == 5 && next == 6) stage = 6;
        else if (stage == 6 && next == 7) {
            int legacy = 0;
            for (auto node = stats[0]->inventory.first; node; node = node->next) {
                auto item = static_cast<Item*>(node->element);
                legacy += item->type == SPELL_ITEM && ::DelveBuilds::isAbilitySpell(item->appearance);
            }
            check(!legacy, "hud_abilities_absent_from_inventory");
            for (int player = 0; player < peers; ++player) stats[player]->HP = 0;
            return;
        }
        std::ofstream(sync + "/ability-stage") << stage;
        return;
    }
    if (scenario == "cooldowns") {
        if (ticks - began < 2 * TICKS_PER_SECOND) return;
        static bool equipped = false;
        if (!equipped) { ::DelveBuilds::smokeEquip(0, CLASS_WARRIOR, 0, 1); equipped = true; }
        const unsigned int elapsed = ticks - began - 2 * TICKS_PER_SECOND;
        const int stage = elapsed / (3 * TICKS_PER_SECOND);
        if (stage >= 5) {
            players[0]->shootmode = true; players[0]->gui_mode = GUI_MODE_NONE;
            for (int player = 0; player < peers; ++player) stats[player]->HP = 0;
            return;
        }
        players[0]->shootmode = stage != 3;
        players[0]->gui_mode = stage == 3 ? GUI_MODE_INVENTORY : GUI_MODE_NONE;
        ::DelveBuilds::smokeCooldownHUD(stage >= 3 ? 1 : stage);
        ::Raid::setSmokeElapsed(0);
        if (elapsed % (3 * TICKS_PER_SECOND) == TICKS_PER_SECOND)
            std::ofstream(sync + "/cooldown") << stage;
        return;
    }
    if (SmokeHooksCommon::parseEnvString("BARONY_SMOKE_DELVE_TOUR", "") == "1" && tourView<4) {
        if(!tourAt) {
            int x=0,y=0;double yaw=0;
            check(layoutPoint("views",tourView,x,y,yaw),"native_tour_point");
            check(players[0]->entity->teleporterMove(x,y,2),"tour_view_reachable");
            players[0]->entity->yaw=yaw;tourAt=ticks;::Raid::setSmokeElapsed(0);
            if(!stats[0]->shield || stats[0]->shield->type!=TOOL_TORCH) {
                auto torch=newItem(TOOL_TORCH,EXCELLENT,0,1,0,true,&stats[0]->inventory);useItem(torch,0);
            }
        }
        if (SmokeHooksCommon::parseEnvString("BARONY_SMOKE_DELVE_HUD", "") == "1") {
            ::Raid::setSmokeElapsed(tourView == 1 ? 315 : tourView == 2 ? 361 : 0);
        }
        if(ticks-tourAt==TICKS_PER_SECOND) {
            std::ofstream view(sync+"/view");view<<currentlevel<<"-"<<tourView;
        }
        if(ticks-tourAt>TICKS_PER_SECOND*4) { ++tourView;tourAt=0;began=ticks; }
        return;
    }
    if (ticks-began < TICKS_PER_SECOND*2) return;
    if (scenario == "appraisal" || scenario == "appraisal-disabled") {
        if (phase==0) {
            checkHoverStats(0);
            const bool expected=scenario=="appraisal";
            auto created=newItem(IRON_SWORD,WORN,-2,3,123,false,nullptr);
            check(created && created->identified==expected,"appraisal_created_before_pickup");
            free(created);
            auto ground=find(&actItem);
            check(ground && bool(ground->skill[15])==expected,"appraisal_native_ground_state");
            if (ground) {
                ground->skill[15]=false;
                auto preview=newItemFromEntity(ground,true);
                check(preview && preview->identified==expected,"appraisal_ground_preview");
                check(preview && preview->status==ground->skill[11] && preview->beatitude==ground->skill[12]
                    && preview->count==ground->skill[13] && preview->appearance==ground->skill[14],"appraisal_properties_preserved");
                free(preview);
                actItem(ground);
                check(bool(ground->skill[15])==expected,"appraisal_ground_entity_update");
            }
            bool caches=true;int checked=0;
            for(auto node=map.entities->first;node;node=node->next) {
                auto cache=static_cast<Entity*>(node->element);
                if(cache->behavior!=&actChest || !cache->getChestInventoryList())continue;
                for(auto entry=cache->getChestInventoryList()->first;entry;entry=entry->next) {
                    caches &= static_cast<Item*>(entry->element)->identified==expected;
                    ++checked;
                }
            }
            check(caches && checked>0,"appraisal_cache_contents");
            phase=1;began=ticks;
        } else for (int slot = 0; slot < peers; ++slot) stats[slot]->HP = 0;
        return;
    }
    if (scenario == "quality" || scenario == "quality-unidentified") {
        auto h=players[0]->entity, g=players[1]->entity;
        if (phase==0) {
            Entity* chest=nullptr;
            for(auto node=map.entities->first;node;node=node->next) {
                auto candidate=static_cast<Entity*>(node->element);
                if(candidate->behavior==&actChest && candidate->skill[20]==0) { chest=candidate;break; }
            }
            check(chest && chest->getChestInventoryList() && chest->getChestInventoryList()->first,"supply_cache_inventory");
            bool roles[5]={};bool roleContents=true;int checked=0;
            for(auto node=map.entities->first;node;node=node->next) {
                auto c=static_cast<Entity*>(node->element);if(c->behavior!=&actChest)continue;
                int role=c->skill[20];if(role<0 || role>4){roleContents=false;continue;}
                roles[role]=true;int pieces=0,books=0,magicRewards=0,diamonds=0,enchants=0,heals=0,mana=0,food=0,tools=0,equipment=0,blood=0,scrap=0;
                for(auto n=c->getChestInventoryList()->first;n;n=n->next) {
                    auto item=static_cast<Item*>(n->element);pieces+=item->count;
                    const auto category=itemCategory(item);
                    if(category==SPELLBOOK || category==MAGICSTAFF)books+=item->count;
                    if(category==SPELLBOOK || category==MAGICSTAFF || category==RING || category==AMULET || category==SCROLL)magicRewards+=item->count;
                    if(category==WEAPON || category==ARMOR || category==RING || category==AMULET || category==MAGICSTAFF)equipment+=item->count;
                    if(item->type==GEM_DIAMOND)diamonds+=item->count;
                    if(item->type==SCROLL_ENCHANTWEAPON || item->type==SCROLL_ENCHANTARMOR)enchants+=item->count;
                    if(item->type==POTION_HEALING)heals+=item->count;
                    if(item->type==POTION_RESTOREMAGIC)mana+=item->count;
                    if(item->type==FOOD_BLOOD)blood+=item->count;
                    if(item->type==TOOL_METAL_SCRAP)scrap+=item->count;
                    if(category==FOOD)food+=item->count;
                    if(item->type==TOOL_LOCKPICK || item->type==TOOL_TORCH)tools+=item->count;
                }
                // Native rooms can supply authored bonus items and merge identical stacks.
                // Verify usable quantities and each role's reward contract, not stack layout.
                bool contents=role==0 ? equipment>=1 && heals>=2 && mana>=1 && food>=3 && tools>=5 && blood>=1 && scrap>=4 :
                    role==1 ? equipment>=4 && heals>=2 && tools>=2 :
                    role==2 ? magicRewards>=4 && heals>=1 && mana>=3 && diamonds>=1 :
                    role==3 ? diamonds>=3 && enchants>=1 && heals>=2 && tools>=1 :
                    equipment>=4 && diamonds>=3 && enchants>=2 && heals>=3;
                if(!contents)printlog("[SMOKE][DELVE] cache_mismatch role=%d pieces=%d equipment=%d books=%d diamonds=%d enchants=%d heal=%d mana=%d food=%d tools=%d",role,pieces,equipment,books,diamonds,enchants,heals,mana,food,tools);
                roleContents &= contents;
                ++checked;
            }
            check(roles[0]&&roles[1]&&roles[2]&&roles[3]&&roles[4],"five_cache_roles");
            check(roleContents&&checked>0,"cache_contents_match_roles");
            int xp=stats[0]->EXP;
            if(chest) {
                move(0,*chest); selectedEntity[0]=chest; inrange[0]=true; chest->actChest();
                ::Raid::openedChest(*chest,0);
                check(stats[0]->EXP-xp==53,"chest_xp_once"); chest->closeChest();
            }
            auto item=newItem(IRON_SWORD,SERVICABLE,0,1,123,false,nullptr);
            auto pickup=itemPickup(0,item);
            check(pickup && pickup->identified==(scenario=="quality"),"identification_toggle");
            if(pickup && pickup!=item) free(item);
            for(int n=0;n<3;++n) if(auto shrine=find(&actFountain,n)) {
                move(0,*shrine); selectedEntity[0]=shrine; inrange[0]=true;
                if(shrine->skill[20]==1) {
                    h->setHP(stats[0]->MAXHP-20); int hp=stats[0]->HP; actFountain(shrine);
                    check(stats[0]->HP>hp,"healing_shrine"); hp=stats[0]->HP;
                    actFountain(shrine); check(stats[0]->HP==hp,"shrine_cooldown");
                } else if(shrine->skill[20]==2) {
                    actFountain(shrine); check(stats[0]->getEffectActive(EFF_FAST)>0,"speed_shrine");
                } else {
                    int before=stats[0]->weapon ? stats[0]->weapon->beatitude : -1;
                    actFountain(shrine); check(stats[0]->weapon && stats[0]->weapon->beatitude==before+1,"enchantment_shrine");
                }
            }
            int lx=0,ly=0;double yaw=0;check(layoutPoint("combat_lane",0,lx,ly,yaw),"native_combat_lane");
            h->teleporterMove(lx,ly,2);g->teleporterMove(lx+3,ly,2);h->yaw=0;
            stats[1]->HP=stats[1]->MAXHP=200;
            previousHP=stats[1]->HP; stats[0]->MP=stats[0]->MAXMP=100;
            stats[0]->setProficiency(spell_fireball.skillID,100);
            check(castSpell(h->getUID(),&spell_fireball,false,false)!=nullptr,"real_spell_created");
            phase=1; began=ticks;
        } else if(phase==1) {
            check(stats[1]->HP<previousHP,"pvp_spell_damage");
            h->yaw=0; previousHP=stats[1]->HP;
            auto bow=newItem(CROSSBOW,EXCELLENT,3,1,0,true,&stats[0]->inventory); useItem(bow,0);
            h->attack(1,MAXCHARGE,nullptr); phase=2; began=ticks;
        } else if(phase==2) {
            check(stats[1]->HP<previousHP,"pvp_ranged_damage");
            previousHP=stats[1]->HP;
            auto acid=newItem(POTION_ACID,EXCELLENT,0,1,0,true,&stats[0]->inventory);
            equipItem(acid,&stats[0]->weapon,0,false); h->yaw=0; h->attack(1,MAXCHARGE,nullptr);
            phase=3; began=ticks;
        } else if(phase==3) {
            check(stats[1]->HP<previousHP,"pvp_thrown_potion_damage");
            Entity* victim=nullptr;
            for(auto node=map.entities->first;node;node=node->next) {
                auto e=static_cast<Entity*>(node->element);
                if(e->behavior==&actMonster && e->getStats() && e->getStats()->type!=SHOPKEEPER) { victim=e; break; }
            }
            if(victim) {
                auto sword=newItem(ARTIFACT_SWORD,EXCELLENT,20,1,0,true,&stats[0]->inventory); useItem(sword,0);
                victim->getStats()->HP=1; victim->teleporterMove((int)h->x/16+1,(int)h->y/16,2); h->yaw=0;
                int before=stats[0]->EXP; h->attack(1,MAXCHARGE,victim);
                // Exact native multiplier/rounding is checked by the build audit;
                // native kill awards vary with species, level and their random roll.
                check(stats[0]->EXP>before,"monster_xp_awarded");
            }
            ::Raid::setSmokeElapsed(1200); phase=4;
        } else ::Raid::setSmokeElapsed(1200);
        return;
    }
    if (scenario == "combat") {
        if (phase == 0) {
            auto h=players[0]->entity, g=players[1]->entity;
            int lx=0,ly=0;double yaw=0;check(layoutPoint("combat_lane",0,lx,ly,yaw),"native_combat_lane");
            h->teleporterMove(lx,ly,2);g->teleporterMove(lx+1,ly,2);h->yaw=0;
            stats[1]->HP=stats[1]->MAXHP=20;
            auto sword=newItem(ARTIFACT_SWORD,EXCELLENT,20,1,0,true,&stats[0]->inventory);
            useItem(sword,0);
            auto marked=newItem(TOOL_SKELETONKEY,EXCELLENT,3,1,0,true,nullptr);
            if(auto drop=dropItemMonster(marked,h,nullptr)) {
                drop->x=g->x; drop->y=g->y; drop->vel_x=drop->vel_y=0;
                lootUID=drop->getUID(); publish(1,drop);
            }
            check(lootUID!=0,"death_loot_fixture"); phase=1; began=ticks;
        } else if (phase == 1 && ticks-began>TICKS_PER_SECOND && !uidToEntity(lootUID)) {
            auto g=players[1]->entity;
            if (g && stats[1]->HP>0) { players[0]->entity->attack(1,MAXCHARGE,g); printlog("[SMOKE][DELVE] melee hp=%d",stats[1]->HP); }
            if (stats[1]->HP<=0) phase=2;
            began=ticks;
        } else if(phase==2 && ticks-began>TICKS_PER_SECOND*2) {
            bool dropped=false;
            Entity* bagEntity=nullptr;
            for(auto node=map.entities->first;node;node=node->next) {
                auto e=static_cast<Entity*>(node->element);
                if(e->behavior==&actItem && e->getUID()!=lootUID && e->skill[10]==TOOL_SKELETONKEY && e->skill[12]==3) dropped=true;
                if(e->behavior==&actItem && e->skill[10]==TOOL_PLAYER_LOOT_BAG) {
                    auto bag=stats[0]->player_lootbags.find(e->skill[14]);
                    if(bag!=stats[0]->player_lootbags.end()) for(const auto& item:bag->second.items)
                        if(item.type==TOOL_SKELETONKEY && item.beatitude==3) { dropped=true; bagEntity=e; }
                }
            }
            if(!dropped && ticks-began<TICKS_PER_SECOND*10) return;
            check(dropped,"death_inventory_drop");
            if(bagEntity) {
                // Recover the actual spawned bag via native inventory APIs; the GUI's Open action
                // uses item_ToolLootBag, rather than generic weapon/item use.
                auto physical=newItemFromEntity(bagEntity);
                auto picked=itemPickup(0,physical);
                if(picked!=physical) free(physical);
                list_RemoveNode(bagEntity->mynode);
                ::Raid::setSmokeElapsed(400);
                if(auto portal=find(&actPortal)) {
                    use(0,*portal);
                    check(::Raid::outcome(0)==O::Playing,"unopened_bag_cannot_be_lost_at_extraction");
                }
                for(auto n=stats[0]->inventory.first;n;n=n->next) {
                    auto item=static_cast<Item*>(n->element);
                    if(item->type==TOOL_PLAYER_LOOT_BAG) { item_ToolLootBag(item,0); break; }
                }
                bool recovered=false;
                for(auto n=stats[0]->inventory.first;n;n=n->next) {
                    auto item=static_cast<Item*>(n->element);
                    if(item->type==TOOL_SKELETONKEY && item->beatitude==3) recovered=true;
                }
                check(recovered,"rival_loot_recovered");
                if(auto portal=find(&actPortal)) use(0,*portal);
            }
            phase=3; ::Raid::setSmokeElapsed(1200);
        }
        return;
    }
    if (scenario == "faults") {
        auto portal=find(&actPortal); if (!portal) return;
        if (phase==0) { // A hidden early portal cannot extract.
            portalUID=portal->getUID(); use(1,*portal); began=ticks; phase=1;
        } else if (phase==1 && ticks-began>TICKS_PER_SECOND*2) {
            printlog("[SMOKE][DELVE] early_portal_denied=%d",::Raid::outcome(1)==O::Playing);
            ::Raid::setSmokeElapsed(400); phase=2;
        } else if (phase==2 && ::Raid::isExtracted(1)) {
            std::memcpy(net_packet->data,"REXT",4); SDLNet_Write32(uniqueGameKey,&net_packet->data[4]);
            SDLNet_Write32(portalUID,&net_packet->data[8]); SDLNet_Write32(mapseed,&net_packet->data[12]);
            net_packet->address=net_clients[0]; net_packet->len=16;
            sendPacketSafe(net_sock,-1,net_packet,0); sendPacketSafe(net_sock,-1,net_packet,0);
            use(0,*portal);
            printlog("[SMOKE][DELVE] spent_portal_denied=%d",::Raid::outcome(0)==O::Playing);
            phase=3; began=ticks;
        } else if (phase==3 && ticks-began>TICKS_PER_SECOND*3) ::Raid::setSmokeElapsed(1200);
        return;
    }
    if (scenario == "host-early" && currentlevel==1) {
        if (phase==0) { ::Raid::setSmokeElapsed(400); if(auto e=find(&actPortal)) { use(0,*e); phase=1; began=ticks; } }
        if (phase==1 && ::Raid::isExtracted(0) && ticks-began>TICKS_PER_SECOND*2) { if(auto e=find(&actLadder)) use(1,*e); }
        return;
    }
    if (currentlevel<3) {
        auto ladder=find(&actLadder); if (!ladder) return;
        if (phase==0) {
            if (::Raid::outcome(0)==O::Playing) use(0,*ladder);
            else use(1,*ladder);
            phase=1; began=ticks;
        } else if (phase==1 && ticks-began>TICKS_PER_SECOND*2) {
            if (::Raid::outcome(0)==O::Descended && players[0]->entity) {
                int hp=stats[0]->HP; players[0]->entity->modHP(-9999);
                printlog("[SMOKE][DELVE] staging floor=%d protected=%d",currentlevel,stats[0]->HP==hp);
            }
            for(int p=1;p<peers;++p) if(::Raid::outcome(p)==O::Playing) use(p,*ladder);
            phase=2;
        }
    } else {
        if (phase==0) ::Raid::setSmokeElapsed(400);
        if (phase==0) {
            int portal=0;
            for(int p=0;p<peers;++p) {
                if(::Raid::outcome(p)!=O::Playing) continue;
                auto exit=find(&actPortal,portal++);
                if(exit) use(p,*exit);
            }
            phase=1; began=ticks;
        }
        if (phase==1 && ticks-began>TICKS_PER_SECOND*7) ::Raid::setSmokeElapsed(1200);
    }
}
void guest() {
    if (scenario == "shops") { shopNetworkAudit(); return; }
    if (scenario == "battle-royale" || scenario == "battle-wipe" || scenario == "battle-host-win") battleGuest();
    if (scenario == "revenants") { ::Raid::smokeReturnLives(); return; }
    if (scenario == "rarity" && !rarityGuest()) return;
    if (scenario=="concealment") {
        if(std::filesystem::exists(sync+"/concealment-hidden") && !std::filesystem::exists(sync+"/concealment-guest-"+std::to_string(clientnum))) {
            bool ready=true;
            for(int slot=0;slot<peers;++slot)ready &= players[slot]->entity && players[slot]->entity->flags[INVISIBLE];
            if(ready) { checkConcealment(); std::ofstream(sync+"/concealment-guest-"+std::to_string(clientnum)) << "ready"; }
        }
        if(std::filesystem::exists(sync+"/concealment-visible") && !std::filesystem::exists(sync+"/concealment-guest-visible-"+std::to_string(clientnum))) {
            bool ready=true;
            for(int slot=0;slot<peers;++slot)ready &= players[slot]->entity && !players[slot]->entity->flags[INVISIBLE];
            if(ready) {
                check(true,"invisibility_restores_visibility_guest");
                std::ofstream(sync+"/concealment-guest-visible-"+std::to_string(clientnum)) << "ready";
            }
        }
    }

    if ((scenario=="appraisal" || scenario=="appraisal-disabled") && players[clientnum]->entity) {
        static bool checked=false;
        if (!checked && ticks>TICKS_PER_SECOND*2) {
            if (auto ground=find(&actItem)) {
                const bool expected=scenario=="appraisal";
                auto preview=newItemFromEntity(ground,true);
                check(::Raid::instantIdentification()==expected,"guest_appraisal_host_setting");
                check(bool(ground->skill[15])==expected,"guest_appraisal_ground_state");
                check(preview && preview->identified==expected,"guest_appraisal_ground_preview");
                free(preview);
                checkHoverStats(clientnum);
                checked=true;
            }
        }
    }
    if (scenario == "abilities" && players[clientnum]->entity && ::DelveBuilds::activeSelection(clientnum)[2] >= 6) {
        static bool checked = false;
        if (!checked) {
            check(std::string(Input::inputs[clientnum].binding("Delve Ability 1")) == "Q"
                && std::string(Input::inputs[clientnum].binding("Delve Ability 2")) == "E", "legacy_hotkeys_migrated_guest");
            checked = true;
        }
    }
    if (observedFloor!=currentlevel) {
        observedFloor=currentlevel;
        printlog("[SMOKE][DELVE] client_floor slot=%d floor=%d",clientnum,currentlevel);
        if(scenario=="faults") {
            std::filesystem::create_directory("delve-profile.json.tmp");
            std::ofstream("delve-profile.json.tmp/fault-blocker") << "test fixture";
        }
    }
    if (::Raid::isExtracted(clientnum)) {
        if(!committed) { committed=true; printlog("[SMOKE][DELVE] client_banked slot=%d",clientnum); }
        return;
    }
    if (::Raid::outcome(clientnum)!=O::Playing || ticks<clickAt || !players[clientnum]->entity) return;
    int floor=0; Uint32 uid=0; std::ifstream input(sync+"/target-"+std::to_string(clientnum));
    if(!(input>>floor>>uid) || floor!=currentlevel || !uid) return;
    auto target=uidToEntity(uid); auto player=players[clientnum]->entity;
    // Keep this isolated teleport fixture's owner prediction in agreement with
    // the server before asking the ordinary CKIR interaction path to run.
    // Normal gameplay never uses this automation.
    static Uint32 localMoveAt=0;
    if(target && std::hypot(target->x-player->x,target->y-player->y)>TOUCHRANGE && ticks>=localMoveAt) {
        move(clientnum,*target);localMoveAt=ticks+TICKS_PER_SECOND/2;
    }
    if(!target || std::hypot(target->x-player->x,target->y-player->y)>TOUCHRANGE) {
        static Uint32 logAt=0;
        if(ticks>logAt) { logAt=ticks+TICKS_PER_SECOND;printlog("[SMOKE][DELVE] target_wait uid=%u found=%d px=%.1f py=%.1f tx=%.1f ty=%.1f",uid,target!=nullptr,player->x/16,player->y/16,target ? target->x/16:-1,target ? target->y/16:-1); }
        return;
    }
    std::memcpy(net_packet->data,"CKIR",4); net_packet->data[4]=clientnum; SDLNet_Write32(uid,&net_packet->data[5]);
    net_packet->address=net_server; net_packet->len=9; sendPacketSafe(net_sock,-1,net_packet,0);
    clickAt=ticks+TICKS_PER_SECOND/4;
}
void restartMatch() {
    if (fadeout) return;
    if (restartSession != uniqueGameKey) {
        restartSession = uniqueGameKey; ++restartRound;
        observedFloor = phase = 0; began = ticks; clickAt = 0;
        restartAction = 0;
        pendingTargets.fill(0); moveAt.fill(0);
        done = committed = retry = duplicate = false;
        printlog("[SMOKE][DELVE] restart_session round=%d session=%u slot=%d",restartRound,uniqueGameKey,clientnum);
    }
    if (multiplayer == CLIENT) {
        guest();
        if (restartRound == 4 && ::Raid::matchComplete() && !done) {
            done = true; printlog("[SMOKE][DELVE] complete role=client");
        }
        return;
    }
    if (restartAction) {
        Uint32 requestedSession=0; std::ifstream input(sync+"/restart-click");
        if (input>>requestedSession && requestedSession==restartSession) {
            if (::MainMenu::smokeActivateRestart(restartAction==1)) restartAction=0;
            return;
        }
    }
    // These fixtures change outcomes through native death, extraction and descent.
    // The Python driver operates the real Restart menus; it never calls Raid::begin.
    for (auto node=map.entities->first;node;node=node->next) {
        auto e=static_cast<Entity*>(node->element);
        if (e->behavior==&actMonster && e->getStats() &&
            (!e->getStats()->getEffectActive(EFF_ASLEEP) || e->getStats()->EFFECTS_TIMERS[EFF_ASLEEP]<1000))
            e->setEffect(EFF_ASLEEP,true,100000,true);
    }
    if (phase == 0) {
        if (!::Raid::smokeWorldReady() || !players[0]->entity || !allLoaded()) return;
        check(currentlevel==1 && !::Raid::matchComplete() && ::Raid::floorSecondsRemaining()>=350,"restart_fresh_floor_and_clock");
        for (int p=0;p<peers;++p) {
            check(::Raid::canSpawn(p) && ::Raid::outcome(p)==O::Playing,"restart_player_can_spawn");
            check(stats[p]->GOLD==0 && stats[p]->EXP==0 && stats[p]->HP==stats[p]->MAXHP &&
                stats[p]->MAXHP>=50 && stats[p]->MAXHP<=60 && stats[p]->weapon && stats[p]->weapon->status==DECREPIT,"restart_fresh_class_kit");
        }
        printlog("[SMOKE][DELVE] restart_ready_match round=%d session=%u",restartRound,uniqueGameKey);
        printlog("[SMOKE][DELVE] hostility enemy=%d friend=%d friendlyfire=%d",players[0]->entity->checkEnemy(players[1]->entity),players[0]->entity->checkFriend(players[1]->entity),(svFlags & SV_FLAG_FRIENDLYFIRE)!=0);
        phase=1; began=ticks;
    }
    if (ticks-began<TICKS_PER_SECOND*2) return;
    if (restartRound == 1 || restartRound == 4) {
        if (phase==1) {
            if (restartRound==1) {
                stats[0]->GOLD=1234;
                if (auto monster=find(&actMonster)) ::Raid::recordKill(*players[0]->entity,*monster);
                if (auto chest=find(&actChest)) ::Raid::openedChest(*chest,0);
            }
            for (int p=0;p<peers;++p) stats[p]->HP=0;
            phase=2;
        }
        if (phase==2 && ::Raid::matchComplete() && !players[0]->entity) {
            if (restartRound==1) {
                printlog("[SMOKE][DELVE] restart_menu round=1 action=death");
                restartAction=1;
            } else {
                done=true; printlog("[SMOKE][DELVE] complete role=host");
            }
            phase=3;
        }
        if (phase==2 && !::Raid::matchComplete() && ::Raid::smokeWorldReady() && allLoaded()) {
            for (int player=0;player<peers;++player) stats[player]->HP=0;
        }
    } else if (restartRound==2) {
        if (phase==1) {
            if (!::Raid::smokeWorldReady() || !allLoaded()) return;
            if (currentlevel<3) {
                for (int player=0;player<peers;++player) stats[player]->HP=0;
                return;
            }
            stats[0]->GOLD=2345;
            ::Raid::setSmokeElapsed(300);
            if (auto portal=find(&actPortal)) { use(0,*portal); phase=2; }
        }
        if (phase==2 && ::Raid::isExtracted(0)) {
            for (int p=1;p<peers;++p) stats[p]->HP=0;
            phase=3;
        }
        if (phase==3 && ::Raid::matchComplete()) {
            printlog("[SMOKE][DELVE] restart_menu round=2 action=pause"); phase=4; restartAction=2;
        }
    } else if (restartRound==3) {
        if (phase==1) {
            if (auto ladder=find(&actLadder)) {
                use(0,*ladder);
                for (int player=1;player<peers;++player) stats[player]->HP=0;
                phase=2;
            }
        }
        for (int p=1;p<peers;++p) if (phase==2 && currentlevel==1 && pendingTargets[p] && ticks>=moveAt[p]) {
            if (auto target=uidToEntity(pendingTargets[p])) move(p,*target);
            moveAt[p]=ticks+TICKS_PER_SECOND/2;
        }
        if (phase==2 && currentlevel==2 && ::Raid::smokeWorldReady() && allLoaded()) {
            ::Raid::setSmokeElapsed(120); stats[0]->GOLD=3456;
            printlog("[SMOKE][DELVE] restart_menu round=3 action=pause"); phase=3; restartAction=2;
        }
    }
}
void narrationFloors() {
    if (fadeout || currentlevel < 1 || currentlevel > 3) return;
    if (multiplayer == SERVER && !::Raid::smokeWorldReady()) return;
    if (!players[clientnum]->entity) return;
    if (observedFloor != currentlevel) {
        if (multiplayer == SERVER && !allLoaded()) return;
        observedFloor = currentlevel; phase = 0; began = ticks;
        printlog("[INTRO-FLOORS] ready floor=%d slot=%d", currentlevel, clientnum);
    }
    if (multiplayer == SERVER && phase == 0) {
        for (int player = 0; player < peers; ++player)
            if (stats[player]->HP > 0) stats[player]->HP = stats[player]->MAXHP;
        for (auto node = map.entities->first; node; node = node->next) {
            auto entity = static_cast<Entity*>(node->element);
            if (entity->behavior == &actMonster && entity->getStats()
                && (!entity->getStats()->getEffectActive(EFF_ASLEEP) || entity->getStats()->EFFECTS_TIMERS[EFF_ASLEEP] < 1000))
                entity->setEffect(EFF_ASLEEP, true, 100000, true);
        }
    }
    if (ticks - began < 23 * TICKS_PER_SECOND) return;
    const std::string marker = sync + "/narration-finished-" + std::to_string(currentlevel) + "-";
    if (!std::filesystem::exists(marker + std::to_string(clientnum)))
        std::ofstream(marker + std::to_string(clientnum)) << "1";
    if (multiplayer == SERVER) {
        for (int player = 0; player < peers; ++player)
            if (!std::filesystem::exists(marker + std::to_string(player))) return;
        if (currentlevel < 3 && phase == 0) {
            for (int player = 0; player < peers; ++player) stats[player]->HP = 0;
            phase = 1;
        }
    }
    if (currentlevel == 3 && !done) {
        done = true;
        printlog("[INTRO-FLOORS] complete slot=%d", clientnum);
    }
}

}
void tick() {
    if (scenario=="balance" && SmokeHooksCommon::parseEnvString("BARONY_BALANCE_NORMAL_SCOPE","")=="1") {
        ::SmokeTestHooks::Balance::normalTick(); return;
    }
    if(scenario.empty() || sync.empty() || !::Raid::active() || intro || loading) return;
    if (scenario=="balance") {
        if(multiplayer==CLIENT) guest();
        ::SmokeTestHooks::Balance::tick(); return;
    }
    if (scenario=="narration") { narrationFloors(); return; }
    if (scenario=="restart") { restartMatch(); return; }
    if(::Raid::matchComplete()) {
        if (scenario == "results" && !done) {
            auto root = gameUIFrame[clientnum];
            auto gameover = root ? root->findFrame("gameover") : nullptr;
            auto window = gameover ? gameover->findFrame("window") : nullptr;
            auto standings = window ? window->findFrame("delve standings") : nullptr;
            static bool checked = false;
            static Uint32 dismissedAt = 0;
            if (checked) {
                if (!dismissedAt) {
                    if (!std::filesystem::exists(std::filesystem::path(sync) / "results-dismiss")) return;
                    auto dismiss = window ? window->findButton("dismiss") : nullptr;
                    check(dismiss != nullptr, "results_dismiss_available");
                    if (dismiss) { dismiss->select(); dismiss->activate(); }
                    dismissedAt = ticks;
                    return;
                }
                if (ticks - dismissedAt < 2 * TICKS_PER_SECOND) return;
                check(!gameover && ::Raid::matchComplete(), "results_native_dismiss");
            } else {
                if (!standings || window->getSize().y < 0 || ticks - began < 12 * TICKS_PER_SECOND) return;
                check(std::string(window->findField("banner")->getText()) == "DUNGEON DELVE", "results_native_banner");
                check(standings->getSize().y + standings->getSize().h <= window->findField("footer")->getSize().y, "results_clear_footer");
                for (int row = 0; row < peers; ++row) {
                    auto entry = standings->findFrame(("result row " + std::to_string(row)).c_str());
                    check(entry && entry->getSize().y + entry->getSize().h <= standings->getSize().h, "results_row_fits");
                    if (entry) {
                        auto name = entry->findField("adventurer");
                        check(name && std::strlen(name->getText()) > 3 && name->getTextObject()->getWidth() <= 122, "results_name_fits");
                        check(entry->findField("loot") && entry->findField("pvp") && entry->findField("pve") && entry->findField("outcome"), "results_stats_present");
                    }
                }
                check(window->findButton("dismiss") && (multiplayer == CLIENT || (window->findButton("restart") && window->findButton("quit"))), "results_native_controls_preserved");
                checked = true;
                printlog("[SMOKE][DELVE] results_ready role=%s", multiplayer == SERVER ? "host" : "client");
                return;
            }
        }
        if(!done) { done=true; printlog("[SMOKE][DELVE] complete role=%s",multiplayer==SERVER ? "host":"client"); }
        return;
    }
    if(multiplayer==SERVER) host(); else if(multiplayer==CLIENT) guest();
}
}
