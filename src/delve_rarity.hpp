#pragma once

#include "main.hpp"
#include "items.hpp"
#include "stat.hpp"
#include "raid.hpp"
#include "delve_affix_rules.hpp"
#include "delve_spell_rolls.hpp"
#include <algorithm>
#include <string>

int checkEquipType(const Item* item);

namespace DelveRarity {
enum class Tier : unsigned char { Grey, White, Blue, Purple, Legendary };
constexpr Uint32 signature = 0xebb00000u;
constexpr Uint32 signatureMask = 0xfff00000u;
constexpr Uint32 variantMask = 0x1ffu;
constexpr unsigned int tierShift = 17;
constexpr unsigned int seedShift = 9;
constexpr Uint32 seedMask = 0xffu;
inline bool tagged(const Item& item) {
    return (item.appearance & signatureMask) == signature || (item.appearance & 0xffff0000u) == 0xead50000u;
}
inline Uint32 seed(const Item& item) {
    return (item.appearance & signatureMask) == signature ? (item.appearance >> seedShift) & seedMask : item.appearance & seedMask;
}
constexpr int weights[3][5] = {
    {6500, 2400, 900, 190, 10},
    {4800, 2900, 1800, 470, 30},
    {3200, 3000, 2700, 1000, 100}
};
inline bool eligible(const Item& item) {
    if (item.type < 0 || item.type >= RMF_FIRST_ITEM || item.type == MAGICSTAFF_SCEPTER || item.type == POTION_EMPTY) return false;
    if (item.type == TOOL_BEARTRAP) return true;
    switch (itemCategory(&item)) {
        case WEAPON: case ARMOR: case RING: case AMULET: case MAGICSTAFF:
        case SPELLBOOK: case THROWN: case GEM: case POTION:
            return items[item.type].variations > 0 && items[item.type].variations <= 256;
        default: return false;
    }
}
inline Tier stored(const Item& item) {
    if (!eligible(item)) return Tier::Grey;
    const auto encodedTier = (item.appearance & signatureMask) == signature ? (item.appearance >> tierShift) & 7u
        : (item.appearance & 0xffff0000u) == 0xead50000u ? (item.appearance >> 12) & 0xfu : 0u;
    if (itemCategory(&item) == GEM) {
        if (item.status == EXCELLENT && encodedTier == 4) return Tier::Legendary;
        return static_cast<Tier>(std::clamp(static_cast<int>(item.status) - 1, 0, 3));
    }
    return encodedTier >= 1 && encodedTier <= 4 ? static_cast<Tier>(encodedTier) : Tier::Grey;
}
inline Tier tier(const Item& item) { return Raid::active() ? stored(item) : Tier::Grey; }
inline Uint32 encoded(const Item& item, Tier rarity, Uint32 entropy) {
    const Uint32 variations = items[item.type].variations;
    const Uint32 base = signature | (static_cast<Uint32>(rarity) << tierShift) | ((entropy & seedMask) << seedShift);
    const Uint32 variation = item.appearance % variations;
    const Uint32 offset = (variation + variations - base % variations) % variations;
    return base + offset;
}
inline bool equipment(const Item& item);
inline void assign(Item& item, Tier rarity, Uint32 entropy = 0) {
    if (!eligible(item)) return;
    if (itemCategory(&item) == GEM || (Raid::active() && equipment(item)))
        item.status = static_cast<Status>(std::min(static_cast<int>(rarity) + 1, static_cast<int>(EXCELLENT)));
    if (rarity == Tier::Grey) {
        if (tagged(item)) item.appearance %= items[item.type].variations;
    } else item.appearance = encoded(item, rarity, entropy);
}
inline Tier roll(int floor, unsigned int rollValue) {
    int value = rollValue % 10000;
    for (int index = 0; index < 5; ++index) {
        value -= weights[std::clamp(floor, 1, 3) - 1][index];
        if (value < 0) return static_cast<Tier>(index);
    }
    return Tier::Grey;
}
inline void rollLoot(Item* item, int floor, unsigned int rollValue, unsigned int entropy) {
    if (item && Raid::active()) assign(*item, roll(floor, rollValue), entropy);
}
inline const char* name(Tier rarity) {
    static const char* names[] = {"Grey", "White", "Uncommon", "Rare", "Legendary"};
    return names[static_cast<int>(rarity)];
}
inline Uint32 color(Tier rarity, unsigned char alpha = 255) {
    static const unsigned char colors[5][3] = {{155,155,155},{245,245,245},{65,155,255},{185,85,255},{255,180,35}};
    const auto& rgb = colors[static_cast<int>(rarity)];
    return makeColor(rgb[0], rgb[1], rgb[2], alpha);
}
inline bool magic(const Item& item) { return itemCategory(&item) == MAGICSTAFF || itemCategory(&item) == SPELLBOOK; }
inline bool equipment(const Item& item) {
    if (!eligible(item)) return false;
    switch (itemCategory(&item)) {
        case WEAPON: case ARMOR: case RING: case AMULET: case MAGICSTAFF: case SPELLBOOK: case THROWN: return true;
        default: return false;
    }
}
inline DelveAffixes::Pool pool(const Item& item) {
    using DelveAffixes::Pool;
    if (magic(item) || item.type == WIZARD_DOUBLET || item.type == HEALER_DOUBLET
        || item.type == ROBE_CULTIST || item.type == ROBE_HEALER || item.type == ROBE_MONK || item.type == ROBE_WIZARD) return Pool::Focus;
    if (itemCategory(&item) == RING || itemCategory(&item) == AMULET) return Pool::Jewelry;
    switch (checkEquipType(&item)) {
        case TYPE_BREASTPIECE: return Pool::Chest;
        case TYPE_SHIELD: return Pool::Shield;
        case TYPE_HELM: case TYPE_HAT: case TYPE_MASK: return Pool::Head;
        case TYPE_GLOVES: return Pool::Gloves;
        case TYPE_BOOTS: return Pool::Boots;
        case TYPE_CLOAK: return Pool::Cloak;
        default: return Pool::Weapon;
    }
}
inline DelveAffixes::Rolls affixes(const Item& item) {
    if (!equipment(item) || itemCategory(&item) == SPELLBOOK) return {};
    return DelveAffixes::withBlessing(DelveAffixes::generate(pool(item), static_cast<int>(tier(item)), seed(item), item.type), item.beatitude);
}
inline int total(const Stat& actorStats, DelveAffixes::Kind kind) {
    if (!Raid::active()) return 0;
    bool playerStats = false;
    for (int player = 0; player < MAXPLAYERS; ++player) {
        if (stats[player] == &actorStats && !Raid::revenant(player)) { playerStats = true; break; }
    }
    if (!playerStats) return 0;
    int result = 0;
    const std::array<const Item*, 10> equipmentSlots{{actorStats.weapon, actorStats.shield, actorStats.helmet,
        actorStats.breastplate, actorStats.gloves, actorStats.shoes, actorStats.cloak, actorStats.ring, actorStats.mask, actorStats.amulet}};
    for (size_t index = 0; index < equipmentSlots.size(); ++index) {
        const auto item = equipmentSlots[index];
        if (!item || item->status == BROKEN || std::find(equipmentSlots.begin(), equipmentSlots.begin() + index, item) != equipmentSlots.begin() + index) continue;
        for (const auto& roll : affixes(*item)) if (roll.kind == kind) result += roll.value;
    }
    return DelveAffixes::capped(kind, result);
}
inline int penetratedArmor(const Stat* actorStats, int armor) {
    return actorStats ? DelveAffixes::penetrate(armor, total(*actorStats, DelveAffixes::Kind::ArmorPenetration)) : armor;
}
inline int effectDuration(const Stat& actorStats, int effect, int duration) {
    if (!Raid::active() || duration <= 0) return duration;
    switch (effect) {
        case EFF_POISONED: case EFF_BLEEDING: case EFF_SLOW: case EFF_WEBBED:
        case EFF_CONFUSED: case EFF_FEAR: case EFF_ASLEEP: case EFF_PARALYZED:
        case EFF_STUNNED: case EFF_BLIND: case EFF_ROOTED: case EFF_DISORIENTED:
            return std::max(1, (duration * (100 - total(actorStats, DelveAffixes::Kind::DebuffResistance)) + 99) / 100);
        case EFF_FAST: case EFF_INVISIBLE: case EFF_LEVITATING: case EFF_MAGICRESIST:
        case EFF_MAGICREFLECT: case EFF_HP_REGEN: case EFF_MP_REGEN: case EFF_POTION_STR:
        case EFF_TROLLS_BLOOD: case EFF_CON_BONUS: case EFF_PWR: case EFF_AGILITY: case EFF_RALLY:
        case EFF_NIMBLENESS: case EFF_GREATER_MIGHT: case EFF_COUNSEL: case EFF_STURDINESS:
            return duration * (100 + total(actorStats, DelveAffixes::Kind::BuffDuration) + DelveSpellRolls::activePower()) / 100;
        default: return duration;
    }
}
inline int castTicks(const Stat& actorStats, int duration) {
    return duration * 100 / (100 + total(actorStats, DelveAffixes::Kind::CastingSpeed));
}
inline int bonus(const Item& item) {
    if (equipment(item)) return 0;
    static const int bonuses[] = {0, 1, 3, 6, 12};
    return bonuses[static_cast<int>(tier(item))];
}
inline int armorBonus(const Item& item) {
    return 0;
}
inline int attackBonus(const Item& item) {
    return itemCategory(&item) == WEAPON || itemCategory(&item) == THROWN || itemCategory(&item) == GEM ? bonus(item) : 0;
}
inline int potencyPercent(const Item& item) {
    static const int percentages[] = {100, 110, 135, 175, 250};
    return itemCategory(&item) == POTION || item.type == TOOL_BEARTRAP ? percentages[static_cast<int>(tier(item))] : 100;
}
inline int scalePotency(const Item& item, int amount) {
    return amount > 0 ? std::max(amount, amount * potencyPercent(item) / 100) : amount;
}
inline const Item* focus(const Stat& stats) {
    const Item* result = nullptr;
    for (auto item : {stats.weapon, stats.shield})
        if (item && item->status != BROKEN && magic(*item) && (!result || tier(*item) > tier(*result))) result = item;
    return result;
}
inline bool ward(const Stat& stats) {
    for (auto item : {stats.helmet,stats.breastplate,stats.gloves,stats.shoes,stats.shield,stats.cloak,stats.ring,stats.mask,stats.amulet})
        if (item && item->status != BROKEN && equipment(*item) && !magic(*item) && tier(*item) == Tier::Legendary) return true;
    return false;
}
inline std::string summary(const Item& item) {
    const auto rarity = tier(item);
    if (!Raid::active() || !eligible(item)) return {};
    if (itemCategory(&item) == SPELLBOOK) return DelveSpellRolls::summary(item);
    std::string result;
    if (equipment(item)) {
        for (const auto& roll : affixes(item)) {
            if (roll.kind == DelveAffixes::Kind::Count) continue;
            const auto& definition = DelveAffixes::definitions[static_cast<int>(roll.kind)];
            if (!result.empty()) result += '\n';
            result += "+" + std::to_string(roll.value) + (definition.percent ? "% " : " ") + definition.name;
        }
    }
    else if (rarity == Tier::Grey) return {};
    else if (itemCategory(&item) == POTION) result = "+" + std::to_string(potencyPercent(item) - 100) + "% potion potency";
    else if (item.type == TOOL_BEARTRAP) result = "+" + std::to_string(potencyPercent(item) - 100) + "% trap damage / bleed duration";
    else if (attackBonus(item)) result = "+" + std::to_string(attackBonus(item)) + " throwing damage";
    if (rarity == Tier::Legendary) {
        if (itemCategory(&item) == ARMOR || itemCategory(&item) == RING || itemCategory(&item) == AMULET)
            result += (result.empty() ? "" : "\n") + std::string("Aegis: -15% hostile damage (non-stacking)");
        else if (magic(item) || itemCategory(&item) == WEAPON || itemCategory(&item) == THROWN || attackBonus(item))
            result += (result.empty() ? "" : "\n") + std::string("Siphon: +4 HP / +2 MP on hit (1s cooldown)");
    }
    return result;
}
}
