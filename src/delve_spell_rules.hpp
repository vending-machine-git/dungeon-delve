#pragma once
#include "delve_affix_rules.hpp"

namespace DelveSpellRules {
enum class Kind { Power, Speed, Efficiency, Count };
struct Imprint {
    int bookType = -1;
    unsigned char tier = 0, seed = 0;
    bool blessed = false;
};
struct Roll { Kind kind = Kind::Count; int value = 0; };
using Rolls = std::array<Roll, 3>;
constexpr int minimum[] = {7, 4, 5};
constexpr int maximum[] = {16, 10, 13};
constexpr int caps[] = {30, 25, 25};
inline Rolls generate(const Imprint& book) {
    Rolls result{};
    if (book.bookType < 0 || book.tier > 4) return result;
    std::array<Kind, 3> candidates{{Kind::Power, Kind::Speed, Kind::Efficiency}};
    int available = 3;
    auto state = static_cast<std::uint32_t>(book.seed) ^ (static_cast<std::uint32_t>(book.bookType) * 0x85ebca6bu);
    for (int index = 0; index < DelveAffixes::count(book.tier); ++index) {
        const int selected = DelveAffixes::next(state) % available;
        const auto kind = candidates[selected]; candidates[selected] = candidates[--available];
        const int key = static_cast<int>(kind);
        result[index] = {kind, (minimum[key] + static_cast<int>(DelveAffixes::next(state) % (maximum[key] - minimum[key] + 1))) * (book.blessed ? 2 : 1)};
    }
    return result;
}
inline int value(const Imprint& book, Kind kind) {
    for (const auto& roll : generate(book)) if (roll.kind == kind) return std::min(roll.value, caps[static_cast<int>(kind)]);
    return 0;
}
}
