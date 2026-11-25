#pragma once
#include "delve_skill_vfx.hpp"
#include "delve_passives.hpp"

namespace DelveEffects::SkillVFX {
inline Design perkDesign(int characterClass, const DelvePassives::Rule& rule) {
    using Kind = DelvePassives::Kind;
    using Reward = DelvePassives::Reward;
    constexpr std::array<Shape,26> crests{{S::Fang,S::Shield,S::Heart,S::Mask,S::Compass,S::Cross,S::Coin,S::Rune,
        S::Hexagon,S::Card,S::Tomb,S::Shuriken,S::Feather,S::Web,S::Skull,S::Mirror,S::Vial,S::Circuit,
        S::Chain,S::Root,S::Eye,S::Note,S::Blast,S::Crown,S::Hammer,S::Sun}};
    Shape trigger=S::Rune, reward=S::Well;
    Motion triggerMotion=M::Reveal;
    switch (rule.kind) {
        case Kind::BlockCharge: case Kind::BlockHaste: case Kind::Guard: trigger=S::Shield; break;
        case Kind::WoundedBlock: trigger=S::Crosscut; break;
        case Kind::ComboSwitch: trigger=S::Knives; triggerMotion=M::Sweep; break;
        case Kind::WeaponWeave: trigger=S::Blade; triggerMotion=M::Sweep; break;
        case Kind::AbilityMark: case Kind::Ability: trigger=S::Rune; triggerMotion=M::Spin; break;
        case Kind::AbilityWeave: trigger=S::Hexagon; triggerMotion=M::Spin; break;
        case Kind::Ambush: trigger=S::Mask; triggerMotion=M::Contract; break;
        case Kind::RearHaste: case Kind::Rear: case Kind::ThrownRear: trigger=S::Fang; break;
        case Kind::TrapSense: trigger=S::Eye; break;
        case Kind::Unlock: trigger=S::Compass; triggerMotion=M::Spin; break;
        case Kind::ThrowMark: trigger=S::Shuriken; triggerMotion=M::Spin; break;
        case Kind::CacheEscape: case Kind::Cache: trigger=S::Card; break;
        case Kind::Shatter: trigger=S::Mirror; triggerMotion=M::Shatter; break;
        case Kind::Cadence: trigger=S::Note; triggerMotion=M::Pulse; break;
        case Kind::Refrain: trigger=S::Note; triggerMotion=M::Rise; break;
        case Kind::Cinder: trigger=S::Blast; triggerMotion=M::Expand; break;
        case Kind::Spellblade: trigger=S::Spear; triggerMotion=M::Strike; break;
        case Kind::ReactiveFrost: trigger=S::Snowflake; break;
        case Kind::SpendReserve: trigger=S::Well; triggerMotion=M::Contract; break;
        case Kind::Overflow: trigger=S::Heart; triggerMotion=M::Expand; break;
        case Kind::HealHaste: trigger=S::Wing; triggerMotion=M::Rise; break;
        case Kind::CleanseBarrier: case Kind::ControlEnd: trigger=S::Breath; triggerMotion=M::Expand; break;
        case Kind::RecoveryStrike: trigger=S::Crescent; triggerMotion=M::Contract; break;
        case Kind::Dressing: trigger=S::Cross; triggerMotion=M::Rise; break;
        case Kind::Stabilize: trigger=S::Heart; triggerMotion=M::Pulse; break;
        case Kind::DrinkCharge: case Kind::PotionBarrier: trigger=S::Vial; break;
        case Kind::CleanDrink: trigger=S::Bubble; triggerMotion=M::Rise; break;
        case Kind::DrinkWeave: trigger=S::Helix; triggerMotion=M::Rise; break;
        case Kind::Recipe: trigger=S::Leaf; triggerMotion=M::Rise; break;
        case Kind::Distill: trigger=S::Vial; triggerMotion=M::Contract; break;
        case Kind::Kill: trigger=S::Skull; triggerMotion=M::Rise; break;
        case Kind::ControlledKill: trigger=S::Web; triggerMotion=M::Shatter; break;
        case Kind::RangedKill: trigger=S::Arrow; triggerMotion=M::Shatter; break;
        case Kind::MeleeCombo: trigger=S::Crosscut; triggerMotion=M::Sweep; break;
        case Kind::Explore: trigger=S::Compass; triggerMotion=M::Expand; break;
        case Kind::Quiet: trigger=S::Feather; triggerMotion=M::Contract; break;
        case Kind::Breacher: trigger=S::Hammer; triggerMotion=M::Strike; break;
        case Kind::Ranged: trigger=S::Arrow; triggerMotion=M::Strike; break;
        case Kind::Melee: trigger=S::Blade; triggerMotion=M::Strike; break;
        case Kind::SpellMark: trigger=S::Lance; triggerMotion=M::Strike; break;
        case Kind::SummonBond: trigger=S::Chain; break;
        case Kind::SummonCast: trigger=S::Skull; triggerMotion=M::Spin; break;
        case Kind::Door: trigger=S::Banner; triggerMotion=M::Shatter; break;
        case Kind::Crossfire: trigger=S::Crosscut; triggerMotion=M::Strike; break;
        case Kind::Variety: trigger=S::Card; triggerMotion=M::Spin; break;
        case Kind::UnarmedCounter: trigger=S::Cross; triggerMotion=M::Strike; break;
        case Kind::SummonEcho: trigger=S::Sound; triggerMotion=M::Expand; break;
        case Kind::BloodHunt: trigger=S::Heart; triggerMotion=M::Contract; break;
        case Kind::ControlledStrike: trigger=S::Web; triggerMotion=M::Strike; break;
        case Kind::Execution: trigger=S::Hammer; triggerMotion=M::Fall; break;
        case Kind::PatientStrike: trigger=S::Eye; triggerMotion=M::Contract; break;
        case Kind::HealingOath: trigger=S::Sun; triggerMotion=M::Rise; break;
        case Kind::DuelCounter: trigger=S::Shield; triggerMotion=M::Strike; break;
    }
    int pigment=Azure, sound=251;
    Anchor attachment=A::Hands;
    Motion sustain=M::Pulse;
    switch (rule.reward) {
        case Reward::Barrier: reward=S::Shield; pigment=Azure; sound=44; sustain=M::Hold; break;
        case Reward::Haste: reward=S::Wind; pigment=Ivory; sound=756; attachment=A::Feet; sustain=M::Flow; break;
        case Reward::Heal: reward=S::Cross; pigment=Jade; sound=168; sustain=M::Rise; break;
        case Reward::Discount: reward=S::Well; pigment=Azure; sound=170; sustain=M::Contract; break;
        case Reward::Physical: reward=S::Blade; pigment=Ember; sustain=M::Pulse; break;
        case Reward::Magic: reward=S::Rune; pigment=Violet; sustain=M::Spin; break;
        case Reward::Recovery: reward=S::Compass; pigment=Gold; sustain=M::Contract; break;
    }
    auto result=design("Reactive perk",pigment,Gold,sound,{
        cast(trigger,A::Hands,2.7,triggerMotion,1),cast(reward,attachment,3.2,M::Reveal,0,0,4),
        cast(crests[std::clamp(characterClass,0,25)],A::Body,2.5,M::Rise,2),
        active(reward,attachment,2.5,G::None,sustain)});
    return result;
}
}
