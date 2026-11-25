#pragma once
#include <array>
class Entity;
namespace DelveBuilds {
struct Proficiency { int id = -1, amount = 0; };
struct Effect { int id = -1, ticks = 0; };
struct Ability {
    int cooldown = 0, manaCost = 0, hpCost = 0, heal = 0, mana = 0, damage = 0;
    int range = 0, arc = 0, cleanse = 0, leech = 0;
    bool physical = false;
    int control = -1, controlTicks = 0;
    std::array<Effect,2> self{};
};
struct Option {
    const char* name; const char* description; bool passive;
    std::array<int,8> stats{};
    std::array<Proficiency,2> proficiencies{};
    Ability ability{};
};
using Build = std::array<int,4>;
const Option& option(int characterClass, int index);
Build selection(int player, int characterClass);
bool setSelection(int player, int characterClass, const Build& build, bool persist = true);
bool validBuild(int characterClass, const Build& build);
Build activeSelection(int player);
int activeClass(int player);
void reduceCooldown(int player, int abilityIndex, unsigned int ticks);
unsigned int cooldownRemaining(int player, int slot);
void drawHUD(int player);
void reset();
void deployed(int player);
void redeploy(int player);
void equipmentReady(int player);
void tick();
bool activate(int player, int slot);
bool isAbilitySpell(int spellId);
bool skillPenaltiesDisabled(int spellId = -1);
const Option* spellOption(int spellId);
int spellIcon(int spellId);
int abilityIcon(int player, int slot);
bool castAbilitySpell(int player, int spellId);
void requestActivation(int slot);
void receiveBuild();
void receiveActivation();
void receiveState();
#ifdef BARONY_SMOKE_TESTS
void smokeEquip(int player, int characterClass, int firstPerk, int secondPerk = -1);
bool smokeAudit(Entity& host, Entity& guest);
bool smokeNetworkConfirmed();
bool smokeReady();
void smokeCooldownHUD(int phase);
bool smokePresentAbility(int characterClass, int index);
#endif
}
