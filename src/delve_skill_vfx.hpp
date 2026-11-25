#pragma once
#include "delve_effect_rules.hpp"
#include <initializer_list>

namespace DelveEffects::SkillVFX {
constexpr double tau = 6.283185307179586;
enum class Stage { Cast, Active, Hit, Control };
enum class Anchor { Body, Hands, Feet, Target, Link };
enum class Gate { None, Strength, Speed, Regeneration, Resistance, Reflection, Sight, Levitation, Amplification, Mana, Hidden };
enum class Motion { Reveal, Sweep, Expand, Contract, Rise, Fall, Spin, Pulse, Flow, Shatter, Strike, Hold };
enum class Shape {
    Blade, Crescent, Crosscut, Spear, Shield, Hexagon, Mirror, Rune, Cross, Helix,
    Leaf, Bubble, Breath, Smoke, Feather, Steps, Wind, Spark, Eye, Compass,
    Crown, Mask, Card, Coin, Skull, Tomb, Chain, Root, Web, Lightning,
    Lance, Arrow, Shuriken, Knives, Pellets, Blast, Wave, Snowflake, Icicle,
    Sound, Note, Sleep, Confusion, Fang, Siphon, Well, Vial, Heart, Circuit,
    Hammer, Sun, Wing, Banner, Count
};
enum Pigment { Crimson, Ember, Gold, Jade, Azure, Violet, Ivory, Smoke };
struct Layer {
    Shape shape = Shape::Rune;
    Stage stage = Stage::Cast;
    Anchor anchor = Anchor::Body;
    Motion motion = Motion::Reveal;
    Gate gate = Gate::None;
    double size = 6, turn = 0, width = .22;
    int tone = 0, delay = 0;
};
struct Design {
    const char* name;
    int primary, secondary, sound;
    std::array<Layer, 6> layers{};
    int count = 0;
};
constexpr Layer cast(Shape shape, Anchor anchor, double size, Motion motion = Motion::Reveal,
    int tone = 0, double turn = 0, int delay = 0, double width = .22) {
    return {shape, Stage::Cast, anchor, motion, Gate::None, size, turn, width, tone, delay};
}
constexpr Layer active(Shape shape, Anchor anchor, double size, Gate gate,
    Motion motion = Motion::Pulse, int tone = 0, double turn = 0) {
    return {shape, Stage::Active, anchor, motion, gate, size, turn, .16, tone, 0};
}
constexpr Layer hit(Shape shape, double size, Motion motion = Motion::Shatter, int tone = 0, double turn = 0) {
    return {shape, Stage::Hit, Anchor::Target, motion, Gate::None, size, turn, .24, tone, 0};
}
constexpr Layer link(Shape shape, Motion motion = Motion::Strike, int tone = 0, double width = .22) {
    return {shape, Stage::Hit, Anchor::Link, motion, Gate::None, 1, 0, width, tone, 0};
}
constexpr Layer control(Shape shape, double size, Motion motion = Motion::Pulse, int tone = 0) {
    return {shape, Stage::Control, Anchor::Target, motion, Gate::None, size, 0, .16, tone, 0};
}
constexpr Design design(const char* name, int primary, int secondary, int sound, std::initializer_list<Layer> layers) {
    Design result{name, primary, secondary, sound};
    for (const auto& layer : layers) if (result.count < 6) result.layers[result.count++] = layer;
    return result;
}
using S = Shape;
using A = Anchor;
using M = Motion;
using G = Gate;
inline constexpr std::array<Design, 104> designs{{
#include "delve_skill_vfx_catalogue.inc"
}};
constexpr const Design& get(int characterClass, int index) {
    return designs[validProfile(characterClass, index) && index >= 6 ? characterClass * 4 + index - 6 : 0];
}
constexpr int segments(Shape shape) {
    switch (shape) {
        case Shape::Cross: case Shape::Crosscut: case Shape::Hammer: return 4;
        case Shape::Spear: case Shape::Arrow: case Shape::Fang: case Shape::Sleep: return 6;
        case Shape::Shield: case Shape::Crown: case Shape::Tomb: case Shape::Vial: case Shape::Banner: return 8;
        case Shape::Mirror: case Shape::Hexagon: case Shape::Shuriken: case Shape::Compass: case Shape::Root: return 12;
        case Shape::Wave: case Shape::Crescent: case Shape::Rune: case Shape::Snowflake: case Shape::Sound: return 18;
        default: return 14;
    }
}
struct Stroke { Point first, second; double width = 1; bool bead = false; };
// Camera-relative forward/right/down, in entity coordinates (positive Z is down).
inline Point cameraPoint(Point eye, double yaw, double pitch, Point point) {
    const double forward = point.forward * std::cos(pitch) - point.height * std::sin(pitch);
    return {eye.forward + forward * std::cos(yaw) - point.side * std::sin(yaw),
        eye.side + forward * std::sin(yaw) + point.side * std::cos(yaw),
        eye.height + point.forward * std::sin(pitch) + point.height * std::cos(pitch)};
}
inline Point mix(Point first, Point second, double amount) {
    return {first.forward + (second.forward - first.forward) * amount,
        first.side + (second.side - first.side) * amount, first.height + (second.height - first.height) * amount};
}
inline Point planar(double horizontal, double vertical) { return {0, horizontal, vertical}; }
inline Point radial(double angle, double radius = 1, double height = 0) { return {std::cos(angle) * radius, std::sin(angle) * radius, height}; }
inline double noise(int ordinal, int salt) { return std::sin(ordinal * 17.13 + salt * 9.71); }
inline Stroke geometry(const Layer& layer, int ordinal, double progress) {
    const int count = segments(layer.shape);
    const double portion = ordinal / static_cast<double>(count), next = (ordinal + 1.0) / count;
    const double angle = tau * portion, nextAngle = tau * next;
    const double clock = std::clamp(progress, 0.0, 1.0);
    Stroke stroke;
    const auto path = [&](auto vertex) { return Stroke{vertex(portion), vertex(next)}; };
    const auto upright = [&](int corners, double rotation = 0.0) {
        const double first = tau * ordinal / corners + rotation, second = tau * (ordinal + 1) / corners + rotation;
        return Stroke{planar(std::cos(first), std::sin(first)), planar(std::cos(second), std::sin(second))};
    };
    switch (layer.shape) {
        case Shape::Blade:
            stroke = path([&](double part) { return Point{std::cos(-1.05 + part * 2.1), std::sin(-1.05 + part * 2.1), -.28 + part * .56}; }); break;
        case Shape::Crescent:
            stroke = path([&](double part) { return radial(-1.35 + part * 2.7, 1, -.25); }); break;
        case Shape::Crosscut: {
            const double side = ordinal % 2 ? -1.0 : 1.0;
            stroke = {planar(-side * .8, ordinal < 2 ? -.7 : .7), planar(side * .8, ordinal < 2 ? .7 : -.7)}; break;
        }
        case Shape::Spear: case Shape::Arrow: {
            const int piece = ordinal % 3; const double lane = ordinal < 3 ? -.025 : .025;
            stroke = piece == 0 ? Stroke{{-.7, lane, 0}, {1, lane, 0}}
                : Stroke{{1, lane, 0}, {.6, piece == 1 ? -.22 : .22, 0}}; break;
        }
        case Shape::Shield: {
            constexpr std::array<Point, 9> outline{{{0,-.7,-.8},{0,0,-1},{0,.7,-.8},{0,.7,.15},{0,.4,.7},{0,0,1},{0,-.4,.7},{0,-.7,.15},{0,-.7,-.8}}};
            stroke = {outline[ordinal], outline[ordinal + 1]}; break;
        }
        case Shape::Hexagon: stroke = upright(6, tau / 12); break;
        case Shape::Mirror: {
            constexpr std::array<Point, 4> corners{{{0,0,-1},{0,.7,0},{0,0,1},{0,-.7,0}}};
            if (ordinal < 8) stroke = {corners[ordinal % 4], corners[(ordinal + 1) % 4]};
            else stroke = {{0,0,0}, corners[ordinal % 4]};
            break;
        }
        case Shape::Rune:
            if (ordinal < 12) stroke = {radial(tau * ordinal / 12), radial(tau * (ordinal + 1) / 12)};
            else stroke = {radial(tau * (ordinal - 12) / 6, .7), radial(tau * (ordinal - 12) / 6, 1.18)};
            break;
        case Shape::Cross: {
            const auto direction = planar(ordinal % 2 ? 0 : (ordinal == 0 ? -1 : 1), ordinal % 2 ? (ordinal == 1 ? -1 : 1) : 0);
            stroke = {{}, direction}; break;
        }
        case Shape::Helix:
            stroke = path([&](double part) { return Point{std::cos(part * tau * 1.5), std::sin(part * tau * 1.5), 1 - part * 2}; }); break;
        case Shape::Leaf: case Shape::Feather: {
            const int pair = ordinal / 2; const double along = pair / 6.0;
            const double side = ordinal % 2 ? -1.0 : 1.0;
            stroke = {{0, 0, 1 - along * 2}, {0, side * std::sin(along * 3.14159) * .65, .5 - along * 1.6}}; break;
        }
        case Shape::Bubble: case Shape::Smoke: {
            const double seed = noise(ordinal, 2), seedSide = noise(ordinal, 5);
            stroke.first = {seed * .8, seedSide * .8, .8 - clock * 1.7 + noise(ordinal, 8) * .3};
            stroke.second = stroke.first; stroke.bead = true; stroke.width = layer.shape == Shape::Smoke ? 2.4 : .9; break;
        }
        case Shape::Breath:
            stroke = path([&](double part) { return Point{.3 + clock, std::sin(part * tau) * (.3 + clock * .7), std::cos(part * tau) * .5}; }); break;
        case Shape::Steps: {
            const double trail = (ordinal / 2) / 6.0;
            stroke = {{-trail * 1.6, ordinal % 2 ? -.35 : .35, .6}, {-.2-trail * 1.6, ordinal % 2 ? -.18 : .18, .6}}; break;
        }
        case Shape::Wind:
            stroke = path([&](double part) { return Point{-part * 1.5, std::sin(part * tau + clock * tau) * .35, std::cos(part * tau) * .2}; }); break;
        case Shape::Spark: case Shape::Lightning:
            stroke = path([&](double part) { return Point{part, std::sin(part * tau * 5) * .12, std::cos(part * tau * 3) * .1}; }); break;
        case Shape::Eye:
            stroke = path([&](double part) { const double phase = part * tau; return planar(std::cos(phase), std::sin(phase) * .45); }); break;
        case Shape::Compass: {
            const double direction = tau * (ordinal / 3) / 4;
            stroke = ordinal % 3 == 0 ? Stroke{radial(direction, .15), radial(direction)}
                : Stroke{radial(direction), radial(direction + (ordinal % 3 == 1 ? -.28 : .28), .65)}; break;
        }
        case Shape::Crown: {
            constexpr std::array<Point, 9> outline{{{0,-1,.4},{0,-1,-.6},{0,-.5,0},{0,0,-1},{0,.5,0},{0,1,-.6},{0,1,.4},{0,0,.65},{0,-1,.4}}};
            stroke = {outline[ordinal], outline[ordinal + 1]}; break;
        }
        case Shape::Mask:
            stroke = path([&](double part) { return planar(std::cos(part*tau), std::sin(part*tau)*.55 + .15*std::cos(part*tau*3)); }); break;
        case Shape::Card: {
            constexpr std::array<Point, 4> corners{{{0,-.45,-.75},{0,.45,-.75},{0,.45,.75},{0,-.45,.75}}};
            stroke = {corners[ordinal % 4], corners[(ordinal + 1) % 4]};
            stroke.first.forward = stroke.second.forward = (ordinal / 4) * .24; break;
        }
        case Shape::Coin: stroke = upright(segments(layer.shape)); break;
        case Shape::Skull:
            stroke = path([&](double part) { const double phase=part*tau; return planar(std::cos(phase)*(.6+.25*std::cos(phase)), std::sin(phase)); }); break;
        case Shape::Tomb: {
            constexpr std::array<Point, 9> outline{{{0,-.7,1},{0,-.7,-.5},{0,-.5,-.85},{0,0,-1},{0,.5,-.85},{0,.7,-.5},{0,.7,1},{0,0,1},{0,-.7,1}}};
            stroke = {outline[ordinal], outline[ordinal + 1]}; break;
        }
        case Shape::Chain: {
            const double along = (ordinal / 4) / 3.0;
            const double phase = tau * (ordinal % 4) / 4;
            stroke = {{along, std::sin(phase)*.14, std::cos(phase)*.14}, {along+.08, std::sin(phase+tau/4)*.14, std::cos(phase+tau/4)*.14}}; break;
        }
        case Shape::Root: {
            const double direction = tau * (ordinal / 3) / 4;
            const double height = (ordinal % 3) * .6;
            stroke = {radial(direction, .9-height*.25, .7-height), radial(direction+.25, .75-height*.25, .1-height)}; break;
        }
        case Shape::Web:
            stroke = ordinal < 7 ? Stroke{radial(tau*ordinal/7,.2),radial(tau*ordinal/7)}
                : Stroke{radial(tau*(ordinal-7)/7,.7),radial(tau*(ordinal-6)/7,.7)}; break;
        case Shape::Lance:
            stroke = path([&](double part) { return Point{part, std::sin(part*tau*2)*.035, std::cos(part*tau*2)*.035}; }); break;
        case Shape::Shuriken: {
            const double direction = tau * (ordinal / 3) / 4;
            const double radius = ordinal % 3 == 0 ? 1 : .25;
            stroke = {planar(std::cos(direction)*radius,std::sin(direction)*radius),
                planar(std::cos(direction+tau/12)*.25,std::sin(direction+tau/12)*.25)}; break;
        }
        case Shape::Knives: case Shape::Pellets: {
            const int lanes = layer.shape == Shape::Knives ? 3 : 5;
            const double direction = ((ordinal % lanes) / double(lanes-1)-.5)*1.25;
            const double distance = .3 + (ordinal / lanes)*.25 + clock*.25;
            stroke = {radial(direction,distance),radial(direction,distance+.22)}; break;
        }
        case Shape::Blast:
            stroke = {radial(angle,.15+clock*.6,noise(ordinal,3)*.3),radial(angle,.55+clock*.45,noise(ordinal,3)*.7)}; break;
        case Shape::Wave: stroke = {radial(angle),radial(nextAngle)}; break;
        case Shape::Snowflake: {
            const double direction = tau * (ordinal / 3) / 6;
            stroke = ordinal % 3 == 0 ? Stroke{radial(direction,.1),radial(direction)}
                : Stroke{radial(direction,.65),radial(direction+(ordinal%3==1?-.35:.35),.9)}; break;
        }
        case Shape::Icicle: {
            const double direction = tau * (ordinal / 2) / 7;
            stroke = {radial(direction,.7, .6),radial(direction+(ordinal%2?-.12:.12),.7,-.6-clock*.5)}; break;
        }
        case Shape::Sound:
            stroke = path([&](double part) { return Point{clock*.6, std::cos(part*tau), std::sin(part*tau)*(.35+.15*std::cos(part*tau*3))}; }); break;
        case Shape::Note: {
            const int piece = ordinal % 7; const double side = ordinal < 7 ? -.45 : .45;
            if (piece == 0) stroke={planar(side,.4),planar(side,-.8)};
            else if (piece == 1) stroke={planar(side,-.8),planar(side+.4,-.55)};
            else { const double phase=tau*(piece-2)/5; stroke={planar(side-.13+std::cos(phase)*.19,.4+std::sin(phase)*.14),planar(side-.13+std::cos(phase+tau/5)*.19,.4+std::sin(phase+tau/5)*.14)}; }
            break;
        }
        case Shape::Sleep: {
            const int piece=ordinal%3; const double offset=ordinal<3?-.25:.4;
            stroke=piece==0?Stroke{planar(-.4,offset-.4),planar(.4,offset-.4)}:piece==1?Stroke{planar(.4,offset-.4),planar(-.4,offset+.1)}:Stroke{planar(-.4,offset+.1),planar(.4,offset+.1)}; break;
        }
        case Shape::Confusion:
            stroke = path([&](double part) { return planar(std::cos(part*tau*1.5)*part,std::sin(part*tau*1.5)*part); }); break;
        case Shape::Fang: {
            const double side=ordinal<3?-.5:.5; const int piece=ordinal%3;
            stroke=piece==0?Stroke{planar(side-.25,-.8),planar(side,.7)}:piece==1?Stroke{planar(side,.7),planar(side+.25,-.8)}:Stroke{planar(side+.25,-.8),planar(side-.25,-.8)}; break;
        }
        case Shape::Siphon:
            stroke.first={std::fmod(portion+clock,1.0),std::sin(angle+clock*tau)*.09,std::cos(angle+clock*tau)*.09};
            stroke.second=stroke.first; stroke.bead=true; stroke.width=.8; break;
        case Shape::Well:
            stroke = path([&](double part) { return radial(part*tau*2,1-part*.8, .8-part*1.6); }); break;
        case Shape::Vial: {
            constexpr std::array<Point, 9> outline{{{0,-.25,-1},{0,.25,-1},{0,.25,-.4},{0,.7,.1},{0,.7,.8},{0,-.7,.8},{0,-.7,.1},{0,-.25,-.4},{0,-.25,-1}}};
            stroke={outline[ordinal],outline[ordinal+1]}; break;
        }
        case Shape::Heart:
            stroke=path([&](double part) { const double phase=part*tau; return planar(std::pow(std::sin(phase),3),-(13*std::cos(phase)-5*std::cos(2*phase)-2*std::cos(3*phase)-std::cos(4*phase))/17); }); break;
        case Shape::Circuit: {
            const double along=ordinal/14.0; const double direction=ordinal%2?-.18:.18;
            stroke={{along,direction,0},{along+.07,direction,ordinal%3?.18:0}}; break;
        }
        case Shape::Hammer:
            stroke=ordinal<2?Stroke{planar(0,.8),planar(0,-.4)}:Stroke{planar(-.7,-.4),planar(.7,-.4)}; break;
        case Shape::Sun:
            stroke={planar(std::cos(angle)*.55,std::sin(angle)*.55),planar(std::cos(angle),std::sin(angle))}; break;
        case Shape::Wing: {
            const double side=ordinal%2?-1.0:1.0; const double part=(ordinal/2)/6.0;
            stroke={planar(side*.2,part*.4),planar(side*(.4+part*.8),-.8+part*.9)}; break;
        }
        case Shape::Banner: {
            constexpr std::array<Point,9> outline{{{0,-.5,1},{0,-.5,-1},{0,.6,-1},{0,.8,-.6},{0,.4,-.2},{0,-.5,-.2},{0,-.5,-1},{0,-.5,0},{0,-.5,1}}};
            stroke={outline[ordinal],outline[ordinal+1]}; break;
        }
        default: break;
    }
    double growth=1, rotation=layer.turn;
    switch (layer.motion) {
        case Motion::Reveal: growth=.45+.55*std::min(1.0,clock*4); break;
        case Motion::Sweep: rotation+=(clock-.5)*1.5; break;
        case Motion::Expand: growth=.12+.88*clock; break;
        case Motion::Contract: growth=1-clock*.75; break;
        case Motion::Spin: rotation+=clock*tau; break;
        case Motion::Pulse: growth=.92+.08*std::sin(clock*tau*2); break;
        case Motion::Shatter: growth=.3+clock; break;
        case Motion::Strike: growth=.6+.4*std::sin(std::min(1.0,clock*3)*1.570796); break;
        default: break;
    }
    if (layer.anchor == Anchor::Link) { growth=1; rotation=0; }
    const double size=std::abs(layer.size)*growth;
    for (auto point : {&stroke.first,&stroke.second}) {
        const double forward=point->forward, side=point->side;
        point->forward=(forward*std::cos(rotation)-side*std::sin(rotation))*size;
        point->side=(forward*std::sin(rotation)+side*std::cos(rotation))*size;
        point->height*=size;
        if (layer.motion==Motion::Rise) point->height-=clock*size*.8;
        if (layer.motion==Motion::Fall) point->height+=clock*size*.8;
        if (layer.motion==Motion::Flow) point->forward-=clock*size*.4;
    }
    return stroke;
}
}
