#pragma once

namespace Raid {
bool active();

// Signed 16-bit XY positions at native 1/32 precision wrap beyond tile 63.
// Delve's 90x90 floors use 1/16 precision, covering +/-128 tiles, including
// negative positions for effects. Z, velocity and normal campaigns stay native.
inline double networkPositionScale() { return active() ? 16.0 : 32.0; }
}
