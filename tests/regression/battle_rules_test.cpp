#include "../../src/raid_battle_rules.hpp"
#include <cassert>

int main() {
    using namespace Raid::BattleRules;
    static_assert(floorSeconds == 360 && descentCount == 1 && victoryPortalCount == 3);
    static_assert(ratRespawnSeconds == 45 && algernonCheese == 10);
    for (int limit = 300; limit <= 480; ++limit) {
        for (int floor = 1; floor <= 2; ++floor) {
            assert(stormStartSecond(floor, limit) == limit);
            assert(unlockSecond(floor, 1, limit) == 0);
            assert(unlockSecond(floor, 2, limit) == limit - 60);
            assert(unlockSecond(floor, 3, limit) == limit);
            assert(unlockSecond(floor, 4, limit) == limit);
        }
        assert(unlockSecond(3, 1, limit) == limit - 60);
        assert(stormStartSecond(3, limit) == limit - 60);
        assert(unlockSecond(3, 2, limit) == limit - 60);
        assert(unlockSecond(3, 3, limit) == limit);
        assert(unlockSecond(3, 4, limit) == limit);
    }
    assert(experienceGain(0) == 0 && experienceGain(-2) == -2);
    assert(stormStartSecond(3, 360) == 300);
    assert(stormStartSecond(3, 30) == 0);
    assert(experienceGain(1) == 2 && experienceGain(2) == 4);
    assert(experienceGain(3) == 5 && experienceGain(4) == 7);
    assert(experienceGain(17) == 30 && experienceGain(30) == 53);
    assert(experienceGain(std::numeric_limits<int>::max()) == std::numeric_limits<int>::max());
    for (int overtime = 1; overtime <= 3600; ++overtime) {
        assert(waveInterval(overtime) <= waveInterval(overtime - 1));
        assert(waveInterval(overtime) >= 1);
        assert(waveSize(overtime) >= waveSize(overtime - 1));
        assert(waveSize(overtime) <= 12);
    }
    assert(waveInterval(0) == 8 && waveSize(0) == 1);
    assert(waveInterval(180) == 1 && waveSize(180) == 12);
}
