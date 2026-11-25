#include "../../src/delve_net_position.hpp"
#include <cassert>
#include <cstdint>
#include <cmath>

namespace { bool delve = false; }
namespace Raid { bool active() { return delve; } }

double roundtrip(double position) {
    const auto wire = static_cast<std::int16_t>(position * Raid::networkPositionScale());
    return wire / Raid::networkPositionScale();
}

int main() {
    assert(Raid::networkPositionScale() == 32.0);
    for (int step = -256; step <= 32767; ++step)
        assert(roundtrip(step / 32.0) == step / 32.0);
    delve = true;
    assert(Raid::networkPositionScale() == 16.0);
    for (int step = -256; step < 90 * 16 * 16; ++step)
        assert(roundtrip(step / 16.0) == step / 16.0);
    assert(roundtrip(1032.0) == 1032.0); // Merchant that wrapped to -1016.
    assert(roundtrip(1432.0) == 1432.0); // Far edge of a 90-tile floor.
    assert(roundtrip(-0.5) == -0.5); // Preserve effects outside the left/top edge.
    assert(std::abs(roundtrip(1100.03) - 1100.03) < 1.0 / 16.0);
}
