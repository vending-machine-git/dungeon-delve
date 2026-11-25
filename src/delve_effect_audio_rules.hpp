#pragma once
#include "delve_skill_vfx.hpp"

namespace DelveEffects::Sonic {
enum class Family { Blade, Wind, Ward, Mend, Mana, Ice, Lightning, Blood, Song, Metal, Veil, Roots, Blast, Voice, Brew };
struct Cue { Family family; double pitch=220, duration=.42; int phrase=0; bool impact=false; };
inline Cue cue(const SkillVFX::Design& design, int characterClass, int index, bool impact) {
    using Shape=SkillVFX::Shape;
    const auto shape=design.layers[index<6?1:0].shape;
    Family family=Family::Mana;
    switch (shape) {
        case Shape::Blade: case Shape::Crescent: case Shape::Crosscut: case Shape::Spear: case Shape::Arrow:
        case Shape::Shuriken: case Shape::Knives: case Shape::Pellets: family=Family::Blade; break;
        case Shape::Steps: case Shape::Wind: case Shape::Wing: case Shape::Feather: case Shape::Breath: family=Family::Wind; break;
        case Shape::Shield: case Shape::Hexagon: case Shape::Mirror: family=Family::Ward; break;
        case Shape::Cross: case Shape::Heart: family=Family::Mend; break;
        case Shape::Snowflake: case Shape::Icicle: family=Family::Ice; break;
        case Shape::Lightning: case Shape::Circuit: case Shape::Spark: family=Family::Lightning; break;
        case Shape::Fang: case Shape::Skull: family=Family::Blood; break;
        case Shape::Note: family=Family::Song; break;
        case Shape::Coin: case Shape::Chain: case Shape::Hammer: case Shape::Crown: family=Family::Metal; break;
        case Shape::Smoke: case Shape::Mask: case Shape::Eye: family=Family::Veil; break;
        case Shape::Leaf: case Shape::Root: case Shape::Web: case Shape::Tomb: family=Family::Roots; break;
        case Shape::Blast: family=Family::Blast; break;
        case Shape::Sound: family=Family::Voice; break;
        case Shape::Vial: case Shape::Bubble: family=Family::Brew; break;
        default: break;
    }
    if (characterClass==21 && index>=6) family=Family::Song;
    if (characterClass==7 && index==7) family=Family::Ice;
    if (characterClass==14 && index==7) family=Family::Blood;
    const double pitch=family==Family::Blood||family==Family::Blast ? 100 : family==Family::Song ? 330 : 210;
    return {family,pitch*std::pow(2.0,((characterClass%5)+(index%4)*2)/12.0),impact?.30:.52,index>=6?index-6:index%4,impact};
}
inline double sample(const Cue& cue, double time, double noise) {
    if (time<=0 || time>=cue.duration) return 0;
    constexpr double tau=6.283185307179586;
    const double progress=time/cue.duration;
    const double envelope=std::min(1.0,time/.006)*std::min(1.0,(cue.duration-time)/.025)*std::pow(1-progress,.8);
    const auto tone=[&](double frequency) { return std::sin(tau*frequency*time); };
    const double pitch=cue.pitch;
    double value=0;
    switch (cue.family) {
        case Family::Blade: value=noise*.6*std::sin(progress*3.14159)+tone(pitch*(1.7-progress))*.25; break;
        case Family::Wind: value=noise*.38*(.5+.5*std::sin(time*25))+tone(pitch*.5)*.09; break;
        case Family::Ward: value=tone(pitch)*.28+tone(pitch*1.5)*.22+tone(pitch*2)*.16; break;
        case Family::Mend: value=(tone(pitch)+tone(pitch*1.25)+tone(pitch*1.5))*.2*(.75+.25*std::cos(time*18)); break;
        case Family::Mana: value=tone(pitch*(1+progress*.45))*.34+tone(pitch*2)*.18; break;
        case Family::Ice: value=tone(pitch*3.12)*.2+tone(pitch*4.3)*.16+tone(pitch*6.1)*.12+noise*.08; break;
        case Family::Lightning: value=noise*.35*(.4+.6*std::pow(std::sin(time*95),2))+tone(pitch*1.9)*.24; break;
        case Family::Blood: value=tone(pitch*(1-progress*.45))*.4+tone(pitch*.5)*.2+noise*.12; break;
        case Family::Song: {
            constexpr std::array<std::array<double,4>,4> melodies{{
                {1,1.25,1.5,2},{1,.84,.75,.67},{1,1.5,1.25,2},{1,1.414,.944,1.414}
            }};
            const int note=std::min(3,static_cast<int>(progress*4));
            const double noteTime=std::fmod(time,cue.duration/4), frequency=pitch*melodies[cue.phrase][note];
            const double attack=std::min(1.0,noteTime/.004);
            value=(std::sin(tau*frequency*noteTime)*.4+std::sin(tau*frequency*2.01*noteTime)*.17)*attack*std::exp(-noteTime*9); break;
        }
        case Family::Metal: value=tone(pitch*2.4)*.25+tone(pitch*3.83)*.19+tone(pitch*6.17)*.12; break;
        case Family::Veil: value=noise*.21*std::sin(progress*3.14159)+tone(pitch*.5)*.15; break;
        case Family::Roots: value=noise*.28*std::pow(std::sin(time*38),2)+tone(pitch*.6)*.22; break;
        case Family::Blast: value=(noise*.56+tone(pitch*(1-progress*.6))*.32)*std::exp(-progress*3); break;
        case Family::Voice: value=(tone(pitch*.65)*.28+tone(pitch*1.3)*.18+noise*.14)*(.7+.3*std::sin(time*29)); break;
        case Family::Brew: value=tone(pitch*(1+std::fmod(progress*3,1)))*.27*std::pow(std::sin(progress*9.42),2)+noise*.12; break;
    }
    return std::clamp(value*envelope,-.85,.85);
}
}
