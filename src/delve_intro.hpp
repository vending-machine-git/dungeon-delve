#pragma once

#include <string>

namespace DelveIntro {
struct Cue {
    double start = 0, end = 0;
    std::string text;
};
bool prepare(int floor);
void start();
void stop();
void setVolume(double volume);
void showSubtitle();
bool active();
bool finished();
int cueIndex();
const Cue* cue();
double position();
}
