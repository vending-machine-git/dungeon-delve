#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace DelveAffixes {
enum class Kind {
    AttackSpeed, CastingSpeed, MoveSpeed, ArmorPenetration, Backstab,
    UndeadSlayer, DemonSlayer, LifeOnKill, ManaOnKill, MaxHealth, MaxMana,
    ProjectileResistance, MagicResistance, DebuffResistance, BlockPower,
    HealingPower, BuffDuration, ManaEfficiency, HealthRegen, ManaRegen,
    Strength, Dexterity, Constitution, Intelligence, Perception, Charisma,
    Stealth, Lockpicking, Count
};
enum class Pool { Weapon, Focus, Chest, Shield, Head, Gloves, Boots, Cloak, Jewelry };
struct Definition {
    const char* name;
    int minimum, maximum, cap;
    bool percent;
};
constexpr std::array<Definition, static_cast<int>(Kind::Count)> definitions{{
    {"attack charge / ranged speed", 3, 8, 20, true},
    {"casting speed", 4, 10, 25, true},
    {"movement speed", 1, 4, 10, true},
    {"armor penetration", 5, 16, 35, true},
    {"rear-hit damage", 8, 20, 25, true},
    {"damage against undead", 10, 23, 25, true},
    {"damage against demons", 10, 23, 25, true},
    {"HP on hostile kill", 1, 4, 6, false},
    {"MP on hostile kill", 1, 4, 5, false},
    {"maximum HP", 5, 16, 40, false},
    {"maximum MP", 5, 18, 40, false},
    {"projectile damage reduction", 4, 10, 25, true},
    {"magic damage reduction", 4, 10, 25, true},
    {"debuff resistance", 7, 16, 35, true},
    {"block damage reduction", 5, 13, 25, true},
    {"healing power", 7, 16, 30, true},
    {"beneficial status duration", 7, 16, 30, true},
    {"spell mana efficiency", 5, 13, 25, true},
    {"HP / 5s", 1, 3, 3, false},
    {"MP / 5s", 1, 3, 4, false},
    {"Strength", 2, 3, 6, false},
    {"Dexterity", 2, 3, 6, false},
    {"Constitution", 2, 3, 6, false},
    {"Intelligence", 3, 4, 8, false},
    {"Perception", 3, 5, 10, false},
    {"Charisma", 4, 6, 12, false},
    {"stealth proficiency", 5, 16, 25, false},
    {"lockpicking / disarming", 5, 16, 25, false}
}};
struct Roll {
    Kind kind = Kind::Count;
    int value = 0;
};
using Rolls = std::array<Roll, 3>;
constexpr int count(int rarity) { return std::clamp(rarity - 1, 0, 3); }
inline bool allowed(Pool pool, Kind kind) {
    switch (pool) {
        case Pool::Weapon:
            return kind == Kind::AttackSpeed || kind == Kind::ArmorPenetration || kind == Kind::Backstab
                || kind == Kind::UndeadSlayer || kind == Kind::DemonSlayer || kind == Kind::LifeOnKill
                || kind == Kind::ManaOnKill || kind == Kind::Strength || kind == Kind::Dexterity || kind == Kind::Perception;
        case Pool::Focus:
            return kind == Kind::CastingSpeed || kind == Kind::ManaEfficiency || kind == Kind::HealingPower
                || kind == Kind::BuffDuration || kind == Kind::MaxMana || kind == Kind::ManaRegen
                || kind == Kind::Intelligence || kind == Kind::Charisma || kind == Kind::UndeadSlayer
                || kind == Kind::DemonSlayer || kind == Kind::LifeOnKill || kind == Kind::ManaOnKill;
        case Pool::Chest:
            return kind == Kind::MaxHealth || kind == Kind::MaxMana || kind == Kind::ProjectileResistance
                || kind == Kind::MagicResistance || kind == Kind::DebuffResistance || kind == Kind::HealthRegen
                || kind == Kind::Constitution || kind == Kind::Strength;
        case Pool::Shield:
            return kind == Kind::BlockPower || kind == Kind::MaxHealth || kind == Kind::ProjectileResistance
                || kind == Kind::MagicResistance || kind == Kind::DebuffResistance || kind == Kind::Constitution;
        case Pool::Head:
            return kind == Kind::CastingSpeed || kind == Kind::HealingPower || kind == Kind::MaxMana
                || kind == Kind::MagicResistance || kind == Kind::DebuffResistance || kind == Kind::BuffDuration
                || kind == Kind::Intelligence || kind == Kind::Perception || kind == Kind::Charisma;
        case Pool::Gloves:
            return kind == Kind::AttackSpeed || kind == Kind::CastingSpeed || kind == Kind::Lockpicking
                || kind == Kind::Dexterity || kind == Kind::Strength || kind == Kind::HealingPower;
        case Pool::Boots:
            return kind == Kind::MoveSpeed || kind == Kind::Stealth || kind == Kind::DebuffResistance
                || kind == Kind::Dexterity || kind == Kind::Lockpicking || kind == Kind::MaxHealth;
        case Pool::Cloak:
            return kind == Kind::MoveSpeed || kind == Kind::Stealth || kind == Kind::ProjectileResistance
                || kind == Kind::MagicResistance || kind == Kind::HealthRegen || kind == Kind::ManaRegen
                || kind == Kind::Backstab || kind == Kind::Perception;
        case Pool::Jewelry: return true;
    }
    return false;
}
inline std::uint32_t next(std::uint32_t& state) {
    state += 0x9e3779b9u;
    std::uint32_t mixed = state;
    mixed = (mixed ^ (mixed >> 16)) * 0x85ebca6bu;
    mixed = (mixed ^ (mixed >> 13)) * 0xc2b2ae35u;
    return mixed ^ (mixed >> 16);
}
inline Rolls generate(Pool pool, int rarity, std::uint32_t seed, std::uint32_t itemType) {
    Rolls result{};
    std::array<Kind, static_cast<int>(Kind::Count)> candidates{};
    int available = 0;
    for (int index = 0; index < static_cast<int>(Kind::Count); ++index) {
        const auto kind = static_cast<Kind>(index);
        if (allowed(pool, kind)) candidates[available++] = kind;
    }
    std::uint32_t state = seed ^ (itemType * 0x85ebca6bu);
    for (int index = 0; index < count(rarity); ++index) {
        const int selected = next(state) % available;
        const auto kind = candidates[selected];
        candidates[selected] = candidates[--available];
        const auto& definition = definitions[static_cast<int>(kind)];
        result[index] = {kind, definition.minimum + static_cast<int>(next(state) % (definition.maximum - definition.minimum + 1))};
    }
    return result;
}
inline Rolls withBlessing(Rolls rolls, int beatitude) {
    if (beatitude > 0) {
        for (auto& roll : rolls) if (roll.kind != Kind::Count) roll.value *= 2;
    }
    return rolls;
}
inline int capped(Kind kind, int value) {
    return std::clamp(value, 0, definitions[static_cast<int>(kind)].cap);
}
inline int reducedCost(int cost, int efficiency) {
    if (efficiency <= 0) return cost;
    return cost > 0 ? std::max(1, (cost * (100 - capped(Kind::ManaEfficiency, efficiency)) + 99) / 100) : cost;
}
inline int penetrate(int armor, int percent) {
    return armor > 0 ? armor - armor * capped(Kind::ArmorPenetration, percent) / 100 : armor;
}
}
