#pragma once
#include <cstdint>

// Local process state only. Steam results never become standalone licenses or
// travel in lobby packets. A failed query retains this session's last verified
// result; a successful query replaces it, including authoritative negatives.
namespace DLC {
class Entitlements {
    std::uint8_t standalone_ = 0;
    std::uint8_t steam_ = 0;
public:
    void standalone(unsigned pack) {
        if (pack < 3) standalone_ |= static_cast<std::uint8_t>(1u << pack);
    }
    void steam(bool available, std::uint8_t installed) {
        if (available) steam_ = installed & 7;
    }
    std::uint8_t effective() const { return standalone_ | steam_; }
};
}
