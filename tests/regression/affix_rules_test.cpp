#include "../../src/delve_affix_rules.hpp"
#include <cassert>
#include <iostream>
#include <set>

int main() {
    using namespace DelveAffixes;
    constexpr std::array<std::array<int, 3>, static_cast<int>(Kind::Count)> originalRanges{{
        {2, 6, 20}, {3, 8, 25}, {1, 3, 10}, {4, 12, 35}, {6, 15, 25},
        {8, 18, 25}, {8, 18, 25}, {1, 3, 6}, {1, 3, 5}, {4, 12, 40},
        {4, 14, 40}, {3, 8, 25}, {3, 8, 25}, {5, 12, 35}, {4, 10, 25},
        {5, 12, 30}, {5, 12, 30}, {4, 10, 25}, {1, 2, 3}, {1, 2, 4},
        {1, 2, 6}, {1, 2, 6}, {1, 2, 6}, {1, 2, 6}, {1, 2, 6}, {1, 2, 6},
        {4, 12, 25}, {4, 12, 25}
    }};
    for (int index = 0; index < static_cast<int>(Kind::Count); ++index) {
        const bool attribute = index >= static_cast<int>(Kind::Strength) && index <= static_cast<int>(Kind::Charisma);
        if (index == static_cast<int>(Kind::Intelligence)) {
            assert(definitions[index].minimum == 3 && definitions[index].maximum == 4 && definitions[index].cap == 8); continue;
        }
        if (index == static_cast<int>(Kind::Perception)) {
            assert(definitions[index].minimum == 3 && definitions[index].maximum == 5 && definitions[index].cap == 10); continue;
        }
        if (index == static_cast<int>(Kind::Charisma)) {
            assert(definitions[index].minimum == 4 && definitions[index].maximum == 6 && definitions[index].cap == 12); continue;
        }
        const int multiplier = attribute ? 150 : 130;
        assert(definitions[index].minimum == (originalRanges[index][0] * multiplier + 50) / 100);
        assert(definitions[index].maximum == (originalRanges[index][1] * multiplier + 50) / 100);
        assert(definitions[index].cap == originalRanges[index][2]);
    }
    std::set<Kind> observed;
    unsigned int combinations = 0;
    for (int pool = 0; pool <= static_cast<int>(Pool::Jewelry); ++pool) {
        for (unsigned int seed = 0; seed < 1024; ++seed) {
            Rolls previous{};
            for (int rarity = 0; rarity < 5; ++rarity) {
                auto rolls = generate(static_cast<Pool>(pool), rarity, seed, 37);
                const auto repeat = generate(static_cast<Pool>(pool), rarity, seed, 37);
                for (int blessing : {-3, 0, 1, 5}) {
                    const auto modified = withBlessing(rolls, blessing);
                    for (int index = 0; index < 3; ++index) {
                        assert(modified[index].kind == rolls[index].kind);
                        assert(modified[index].value == rolls[index].value * (blessing > 0 ? 2 : 1));
                    }
                }
                std::set<Kind> unique;
                for (int index = 0; index < 3; ++index) {
                    assert(rolls[index].kind == repeat[index].kind && rolls[index].value == repeat[index].value);
                    if (index >= count(rarity)) {
                        assert(rolls[index].kind == Kind::Count && rolls[index].value == 0);
                        continue;
                    }
                    const auto& definition = definitions[static_cast<int>(rolls[index].kind)];
                    assert(allowed(static_cast<Pool>(pool), rolls[index].kind));
                    assert(unique.insert(rolls[index].kind).second);
                    assert(rolls[index].value >= definition.minimum && rolls[index].value <= definition.maximum);
                    observed.insert(rolls[index].kind);
                    if (index < count(rarity - 1)) {
                        assert(rolls[index].kind == previous[index].kind && rolls[index].value == previous[index].value);
                    }
                }
                previous = rolls;
                ++combinations;
            }
        }
    }
    assert(observed.size() == static_cast<int>(Kind::Count));
    Rolls healthRoll{};
    healthRoll[0] = {Kind::MaxHealth, 14};
    assert(withBlessing(healthRoll, 1)[0].value == 28);
    for (int index = 0; index < static_cast<int>(Kind::Count); ++index) {
        auto kind = static_cast<Kind>(index);
        assert(capped(kind, -1) == 0);
        assert(capped(kind, 10000) == definitions[index].cap);
    }
    assert(reducedCost(0, 25) == 0);
    assert(reducedCost(1, 25) == 1);
    assert(reducedCost(20, 10) == 18);
    assert(reducedCost(20, 10000) == 15);
    assert(penetrate(-2, 35) == -2);
    assert(penetrate(100, 10000) == 65);
    assert(penetrate(10, 10) == 9);
    std::cout << "PASS " << combinations << " deterministic affix combinations, 28 effects, pools, ranges, uniqueness, tier prefixes, blessing multipliers and caps\n";
}
