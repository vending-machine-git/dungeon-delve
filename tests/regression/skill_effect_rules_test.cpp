#include "../../src/delve_effect_rules.hpp"
#include "../../src/delve_skill_vfx.hpp"
#include "../../src/delve_perk_vfx.hpp"
#include "../../src/delve_effect_audio_rules.hpp"
#include <cassert>
#include <iostream>
#include <set>
#include <sstream>
#include <iomanip>

int main() {
    using namespace DelveEffects;
    int cameraChecks = 0;
    for (double yaw : {0.0,.7,1.570796326794897,3.141592653589793,5.9})
        for (double pitch : {-1.1,-.45,0.0,.55,1.1}) for (double height : {-12.0,-2.0,4.0}) {
            const Point eye{112,240,height};
            const auto dot = [](Point a, Point b) { return a.forward*b.forward + a.side*b.side + a.height*b.height; };
            const Point forward{std::cos(yaw)*std::cos(pitch),std::sin(yaw)*std::cos(pitch),std::sin(pitch)};
            const Point right{-std::sin(yaw),std::cos(yaw),0};
            const Point down{-std::cos(yaw)*std::sin(pitch),-std::sin(yaw)*std::sin(pitch),std::cos(pitch)};
            for (Point local : {Point{16,0,0},Point{18,-3,2},Point{14,3,-2}}) {
                const auto world = SkillVFX::cameraPoint(eye,yaw,pitch,local);
                const Point relative{world.forward-eye.forward,world.side-eye.side,world.height-eye.height};
                assert(std::abs(dot(relative,forward)-local.forward)<1e-10);
                assert(std::abs(dot(relative,right)-local.side)<1e-10);
                assert(std::abs(dot(relative,down)-local.height)<1e-10);
                ++cameraChecks;
            }
        }
    std::cout << "PASS " << cameraChecks << " camera-space projections across yaw, pitch and body heights\n";
    std::set<int> identities;
    std::set<Motif> shapes;
    for (int characterClass = 0; characterClass < 26; ++characterClass) {
        for (int index = 0; index < 10; ++index) {
            assert(validProfile(characterClass, index));
            const auto effect = profile(characterClass, index);
            assert(identities.insert(effect.variation).second);
            assert(effect.primary >= 168 && effect.primary <= 175);
            assert(effect.secondary >= 168 && effect.secondary <= 175);
            assert(effect.sound >= 0 && effect.sound < 800);
            if (index >= 6) shapes.insert(effect.motif);
            for (bool sustained : {false, true}) {
                for (int ordinal = 0; ordinal < 16; ++ordinal) {
                    const auto point = sample(effect, ordinal, 16, .75, sustained);
                    assert(std::isfinite(point.forward) && std::isfinite(point.side) && std::isfinite(point.height));
                    assert(std::abs(point.forward) < 40 && std::abs(point.side) < 40 && std::abs(point.height) < 20);
                }
            }
        }
    }
    assert(identities.size() == 260 && shapes.size() == static_cast<int>(Motif::Count));
    assert(!validProfile(-1, 6) && !validProfile(26, 6) && !validProfile(0, -1) && !validProfile(0, 10));
    assert(profile(-1, 6).primary == 174);
    std::set<std::string> choreographies, names;
    std::set<SkillVFX::Shape> authoredShapes;
    int layerCount = 0;
    for (const auto& design : SkillVFX::designs) {
        assert(names.insert(design.name).second);
        assert(design.count >= 3 && design.count <= 6);
        assert(design.primary >= 0 && design.primary < 8 && design.secondary >= 0 && design.secondary < 8);
        std::ostringstream fingerprint;
        fingerprint << std::fixed << std::setprecision(3);
        bool hasCast = false;
        for (int layerIndex = 0; layerIndex < design.count; ++layerIndex) {
            const auto& layer = design.layers[layerIndex];
            ++layerCount; authoredShapes.insert(layer.shape);
            hasCast |= layer.stage == SkillVFX::Stage::Cast;
            assert(layer.width > 0 && layer.width <= .4 && layer.delay >= 0 && layer.delay < 15);
            assert(layer.stage != SkillVFX::Stage::Active || layer.gate != SkillVFX::Gate::None);
            assert(layer.anchor != SkillVFX::Anchor::Link || layer.stage == SkillVFX::Stage::Hit);
            fingerprint << static_cast<int>(layer.stage) << ':' << static_cast<int>(layer.anchor) << ':';
            for (double progress : {0.0,.15,.4,.7,1.0}) for (int ordinal = 0; ordinal < SkillVFX::segments(layer.shape); ++ordinal) {
                const auto stroke = SkillVFX::geometry(layer,ordinal,progress);
                for (auto point : {stroke.first,stroke.second}) {
                    assert(std::isfinite(point.forward) && std::isfinite(point.side) && std::isfinite(point.height));
                    assert(std::abs(point.forward)<50 && std::abs(point.side)<50 && std::abs(point.height)<30);
                    fingerprint << point.forward << ',' << point.side << ',' << point.height << ';';
                }
            }
        }
        assert(hasCast && choreographies.insert(fingerprint.str()).second);
    }
    assert(choreographies.size() == 104 && layerCount >= 350);
    assert(authoredShapes.size() >= 45);
    using namespace DelvePassives;
    constexpr std::array<Rule,156> perkRules{{
#include "../../src/delve_passives_catalogue.inc"
    }};
    std::set<std::string> perkChoreographies;
    for (int ordinal=0; ordinal<156; ++ordinal) {
        const auto design=SkillVFX::perkDesign(ordinal/6,perkRules[ordinal]);
        assert(design.count==4 && design.layers[3].stage==SkillVFX::Stage::Active);
        std::ostringstream fingerprint;
        fingerprint << std::fixed << std::setprecision(3);
        for (int layerIndex=0;layerIndex<design.count;++layerIndex) {
            const auto& layer=design.layers[layerIndex];
            fingerprint << static_cast<int>(layer.stage) << ':' << static_cast<int>(layer.anchor) << ';';
            for (double progress : {0.0,.5,1.0}) for (int pointIndex=0;pointIndex<SkillVFX::segments(layer.shape);++pointIndex) {
                const auto stroke=SkillVFX::geometry(layer,pointIndex,progress);
                for (auto point : {stroke.first,stroke.second}) {
                    assert(std::isfinite(point.forward) && std::isfinite(point.side) && std::isfinite(point.height));
                    fingerprint << point.forward << ',' << point.side << ',' << point.height << ';';
                }
            }
        }
        if (!perkChoreographies.insert(fingerprint.str()).second) {
            std::cerr << "Duplicate perk choreography class=" << ordinal/6 << " index=" << ordinal%6 << '\n';
            assert(false);
        }
    }
    std::set<Sonic::Family> soundFamilies;
    for (int characterClass=0;characterClass<26;++characterClass) for (int index=0;index<10;++index) {
        const auto design=index>=6?SkillVFX::get(characterClass,index):SkillVFX::perkDesign(characterClass,perkRules[characterClass*6+index]);
        for (bool impact:{false,true}) {
            const auto cue=Sonic::cue(design,characterClass,index,impact);
            soundFamilies.insert(cue.family);
            assert(Sonic::sample(cue,0,1)==0 && Sonic::sample(cue,cue.duration,1)==0);
            double energy=0;
            for (int frame=0;frame<static_cast<int>(cue.duration*22050);++frame) {
                const double value=Sonic::sample(cue,frame/22050.0,std::sin(frame*17.13));
                assert(std::isfinite(value) && std::abs(value)<=.85); energy+=value*value;
            }
            assert(energy>1);
        }
    }
    assert(soundFamilies.size()>=12);
    std::cout << "PASS 104 geometrically unique skill choreographies, " << layerCount << " authored layers, " << authoredShapes.size()
        << " shapes; 156 unique trigger/reward/class perk choreographies; 520 bounded non-silent audio cues, confirmed-hit-only links\n";
}
