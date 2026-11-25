#pragma once
#include "main.hpp"
#include <array>

namespace DelveLobby {
// Preview-only scene: never moves gameplay entities or sends pose packets.
void draw(SDL_Rect viewport, const std::array<bool, 4>& occupied);
void tickMusic(bool visible, real_t volume);
void stopMusic();
#ifdef BARONY_SMOKE_TESTS
bool musicPlaying();
Uint64 musicFrames();
#endif
}
