// Reyes Mod Framework 0.1. Original extension code; see framework/README.md.
#include "main.hpp"
#include "mod_framework.hpp"
#include "entity.hpp"
#include "stat.hpp"
#include "items.hpp"
#include "mod_tools.hpp"
#include "game.hpp"
#include "player.hpp"
#include "magic/magic.hpp"
#include "interface/consolecommand.hpp"
#include <fstream>
#include <array>
#include <cstring>

namespace rmf {
namespace {
struct Definition {
    bool enabled = false;
    std::string id;
    std::string name;
    std::string unidentified;
    std::string base = "ring_adornment";
    bool health = false;
    bool mana = false;
    bool hunger = false;
};
std::array<Definition, kCustomItemSlots> definitions;
std::string internalName(int slot) {
    return definitions[slot].enabled ? definitions[slot].id : "rmf_unused_" + std::to_string(slot);
}
bool validId(const std::string& id) {
    if (id.empty() || id.size() >= 32) return false;
    for (unsigned char c : id) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return id.find("rmf_unused_") != 0;
}
bool booleanMember(const rapidjson::Value& effects, const char* key, bool& out) {
    if (!effects.HasMember(key)) { out = false; return true; }
    if (!effects[key].IsBool()) return false;
    out = effects[key].GetBool();
    return true;
}
void loadDefinitions(const rapidjson::Value& vanillaItems) {
    definitions = {};
    char** names = PHYSFS_enumerateFiles("data/rmf/items");
    if (!names) return;
    // Sort independently of filesystem enumeration order. A conflicting slot is rejected.
    std::vector<std::string> files;
    for (char** p = names; *p; ++p) files.emplace_back(*p);
    PHYSFS_freeList(names);
    std::sort(files.begin(), files.end());
    for (const auto& name : files) {
        if (name.size() < 6 || name.substr(name.size() - 5) != ".json") continue;
        const std::string path = "data/rmf/items/" + name;
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        if (!file) continue;
        const PHYSFS_sint64 size = PHYSFS_fileLength(file);
        if (size <= 0 || size > 65536) {
            PHYSFS_close(file);
            printlog("[RMF] Rejected %s: definition must be 1..65536 bytes", path.c_str());
            continue;
        }
        std::string json(static_cast<size_t>(size), '\0');
        const PHYSFS_sint64 read = PHYSFS_readBytes(file, &json[0], size);
        PHYSFS_close(file);
        rapidjson::Document d;
        d.Parse(json.data(), json.size());
        if (read != size || d.HasParseError() || !d.IsObject()
            || !d.HasMember("api_version") || !d["api_version"].IsInt() || d["api_version"].GetInt() != kApiVersion
            || !d.HasMember("slot") || !d["slot"].IsInt()
            || !d.HasMember("id") || !d["id"].IsString()
            || !d.HasMember("name") || !d["name"].IsString()
            || !d.HasMember("effects") || !d["effects"].IsObject()) {
            printlog("[RMF] Rejected %s: invalid schema or API version", path.c_str());
            continue;
        }
        const int slot = d["slot"].GetInt();
        Definition def;
        def.id = d["id"].GetString();
        def.name = d["name"].GetString();
        def.unidentified = def.name;
        if (d.HasMember("unidentified_name")) {
            if (!d["unidentified_name"].IsString()) continue;
            def.unidentified = d["unidentified_name"].GetString();
        }
        if (d.HasMember("base_item")) {
            if (!d["base_item"].IsString()) continue;
            def.base = d["base_item"].GetString();
        }
        const auto& effects = d["effects"];
        bool valid = slot >= 0 && slot < kCustomItemSlots && validId(def.id)
            && !def.name.empty() && def.name.size() < 128 && def.unidentified.size() < 128
            && booleanMember(effects, "infinite_health", def.health)
            && booleanMember(effects, "infinite_mana", def.mana)
            && booleanMember(effects, "no_hunger", def.hunger);
        for (auto it = effects.MemberBegin(); it != effects.MemberEnd(); ++it) {
            const std::string key = it->name.GetString();
            if (key != "infinite_health" && key != "infinite_mana" && key != "no_hunger") valid = false;
        }
        if (valid && (definitions[slot].enabled || vanillaItems.HasMember(def.id.c_str()))) valid = false;
        for (const auto& old : definitions) {
            if (old.enabled && old.id == def.id) valid = false;
        }
        if (valid) {
            if (!vanillaItems.HasMember(def.base.c_str())) valid = false;
            else {
                const auto& base = vanillaItems[def.base.c_str()];
                valid = base.HasMember("item_category") && base["item_category"].IsString()
                    && std::strcmp(base["item_category"].GetString(), "RING") == 0
                    && base.HasMember("equip_slot") && base["equip_slot"].IsString()
                    && std::strcmp(base["equip_slot"].GetString(), "ring") == 0;
            }
        }
        if (!valid) {
            printlog("[RMF] Rejected %s: invalid values, duplicate ID/slot, unknown effect, or non-ring base", path.c_str());
            continue;
        }
        def.enabled = true;
        definitions[slot] = def;
        printlog("[RMF] Registered %s in stable slot %d (item %d)", def.id.c_str(), slot, RMF_FIRST_ITEM + slot);
    }
}
}

void appendCustomItems(rapidjson::Document& d) {
    auto& objects = d["items"];
    if (!objects.IsObject() || objects.MemberCount() != RMF_FIRST_ITEM || !objects.HasMember("ring_adornment")) {
        definitions = {};
        printlog("[RMF] Unsupported base item table: expected %d vanilla items", RMF_FIRST_ITEM);
        return;
    }
    // Remove the old runtime names before reloading a different mod set.
    for (int slot = 0; slot < kCustomItemSlots; ++slot) {
        const char* old = itemNameStrings[RMF_FIRST_ITEM + slot + 2];
        ItemTooltips.itemNameStringToItemID.erase(old);
        ItemTooltips.itemNameLocalizations.erase(old);
    }
    loadDefinitions(objects);
    auto& alloc = d.GetAllocator();
    for (int slot = 0; slot < kCustomItemSlots; ++slot) {
        const Definition& def = definitions[slot];
        const std::string key = internalName(slot);
        rapidjson::Value value;
        value.CopyFrom(objects[def.enabled ? def.base.c_str() : "ring_adornment"], alloc);
        value["item_id"].SetInt(RMF_FIRST_ITEM + slot);
        value["item_level"].SetInt(-1); // Deliberate test content, excluded from random drops.
        value["gold_value"].SetInt(0);
        value["weight_value"].SetInt(0);
        value["stats"].SetObject();
        value["stats"].AddMember("AC", 0, alloc);
        if (def.health) value["stats"].AddMember("RMF_INFINITE_HEALTH", 1, alloc);
        if (def.mana) value["stats"].AddMember("RMF_INFINITE_MANA", 1, alloc);
        if (def.hunger) value["stats"].AddMember("RMF_NO_HUNGER", 1, alloc);
        objects.AddMember(rapidjson::Value(key.c_str(), alloc).Move(), value, alloc);
        stringCopy(itemNameStrings[RMF_FIRST_ITEM + slot + 2], key.c_str(), 32, key.size());
        ItemTooltips.itemNameStringToItemID[key] = RMF_FIRST_ITEM + slot;
    }
    printlog("[RMF] API %d initialized; %d reserved custom ring slots", kApiVersion, kCustomItemSlots);
}

void applyCustomItemNames() {
    for (int slot = 0; slot < kCustomItemSlots; ++slot) {
        const Definition& def = definitions[slot];
        auto& entry = ItemTooltips.itemNameLocalizations[internalName(slot)];
        entry.name_identified = def.enabled ? def.name : "[RMF unused ring slot]";
        entry.name_unidentified = def.enabled ? def.unidentified : "[RMF unused ring slot]";
    }
}

std::string itemDescription(int itemType) {
    const int slot = itemType - RMF_FIRST_ITEM;
    if (slot < 0 || slot >= kCustomItemSlots || !definitions[slot].enabled) return "";
    const auto& def = definitions[slot];
    std::string text = "While equipped and unbroken:";
    if (def.health) text += "\nUnlimited health";
    if (def.mana) text += "\nUnlimited magic";
    if (def.hunger) text += "\nNo hunger";
    return text;
}

bool hasEquipmentTrait(const Stat* stats, const char* trait) {
    if (!stats) return false;
    const Item* equipped[] = {stats->helmet, stats->breastplate, stats->gloves, stats->shoes,
        stats->shield, stats->weapon, stats->cloak, stats->amulet, stats->ring, stats->mask};
    for (const Item* item : equipped) {
        if (!item || item->status == BROKEN || item->type < 0 || item->type >= NUMITEMS) continue;
        const auto& attributes = items[item->type].attributes;
        const auto found = attributes.find(trait);
        if (found != attributes.end() && found->second > 0) return true;
    }
    return false;
}
bool hasInfiniteHealth(const Entity* entity) {
    return entity && entity->behavior == &actPlayer && hasEquipmentTrait(entity->getStats(), "RMF_INFINITE_HEALTH");
}
bool hasInfiniteMana(const Entity* entity) {
    return entity && entity->behavior == &actPlayer && hasEquipmentTrait(entity->getStats(), "RMF_INFINITE_MANA");
}
bool hasNoHunger(const Entity* entity) {
    return entity && entity->behavior == &actPlayer && hasEquipmentTrait(entity->getStats(), "RMF_NO_HUNGER");
}
void enforceResources(Entity* entity, Stat* stats) {
    if (!stats || !entity || entity->behavior != &actPlayer) return;
    if (hasInfiniteHealth(entity)) stats->HP = stats->MAXHP;
    if (hasInfiniteMana(entity)) stats->MP = stats->MAXMP;
    if (hasNoHunger(entity)) stats->HUNGER = 1500;
}
}

namespace rmf {
bool selfTestRequested = false;
namespace {
bool liveSolo() {
    return !intro && multiplayer == SINGLE && players[0] && players[0]->entity && stats[0];
}
int findItem(const std::string& id) {
    for (int slot = 0; slot < kCustomItemSlots; ++slot)
        if (definitions[slot].enabled && definitions[slot].id == id) return RMF_FIRST_ITEM + slot;
    return -1;
}
static ConsoleCommand giveCommand("/rmf_give", "Give a registered RMF item: /rmf_give game_dev_ring (single player)",
    [](int argc, const char** argv) {
        if (!liveSolo() || argc != 2) return;
        const int type = findItem(argv[1]);
        if (type < 0) { printlog("[RMF] Unknown item: %s", argv[1]); return; }
        newItem(static_cast<ItemType>(type), EXCELLENT, 0, 1, 0, true, &stats[0]->inventory);
        messagePlayer(0, MESSAGE_MISC, "[RMF] Added %s to inventory", argv[1]);
    });
static ConsoleCommand testCommand("/rmf_test", "Test Game Dev ring in a disposable single-player run",
    [](int, const char**) { if (liveSolo()) selfTestRequested = true; });
}
void tickSelfTest(Entity* entity, Stat* st) {
    if (!selfTestRequested || !liveSolo() || entity != players[0]->entity || entity->ticks < 60) return;
    selfTestRequested = false;
    int passed = 0, failed = 0;
    std::ofstream result(std::string(outputdir) + "/rmf-selftest.txt");
    auto check = [&](bool ok, const char* label) {
        if (ok) ++passed; else ++failed;
        printlog("[RMF-TEST] %s %s", ok ? "PASS" : "FAIL", label);
        result << (ok ? "PASS " : "FAIL ") << label << "\n";
    };
    const int type = findItem("game_dev_ring");
    check(type >= RMF_FIRST_ITEM && type < NUMITEMS, "custom item registered");
    if (type < RMF_FIRST_ITEM || type >= NUMITEMS) return;
    check(ItemTooltips.itemNameLocalizations["game_dev_ring"].name_identified == "Game Dev ring", "display name");
    check(items[type].category == RING && items[type].level == -1, "ring category and excluded from random drops");
    check(!godmode && !buddhamode, "global cheats disabled");
    if (st->ring) {
        st->ring->beatitude = 0; // Only the explicit disposable-profile self-test clears a starting curse.
        useItem(st->ring, 0);
    }
    Item* ring = newItem(static_cast<ItemType>(type), EXCELLENT, 0, 1, 0, true, &st->inventory);
    check(st->ring == nullptr && !hasInfiniteHealth(entity) && !hasInfiniteMana(entity) && !hasNoHunger(entity), "inventory alone grants no traits");
    useItem(ring, 0);
    check(st->ring == ring && hasInfiniteHealth(entity) && hasInfiniteMana(entity) && hasNoHunger(entity), "normal item use equips custom ring");
    enforceResources(entity, st);
    entity->modHP(-st->MAXHP - 100);
    check(st->HP == st->MAXHP, "lethal damage blocked");
    entity->setHP(0);
    check(st->HP == st->MAXHP, "zero-health setter blocked");
    entity->modMP(-st->MAXMP - 100);
    check(st->MP == st->MAXMP, "mana loss blocked");
    entity->setMP(0);
    check(st->MP == st->MAXMP, "zero-mana setter blocked");
    const bool cast = entity->safeConsumeMP(st->MAXMP + 100);
    check(cast && st->MP == st->MAXMP, "spell consumption above maximum mana succeeds");
    entity->drainMP(st->MAXMP + 100);
    check(st->MP == st->MAXMP && st->HP == st->MAXHP, "overdraw consumes neither health nor mana");
    const int originalMaxMana = st->MAXMP;
    const int originalCastingSkill = st->getProficiency(spell_fireball.skillID);
    st->setProficiency(spell_fireball.skillID, 100);
    st->MAXMP = st->MP = 1;
    Entity* projectile = castSpell(entity->getUID(), &spell_fireball, false, false);
    check(projectile != nullptr && st->MP == 1 && st->HP == st->MAXHP, "real fireball above maximum mana costs no resources");
    st->MAXMP = originalMaxMana;
    st->setProficiency(spell_fireball.skillID, originalCastingSkill);
    enforceResources(entity, st);
    st->HUNGER = 0;
    entity->handleEffects(st);
    check(st->HUNGER == 1500 && st->HP == st->MAXHP, "effects tick prevents starvation");
    ring->status = BROKEN;
    check(!hasInfiniteHealth(entity) && !hasInfiniteMana(entity) && !hasNoHunger(entity), "broken ring grants no traits");
    ring->status = EXCELLENT;
    useItem(ring, 0);
    check(st->ring == nullptr && !hasInfiniteHealth(entity) && !hasInfiniteMana(entity) && !hasNoHunger(entity), "normal item use removes all traits");
    entity->setHP(st->MAXHP);
    entity->modHP(-1);
    check(st->HP == st->MAXHP - 1, "ordinary damage resumes after removal");
    entity->setMP(st->MAXMP);
    const bool ordinaryCast = entity->safeConsumeMP(1);
    check(ordinaryCast && st->MP == st->MAXMP - 1, "ordinary mana consumption resumes");
    st->HUNGER = 1000;
    const auto savedEntityTicks = entity->ticks;
    for (int i = 0; i < 240 && st->HUNGER == 1000; ++i) {
        ++entity->ticks;
        entity->handleEffects(st);
    }
    entity->ticks = savedEntityTicks;
    check(st->HUNGER < 1000, "ordinary hunger resumes after removal");
    Item* vanilla = newItem(RING_ADORNMENT, EXCELLENT, 0, 1, 0, true, &st->inventory);
    useItem(vanilla, 0);
    check(st->ring == vanilla && !hasInfiniteHealth(entity) && !hasInfiniteMana(entity) && !hasNoHunger(entity), "vanilla ring remains ordinary");
    useItem(vanilla, 0);
    useItem(ring, 0);
    enforceResources(entity, st);
    check(st->ring == ring && st->HP == st->MAXHP && st->MP == st->MAXMP && st->HUNGER == 1500, "test ends with Game Dev ring equipped");
    printlog("[RMF-TEST] COMPLETE pass=%d fail=%d", passed, failed);
    result << "COMPLETE pass=" << passed << " fail=" << failed << "\n";
    messagePlayer(0, MESSAGE_MISC, "[RMF] Ring tests: %d passed, %d failed", passed, failed);
}
}
