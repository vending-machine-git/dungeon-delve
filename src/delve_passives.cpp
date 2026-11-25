#include "main.hpp"
#include "game.hpp"
#include "entity.hpp"
#include "stat.hpp"
#include "player.hpp"
#include "net.hpp"
#include "items.hpp"
#include "monster.hpp"
#include "magic/magic.hpp"
#include "collision.hpp"
#include "draw.hpp"
#include "delve_hud.hpp"
#include "interface/interface.hpp"
#include "mod_tools.hpp"
#include "raid.hpp"
#include "delve_builds.hpp"
#include "delve_passives.hpp"
#include "delve_effects.hpp"
#include "delve_rarity.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <set>

namespace DelvePassives {
namespace {
const std::array<Rule, 156> catalogue{{
#include "delve_passives_catalogue.inc"
}};
struct Slot {
    unsigned int cooldown = 0, window = 0, target = 0, token = 0;
    int charges = 0, stacks = 0, category = -1, cost = 0;
};
struct State {
    std::array<Slot, 2> slots{};
    int barrier = 0, discount = 0, haste = 0, boosters = 0, healing = 0;
    unsigned int barrierTime = 0, discountTime = 0, hasteTime = 0, guardTime = 0;
    unsigned int quietTime = 0, healTime = 0, healInterval = 0, serial = 0, received = 0;
    int floor = -1, effects = 0;
    unsigned int siphonCooldown = 0;
    int equipmentHealth = 0, equipmentMana = 0;
    unsigned int healthRegen = 0, manaRegen = 0;
    bool invisible = false, guardSpent = false;
    double guardX = 0, guardY = 0;
    std::set<int> regions;
    std::set<unsigned int> kills, caches;
    std::deque<std::array<unsigned int, 3>> hits;
    std::deque<unsigned int> refunds;
};
std::array<State, MAXPLAYERS> states;
Action current;
unsigned int sequence = 0, lastSend = 0;
bool processing = false;
#ifdef BARONY_SMOKE_TESTS
bool auditing = false;
#endif
bool activePlayer(int player) {
    return player >= 0 && player < MAXPLAYERS && Raid::active() && !intro && !loading && !fadeout
        && !gamePaused && !Raid::matchComplete() && !client_disconnected[player]
        && Raid::outcome(player) == Raid::Outcome::Playing && !Raid::protectedPlayer(player)
        && players[player] && players[player]->entity && stats[player] && stats[player]->HP > 0;
}
int owner(Entity* entity) {
    if (!entity) return -1;
    if (entity->behavior == &actPlayer) return entity->skill[2];
    if (entity->behavior == &actMonster) {
        auto leader = entity->monsterAllyGetPlayerLeader();
        if (leader && leader->behavior == &actPlayer) return leader->skill[2];
    }
    return -1;
}
bool authority(int player) { return multiplayer == SERVER && activePlayer(player); }
bool hostile(Entity* source, Entity& target) {
    if (!source || source == &target || !target.getStats() || target.getStats()->HP <= 0) return false;
    if (target.behavior != &actMonster && target.behavior != &actPlayer) return false;
    if (target.behavior == &actPlayer && !activePlayer(target.skill[2])) return false;
    return source->checkEnemy(&target);
}
const Rule* equipped(int player, int slot) {
    auto build = DelveBuilds::activeSelection(player);
    if (build[slot] < 0 || build[slot] >= 6) return nullptr;
    return &rule(DelveBuilds::activeClass(player), build[slot]);
}
int removable(Entity* entity) {
    if (!entity || !entity->getStats()) return 0;
    int mask = 0, bit = 1;
    for (int effect : {EFF_POISONED, EFF_BLEEDING, EFF_SLOW, EFF_WEBBED, EFF_CONFUSED, EFF_FEAR, EFF_ASLEEP, EFF_PARALYZED, EFF_STUNNED}) {
        if (entity->getStats()->getEffectActive(effect)) mask |= bit;
        bit <<= 1;
    }
    return mask;
}
bool beneficial(int item) {
    return item == POTION_HEALING || item == POTION_EXTRAHEALING || item == POTION_RESTOREMAGIC
        || item == POTION_CUREAILMENT || item == POTION_SPEED || item == POTION_LEVITATION
        || item == POTION_INVISIBILITY || item == POTION_STRENGTH || item == POTION_WATER || item == POTION_JUICE;
}
bool damaging(int spell) {
    if (auto ability = DelveBuilds::spellOption(spell)) return ability->ability.damage > 0;
    auto found = ItemTooltips.spellItems.find(spell);
    return found != ItemTooltips.spellItems.end() && found->second.spellTags.count(ItemTooltips_t::SPELL_TAG_DAMAGE) > 0;
}
bool abilityMatches(const Rule& definition, int spell) {
    if (!DelveBuilds::isAbilitySpell(spell)) return false;
    return definition.filter < 0 || 6 + (spell - 600) % 4 == definition.filter;
}
bool rear(Entity& source, Entity& target) {
    double direction = std::atan2(source.y - target.y, source.x - target.x);
    return std::abs(std::remainder(direction - target.yaw, 2 * PI)) >= 2 * PI / 3;
}
unsigned int extendedWindow(int seconds) {
    return seconds * TICKS_PER_SECOND * 5 / 4;
}
bool vulnerable(Entity& target) {
    auto targetStats = target.getStats();
    return targetStats && (targetStats->getEffectActive(EFF_ASLEEP)
        || targetStats->getEffectActive(EFF_FEAR) || targetStats->getEffectActive(EFF_PARALYZED));
}
void barrier(int player, int value, unsigned int duration) {
    auto& state = states[player];
    state.barrier = std::max(state.barrier, std::clamp(value, 0, 10));
    state.barrierTime = std::max(state.barrierTime, duration);
}
void discount(int player, int value, unsigned int duration) {
    auto& state = states[player];
    state.discount = std::max(state.discount, std::clamp(value, 0, 2));
    state.discountTime = std::max(state.discountTime, duration);
}
void feedback(int player, const Slot* triggered = nullptr) {
    if (!authority(player)) return;
    const auto build = DelveBuilds::activeSelection(player);
    const auto& state = states[player];
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
        const auto definition = equipped(player, slotIndex);
        if (!definition) continue;
        const auto& slot = state.slots[slotIndex];
        unsigned int remaining = slot.charges || slot.stacks ? slot.window : 0;
        if (definition->reward == Reward::Barrier && state.barrier > 0) remaining = std::max(remaining, state.barrierTime);
        if (definition->reward == Reward::Haste && state.haste > 0) remaining = std::max(remaining, state.hasteTime);
        if (definition->reward == Reward::Discount && state.discount > 0) remaining = std::max(remaining, state.discountTime);
        if (definition->kind == Kind::Stabilize && state.healing > 0) remaining = std::max(remaining, state.healTime + (state.healing - 1) * state.healInterval);
        DelveEffects::passive(player, build[slotIndex], remaining, slot.charges, slot.stacks, slot.cooldown, triggered == &slot);
    }
}
void reward(int player, Slot& slot, const Rule& definition, int overrideValue = -1) {
    int value = overrideValue >= 0 ? overrideValue : definition.value;
    auto& state = states[player];
    switch (definition.reward) {
        case Reward::Barrier: barrier(player, value, definition.duration); break;
        case Reward::Haste:
            state.haste = std::max(state.haste, std::clamp(value, 0, 10));
            state.hasteTime = std::max(state.hasteTime, static_cast<unsigned int>(definition.duration)); break;
        case Reward::Heal: players[player]->entity->modHP(std::clamp(value, 0, 6)); break;
        case Reward::Discount: discount(player, value, definition.duration); break;
        case Reward::Physical: case Reward::Magic:
            slot.charges = 1; slot.window = definition.duration; break;
        case Reward::Recovery:
            DelveBuilds::reduceCooldown(player, definition.filter, std::clamp(value, 0, 2) * TICKS_PER_SECOND); break;
    }
    slot.cooldown = definition.cooldown;
    feedback(player, &slot);
}
bool firstHit(State& state, unsigned int token, unsigned int target) {
    std::array<unsigned int, 3> key{{token, target, static_cast<unsigned int>(current.channel)}};
    if (std::find(state.hits.begin(), state.hits.end(), key) != state.hits.end()) return false;
    state.hits.push_back(key);
    if (state.hits.size() > 128) state.hits.pop_front();
    return true;
}
bool refund(int player, unsigned int token, int paid, int value) {
    auto& state = states[player];
    if (!paid || !token || std::find(state.refunds.begin(), state.refunds.end(), token) != state.refunds.end()) return false;
    state.refunds.push_back(token);
    if (state.refunds.size() > 128) state.refunds.pop_front();
    players[player]->entity->modMP(std::min(paid, value));
    return true;
}
void invisibilityEnded(int player) {
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
        auto definition = equipped(player, slotIndex);
        auto& slot = states[player].slots[slotIndex];
        if (definition && definition->kind == Kind::Ambush && !slot.cooldown) reward(player, slot, *definition);
    }
}
int damageBonus(Entity& source, Entity& target, const Rule& definition) {
    int value = definition.value;
    auto targetStats = target.getStats();
    if (definition.reward == Reward::Magic) {
        if (target.getReflection() > 0) return 0;
        value = std::max(1, value / (1 + std::max(0, Entity::getMagicResistance(targetStats))));
    } else value = std::max(1, value - std::max(0, AC(targetStats)) / 2);
    double race = Entity::getDamageTableMultiplier(&target, *targetStats,
        definition.reward == Reward::Magic ? DAMAGE_TABLE_MAGIC : DAMAGE_TABLE_SWORD, nullptr, nullptr, &source);
    return std::max(1, static_cast<int>(value * race));
}
int outgoing(int player, Entity& source, Entity& target) {
    int bonus = 0;
    bool melee = current.channel == Channel::Melee;
    bool physical = melee || current.channel == Channel::Ranged || current.channel == Channel::Thrown || current.physical;
    bool spell = current.channel == Channel::Spell || current.channel == Channel::Ability;
    auto& state = states[player];
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
        auto definition = equipped(player, slotIndex);
        if (!definition) continue;
        auto& slot = state.slots[slotIndex];
        if (definition->kind == Kind::ControlledKill) slot.target = vulnerable(target) ? target.getUID() : 0;
        bool opening = definition->kind == Kind::PatientStrike && state.quietTime >= 10 * TICKS_PER_SECOND;
        bool execution = definition->kind == Kind::Execution && target.getStats()->HP * 10 < target.getStats()->MAXHP * 3;
        bool illusion = definition->kind == Kind::ControlledStrike
            && (target.getStats()->getEffectActive(EFF_CONFUSED) || target.getStats()->getEffectActive(EFF_FEAR));
        if (melee && !slot.cooldown && (opening || execution || illusion)) {
            bonus += damageBonus(source, target, *definition); slot.cooldown = definition->cooldown;
        }
        bool consume = slot.charges && (melee || (definition->kind == Kind::Ambush && definition->filter == 1 && current.channel == Channel::Thrown));
        if (definition->kind == Kind::UnarmedCounter) consume = slot.charges && melee && !source.getStats()->weapon;
        if (definition->kind == Kind::WoundedBlock || definition->kind == Kind::DuelCounter)
            consume = slot.charges && melee && slot.target == target.getUID();
        if (definition->kind == Kind::SpellMark) consume = slot.charges && current.channel == Channel::Ranged;
        if (definition->kind == Kind::Recipe) consume = slot.charges && current.channel == Channel::Potion;
        if (definition->kind == Kind::Shatter) consume = slot.charges && physical && slot.target == target.getUID();
        if (definition->kind == Kind::Cinder) consume = slot.charges && spell && slot.target == target.getUID() && slot.category != current.detail;
        if (definition->kind == Kind::ComboSwitch) consume = slot.stacks >= 2 && slot.window && melee && slot.target != target.getUID();
        if (definition->kind == Kind::AbilityMark || definition->kind == Kind::ThrowMark)
            consume = slot.charges && slot.target == target.getUID() && (melee || (DelveBuilds::activeClass(player) == CLASS_MACHINIST && current.channel == Channel::Ranged) || (DelveBuilds::activeClass(player) == CLASS_HUNTER && current.channel == Channel::Ranged));
        if (!consume || definition->kind == Kind::SpendReserve || definition->kind == Kind::Distill || definition->kind == Kind::CacheEscape || definition->kind == Kind::Guard) continue;
        slot.charges = 0; slot.stacks = 0; slot.window = 0;
        feedback(player, &slot);
        if (definition->reward == Reward::Physical || definition->reward == Reward::Magic) bonus += damageBonus(source, target, *definition);
        else if (definition->kind == Kind::RecoveryStrike) refund(player, slot.token, slot.cost, definition->value);
        else reward(player, slot, *definition);
        if (definition->kind == Kind::Spellblade) discount(player, 2, extendedWindow(8));
        if (definition->kind == Kind::Shatter || definition->kind == Kind::Cinder || definition->kind == Kind::ThrowMark || definition->kind == Kind::AbilityMark)
            slot.cooldown = definition->cooldown;
    }
    return std::min(12, bonus);
}
void cast(int player) {
    auto& state = states[player];
    if (current.paid > 0 && state.discount > 0 && (current.channel == Channel::Spell || current.channel == Channel::Ability)) {
        int refunded = std::min(current.paid, state.discount);
        players[player]->entity->modMP(refunded);
        current.paid -= refunded; state.discount = 0; state.discountTime = 0;
    }
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
        auto definition = equipped(player, slotIndex);
        if (!definition) continue;
        auto& slot = state.slots[slotIndex];
        if (definition->kind == Kind::SpendReserve) {
            if (!slot.charges) { slot.stacks = std::min(10, slot.stacks + current.paid); if (slot.stacks >= 10) slot.charges = 1; }
            if (slot.charges && abilityMatches(*definition, current.detail)) {
                slot.charges = slot.stacks = 0; reward(player, slot, *definition);
            }
            continue;
        }
        if (slot.cooldown) continue;
        if (definition->kind == Kind::SummonEcho && current.paid > 0 && damaging(current.detail)) {
            slot.stacks = 1; slot.window = extendedWindow(5);
        }
        if (definition->kind == Kind::Refrain && DelveBuilds::isAbilitySpell(current.detail)) {
            if (slot.window && slot.stacks == 2 && slot.category != current.detail) {
                reward(player, slot, *definition); slot.stacks = 0; slot.window = 0;
            } else { slot.category = current.detail; slot.stacks = 1; slot.window = extendedWindow(8); }
        }
        if ((definition->kind == Kind::Spellblade && current.paid > 0 && damaging(current.detail))
            || (definition->kind == Kind::SpellMark && current.paid > 0)) reward(player, slot, *definition);
        if (definition->kind == Kind::SummonCast && current.paid > 0 && current.detail == SPELL_SUMMON) reward(player, slot, *definition);
        if ((definition->kind == Kind::Ability || definition->kind == Kind::Recipe) && abilityMatches(*definition, current.detail)) reward(player, slot, *definition);
        if (definition->kind == Kind::AbilityWeave && DelveBuilds::isAbilitySpell(current.detail)) {
            if (slot.window && slot.category != current.detail) {
                reward(player, slot, *definition); slot.category = -1;
            } else { slot.category = current.detail; slot.window = extendedWindow(8); }
        }
    }
}
void hitEvent(int player, Entity& source, Entity& target) {
    auto& state = states[player];
    state.quietTime = 0;
    bool melee = current.channel == Channel::Melee;
    bool spell = current.channel == Channel::Spell || current.channel == Channel::Ability;
    bool summoned = source.behavior == &actMonster;
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
        auto definition = equipped(player, slotIndex);
        if (!definition) continue;
        auto& slot = state.slots[slotIndex];
        if (definition->kind == Kind::SummonBond) {
            int category = summoned ? 1 : 0;
            if (!slot.cooldown && slot.window && slot.target == target.getUID() && slot.category != category) reward(player, slot, *definition);
            slot.target = target.getUID(); slot.category = category; slot.window = extendedWindow(5);
        }
        if (definition->kind == Kind::SummonEcho && summoned && !slot.cooldown && slot.window && slot.stacks) {
            slot.stacks = 0; slot.window = 0; reward(player, slot, *definition);
        }
        if (summoned || slot.cooldown) continue;
        if (definition->kind == Kind::ThrownRear && current.channel == Channel::Thrown && rear(source, target)) reward(player, slot, *definition);
        if (definition->kind == Kind::BloodHunt && melee && source.getStats()->HP * 2 < source.getStats()->MAXHP) reward(player, slot, *definition);
        if (definition->kind == Kind::Refrain && melee && slot.window && slot.stacks == 1) slot.stacks = 2;
        if (definition->kind == Kind::Crossfire) {
            if (current.channel == Channel::Ranged && slot.window && slot.target == target.getUID()) {
                slot.window = 0; slot.target = 0; reward(player, slot, *definition);
            } else if (spell) { slot.target = target.getUID(); slot.window = extendedWindow(5); }
        }
        if (definition->kind == Kind::Variety && current.channel >= Channel::Melee && current.channel <= Channel::Ability) {
            if (!slot.window) { slot.category = 0; slot.stacks = 0; slot.window = extendedWindow(5); }
            int channelBit = 1 << static_cast<int>(current.channel);
            if (!(slot.category & channelBit)) { slot.category |= channelBit; ++slot.stacks; }
            if (slot.stacks >= 3) { slot.stacks = 0; slot.window = 0; reward(player, slot, *definition); }
        }
        if ((definition->kind == Kind::RearHaste || definition->kind == Kind::Rear) && melee && rear(source, target)) reward(player, slot, *definition);
        if ((definition->kind == Kind::Ranged && current.channel == Channel::Ranged) || (definition->kind == Kind::Melee && melee)
            || (definition->kind == Kind::PotionBarrier && current.channel == Channel::Potion)) reward(player, slot, *definition);
        if (definition->kind == Kind::MeleeCombo && melee && (definition->filter != 1 || !source.getStats()->weapon)) {
            if (++slot.stacks >= 3) { slot.stacks = 0; reward(player, slot, *definition); }
            else slot.window = extendedWindow(5);
        }
        if (definition->kind == Kind::ComboSwitch && melee) {
            slot.stacks = slot.window && slot.target == target.getUID() ? std::min(2, slot.stacks + 1) : 1;
            slot.target = target.getUID(); slot.window = definition->duration;
        }
        if (definition->kind == Kind::WeaponWeave && melee) {
            int category = source.getStats()->weapon ? getWeaponSkill(source.getStats()->weapon) : PRO_UNARMED;
            if (slot.window && slot.category != category) reward(player, slot, *definition);
            slot.category = category; slot.window = extendedWindow(5);
        }
        if (definition->kind == Kind::Cadence && spell) {
            if (slot.window && slot.category != current.detail && refund(player, current.token, current.paid, definition->value)) slot.cooldown = definition->cooldown;
            slot.category = current.detail; slot.window = definition->duration;
        }
        if (definition->kind == Kind::AbilityMark && abilityMatches(*definition, current.detail)) {
            slot.target = target.getUID(); slot.charges = 1; slot.window = definition->duration;
        }
        if (definition->kind == Kind::ThrowMark && current.channel == Channel::Thrown) {
            slot.target = target.getUID(); slot.charges = 1; slot.window = definition->duration;
        }
        bool cold = spell && (current.detail == SPELL_COLD || (DelveBuilds::isAbilitySpell(current.detail) &&
            ((DelveBuilds::activeClass(player) == CLASS_WIZARD && (current.detail - 600) % 4 == 1)
            || (DelveBuilds::activeClass(player) == CLASS_MESMER && (current.detail - 600) % 4 == 2))));
        bool fire = spell && (current.detail == SPELL_FIREBALL || (DelveBuilds::activeClass(player) == CLASS_WIZARD && current.detail == 600 + CLASS_WIZARD * 4));
        if ((definition->kind == Kind::Shatter && cold) || (definition->kind == Kind::Cinder && fire)) {
            slot.target = target.getUID(); slot.category = current.detail; slot.charges = 1; slot.window = definition->duration;
        }
    }
}
void healEvent(int player, int requested, int restored) {
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
        auto definition = equipped(player, slotIndex);
        if (!definition) continue;
        auto& slot = states[player].slots[slotIndex];
        if (definition->kind == Kind::Overflow && requested > restored) {
            barrier(player, std::min(definition->value, requested - restored), definition->duration);
            feedback(player, &slot);
        }
        if (slot.cooldown) continue;
        if (definition->kind == Kind::HealHaste && restored >= 5) reward(player, slot, *definition);
        if ((definition->kind == Kind::RecoveryStrike || definition->kind == Kind::HealingOath) && restored > 0 && current.paid > 0) {
            slot.charges = 1; slot.window = definition->duration; slot.cost = current.paid; slot.token = current.token; slot.cooldown = definition->cooldown;
        }
    }
}
void sendState(int player) {
#ifdef BARONY_SMOKE_TESTS
    if (auditing) return;
#endif
    if (player < 1 || multiplayer != SERVER || client_disconnected[player] || !players[player] || players[player]->isLocalPlayer()) return;
    auto& state = states[player];
    std::memcpy(net_packet->data, "DPST", 4); SDLNet_Write32(uniqueGameKey, net_packet->data + 4);
    net_packet->data[8] = player; SDLNet_Write32(++state.serial, net_packet->data + 9);
    SDLNet_Write16(state.barrier, net_packet->data + 13); SDLNet_Write16(state.barrierTime, net_packet->data + 15);
    SDLNet_Write16(state.hasteTime, net_packet->data + 17); SDLNet_Write16(state.discount, net_packet->data + 19);
    SDLNet_Write16(state.discountTime, net_packet->data + 21);
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
        int offset = 23 + slotIndex * 8; auto& slot = state.slots[slotIndex];
        SDLNet_Write16(slot.cooldown, net_packet->data + offset); SDLNet_Write16(slot.window, net_packet->data + offset + 2);
        SDLNet_Write16(slot.charges, net_packet->data + offset + 4); SDLNet_Write16(slot.stacks, net_packet->data + offset + 6);
    }
    net_packet->data[39] = state.haste; net_packet->len = 40; net_packet->address = net_clients[player - 1];
    sendPacketSafe(net_sock, -1, net_packet, player - 1);
}
}
const Rule& rule(int characterClass, int index) { return catalogue[std::clamp(characterClass, 0, 25) * 6 + std::clamp(index, 0, 5)]; }
Scope::Scope(Entity* source, Channel channel, int detail, unsigned int token, int paid, bool physical) : previous(current) {
    actor = source ? source->getUID() : 0;
    current = {}; current.source = actor; current.channel = channel; current.detail = detail;
    current.token = token ? token : ++sequence; current.paid = std::max(0, paid); current.physical = physical;
    current.startingMP = source && source->getStats() ? source->getStats()->MP : 0;
    beforeEffects = removable(source);
    current.invisibleAtStart = source && source->getStats() && source->getStats()->getEffectActive(EFF_INVISIBLE);
}
Scope::~Scope() {
    auto source = uidToEntity(actor); int player = owner(source);
    if (completed && !processing && authority(player) && (beforeEffects & ~removable(source))) {
        processing = true;
        for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
            auto definition = equipped(player, slotIndex); auto& slot = states[player].slots[slotIndex];
            if (definition && definition->kind == Kind::CleanseBarrier && !slot.cooldown) reward(player, slot, *definition);
        }
        processing = false;
    }
    feedback(player);
    current = previous;
}
void Scope::commit() {
    if (completed) return; completed = true;
    auto source = uidToEntity(actor); int player = owner(source);
    if (processing || !authority(player)) return;
    processing = true;
    if (current.channel == Channel::Spell || current.channel == Channel::Ability) cast(player);
    if (current.channel == Channel::Drink && beneficial(current.detail)) {
        for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
            auto definition = equipped(player, slotIndex); auto& slot = states[player].slots[slotIndex];
            if (!definition || slot.cooldown) continue;
            if (definition->kind == Kind::DrinkCharge) reward(player, slot, *definition);
            if (definition->kind == Kind::CleanDrink) {
                const bool cleansed = source->getStats()->getEffectActive(EFF_POISONED) || source->getStats()->getEffectActive(EFF_BLEEDING);
                source->setEffect(EFF_POISONED, false, 0, true); source->setEffect(EFF_BLEEDING, false, 0, true); slot.cooldown = definition->cooldown;
                if (cleansed) feedback(player, &slot);
            }
            if (definition->kind == Kind::Dressing && (current.detail == POTION_HEALING || current.detail == POTION_EXTRAHEALING)) {
                source->setEffect(EFF_BLEEDING, false, 0, true); reward(player, slot, *definition);
            }
            if (definition->kind == Kind::DrinkWeave) {
                if (slot.window && slot.category != current.detail) reward(player, slot, *definition);
                slot.category = current.detail; slot.window = extendedWindow(8);
            }
            if (definition->kind == Kind::Distill && slot.charges) {
                bool healing = current.detail == POTION_HEALING || current.detail == POTION_EXTRAHEALING;
                if (healing || current.detail == POTION_RESTOREMAGIC) {
                    slot.charges = 0;
                    if (healing) source->modHP(4); else source->modMP(2);
                }
            }
        }
    }
    processing = false;
}
unsigned int Scope::token() const { return current.token; }
int Scope::paid() const { return current.paid; }
void reset() { states = {}; current = {}; sequence = 0; lastSend = ticks; }
void resetPlayer(int player) {
    if (player < 0 || player >= MAXPLAYERS) return;
    const auto serial = states[player].serial;
    const auto received = states[player].received;
    states[player] = State{};
    states[player].serial = serial;
    states[player].received = received;
}
int modifyHealth(Entity& target, int amount) {
    int player = target.behavior == &actPlayer ? target.skill[2] : -1;
    if (!Raid::active() || multiplayer != SERVER || !target.getStats() || target.getStats()->HP <= 0) return amount;
    if (processing) return amount;
    auto source = uidToEntity(current.source); int attacker = owner(source);
    if (amount > 0) {
        if (authority(attacker) && source->behavior == &actPlayer
            && (current.channel == Channel::Spell || current.channel == Channel::Ability || current.channel == Channel::Drink))
            amount += amount * DelveRarity::total(*stats[attacker], DelveAffixes::Kind::HealingPower) / 100;
        return amount;
    }
    if (!amount) return amount;
    current.eligible = false;
    if (hostile(source, target)) {
        processing = true;
        if (authority(attacker) && firstHit(states[attacker], current.token, target.getUID())) {
            current.eligible = true;
            if (source->behavior == &actPlayer && current.channel == Channel::Spell) {
                if (auto focus = DelveRarity::focus(*stats[attacker])) amount -= DelveRarity::bonus(*focus);
            }
            if (current.channel == Channel::Melee && current.invisibleAtStart) invisibilityEnded(attacker);
            if (source->behavior == &actPlayer) amount -= outgoing(attacker, *source, target);
            if (source->behavior == &actPlayer) {
                using DelveAffixes::Kind;
                int percent = 0;
                const bool physical = current.channel == Channel::Melee || current.channel == Channel::Ranged
                    || current.channel == Channel::Thrown || (current.channel == Channel::Ability && current.physical);
                if (physical && rear(*source, target)) percent += DelveRarity::total(*stats[attacker], Kind::Backstab);
                switch (target.getStats()->type) {
                    case SKELETON: case GHOUL: case VAMPIRE: case LICH: case LICH_FIRE: case LICH_ICE:
                        percent += DelveRarity::total(*stats[attacker], Kind::UndeadSlayer); break;
                    case DEMON: case CREATURE_IMP: case SUCCUBUS: case INCUBUS: case DEVIL:
                        percent += DelveRarity::total(*stats[attacker], Kind::DemonSlayer); break;
                    default: break;
                }
                amount -= (-amount) * percent / 100;
            }
        }
        if (authority(player)) {
            using DelveAffixes::Kind;
            auto reduce = [&](int percent) { amount = -std::max(1, ((-amount) * (100 - percent) + 99) / 100); };
            if (current.channel == Channel::Ranged || current.channel == Channel::Thrown)
                reduce(DelveRarity::total(*stats[player], Kind::ProjectileResistance));
            const bool magical = current.channel == Channel::Spell || current.channel == Channel::Potion
                || (current.channel == Channel::Ability && !current.physical);
            if (magical) reduce(DelveRarity::total(*stats[player], Kind::MagicResistance));
            else if (stats[player]->defending && stats[player]->shield && checkEquipType(stats[player]->shield) == TYPE_SHIELD)
                reduce(DelveRarity::total(*stats[player], Kind::BlockPower));
            if (DelveRarity::ward(*stats[player])) amount = -std::max(1, (-amount * 85 + 99) / 100);
            auto& state = states[player];
            for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
                auto definition = equipped(player, slotIndex); auto& slot = state.slots[slotIndex];
                if (definition && definition->kind == DelvePassives::Kind::Guard && slot.charges) {
                    int absorbed = std::min(-amount, definition->value); amount += absorbed;
                    slot.charges = 0; slot.cooldown = definition->cooldown; state.guardSpent = true;
                }
            }
        }
        processing = false;
    }
    if (authority(player) && states[player].barrier > 0) {
        int absorbed = std::min(-amount, states[player].barrier); states[player].barrier -= absorbed; amount += absorbed;
    }
    return amount;
}
void healthChanged(Entity& target, int requested, int previousHP) {
    if (!Raid::active() || multiplayer != SERVER || processing || !target.getStats()) return;
    auto source = uidToEntity(current.source); int player = owner(source);
    int victim = target.behavior == &actPlayer ? target.skill[2] : -1;
    int damage = previousHP - target.getStats()->HP;
    processing = true;
    if (damage > 0 && source && source != &target && source->checkEnemy(&target)) {
        if (authority(player) && current.eligible && source->behavior == &actPlayer && !states[player].siphonCooldown) {
            const Item* item = current.channel == Channel::Spell ? DelveRarity::focus(*stats[player]) : stats[player]->weapon;
            if (current.channel == Channel::Spell && item && itemCategory(item) == SPELLBOOK) {
                item = stats[player]->weapon && itemCategory(stats[player]->weapon) == MAGICSTAFF ? stats[player]->weapon : nullptr;
            }
            if ((DelveSpellRolls::activeLegendary(source) || (item && item->status != BROKEN && DelveRarity::tier(*item) == DelveRarity::Tier::Legendary
                && (DelveRarity::magic(*item) || itemCategory(item) == WEAPON || itemCategory(item) == THROWN || DelveRarity::attackBonus(*item))))
                && (current.channel == Channel::Melee || current.channel == Channel::Ranged || current.channel == Channel::Thrown || current.channel == Channel::Spell)) {
                states[player].siphonCooldown = TICKS_PER_SECOND;
                source->modHP(4); source->modMP(2);
            }
        }
        if (authority(player) && current.eligible) hitEvent(player, *source, target);
        if (target.getStats()->HP <= 0 && authority(player)) killed(*source, target);
        if (authority(victim)) {
            auto& state = states[victim]; state.quietTime = 0;
            for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
                auto definition = equipped(victim, slotIndex); auto& slot = state.slots[slotIndex];
                if (!definition || slot.cooldown) continue;
                if (definition->kind == Kind::CacheEscape && slot.charges) { slot.charges = 0; reward(victim, slot, *definition); }
                if (definition->kind == Kind::ReactiveFrost && current.channel == Channel::Melee) {
                    Raid::applyControl(*source, &target, EFF_SLOW, definition->duration, true); slot.cooldown = definition->cooldown;
                }
                if (definition->kind == Kind::Stabilize && previousHP * 10 >= target.getStats()->MAXHP * 3 && target.getStats()->HP * 10 < target.getStats()->MAXHP * 3) {
                    state.healing = definition->value;
                    state.healInterval = std::max(1, definition->duration / std::max(1, definition->value));
                    state.healTime = definition->duration - state.healInterval * (definition->value - 1);
                    slot.cooldown = definition->cooldown;
                }
            }
        }
    }
    if (requested > 0 && source == &target && authority(player) && (current.channel == Channel::Spell || current.channel == Channel::Ability || current.channel == Channel::Drink))
        healEvent(player, requested, std::max(0, target.getStats()->HP - previousHP));
    processing = false;
}
void blocked(Entity& defender, Entity& attacker, int prevented) {
    int player = owner(&defender);
    if (processing || !authority(player) || prevented <= 0 || !attacker.checkEnemy(&defender)) return;
    processing = true;
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
        auto definition = equipped(player, slotIndex); auto& slot = states[player].slots[slotIndex];
        if (!definition || slot.cooldown) continue;
        bool wounded = definition->kind == Kind::WoundedBlock && defender.getStats()->HP * 2 < defender.getStats()->MAXHP;
        bool unarmed = definition->kind == Kind::UnarmedCounter && !defender.getStats()->weapon;
        if (definition->kind == Kind::BlockCharge || definition->kind == Kind::BlockHaste
            || definition->kind == Kind::DuelCounter || wounded || unarmed) {
            reward(player, slot, *definition);
            if (definition->kind == Kind::WoundedBlock || definition->kind == Kind::DuelCounter) slot.target = attacker.getUID();
        }
    }
    processing = false;
}
void openedCache(Entity& chest, int player) {
    if (!authority(player) || !states[player].caches.insert(chest.getUID()).second) return;
    processing = true;
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
        auto definition = equipped(player, slotIndex); auto& slot = states[player].slots[slotIndex];
        if (!definition) continue;
        if (definition->kind == Kind::Distill && !slot.charges && states[player].boosters < 3) { slot.charges = 1; ++states[player].boosters; }
        if (definition->kind == Kind::CacheEscape && !slot.cooldown) slot.charges = 1;
        if (definition->kind == Kind::Cache && !slot.cooldown) reward(player, slot, *definition);
    }
    feedback(player);
    processing = false;
}
void killed(Entity& killer, Entity& victim) {
    int player = owner(&killer);
    if (!authority(player) || !states[player].kills.insert(victim.getUID()).second || !killer.checkEnemy(&victim)) return;
    if (killer.behavior == &actPlayer) {
        killer.modHP(DelveRarity::total(*stats[player], DelveAffixes::Kind::LifeOnKill));
        killer.modMP(DelveRarity::total(*stats[player], DelveAffixes::Kind::ManaOnKill));
    }
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
        auto definition = equipped(player, slotIndex); auto& slot = states[player].slots[slotIndex];
        if (!definition || slot.cooldown || killer.behavior != &actPlayer) continue;
        if (definition->kind == Kind::Kill
            || (definition->kind == Kind::ControlledKill && slot.target == victim.getUID())
            || (definition->kind == Kind::RangedKill && current.channel == Channel::Ranged)) reward(player, slot, *definition);
    }
}
bool unlockChest(Entity& chest, int player) {
    if (!authority(player) || !chest.chestLocked || chest.skill[20] >= 4 || std::hypot(chest.x - players[player]->entity->x, chest.y - players[player]->entity->y) > TOUCHRANGE) return false;
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
        auto definition = equipped(player, slotIndex);
        if (definition && definition->kind == Kind::Unlock) { chest.chestLocked = 0; feedback(player, &states[player].slots[slotIndex]); return true; }
    }
    return false;
}
bool disarmTrap(Entity& trap) {
    if (multiplayer != SERVER || trap.behavior != &actSpearTrap || trap.actTrapSabotaged) return false;
    for (int player = 0; player < MAXPLAYERS; ++player) {
        if (!authority(player) || !inrange[player] || (selectedEntity[player] != &trap && client_selected[player] != &trap)) continue;
        for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
            auto definition = equipped(player, slotIndex);
            if (definition && definition->kind == Kind::TrapSense) {
                trap.actTrapSabotaged = 1; serverUpdateEntitySkill(&trap, 30);
                feedback(player, &states[player].slots[slotIndex]);
                messagePlayer(player, MESSAGE_MISC, "Trap disarmed."); return true;
            }
        }
    }
    return false;
}
void hitDoor(Entity& attacker, Entity& door, int charge) {
    int player = owner(&attacker);
    if (!authority(player) || door.behavior != &actDoor || door.doorForceLockedUnlocked == 1 || charge < Stat::getMaxAttackCharge(attacker.getStats())) return;
    processing = true;
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
        auto definition = equipped(player, slotIndex); auto& slot = states[player].slots[slotIndex];
        if (definition && definition->kind == Kind::Breacher && !slot.cooldown) { door.doorHealth = 0; reward(player, slot, *definition); }
    }
    processing = false;
}
double speedMultiplier(int player) {
    if (!activePlayer(player)) return 1.0;
    const double equipment = 1.0 + DelveRarity::total(*stats[player], DelveAffixes::Kind::MoveSpeed) / 100.0;
    return equipment * (states[player].hasteTime > 0 ? 1.0 + states[player].haste / 100.0 : 1.0);
}
void refreshEquipment(int player) {
    if (!authority(player)) return;
    auto& state = states[player];
    auto& actorStats = *stats[player];
    const int health = DelveRarity::total(actorStats, DelveAffixes::Kind::MaxHealth);
    const int mana = DelveRarity::total(actorStats, DelveAffixes::Kind::MaxMana);
    if (health == state.equipmentHealth && mana == state.equipmentMana) return;
    actorStats.MAXHP = std::max(1, actorStats.MAXHP + health - state.equipmentHealth);
    actorStats.MAXMP = std::max(0, actorStats.MAXMP + mana - state.equipmentMana);
    actorStats.HP = std::min(actorStats.HP, actorStats.MAXHP);
    actorStats.MP = std::min(actorStats.MP, actorStats.MAXMP);
    state.equipmentHealth = health; state.equipmentMana = mana;
    serverUpdatePlayerStats();
}
void tick(unsigned int delta) {
    if (!Raid::active() || intro || loading || fadeout || gamePaused || Raid::matchComplete()) return;
    auto decrease = [delta](unsigned int& remaining) { remaining -= std::min(delta, remaining); };
    for (int player = 0; player < MAXPLAYERS; ++player) {
        if (!activePlayer(player)) continue;
        auto& state = states[player];
        if (state.floor != currentlevel) {
            state.floor = currentlevel; state.regions.clear(); state.kills.clear(); state.caches.clear(); state.hits.clear(); state.boosters = 0;
            for (auto& slot : state.slots) {
                if (slot.target) { slot.charges = slot.stacks = 0; slot.window = 0; }
                slot.target = 0; slot.category = -1;
            }
        }
        decrease(state.barrierTime); decrease(state.discountTime); decrease(state.hasteTime);
        decrease(state.siphonCooldown);
        if (!state.barrierTime) state.barrier = 0;
        if (!state.discountTime) state.discount = 0;
        for (auto& slot : state.slots) {
            decrease(slot.cooldown);
            if (slot.window) { decrease(slot.window); if (!slot.window) { slot.charges = slot.stacks = 0; slot.category = -1; slot.target = 0; } }
        }
        if (multiplayer != SERVER) continue;
        refreshEquipment(player);
        processing = true;
        auto entity = players[player]->entity;
        const int healthRegen = DelveRarity::total(*stats[player], DelveAffixes::Kind::HealthRegen);
        const int manaRegen = DelveRarity::total(*stats[player], DelveAffixes::Kind::ManaRegen);
        state.healthRegen = healthRegen ? state.healthRegen + delta * healthRegen : 0;
        state.manaRegen = manaRegen ? state.manaRegen + delta * manaRegen : 0;
        const unsigned int interval = 5 * TICKS_PER_SECOND;
        if (state.healthRegen >= interval) { entity->modHP(state.healthRegen / interval); state.healthRegen %= interval; }
        if (state.manaRegen >= interval) { entity->modMP(state.manaRegen / interval); state.manaRegen %= interval; }
        if (state.invisible && !stats[player]->getEffectActive(EFF_INVISIBLE)) invisibilityEnded(player);
        state.invisible = stats[player]->getEffectActive(EFF_INVISIBLE);
        int effects = removable(entity);
        state.quietTime = std::min<unsigned int>(60 * TICKS_PER_SECOND, state.quietTime + delta);
        bool stationary = std::hypot(entity->x - state.guardX, entity->y - state.guardY) < 0.25;
        if (stats[player]->defending && stationary) state.guardTime += delta;
        else {
            state.guardTime = 0;
            if (!stats[player]->defending) {
                state.guardSpent = false;
                for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
                    auto definition = equipped(player, slotIndex);
                    if (definition && definition->kind == Kind::Guard) state.slots[slotIndex].charges = 0;
                }
            }
        }
        state.guardX = entity->x; state.guardY = entity->y;
        int region = (static_cast<int>(entity->x) / 128) * 1024 + static_cast<int>(entity->y) / 128;
        bool explored = state.regions.insert(region).second;
        for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
            auto definition = equipped(player, slotIndex); auto& slot = state.slots[slotIndex];
            if (!definition || slot.cooldown) continue;
            if (definition->kind == Kind::Guard && state.guardTime >= TICKS_PER_SECOND && !state.guardSpent) slot.charges = 1;
            if (definition->kind == Kind::ControlEnd && (state.effects & ~effects & 0x1F8)) reward(player, slot, *definition);
            if (definition->kind == Kind::Explore && explored) reward(player, slot, *definition);
            if (definition->kind == Kind::Quiet && state.quietTime >= 10 * TICKS_PER_SECOND) reward(player, slot, *definition);
        }
        state.effects = effects;
        if (state.healing > 0) {
            decrease(state.healTime);
            if (!state.healTime) { entity->modHP(1); --state.healing; state.healTime = state.healInterval; }
        }
        processing = false;
    }
    for (int player = 0; player < MAXPLAYERS; ++player) feedback(player);
    if (multiplayer == SERVER && ticks - lastSend >= 10) { lastSend = ticks; for (int player = 1; player < MAXPLAYERS; ++player) sendState(player); }
}
void receiveState() {
    if (!Raid::active() || multiplayer != CLIENT || net_packet->len != 40 || net_packet->address.host != net_server.host
        || net_packet->address.port != net_server.port || SDLNet_Read32(net_packet->data + 4) != uniqueGameKey || net_packet->data[8] != clientnum) return;
    auto& state = states[clientnum]; unsigned int serial = SDLNet_Read32(net_packet->data + 9);
    if (serial <= state.received) return;
    state.received = serial; state.barrier = std::min<int>(10, SDLNet_Read16(net_packet->data + 13));
    state.barrierTime = SDLNet_Read16(net_packet->data + 15); state.hasteTime = SDLNet_Read16(net_packet->data + 17);
    state.discount = std::min<int>(2, SDLNet_Read16(net_packet->data + 19)); state.discountTime = SDLNet_Read16(net_packet->data + 21);
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
        int offset = 23 + slotIndex * 8; auto& slot = state.slots[slotIndex];
        slot.cooldown = SDLNet_Read16(net_packet->data + offset); slot.window = SDLNet_Read16(net_packet->data + offset + 2);
        slot.charges = SDLNet_Read16(net_packet->data + offset + 4); slot.stacks = SDLNet_Read16(net_packet->data + offset + 6);
    }
    state.haste = std::min<int>(10, net_packet->data[39]);
}
void drawHUD(int player) {
    if (!activePlayer(player) || player != clientnum) return;
    bool nearbyTrap = false;
    for (int slotIndex = 0; slotIndex < 2 && !nearbyTrap; ++slotIndex) {
        auto definition = equipped(player, slotIndex);
        if (!definition) continue;
        if (definition->kind == Kind::TrapSense) {
            for (auto node = map.entities->first; node; node = node->next) {
                auto trap = static_cast<Entity*>(node->element);
                if ((trap->behavior != &actSpearTrap && trap->behavior != &actBeartrap) || trap->actTrapSabotaged) continue;
                auto entity = players[player]->entity; double distance = std::hypot(trap->x - entity->x, trap->y - entity->y);
                if (distance > 48) continue;
                auto previousHit = hit;
                lineTraceTarget(entity, entity->x, entity->y, std::atan2(trap->y - entity->y, trap->x - entity->x), distance + 2, 0, false, trap);
                bool visible = hit.entity == trap; hit = previousHit;
                if (visible) { nearbyTrap = true; break; }
            }
        }
    }
    if (nearbyTrap) {
        const DelveHUD::Canvas hud;
        hud.panel(106, 46);
        hud.icon("*#images/ui/HUD/warning_glyph.png", {12, 119, 18, 18}, DelveHUD::danger);
        hud.label(38, 113, 248, "TRAP NEARBY", DelveHUD::danger, "lang/en.ttf#12#1");
        hud.label(38, 131, 248, "Interact nearby to disarm", DelveHUD::muted, "lang/en.ttf#12#1");
    }
}
unsigned int cooldownRemaining(int player, int slot) {
    if (!Raid::active() || player < 0 || player >= MAXPLAYERS || slot < 0 || slot >= 2 || !equipped(player, slot)) return 0;
    return states[player].slots[slot].cooldown;
}
#ifdef BARONY_SMOKE_TESTS
void smokeCooldownHUD(int phase) {
    for (int slot = 0; slot < 2; ++slot) {
        const auto definition = equipped(clientnum, slot);
        states[clientnum].slots[slot].cooldown = definition && phase < 2 ? definition->cooldown / (phase + 1) : 0;
    }
}
bool smokeNetworkConfirmed() {
    const auto& state = states[clientnum];
    return state.received > 0 && state.slots[0].charges == 1 && state.hasteTime > 0;
}
#include "smoke/DelvePassiveAudit.inc"
#endif
}
