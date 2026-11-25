#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace DelveEffects {
enum class Motif { Fury, Shout, Slash, Mend, Ward, Veil, Dash, Sand, Bolt, Snare, Nova, Focus, Song, Blast, Drain, Survey, Quake, Cleanse, Frost, Volley, Count };
struct Profile {
    Motif motif;
    int primary, secondary, sound, variation;
};
struct Point { double forward = 0, side = 0, height = 0; };
constexpr std::array<std::array<Motif, 4>, 26> motifs{{
    {Motif::Fury, Motif::Shout, Motif::Slash, Motif::Mend},
    {Motif::Ward, Motif::Slash, Motif::Mend, Motif::Shout},
    {Motif::Mend, Motif::Cleanse, Motif::Ward, Motif::Bolt},
    {Motif::Veil, Motif::Slash, Motif::Dash, Motif::Sand},
    {Motif::Dash, Motif::Mend, Motif::Slash, Motif::Survey},
    {Motif::Mend, Motif::Ward, Motif::Bolt, Motif::Dash},
    {Motif::Shout, Motif::Dash, Motif::Ward, Motif::Nova},
    {Motif::Bolt, Motif::Frost, Motif::Ward, Motif::Drain},
    {Motif::Bolt, Motif::Ward, Motif::Blast, Motif::Focus},
    {Motif::Veil, Motif::Shout, Motif::Volley, Motif::Focus},
    {Motif::Shout, Motif::Ward, Motif::Quake, Motif::Survey},
    {Motif::Veil, Motif::Volley, Motif::Snare, Motif::Dash},
    {Motif::Shout, Motif::Cleanse, Motif::Dash, Motif::Ward},
    {Motif::Snare, Motif::Ward, Motif::Focus, Motif::Survey},
    {Motif::Fury, Motif::Drain, Motif::Veil, Motif::Shout},
    {Motif::Bolt, Motif::Veil, Motif::Frost, Motif::Ward},
    {Motif::Mend, Motif::Sand, Motif::Cleanse, Motif::Dash},
    {Motif::Fury, Motif::Volley, Motif::Ward, Motif::Nova},
    {Motif::Slash, Motif::Dash, Motif::Snare, Motif::Shout},
    {Motif::Focus, Motif::Snare, Motif::Mend, Motif::Shout},
    {Motif::Bolt, Motif::Survey, Motif::Snare, Motif::Veil},
    {Motif::Song, Motif::Song, Motif::Song, Motif::Song},
    {Motif::Blast, Motif::Nova, Motif::Ward, Motif::Dash},
    {Motif::Slash, Motif::Ward, Motif::Cleanse, Motif::Dash},
    {Motif::Mend, Motif::Ward, Motif::Quake, Motif::Veil},
    {Motif::Slash, Motif::Ward, Motif::Cleanse, Motif::Dash}
}};
constexpr std::array<std::array<int, 2>, 26> palettes{{
    {168,169}, {174,170}, {171,174}, {173,175}, {171,170}, {170,174},
    {170,171}, {172,173}, {173,172}, {173,170}, {175,173}, {175,174},
    {174,172}, {173,171}, {168,175}, {173,172}, {171,169}, {172,170},
    {168,174}, {171,172}, {171,174}, {170,173}, {169,174}, {173,170},
    {174,171}, {170,174}
}};
constexpr bool validProfile(int characterClass, int index) { return characterClass >= 0 && characterClass < 26 && index >= 0 && index < 10; }
constexpr Profile profile(int characterClass, int index) {
    if (!validProfile(characterClass, index)) return {Motif::Focus, 174, 172, 251, 0};
    const auto motif = index >= 6 ? motifs[characterClass][index - 6] : Motif::Focus;
    int sound = 251;
    switch (motif) {
        case Motif::Fury: sound = 420; break;
        case Motif::Shout: case Motif::Quake: sound = 174; break;
        case Motif::Slash: sound = 24 + characterClass % 5; break;
        case Motif::Mend: case Motif::Cleanse: sound = 168; break;
        case Motif::Ward: sound = 166; break;
        case Motif::Veil: case Motif::Dash: sound = 77; break;
        case Motif::Sand: sound = 52; break;
        case Motif::Bolt: case Motif::Volley: sound = 75; break;
        case Motif::Blast: case Motif::Nova: sound = 164; break;
        case Motif::Frost: sound = 167; break;
        case Motif::Song: sound = 576 + (index % 2); break;
        default: break;
    }
    int primary = palettes[characterClass][0], secondary = palettes[characterClass][1];
    if (motif == Motif::Frost) { primary = 172; secondary = 174; }
    if (motif == Motif::Mend && characterClass != 14) secondary = 171;
    if (motif == Motif::Sand) secondary = 169;
    return {motif, primary, secondary, sound, characterClass * 10 + index};
}
inline Point sample(const Profile& effect, int ordinal, int count, double phase, bool sustained) {
    constexpr double circle = 6.283185307179586;
    const double portion = ordinal / static_cast<double>(std::max(1, count));
    const double angle = circle * portion + phase * (1.0 + effect.variation % 3 * .25);
    const double radius = (sustained ? 5.0 : 9.0) + effect.variation % 4 * .55;
    Point point{std::cos(angle) * radius, std::sin(angle) * radius, -2.0};
    switch (effect.motif) {
        case Motif::Fury: point.height = -1 - (ordinal % 4) * 2.5; break;
        case Motif::Shout: point.forward *= 1.7; point.side *= 1.7; point.height = -4 + (ordinal % 2) * 3; break;
        case Motif::Slash: {
            const double sweep = (portion - .5) * 2.6;
            point = {std::cos(sweep) * radius * 1.8, std::sin(sweep) * radius * 1.8, -4 + portion * 5}; break;
        }
        case Motif::Mend: point.height = 6 - portion * 16; point.forward *= .65; point.side *= .65; break;
        case Motif::Ward: point.height = -8 + (ordinal % 3) * 5; break;
        case Motif::Veil: point.height = -5 + (ordinal % 4) * 3; point.forward *= .7; point.side *= .7; break;
        case Motif::Dash: point = {-portion * radius * 2, (ordinal % 2 ? -1.0 : 1.0) * 3, 4.0 - (ordinal % 3) * 2}; break;
        case Motif::Sand: point = {5 + portion * radius, std::sin(angle) * radius, -2.0 + (ordinal % 3) * 2}; break;
        case Motif::Bolt: point = {4 + portion * radius * 2, std::sin(angle) * 1.3, -3 + std::cos(angle) * 1.3}; break;
        case Motif::Snare: point.height = 6 - (ordinal % 3) * 3; point.forward *= .8; point.side *= .8; break;
        case Motif::Nova: point.forward *= 1.4; point.side *= 1.4; point.height = ordinal % 2 ? 4 : -6; break;
        case Motif::Focus: point.height = -10 + portion * 14; point.forward *= .6; point.side *= .6; break;
        case Motif::Song: point.height = -6 + std::sin(angle * (2 + effect.variation % 3)) * 4; point.forward *= 1.3; point.side *= 1.3; break;
        case Motif::Blast: point.forward *= 1.5; point.side *= 1.5; point.height = -8 + (ordinal % 4) * 4; break;
        case Motif::Drain: point.height = -6 + portion * 12; point.forward *= 1.2; point.side *= 1.2; break;
        case Motif::Survey: point.height = -10; point.forward *= 1.2; point.side *= 1.2; break;
        case Motif::Quake: point.height = 7; point.forward *= 1.6; point.side *= 1.6; break;
        case Motif::Cleanse: point.height = -10 + portion * 18; break;
        case Motif::Frost: point.height = ordinal % 2 ? 6 : -3; point.forward *= 1.4; point.side *= 1.4; break;
        case Motif::Volley: point = {6 + (ordinal % 3) * 5.0, (portion - .5) * radius * 2.5, -3 + ordinal % 2 * 2.0}; break;
        default: break;
    }
    return point;
}
}
