#include "main.hpp"
#include "game.hpp"
#include "entity.hpp"
#include "stat.hpp"
#include "items.hpp"
#include "net.hpp"
#include "player.hpp"
#include "raid.hpp"
#include "raid_battle_rules.hpp"
#include "delve_builds.hpp"
#include "delve_passives.hpp"
#include "delve_hud.hpp"
#include "delve_intro.hpp"
#include "ui/Field.hpp"
#include "ui/Button.hpp"
#include "ui/MainMenu.hpp"
#include "ui/GameUI.hpp"
#include "gear_loadout.hpp"
#include "files.hpp"
#include "charclass.hpp"
#include "draw.hpp"
#include "interface/interface.hpp"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <string_view>

namespace MainMenu { double dungeonNarrationVolume(); }

namespace Raid {
void populateWorld(); // raid_world.cpp, server only
bool configureWorld(int floor);
int generateOfficialFloor(unsigned seed,int* checkMapHash);
void configureChest(Entity& chest);
namespace {
struct Result {
    bool participant = false;
    unsigned short kills = 0, mobs = 0;
    Uint32 worth = 0, gold = 0;
    char name[32] = {};
};
std::string profilePath;
bool menuSelected = false;
bool badImport = false, inRaid = false, prepared = false, sendKit = false;
std::array<bool, 3> narrationStarted{};
int narrationCue = -1;
bool complete = false, populated = false, identify = true, localBanked = false;
Uint32 originalFlags = 0; bool flagsSaved = false;
int floorLimit = BattleRules::floorSeconds, floorNumber = 1, elapsed = 0, lastStanding = -1, lastFloorSecond = -1;
Uint32 lastTick = 0, lastState = 0, serial = 0, receivedSerial = 0, localApproval = 0;
std::array<Outcome, MAXPLAYERS> outcomes;
std::array<Result, MAXPLAYERS> results;
std::array<bool, MAXPLAYERS> loadoutReceived;
std::array<Uint32, MAXPLAYERS> pendingPortal, pendingSince;
std::array<std::pair<int,int>, MAXPLAYERS> returnTiles;
struct PendingKill { int killer = -1; Uint32 victim = 0; };
std::array<PendingKill, MAXPLAYERS> pendingKills;
std::array<bool, MAXPLAYERS> awaitingDeath;
std::array<Uint32, MAXPLAYERS> deathObserved, controlRecoveryUntil;
std::set<Uint32> scoredVictims, openedChests;
struct ReturnLife {
    Uint32 loot = 0, baseline = 0, nonce = 0, body = 0, cooldown = 0, applied = 0, evolved = 0;
    bool measured = false, used = false, alive = false, rat = false;
    Uint8 cheese = 0;
    bool algernon = false;
    int choice = 0;
};
std::array<ReturnLife, MAXPLAYERS> returnLives;
constexpr int header = 28, rowBytes = 68, stateSize = header + MAXPLAYERS * rowBytes;
bool valid(int p) { return p >= 0 && p < MAXPLAYERS; }
Entity* playerSource(Entity* source) {
    return !source || source->behavior == &actPlayer ? source : source->monsterAllyGetPlayerLeader();
}
bool hostilePlayerHit(Entity* target, Entity* source) {
    source = playerSource(source);
    return active() && target && target->behavior == &actPlayer && source && source != target &&
        source->behavior == &actPlayer && valid(target->skill[2]) && valid(source->skill[2]) && source->checkEnemy(target);
}
bool fromHost() { return net_packet->address.host == net_server.host && net_packet->address.port == net_server.port; }
int packetPlayer() {
    if (net_packet->len < 9) return -1;
    int p = net_packet->data[8];
    if (p <= 0 || p >= MAXPLAYERS || client_disconnected[p]) return -1;
    const auto& a = net_clients[p-1];
    return net_packet->address.host == a.host && net_packet->address.port == a.port ? p : -1;
}
const char* status(Outcome o) {
    switch (o) {
        case Outcome::Playing: return "Delving";
        case Outcome::Descended: return "Descended";
        case Outcome::PendingExtraction: return "Claiming victory";
        case Outcome::Extracted: return "Winner";
        case Outcome::Slain: return complete || floorNumber == 3 ? "Eliminated" : "Out this floor";
        default: return "Abandoned";
    }
}
std::vector<int> ranking() {
    std::vector<int> order;
    for (int p = 0; p < MAXPLAYERS; ++p) if (results[p].participant) order.push_back(p);
    std::stable_sort(order.begin(), order.end(), [](int a, int b) {
        bool ea = outcomes[a] == Outcome::Extracted, eb = outcomes[b] == Outcome::Extracted;
        if (ea != eb) return ea;
        if (results[a].worth != results[b].worth) return results[a].worth > results[b].worth;
        if (results[a].kills != results[b].kills) return results[a].kills > results[b].kills;
        return results[a].mobs > results[b].mobs;
    });
    return order;
}
void writeResults() {
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
    w.StartObject(); w.Key("mode"); w.String("Dungeon Delve");
    w.Key("protocol"); w.Int(protocolVersion); w.Key("session"); w.Uint(uniqueGameKey);
    w.Key("complete"); w.Bool(complete); w.Key("last_standing_slot"); w.Int(lastStanding);
    w.Key("ranking"); w.StartArray();
    int rank = 0;
    for (int p : ranking()) {
        w.StartObject(); w.Key("rank"); w.Int(++rank); w.Key("slot"); w.Int(p);
        w.Key("name"); w.String(results[p].name); w.Key("status"); w.String(status(outcomes[p]));
        w.Key("extracted_worth"); w.Uint(results[p].worth); w.Key("extracted_gold"); w.Uint(results[p].gold);
        w.Key("players_killed"); w.Uint(results[p].kills); w.Key("monsters_slain"); w.Uint(results[p].mobs); w.EndObject();
    }
    w.EndArray(); w.EndObject();
    auto path = std::filesystem::path(profilePath).parent_path() / "delve-results.json";
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(buffer.GetString(), buffer.GetSize()); output.close();
    if (!output) printlog("[DELVE] results write failed: %s", path.string().c_str());
}
void sendState() {
    if (multiplayer != SERVER) return;
    ++serial;
    for (int p = 1; p < MAXPLAYERS; ++p) {
        if (client_disconnected[p]) continue;
        std::memset(net_packet->data, 0, stateSize); std::memcpy(net_packet->data, "RSTA", 4);
        SDLNet_Write32(uniqueGameKey, &net_packet->data[4]); SDLNet_Write32(serial, &net_packet->data[8]);
        net_packet->data[12] = protocolVersion; net_packet->data[13] = floorNumber;
        net_packet->data[14] = complete; net_packet->data[15] = identify;
        SDLNet_Write32(elapsed, &net_packet->data[16]); SDLNet_Write32(floorLimit, &net_packet->data[20]);
        net_packet->data[24] = lastStanding + 1;
        for (int slot = 0; slot < MAXPLAYERS; ++slot) {
            int at = header + rowBytes * slot;
            net_packet->data[at] = static_cast<Uint8>(outcomes[slot]); net_packet->data[at+1] = results[slot].participant;
            SDLNet_Write16(results[slot].kills, &net_packet->data[at+2]); SDLNet_Write16(results[slot].mobs, &net_packet->data[at+4]);
            SDLNet_Write32(results[slot].worth, &net_packet->data[at+6]); SDLNet_Write32(results[slot].gold, &net_packet->data[at+10]);
            const auto& life = returnLives[slot];
            SDLNet_Write32(life.loot, &net_packet->data[at+46]);
            SDLNet_Write32(life.nonce, &net_packet->data[at+50]);
            SDLNet_Write32(life.body, &net_packet->data[at+54]);
            SDLNet_Write32(life.cooldown, &net_packet->data[at+58]);
            net_packet->data[at+62] = life.used; net_packet->data[at+63] = life.alive;
            net_packet->data[at+64] = life.choice; net_packet->data[at+65] = life.rat;
            net_packet->data[at+66] = life.cheese; net_packet->data[at+67] = life.algernon;
            std::memcpy(&net_packet->data[at+14], results[slot].name, 32);
        }
        net_packet->address = net_clients[p-1]; net_packet->len = stateSize;
        sendPacketSafe(net_sock, -1, net_packet, p-1);
    }
    lastState = ticks;
}
void park(int p) {
    if (!players[p] || !players[p]->entity) return;
    players[p]->entity->teleporterMove(3+p*8, 4, 2);
    players[p]->entity->vel_x = players[p]->entity->vel_y = 0;
    if (players[p]->isLocalPlayer()) {
        players[p]->closeAllGUIs(CloseGUIShootmode::CLOSEGUI_ENABLE_SHOOTMODE, CloseGUIIgnore::CLOSEGUI_CLOSE_ALL);
        players[p]->bControlEnabled = false;
    }
}
bool bankLocalGear() {
    uint64_t worth = std::max(0, stats[clientnum]->GOLD);
    for (auto node = stats[clientnum]->inventory.first; node; node = node->next) {
        auto item = static_cast<Item*>(node->element);
        if (item->type >= 0 && item->type < NUMITEMS && itemCategory(item) != SPELL_CAT)
            worth += static_cast<uint64_t>(std::max(0, items[item->type].gold_value)) * std::max(0, (int)item->count);
    }
    results[clientnum].worth = std::min<uint64_t>(worth, 100000000);
    results[clientnum].gold = std::max(0, stats[clientnum]->GOLD);
    outcomes[clientnum] = Outcome::Extracted; localBanked = true;
    printlog("[RAID] extraction committed slot=%d seed=%u", clientnum, mapseed);
    messagePlayer(clientnum, MESSAGE_MISC, "Victory! You claimed the final exit."); return true;
}
void finishExtraction(int p, Uint32 worth, Uint32 gold) {
    lastStanding = p;
    outcomes[p] = Outcome::Extracted; results[p].worth = worth; results[p].gold = gold;
    if (auto exit = uidToEntity(pendingPortal[p])) { exit->skill[24] = -1; serverUpdateEntitySkill(exit, 24); }
    park(p); complete = true;
    for (int rival = 0; rival < MAXPLAYERS; ++rival)
        if (rival != p && results[rival].participant && outcomes[rival] != Outcome::Abandoned) {
            outcomes[rival] = Outcome::Slain;
            if (players[rival]) players[rival]->bControlEnabled = false;
        }
    writeResults(); printlog("[DELVE] victory slot=%d seed=%u", p, mapseed); sendState();
}
void releasePortal(int p) {
    if (auto exit = uidToEntity(pendingPortal[p])) if (exit->skill[24] == p+1) { exit->skill[24] = 0; serverUpdateEntitySkill(exit, 24); }
    outcomes[p] = Outcome::Playing;
    if (players[p] && players[p]->entity) { players[p]->entity->teleporterMove(returnTiles[p].first, returnTiles[p].second, 2); players[p]->bControlEnabled = true; }
    sendState();
}
void configure() {
    identify = true; floorLimit = BattleRules::floorSeconds;
    auto input = PHYSFS_openRead("data/rmf/raid.json"); if (!input) return;
    auto length = PHYSFS_fileLength(input); std::string json;
    if (length > 0 && length < 65536) { json.resize(length); PHYSFS_readBytes(input, json.data(), length); }
    PHYSFS_close(input); rapidjson::Document d; d.Parse(json.c_str());
    if (d.HasParseError() || !d.IsObject()) return;
    if (d.HasMember("instant_identification") && d["instant_identification"].IsBool()) identify = d["instant_identification"].GetBool();
    if (d.HasMember("floor_seconds") && d["floor_seconds"].IsInt()) floorLimit = std::clamp(d["floor_seconds"].GetInt(), 300, 480);
}
void nerfKit(int p) {
    stats[p]->GOLD = 0;
    // Give one starter spell/burn time to be answered, keeping the class HP gap.
    stats[p]->HP = stats[p]->MAXHP = std::max(50, stats[p]->MAXHP + 20);
    stats[p]->OLDHP = stats[p]->HP;
    std::array<int, NUMITEMS> quantities{};
    Item* readyWeapon = nullptr;
    for (auto node = stats[p]->inventory.first; node;) {
        auto next = node->next;
        auto item = static_cast<Item*>(node->element); auto category = itemCategory(item);
        item->beatitude = 0;
        if (item->type == CRYSTAL_SWORD) item->type = IRON_SWORD;
        if (item->type == CRYSTAL_BATTLEAXE) item->type = IRON_AXE;
        if (item->type == LEATHER_BOOTS_SPEED) item->type = LEATHER_BOOTS;
        if (item->type == AMULET_MAGICREFLECTION) {
            if (stats[p]->amulet == item) equipItem(item, &stats[p]->amulet, p, false);
            item->type = RING_MAGICRESISTANCE;
            if (!stats[p]->ring) equipItem(item, &stats[p]->ring, p, false);
        }
        if (item->type == POTION_FIRESTORM || item->type == POTION_ICESTORM || item->type == POTION_THUNDERSTORM)
            item->type = POTION_ACID;
        if (category == WEAPON || category == ARMOR || category == MAGICSTAFF) item->status = DECREPIT;
        int budget = 0;
        if (category == THROWN) { item->status = DECREPIT; budget = 2; }
        if (category == POTION || category == FOOD || category == SCROLL) budget = 1;
        // Brewer retains two offensive doses after its two storm types become acid.
        if (client_classes[p] == CLASS_BREWER && item->type == POTION_ACID) budget = 2;
        if (item->type == POTION_ACID || item->type == POTION_SICKNESS) item->status = WORN;
        if (item->type == TOOL_BEARTRAP) budget = 1;
        if ((item->type >= QUIVER_SILVER && item->type <= QUIVER_HUNTING) || item->type == QUIVER_BONE || item->type == QUIVER_BLACKIRON)
            budget = 12;
        if (budget) {
            item->count = std::min<int>(item->count, std::max(0, budget - quantities[item->type]));
            if (!item->count) { list_RemoveNode(node); node = next; continue; }
        }
        quantities[item->type] += item->count;
        if ((client_classes[p] == CLASS_BREWER && item->type == IRON_AXE) ||
            (client_classes[p] == CLASS_MESMER && item->type == MAGICSTAFF_COLD) ||
            (client_classes[p] == CLASS_JOKER && item->type == SLING)) readyWeapon = item;
        node = next;
    }
    if (readyWeapon && readyWeapon != stats[p]->weapon) equipItem(readyWeapon, &stats[p]->weapon, p, false);
    auto ensure = [&](ItemType type, int count = 1) {
        if (quantities[type] < count) newItem(type, SERVICABLE, 0, count - quantities[type], 0, true, &stats[p]->inventory);
    };
    ensure(POTION_HEALING);
    // Supply the native resource each innate race can actually use. Loot remains
    // shared; caches below also carry blood/scrap rather than only human rations.
    const int race = stats[p]->stat_appearance == 0 ? stats[p]->playerRace : RACE_HUMAN;
    if (race == RACE_SKELETON) ensure(POTION_RESTOREMAGIC);
    else if (race == RACE_VAMPIRE) ensure(FOOD_BLOOD);
    else if (race == RACE_AUTOMATON) ensure(TOOL_METAL_SCRAP, 4);
    else ensure(FOOD_BREAD);
    if (race == RACE_GOATMAN) {
        stats[p]->clearEffect(EFF_ASLEEP); stats[p]->EFFECTS_TIMERS[EFF_ASLEEP] = 0;
    }
}
}
bool parseArgument(const char* argument) {
    const std::string_view v(argument), profile = "-raid-profile=", import = "-raid-import=";
    if (v.substr(0, profile.size()) == profile) { profilePath = v.substr(profile.size()); menuSelected = true; return true; }
    const std::string_view menuProfile = "-delve-menu-profile=";
    if (v.substr(0, menuProfile.size()) == menuProfile) { profilePath = v.substr(menuProfile.size()); return true; }
    if (v.substr(0, import.size()) == import) { badImport = true; return true; }
    return false;
}
bool requested() { return menuSelected && !profilePath.empty(); }
bool menuAvailable() {
    std::error_code error;
    if (!std::filesystem::is_regular_file("data/rmf/raid.json", error)) return false;
    for (int floor = 1; floor <= 3; ++floor)
        if (!std::filesystem::is_regular_file("maps/delve" + std::to_string(floor) + ".lmp", error)) return false;
    return true;
}
void selectFromMenu(bool enabled) {
    if (enabled && profilePath.empty()) profilePath = "delve-profile.json";
    menuSelected = enabled;
    printlog("[DELVE] menu selection=%s", enabled ? "dungeon-delve" : "campaign");
}
int experienceGain(int amount) { return active() ? BattleRules::experienceGain(amount) : amount; }
bool neutralShopkeeperPair(const Entity& first, const Entity& second) {
    if (!active() || first.behavior != &actMonster || second.behavior != &actMonster) return false;
    const auto firstStats = first.getStats(), secondStats = second.getStats();
    return firstStats && secondStats && (firstStats->type == SHOPKEEPER || secondStats->type == SHOPKEEPER);
}
// Delve monsters only fight adventurers: two leaderless dungeon monsters never infight.
// Player-led followers keep native hostility so summons can still defend their owner.
bool neutralMonsterPair(const Entity& first, const Entity& second) {
    if (!active() || &first == &second || first.behavior != &actMonster || second.behavior != &actMonster) return false;
    const auto firstStats = first.getStats(), secondStats = second.getStats();
    return firstStats && secondStats && !firstStats->leader_uid && !secondStats->leader_uid
        && first.monsterAllyIndex < 0 && second.monsterAllyIndex < 0;
}
bool active() { return inRaid && multiplayer != SINGLE; }
bool concealedFromObserver(const Entity& entity, int observer) {
    if (!active() || intro) return false;
    for (int h = 0; h < MAXPLAYERS; ++h) {
        const Entity* root = players[h] ? players[h]->entity : nullptr;
        if (!root || h == observer || !root->flags[INVISIBLE]) continue;
        if (&entity == root || entity.parent == root->getUID()) return true;
        const Entity* part = &entity;
        for (int depth = 0; part->parent && depth < 8; ++depth) {
            part = uidToEntity(part->parent);
            if (!part) break;
            if (part == root) return true;
        }
    }
    return false;
}
Outcome outcome(int p) { return valid(p) ? outcomes[p] : Outcome::Abandoned; }
bool isExtracted(int p) { return active() && outcome(p) == Outcome::Extracted; }
bool hasLoadout(int p) { return valid(p) && loadoutReceived[p]; }
bool matchComplete() { return active() && complete; }
int floorSecondsRemaining() { return std::max(0, floorLimit - elapsed / TICKS_PER_SECOND); }
bool instantIdentification() { return active() && identify; }
bool protectedPlayer(int p) { return active() && valid(p) && (outcomes[p] == Outcome::Descended || outcomes[p] == Outcome::PendingExtraction || outcomes[p] == Outcome::Extracted); }
bool canSpawn(int p) { return !active() || (valid(p) && results[p].participant && (outcomes[p] == Outcome::Playing || outcomes[p] == Outcome::Descended || revenant(p))); }
bool holdPlayer(Entity& p) { if (!protectedPlayer(p.skill[2])) return false; p.vel_x = p.vel_y = 0; return true; }
#include "raid_revenants.inc"
bool applyControl(Entity& target, Entity* source, int effect, int duration, bool updateClients, unsigned char strength) {
    int cap = 0;
    switch (effect) {
        case EFF_ASLEEP: case EFF_CONFUSED: cap = 3*TICKS_PER_SECOND; break;
        case EFF_FEAR: case EFF_WEBBED: cap = 2*TICKS_PER_SECOND; break;
        case EFF_PARALYZED: cap = 3*TICKS_PER_SECOND/2; break;
    }
    auto apply = [&](int time) { return target.setEffect(effect, Uint8(strength), time, updateClients, true, effect == EFF_CONFUSED); };
    if (!cap || !strength || !hostilePlayerHit(&target, source)) return apply(duration);
    const int p = target.skill[2];
    auto stat = target.getStats();
    if (!stat || stat->getEffectActive(EFF_ASLEEP) || stat->getEffectActive(EFF_PARALYZED) ||
        (controlRecoveryUntil[p] && static_cast<Sint32>(controlRecoveryUntil[p] - ticks) > 0)) return false;
    duration = std::clamp(duration, 1, cap);
    if (!apply(duration)) return false;
    controlRecoveryUntil[p] = ticks + duration + 2*TICKS_PER_SECOND;
    return true;
}
double pvpRaceMultiplier(Entity* target, Entity* source, double nativeMultiplier) {
    return hostilePlayerHit(target, source) ? std::clamp(nativeMultiplier, 0.8, 1.2) : nativeMultiplier;
}
bool prepareCharacter(int p, std::string& error) {
    if (!requested()) return true;
    if (std::filesystem::path(profilePath).extension() != ".json") { error = "Use a separate .json extraction profile."; return false; }
    if (badImport) { error = "Dungeon Delve starts with fresh class kits. Campaign imports belong to Bring Your Gear."; return false; }
    prepared = true; return true;
}
void restoreLocalIdentity() {} // Class selection defines identity; banked kits are never redeployed.
void begin(bool enabled) {
    DelveIntro::stop(); narrationStarted.fill(false); narrationCue = -1;
    DelveBuilds::reset();
    if (enabled && !flagsSaved) { originalFlags = svFlags; flagsSaved = true; }
    if (!enabled && flagsSaved) { svFlags = originalFlags; flagsSaved = false; }
    inRaid = prepared = enabled; complete = populated = sendKit = localBanked = false;
    outcomes.fill(Outcome::Playing); loadoutReceived.fill(false); pendingPortal.fill(0); pendingSince.fill(0);
    pendingKills.fill(PendingKill{}); awaitingDeath.fill(false); deathObserved.fill(0); controlRecoveryUntil.fill(0);
    results.fill(Result{}); returnTiles.fill({0, 0}); scoredVictims.clear(); openedChests.clear();
    returnLives.fill(ReturnLife{});
    adventurerProgression.fill(AdventurerProgression{});
    serial = receivedSerial = localApproval = 0; elapsed = 0; lastStanding = -1; floorNumber = 1; lastFloorSecond = -1;
    lastTick = lastState = ticks;
    if (!enabled) return;
    configure(); svFlags |= SV_FLAG_FRIENDLYFIRE | SV_FLAG_TRAPS;
    svFlags &= ~(SV_FLAG_KEEPINVENTORY | SV_FLAG_LIFESAVING | SV_FLAG_ASSIST_ITEMS);
    for (int p = 0; p < MAXPLAYERS; ++p) { results[p].participant = !client_disconnected[p]; if (stats[p]) std::strncpy(results[p].name, stats[p]->name, 31); }
}
void deployLocalCharacter() {
    if (!active() || !prepared) return; prepared = false;
    if (multiplayer == SERVER) { for (int p = 0; p < MAXPLAYERS; ++p) if (!client_disconnected[p]) { nerfKit(p); DelveBuilds::deployed(p); } }
    else { nerfKit(clientnum); DelveBuilds::deployed(clientnum); }
    sendKit = multiplayer == CLIENT;
    for (int p = 0; p < MAXPLAYERS; ++p) if (players[p] && players[p]->isLocalPlayer()) measureReturnLoot(p);
    printlog("[RAID] deployed slot=%d seed=%u source=fresh-class", clientnum, mapseed);
}
int loadFloor(int floor, unsigned int seed, int* checkMapHash) {
    if (floor < 1 || floor > 3) { printlog("[DELVE] rejected floor=%d", floor); return -1; }
    DelveIntro::stop(); narrationCue = -1;
    floorNumber = floor; populated = false; elapsed = 0; lastTick = ticks; mapseed = seed; secretlevel = false; lastFloorSecond = -1;
    for (int p = 0; p < MAXPLAYERS; ++p) {
        if (floor > 1 && results[p].participant && !client_disconnected[p]
            && (outcomes[p] == Outcome::Slain || stats[p]->HP <= 0)) {
            list_FreeAll(&players[p]->magic.spellList);
            auto& progression = adventurerProgression[p];
            if (!returnLives[p].used || !progression.saved) progression.capture(*stats[p]);
            stats[p]->clearStats();
            outcomes[p] = Outcome::Playing;
            awaitingDeath[p] = false; pendingKills[p] = PendingKill{};
            returnLives[p] = ReturnLife{};
            initClass(p); nerfKit(p);
            loadoutReceived[p] = players[p]->isLocalPlayer();
            DelveBuilds::redeploy(p);
            progression.restore(*stats[p]);
            progression.awaitingKit = multiplayer == SERVER && !players[p]->isLocalPlayer();
            if (players[p]->isLocalPlayer() && multiplayer == CLIENT) sendKit = true;
            printlog("[DELVE] floor_return slot=%d floor=%d source=fresh-class", p, floor);
        }
        if (outcomes[p] == Outcome::Descended) outcomes[p] = Outcome::Playing;
        if (players[p] && outcomes[p] == Outcome::Playing) players[p]->bControlEnabled = true;
    }
    if (!configureWorld(floor)) { printlog("[DELVE] invalid/missing authored layout for floor=%d; restage the updated content package",floor); return -1; }
    int result = generateOfficialFloor(seed,checkMapHash);
    if (result < 0) return result;
    map.flags[MAP_FLAG_GENBYTES3] |= (1 << 24) | (1 << 16); // No digging/teleports into staging cells.
    int portal = 0, shrine = 0, descent = 0;
    for (auto node = map.entities->first; node; node = node->next) {
        auto e = static_cast<Entity*>(node->element);
        if (e->sprite == 45 || e->sprite == 11) {
            if (e->sprite == 45) ++portal;
            e->skill[20] = ++descent; e->skill[24] = 0;
            e->flags[INVISIBLE] = BattleRules::unlockSecond(floor, e->skill[20], floorLimit) > 0;
        }
        if (e->sprite == 14) e->skill[20] = ++shrine;
        if (e->sprite == 21) {
            configureChest(*e);
        }
    }
    printlog("[DELVE] load floor=%d portals=%d limit=%d", floor, portal, floorLimit); return result;
}
void placeSpawnMarkers() {} // Original maps provide four perimeter starts.
void receiveLoadout() {
    if (!active() || multiplayer != SERVER || net_packet->len != GearLoadout::equipmentPacketSize || SDLNet_Read32(&net_packet->data[4]) != uniqueGameKey) return;
    int p = packetPlayer();
    if (p > 0 && !loadoutReceived[p] && outcomes[p] == Outcome::Playing) {
        loadoutReceived[p] = GearLoadout::receiveEquipment(p);
        if (loadoutReceived[p]) {
            printlog("[DELVE] equipment_ready slot=%d",p); DelveBuilds::equipmentReady(p);
            auto& progression = adventurerProgression[p];
            if (progression.awaitingKit) {
                progression.restore(*stats[p]); progression.awaitingKit = false;
                serverUpdatePlayerStats();
            }
        }
    }
}
void receiveEquipmentRequest() {
    if (!active() || multiplayer != CLIENT || net_packet->len != 8 || !fromHost() ||
        SDLNet_Read32(&net_packet->data[4]) != uniqueGameKey || intro || !players[clientnum]->entity) return;
    GearLoadout::sendEquipmentToHost("RKIT", clientnum);
}
void useExit(Entity& exit) {
    if (!active() || complete) return;
    bool escape = exit.behavior == &actPortal;
    const bool victoryExit = floorNumber == 3;
    const int ordinal = std::max(1, exit.skill[20]);
    int unlock = BattleRules::unlockSecond(floorNumber, ordinal, floorLimit);
    bool available = elapsed/TICKS_PER_SECOND >= unlock && exit.skill[24] == 0;
    {
        bool hidden = elapsed/TICKS_PER_SECOND < unlock || exit.skill[24] != 0;
        if (exit.flags[INVISIBLE] != hidden) { exit.flags[INVISIBLE] = hidden; if (multiplayer == SERVER) serverUpdateEntityFlag(&exit, INVISIBLE); }
        if (escape && hidden && exit.light) exit.removeLightField();
        if (escape && !hidden && !exit.light) exit.light = addLight(exit.x / 16, exit.y / 16, "portal_blue");
    }
    if (multiplayer != SERVER || !available
        || (victoryExit && std::find(outcomes.begin(), outcomes.end(), Outcome::PendingExtraction) != outcomes.end())) return;
    for (int p = 0; p < MAXPLAYERS; ++p) {
        if (client_disconnected[p] || outcomes[p] != Outcome::Playing || !players[p] || !players[p]->entity || stats[p]->HP <= 0 ||
            (selectedEntity[p] != &exit && client_selected[p] != &exit)) continue;
        auto& player = *players[p]->entity;
        if (std::hypot(player.x-exit.x, player.y-exit.y) > TOUCHRANGE) continue;
        selectedEntity[p] = client_selected[p] = nullptr;
        if (!victoryExit) {
            exit.skill[24] = -1; serverUpdateEntitySkill(&exit, 24);
            outcomes[p] = Outcome::Descended; park(p); messagePlayer(p, MESSAGE_MISC, "Descended. Waiting safely for the remaining rivals.");
            printlog("[DELVE] descended slot=%d floor=%d", p, floorNumber); sendState(); break;
        }
        exit.skill[24] = p+1; pendingPortal[p] = exit.getUID(); pendingSince[p] = ticks;
        returnTiles[p] = {static_cast<int>(player.x)/16, static_cast<int>(player.y)/16};
        outcomes[p] = Outcome::PendingExtraction; park(p); serverUpdateEntitySkill(&exit, 24);
        if (p == clientnum) { if (bankLocalGear()) finishExtraction(p, results[p].worth, results[p].gold); else releasePortal(p); }
        else {
            std::memcpy(net_packet->data, "REXT", 4); SDLNet_Write32(uniqueGameKey, &net_packet->data[4]);
            SDLNet_Write32(pendingPortal[p], &net_packet->data[8]); SDLNet_Write32(mapseed, &net_packet->data[12]);
            net_packet->address = net_clients[p-1]; net_packet->len = 16; sendPacketSafe(net_sock, -1, net_packet, p-1);
            printlog("[RAID] extraction approved slot=%d seed=%u", p, mapseed); sendState();
        }
        break; // One-use portal, including simultaneous clicks.
    }
}
void receiveExtraction() {
    if (!active() || multiplayer != CLIENT || net_packet->len != 16 || !fromHost() || SDLNet_Read32(&net_packet->data[4]) != uniqueGameKey || SDLNet_Read32(&net_packet->data[12]) != mapseed) return;
    Uint32 portal = SDLNet_Read32(&net_packet->data[8]);
    if (isExtracted(clientnum) && localApproval != portal) return;
    if (localApproval == portal && isExtracted(clientnum)) printlog("[RAID] duplicate extraction approval ignored slot=%d", clientnum);
    bool success = (localApproval == portal && isExtracted(clientnum)) || bankLocalGear(); localApproval = portal;
    std::memcpy(net_packet->data, "RACK", 4); SDLNet_Write32(uniqueGameKey, &net_packet->data[4]);
    net_packet->data[8] = clientnum; net_packet->data[9] = success;
    SDLNet_Write32(portal, &net_packet->data[10]); SDLNet_Write32(mapseed, &net_packet->data[14]);
    SDLNet_Write32(results[clientnum].worth, &net_packet->data[18]); SDLNet_Write32(results[clientnum].gold, &net_packet->data[22]);
    net_packet->address = net_server; net_packet->len = 26; sendPacketSafe(net_sock, -1, net_packet, 0);
}
void receiveExtractionResult() {
    if (!active() || multiplayer != SERVER || net_packet->len != 26 || SDLNet_Read32(&net_packet->data[4]) != uniqueGameKey || SDLNet_Read32(&net_packet->data[14]) != mapseed) return;
    int p = packetPlayer();
    if (p < 0 || outcomes[p] != Outcome::PendingExtraction || SDLNet_Read32(&net_packet->data[10]) != pendingPortal[p]) return;
    if (net_packet->data[9] == 1) finishExtraction(p, std::min(100000000u, SDLNet_Read32(&net_packet->data[18])), std::min(100000000u, SDLNet_Read32(&net_packet->data[22])));
    else releasePortal(p);
}
void receiveState() {
    if (!active() || multiplayer != CLIENT || net_packet->len != stateSize || !fromHost() || SDLNet_Read32(&net_packet->data[4]) != uniqueGameKey || net_packet->data[12] != protocolVersion) return;
    Uint32 nextSerial = SDLNet_Read32(&net_packet->data[8]);
    if (nextSerial <= receivedSerial) return; receivedSerial = nextSerial;
    if (net_packet->data[13] != currentlevel) return;
    floorNumber = net_packet->data[13]; complete = net_packet->data[14]; identify = net_packet->data[15];
    elapsed = SDLNet_Read32(&net_packet->data[16]); floorLimit = SDLNet_Read32(&net_packet->data[20]); lastStanding = (int)net_packet->data[24]-1;
    for (int p = 0; p < MAXPLAYERS; ++p) {
        int at = header + rowBytes*p;
        if (net_packet->data[at] > static_cast<Uint8>(Outcome::Abandoned)) return;
        auto next = static_cast<Outcome>(net_packet->data[at]);
        bool committed = p == clientnum && localBanked && next != Outcome::Extracted;
        outcomes[p] = committed ? Outcome::Extracted : next; results[p].participant = net_packet->data[at+1];
        results[p].kills = SDLNet_Read16(&net_packet->data[at+2]); results[p].mobs = SDLNet_Read16(&net_packet->data[at+4]);
        if (!committed) { results[p].worth = SDLNet_Read32(&net_packet->data[at+6]); results[p].gold = SDLNet_Read32(&net_packet->data[at+10]); }
        std::memcpy(results[p].name, &net_packet->data[at+14], 31); results[p].name[31] = 0;
    }
    for (int p = 0; p < MAXPLAYERS; ++p) {
        const int at = header + rowBytes*p;
        auto& life = returnLives[p];
        life.loot = SDLNet_Read32(&net_packet->data[at+46]);
        life.nonce = SDLNet_Read32(&net_packet->data[at+50]);
        life.body = SDLNet_Read32(&net_packet->data[at+54]);
        life.cooldown = SDLNet_Read32(&net_packet->data[at+58]);
        life.used = net_packet->data[at+62]; life.alive = net_packet->data[at+63];
        life.choice = net_packet->data[at+64]; life.rat = net_packet->data[at+65];
        life.cheese = std::min<Uint8>(10, net_packet->data[at+66]); life.algernon = net_packet->data[at+67] != 0;
    }
    if (players[clientnum]) players[clientnum]->bControlEnabled = outcomes[clientnum] == Outcome::Playing || revenant(clientnum);
    if (complete) { writeResults(); printlog("[DELVE] results received session=%u", uniqueGameKey); }
}
void openedChest(Entity& chest, int p) {
    if (active() && multiplayer == SERVER && valid(p) && outcomes[p] == Outcome::Playing && openedChests.insert(chest.getUID()).second) {
        const char* caches[]={"SUPPLY CACHE","ARMORY CACHE","ARCANE CACHE","TREASURY CACHE","GUARDED RELIC CACHE"};
        messagePlayer(p,MESSAGE_MISC,"%s: +%d XP",caches[std::clamp(chest.skill[20],0,4)],experienceGain(30));
        DelvePassives::openedCache(chest, p);
        stats[p]->EXP += experienceGain(30); printlog("[DELVE] chest_xp slot=%d amount=%d", p, experienceGain(30));
    }
}
void recordKill(Entity& killer, Entity& victim) {
    if (!active() || multiplayer != SERVER || killer.behavior != &actPlayer || !valid(killer.skill[2])) return;
    if (victim.behavior == &actPlayer) {
        const int p = victim.skill[2];
        if (valid(p) && p != killer.skill[2] && outcomes[p] == Outcome::Playing && !scoredVictims.count(victim.getUID()))
            pendingKills[p] = {killer.skill[2], victim.getUID()};
        return; // Native resurrection resolves before elimination and PvP credit.
    }
    if (!scoredVictims.insert(victim.getUID()).second) return;
    auto& r = results[killer.skill[2]];
    if (victim.behavior == &actPlayer && victim.skill[2] != killer.skill[2]) { if (r.kills < 65535) ++r.kills; }
    else if (victim.behavior == &actMonster) { if (r.mobs < 65535) ++r.mobs; }
}
void tick() {
    DelveBuilds::tick();
    if (!active() || intro || loading || fadeout || complete || floorNumber < 1 || floorNumber > 3) {
        if (DelveIntro::active()) DelveIntro::stop();
    }
    if (!active() || intro || loading || fadeout) { lastTick = ticks; return; }
    if (sendKit && players[clientnum]->entity) { sendKit = false; GearLoadout::sendEquipmentToHost("RKIT", clientnum); }
    if (complete) return;
    DelveIntro::setVolume(MainMenu::dungeonNarrationVolume());
    if (floorNumber >= 1 && floorNumber <= 3 && !narrationStarted[floorNumber - 1] && !prepared && players[clientnum] && players[clientnum]->entity) {
        narrationStarted[floorNumber - 1] = true;
        if (DelveIntro::prepare(floorNumber)) {
            DelveIntro::start();
            printlog("[DELVE-INTRO] dungeon-start floor=%d slot=%d", floorNumber, clientnum);
        }
    }
    const int currentCue = DelveIntro::cueIndex();
    if (currentCue != narrationCue) {
        narrationCue = currentCue;
        if (currentCue >= 0) {
            DelveIntro::showSubtitle();
            printlog("[DELVE-INTRO] subtitle=%d position=%.3f floor=%d", currentCue, DelveIntro::position(), floorNumber);
        }
    }
    if (DelveIntro::finished()) DelveIntro::stop();
    Uint32 delta = ticks-lastTick; lastTick = ticks;
    tickReturnLives(std::min<Uint32>(delta, TICKS_PER_SECOND));
    if (!gamePaused) elapsed += std::min<Uint32>(delta, TICKS_PER_SECOND);
    if (multiplayer != SERVER) return;
    if (ticks % TICKS_PER_SECOND == 0) for (int p=1;p<MAXPLAYERS;++p) {
        if (client_disconnected[p] || loadoutReceived[p] || outcomes[p] != Outcome::Playing) continue;
        std::memcpy(net_packet->data,"RREQ",4); SDLNet_Write32(uniqueGameKey,&net_packet->data[4]);
        net_packet->address=net_clients[p-1]; net_packet->len=8;
        sendPacketSafe(net_sock,-1,net_packet,p-1);
    }
    if (!populated) { bool ready = false; for (int p=0; p<MAXPLAYERS; ++p) if (players[p] && players[p]->entity && canSpawn(p)) ready = true; if (!ready) return; }
    if (!populated) { populated = true; populateWorld(); sendState(); }
    const int floorSecond = elapsed / TICKS_PER_SECOND;
    const bool secondChanged = floorSecond != lastFloorSecond;
    lastFloorSecond = floorSecond;
    const int stormStart = BattleRules::stormStartSecond(floorNumber, floorLimit);
    if (!gamePaused && floorSecond >= stormStart && secondChanged) spawnStormWave(floorSecond - stormStart);
    int playing = 0, descended = 0, participants = 0; bool changed = false;
    for (int p = 0; p < MAXPLAYERS; ++p) {
        if (!results[p].participant) continue; ++participants;
        const bool unresolved = outcomes[p] == Outcome::Playing || outcomes[p] == Outcome::Descended || outcomes[p] == Outcome::PendingExtraction;
        bool dead = false;
        if (unresolved && !client_disconnected[p] && stats[p]->HP <= 0) {
            dead = !players[p] || !players[p]->entity;
        } else if (stats[p]->HP > 0) { awaitingDeath[p] = false; pendingKills[p] = PendingKill{}; }
        if (unresolved && (client_disconnected[p] || dead)) {
            outcomes[p] = client_disconnected[p] ? Outcome::Abandoned : Outcome::Slain;
            if (dead && !client_disconnected[p]) ++returnLives[p].nonce;
            const auto credit = pendingKills[p];
            if (dead && !client_disconnected[p] && valid(credit.killer) && scoredVictims.insert(credit.victim).second && results[credit.killer].kills < 65535)
                ++results[credit.killer].kills;
            pendingKills[p] = PendingKill{}; awaitingDeath[p] = false;
            if (auto portal = uidToEntity(pendingPortal[p])) if (portal->skill[24] == p+1) { portal->skill[24] = 0; serverUpdateEntitySkill(portal, 24); }
            printlog("[DELVE] eliminated slot=%d status=%s", p, status(outcomes[p])); changed = true;
        }
        if (outcomes[p] == Outcome::PendingExtraction && ticks-pendingSince[p] > TICKS_PER_SECOND*15) releasePortal(p);
        if (outcomes[p] == Outcome::Playing || outcomes[p] == Outcome::PendingExtraction) ++playing;
        if (outcomes[p] == Outcome::Descended) ++descended;
    }
    int returning = 0;
    for (int p = 0; p < MAXPLAYERS; ++p)
        if (results[p].participant && !client_disconnected[p] && outcomes[p] == Outcome::Slain) ++returning;
    if (!playing && (descended || returning) && floorNumber < 3) {
        if (!loadnextlevel) {
            secretlevel = false; skipLevelsOnLoad = 0; loadnextlevel = true;
            printlog("[DELVE] advancing floor=%d survivors=%d", floorNumber, descended); sendState();
        }
    } else if (!playing && !descended) {
        if (lastStanding >= 0 && outcomes[lastStanding] != Outcome::Extracted) lastStanding = -1;
        for (int p = 0; p < MAXPLAYERS; ++p) if (returnLives[p].alive) {
            stats[p]->HP = 0; returnLives[p].alive = false;
            if (players[p]) players[p]->bControlEnabled = false;
        }
        complete = true; writeResults(); sendState(); printlog("[DELVE] match_complete session=%u participants=%d", uniqueGameKey, participants);
    } else if (changed || ticks-lastState >= TICKS_PER_SECOND) sendState();
    if (secondChanged && floorSecond % 30 == 0 && floorSecondsRemaining() <= 60 && floorSecondsRemaining() > 0)
        for (int p = 0; p < MAXPLAYERS; ++p) if (results[p].participant && outcomes[p] == Outcome::Playing)
            messagePlayer(p, MESSAGE_MISC, "Dungeon collapse in %d seconds. Find an exit!", floorSecondsRemaining());
}
void updateResultsWindow(Frame& window, int player) {
    if (active() && !complete && valid(player) && outcomes[player] == Outcome::Slain) {
        updateReturnWindow(window, player); return;
    }
    if (!active() || !complete || !valid(player) || window.findFrame("delve standings")) return;
    if (auto returns = window.findFrame("delve return")) {
        returns->setDisabled(true);
        if (auto parent = static_cast<Frame*>(window.getParent())) {
            window.setSize({(parent->getSize().w - 500) / 2, (parent->getSize().h - 336) / 2, 500, 336});
            window.setActualSize({0, 0, 500, 336});
        }
        if (auto dismiss = window.findButton("dismiss")) dismiss->setSize({205, 294, 90, 34});
    }
    if (auto background = window.findImage("background")) background->disabled = false;
    for (const auto name : {"delve return border", "delve return background"})
        if (auto image = window.findImage(name)) image->disabled = true;
    if (auto banner = window.findField("banner")) banner->setDisabled(false);
    auto banner = window.findField("banner");
    auto epitaph = window.findField("epitaph");
    auto footer = window.findField("footer");
    if (!banner || !epitaph || !footer) return;
    banner->setText("DUNGEON DELVE");
    footer->setDisabled(false);
    banner->setTextColor(DelveHUD::brass);
    epitaph->setDisabled(true);
    auto standings = window.addFrame("delve standings");
    standings->setSize(SDL_Rect{94, 116, 312, 100});
    standings->setActualSize(SDL_Rect{0, 0, 312, 100});
    standings->setBorder(0);
    standings->setHollow(true);
    const auto label = [](Frame& parent, const char* name, SDL_Rect bounds, const char* value,
        Uint32 color, const char* font = "fonts/pixel_maz_multiline.ttf#12#1", bool right = false) {
        auto field = parent.addField(name, 128);
        field->setSize(bounds);
        field->setFont(font);
        field->setTextColor(color);
        field->setOutlineColor(makeColor(20, 16, 24, 255));
        field->setText(value);
        if (right) field->setHJustify(Field::justify_t::RIGHT);
        return field;
    };
    label(*standings, "adventurer heading", {18, 0, 122, 12}, "ADVENTURER", DelveHUD::muted, "fonts/pixel_maz_multiline.ttf#10#1");
    label(*standings, "loot heading", {146, 0, 66, 12}, "LOOT", DelveHUD::brass, "fonts/pixel_maz_multiline.ttf#10#1", true);
    label(*standings, "pvp heading", {218, 0, 40, 12}, "PVP", DelveHUD::muted, "fonts/pixel_maz_multiline.ttf#10#1", true);
    label(*standings, "pve heading", {264, 0, 44, 12}, "PVE", DelveHUD::muted, "fonts/pixel_maz_multiline.ttf#10#1", true);
    const auto order = ranking();
    int row = 0, localRank = 0;
    char text[128];
    for (int slot : order) {
        const bool local = slot == player;
        if (local) localRank = row + 1;
        snprintf(text, sizeof(text), "result row %d", row);
        auto entry = standings->addFrame(text);
        entry->setSize(SDL_Rect{0, 14 + row * 21, 312, 21});
        entry->setActualSize(SDL_Rect{0, 0, 312, 21});
        entry->setBorder(0);
        entry->setHollow(true);
        entry->addImage(SDL_Rect{0, 0, 312, 21}, local ? makeColor(62, 48, 30, 235) : makeColor(23, 21, 29, 210), "*images/system/white.png", "row background");
        snprintf(text, sizeof(text), "%d", row + 1);
        label(*entry, "rank", {3, 3, 14, 14}, text, row == 0 ? DelveHUD::brass : DelveHUD::muted);
        std::string name = results[slot].name;
        if (Text::get(name.c_str(), "fonts/pixel_maz_multiline.ttf#12#1", 0xffffffff, 0)->getWidth() > 120) {
            while (!name.empty() && Text::get((name + "...").c_str(), "fonts/pixel_maz_multiline.ttf#12#1", 0xffffffff, 0)->getWidth() > 120) {
                auto boundary = name.size() - 1;
                while (boundary > 0 && (static_cast<unsigned char>(name[boundary]) & 0xc0) == 0x80) --boundary;
                name.resize(boundary);
            }
            name += "...";
        }
        auto adventurer = label(*entry, "adventurer", {18, 0, 122, 13}, name.c_str(), local ? DelveHUD::brass : DelveHUD::ivory);
        adventurer->setTooltip(results[slot].name);
        const auto color = outcomes[slot] == Outcome::Extracted ? DelveHUD::cyan : outcomes[slot] == Outcome::Slain ? DelveHUD::danger : DelveHUD::muted;
        label(*entry, "outcome", {18, 11, 122, 10}, status(outcomes[slot]), color, "fonts/pixel_maz_multiline.ttf#10#1");
        snprintf(text, sizeof(text), "%u", results[slot].worth);
        label(*entry, "loot", {146, 4, 66, 14}, text, DelveHUD::brass, "fonts/pixel_maz_multiline.ttf#12#1", true);
        snprintf(text, sizeof(text), "%u", results[slot].kills);
        label(*entry, "pvp", {218, 4, 40, 14}, text, DelveHUD::ivory, "fonts/pixel_maz_multiline.ttf#12#1", true);
        snprintf(text, sizeof(text), "%u", results[slot].mobs);
        label(*entry, "pve", {264, 4, 44, 14}, text, DelveHUD::ivory, "fonts/pixel_maz_multiline.ttf#12#1", true);
        ++row;
    }
    snprintf(text, sizeof(text), "PLACE %d OF %d  -  %s\n%u PVP  /  %u MONSTERS", localRank,
        static_cast<int>(order.size()), status(outcomes[player]), results[player].kills, results[player].mobs);
    footer->setText(text);
}
void drawHUD(int p) {
    if (!active() || intro || p != clientnum) return;
    char text[256];
    if (!complete) {
        if (!players[p] || !players[p]->shootmode || players[p]->gui_mode != GUI_MODE_NONE) return;
        const int left = floorSecondsRemaining();
        const bool escaping = outcomes[p] == Outcome::Playing || outcomes[p] == Outcome::PendingExtraction;
        const auto timerColor = escaping && left <= 60 ? DelveHUD::danger : DelveHUD::brass;
        const DelveHUD::Canvas hud;
        const bool returned = outcomes[p] == Outcome::Slain;
        hud.panel(0, returned ? 124 : 100);
        if (returned) {
            if (algernon(p)) snprintf(text, sizeof(text), "ALGERNON / NPCS IGNORE YOU");
            else if (ratReturn(p)) snprintf(text, sizeof(text), "RAT / CHEESE %u / 10", returnLives[p].cheese);
            else snprintf(text, sizeof(text), "%s", revenant(p) ? "MONSTER LIFE / NPCS IGNORE YOU" : "R: OPEN MONSTER RESPAWN MENU");
            hud.label(14, 104, 272, text, DelveHUD::cyan, "lang/en.ttf#11#1");
        }
        hud.icon("*#images/ui/HUD/death_skull.png", {12, 12, 18, 18}, DelveHUD::brass);
        hud.label(36, 12, 204, "DUNGEON DELVE", DelveHUD::brass, "lang/en.ttf#16#1");
        hud.label(244, 15, 42, "FFA", DelveHUD::muted, "lang/en.ttf#12#1", true);
        hud.rule(38);
        snprintf(text, sizeof(text), "FLOOR %d / 3", floorNumber);
        hud.label(14, 49, 130, text, DelveHUD::ivory, "lang/en.ttf#14#1");
        for (int depth = 1; depth <= 3; ++depth) {
            const int position = 16 + (depth - 1) * 16;
            const auto color = depth <= floorNumber ? DelveHUD::brass : makeColorRGB(78, 76, 69);
            hud.rectangle({position, 76, 8, 8}, color);
            if (depth < 3) hud.rectangle({position + 9, 79, 6, 1}, makeColorRGB(78, 76, 69));
        }
        hud.label(74, 73, 110, status(outcomes[p]), outcomes[p] == Outcome::Extracted ? DelveHUD::cyan : DelveHUD::muted, "lang/en.ttf#12#1");
        snprintf(text, sizeof(text), "%02d:%02d", left / 60, left % 60);
        hud.label(186, 44, 100, text, timerColor, "lang/en.ttf#24#1", true);
        hud.label(184, 73, 102, left ? "COLLAPSE" : "COLLAPSING", timerColor, "lang/en.ttf#12#1", true);
        hud.rectangle({14, 90, DelveHUD::width - 28, 4}, makeColorRGB(10, 12, 15));
        const int remaining = (DelveHUD::width - 30) * std::clamp(left, 0, floorLimit) / std::max(1, floorLimit);
        if (remaining) hud.rectangle({15, 91, remaining, 2}, timerColor);
        DelvePassives::drawHUD(p);
        DelveBuilds::drawHUD(p);
    } else {
        if (!stats[p] || gamePaused || !players[p]
            || (gameUIFrame[p] && gameUIFrame[p]->findFrame("gameover"))
            || !players[p]->shootmode || players[p]->gui_mode != GUI_MODE_NONE) return;
        const auto order = ranking();
        const DelveHUD::Canvas hud;
        hud.panel(0, 106 + static_cast<int>(order.size()) * 28);
        hud.icon("*#images/ui/HUD/death_skull.png", {12, 12, 18, 18}, DelveHUD::brass);
        hud.label(36, 12, 250, "DELVE COMPLETE", DelveHUD::brass, "lang/en.ttf#16#1");
        hud.rule(38);
        hud.label(14, 47, 172, "ADVENTURER", DelveHUD::muted, "lang/en.ttf#11#1");
        hud.label(204, 47, 82, "LOOT", DelveHUD::muted, "lang/en.ttf#11#1", true);
        int row = 0;
        for (int slot : order) {
            const int top = 64 + row * 28;
            if (slot == p) hud.rectangle({8, top - 1, 2, 25}, DelveHUD::brass);
            snprintf(text, sizeof(text), "%d", row + 1);
            hud.label(14, top, 20, text, row == 0 ? DelveHUD::brass : DelveHUD::muted);
            hud.label(38, top, 158, results[slot].name, slot == p ? DelveHUD::brass : DelveHUD::ivory);
            hud.label(38, top + 13, 158, status(outcomes[slot]), outcomes[slot] == Outcome::Extracted ? DelveHUD::cyan : DelveHUD::muted, "lang/en.ttf#10#1");
            snprintf(text, sizeof(text), "%u", results[slot].worth);
            hud.label(204, top + 5, 82, text, DelveHUD::brass, "lang/en.ttf#12#1", true);
            ++row;
        }
        const int bottom = 70 + row * 28;
        hud.rule(bottom);
        snprintf(text, sizeof(text), "PVP %u   /   MONSTERS %u", results[p].kills, results[p].mobs);
        hud.label(14, bottom + 10, 272, text, DelveHUD::ivory, "lang/en.ttf#12#1");
    }
}
#ifdef BARONY_SMOKE_TESTS
void smokeResultsNames() {
    const char* names[] = {"AlexandertheUnvanquished", "SeraphinatheTreasureHunter", "MorriganofTheForgottenVault", "WandererOfTheAncientCrypt"};
    for (int slot = 0; slot < MAXPLAYERS; ++slot) {
        if (!results[slot].participant) continue;
        snprintf(results[slot].name, sizeof(results[slot].name), "%s", names[slot]);
        results[slot].kills = slot;
        results[slot].mobs = 65535 - slot;
    }
}
void smokeResetControl() { controlRecoveryUntil.fill(0); }
void smokeNerfKit(int player) { nerfKit(player); }
bool smokeWorldReady() { return populated; }
void setSmokeElapsed(int seconds) {
    const int requested = std::max(0,seconds)*TICKS_PER_SECOND;
    // Positive fixture jumps advance the clock; repeated calls must not rewind
    // the same second and suppress subsequent collapse pulses. Zero can freeze it.
    if (seconds > 0 && elapsed >= requested) return;
    elapsed = requested; lastTick = ticks;
}
#endif
}
