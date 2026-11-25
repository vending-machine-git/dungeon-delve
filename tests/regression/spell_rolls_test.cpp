#include "../../src/delve_spell_rules.hpp"
#include <cassert>
#include <iostream>
#include <set>
int main() {
    using namespace DelveSpellRules;
    unsigned int cases = 0;
    for (int type : {1, 37, 300, 750}) for (int seed = 0; seed < 256; ++seed)
    for (int tier = 0; tier <= 4; ++tier) for (bool blessed : {false, true}) {
        Imprint imprint{type, static_cast<unsigned char>(tier), static_cast<unsigned char>(seed), blessed};
        const auto rolls = generate(imprint), repeat = generate(imprint);
        std::set<Kind> kinds;
        for (int index = 0; index < 3; ++index) {
            assert(rolls[index].kind == repeat[index].kind && rolls[index].value == repeat[index].value);
            if (index >= DelveAffixes::count(tier)) { assert(rolls[index].kind == Kind::Count); continue; }
            assert(kinds.insert(rolls[index].kind).second);
            const int key = static_cast<int>(rolls[index].kind), factor = blessed ? 2 : 1;
            assert(rolls[index].value >= minimum[key] * factor && rolls[index].value <= maximum[key] * factor);
            assert(value(imprint, rolls[index].kind) == std::min(rolls[index].value, caps[key]));
        }
        ++cases;
    }
    assert(generate(Imprint{})[0].kind == Kind::Count);
    assert(generate(Imprint{37, 5, 0, false})[0].kind == Kind::Count);
    Imprint learned{37, 4, 17, true};
    const auto inFlight = learned;
    learned = {37, 2, 9, false};
    assert(generate(learned)[1].kind == Kind::Count);
    assert(generate(inFlight)[2].kind != Kind::Count);
    learned = {37, 0, 0, false};
    assert(value(learned, Kind::Power) == 0);
    std::cout << "PASS " << cases << " spellbook combinations, distinct rolls, blessing, caps, replacement and immutable cast snapshots\n";
}
