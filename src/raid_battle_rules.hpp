#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace Raid::BattleRules {
constexpr int floorSeconds = 360;
constexpr int descentCount = 1;
constexpr int victoryPortalCount = 3;
constexpr int stormMarker = 0x42524f59;
constexpr int maximumStormBoulders = 192;
constexpr int dungeonTop = 9;
constexpr int ratRespawnSeconds = 45;
constexpr int algernonCheese = 10;

inline int unlockSecond(int floor, int ordinal, int limit) {
    return ordinal == 1 ? (floor == 3 ? limit - 60 : 0) : ordinal == 2 ? limit - 60 : limit;
}
inline int stormStartSecond(int floor, int limit) {
    return floor == 3 ? std::max(0, limit - 60) : limit;
}
inline int experienceGain(int amount) {
    return amount <= 0 ? amount : static_cast<int>(std::min<std::int64_t>(
        std::numeric_limits<int>::max(), (static_cast<std::int64_t>(amount) * 5 + 1) / 2));
}
inline int waveInterval(int overtime) { return std::max(1, 8 - overtime / 10); }
inline int waveSize(int overtime) { return std::min(12, 1 + overtime / 15); }
}
