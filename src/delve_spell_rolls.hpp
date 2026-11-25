#pragma once
#include "delve_spell_rules.hpp"
#include <string>
class Item;
class Entity;
struct spell_t;
namespace DelveSpellRolls {
using DelveSpellRules::Imprint;
using DelveSpellRules::Kind;
Imprint fromBook(const Item& book);
bool valid(const Imprint& book, int spellID);
std::string encode(const Imprint& book);
Imprint decode(const std::string& text);
std::string summary(const Imprint& book);
std::string summary(const Item& book);
// Keeps learned spell pointers, hotbars and in-flight spell copies stable.
bool replace(int player, const Item& book);
void learned(int player, const Item& book);
int value(Entity* caster, const spell_t* spell, Kind kind, bool usingBook = false);
int activeValue(Entity* caster, Kind kind);
int activePower();
bool activeLegendary(Entity* caster);
class Scope {
public:
    Scope(Entity* caster, const spell_t* spell);
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
private:
    Entity* previousCaster;
    const spell_t* previousSpell;
};
}
