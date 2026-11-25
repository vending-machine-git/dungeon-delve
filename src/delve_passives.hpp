#pragma once
#include <array>
#include <functional>
#include <string>

class Entity;
namespace DelvePassives {
enum class Kind {
    BlockCharge, BlockHaste, ComboSwitch, WeaponWeave, Guard, AbilityMark,
    Ambush, RearHaste, TrapSense, Unlock, ThrowMark, CacheEscape,
    Shatter, Cadence, Cinder, Spellblade, ReactiveFrost, SpendReserve,
    Overflow, HealHaste, CleanseBarrier, RecoveryStrike, Dressing, Stabilize,
    DrinkCharge, PotionBarrier, CleanDrink, DrinkWeave, Recipe, Distill,
    Kill, Cache, Ability, ControlEnd, MeleeCombo, AbilityWeave, Explore,
    Quiet, Breacher, Rear, Ranged, Melee, SpellMark, SummonBond, SummonCast, Door,
    WoundedBlock, Crossfire, Variety, ControlledKill, ThrownRear, UnarmedCounter,
    SummonEcho, BloodHunt, ControlledStrike, RangedKill, Refrain, Execution,
    PatientStrike, HealingOath, DuelCounter
};
enum class Reward { Barrier, Haste, Heal, Discount, Physical, Magic, Recovery };
enum class Channel { None, Melee, Ranged, Thrown, Potion, Spell, Ability, Drink };
struct Rule {
    Kind kind;
    Reward reward;
    int value, duration, cooldown, filter;
};
struct Action {
    unsigned int source = 0, token = 0;
    Channel channel = Channel::None;
    int detail = -1, paid = 0, startingMP = 0, removed = 0;
    bool physical = false, eligible = false, invisibleAtStart = false;
};
class Scope {
public:
    Scope(Entity* source, Channel channel, int detail = -1, unsigned int token = 0, int paid = 0, bool physical = false);
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    void commit();
    unsigned int token() const;
    int paid() const;
private:
    Action previous;
    unsigned int actor = 0;
    int beforeEffects = 0;
    bool completed = false;
};
const Rule& rule(int characterClass, int index);
void reset();
void resetPlayer(int player);
void refreshEquipment(int player);
void tick(unsigned int delta);
void drawHUD(int player);
unsigned int cooldownRemaining(int player, int slot);
void receiveState();
int modifyHealth(Entity& target, int amount);
void healthChanged(Entity& target, int requested, int previousHP);
void blocked(Entity& defender, Entity& attacker, int prevented);
void openedCache(Entity& chest, int player);
void killed(Entity& killer, Entity& victim);
bool unlockChest(Entity& chest, int player);
bool disarmTrap(Entity& trap);
void hitDoor(Entity& attacker, Entity& door, int charge);
double speedMultiplier(int player);
#ifdef BARONY_SMOKE_TESTS
bool smokeNetworkConfirmed();
void smokeCooldownHUD(int phase);
void smokeAudit(Entity& host, Entity& guest, const std::function<void(const std::string&, bool)>& check);
#endif
}
