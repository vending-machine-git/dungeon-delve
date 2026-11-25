#include "main.hpp"
#include "entity.hpp"
#include "player.hpp"
#include "net.hpp"
#include "magic/magic.hpp"
#include "delve_spell_rolls.hpp"
#include "delve_rarity.hpp"
#include "interface/interface.hpp"
#include <sstream>

namespace DelveSpellRolls {
namespace { Entity* currentCaster = nullptr; const spell_t* currentSpell = nullptr; }
Imprint fromBook(const Item& book) {
    if (!Raid::active() || itemCategory(&book) != SPELLBOOK || book.status == BROKEN) return {};
    return {static_cast<int>(book.type), static_cast<unsigned char>(DelveRarity::tier(book)),
        static_cast<unsigned char>(DelveRarity::seed(book)), book.beatitude > 0};
}
bool valid(const Imprint& book, int spellID) {
    return book.bookType >= 0 && book.bookType < RMF_FIRST_ITEM && book.tier <= 4
        && items[book.bookType].category == SPELLBOOK
        && getSpellIDFromSpellbook(static_cast<ItemType>(book.bookType)) == spellID;
}
std::string encode(const Imprint& book) {
    return std::to_string(book.bookType) + " " + std::to_string(book.tier) + " "
        + std::to_string(book.seed) + " " + std::to_string(book.blessed);
}
Imprint decode(const std::string& text) {
    int type = -1, tier = -1, seed = -1, blessed = -1;
    std::istringstream stream(text);
    if (!(stream >> type >> tier >> seed >> blessed) || tier < 0 || tier > 4 || seed < 0 || seed > 255
        || blessed < 0 || blessed > 1) return {};
    stream >> std::ws;
    if (!stream.eof()) return {};
    return {type, static_cast<unsigned char>(tier), static_cast<unsigned char>(seed), blessed != 0};
}
std::string summary(const Imprint& book) {
    if (!Raid::active() || book.bookType < 0) return {};
    static const char* names[] = {"spell power (damage / healing / beneficial duration)", "casting speed", "spell mana efficiency"};
    std::string result;
    for (const auto& roll : DelveSpellRules::generate(book)) {
        if (roll.kind == Kind::Count) continue;
        if (!result.empty()) result += '\n';
        result += "+" + std::to_string(roll.value) + "% " + names[static_cast<int>(roll.kind)];
    }
    if (book.tier == 4) result += (result.empty() ? "" : "\n") + std::string("Siphon: +4 HP / +2 MP on hit (1s cooldown)");
    return result;
}
std::string summary(const Item& book) {
    auto result = summary(fromBook(book));
    if (!result.empty()) result += "\nRead: replaces this spell's previous book rolls.";
    return result;
}
bool replace(int player, const Item& book) {
    if (!Raid::active() || itemCategory(&book) != SPELLBOOK || book.status == BROKEN) return false;
    const int id = getSpellIDFromSpellbook(book.type);
    for (auto node = players[player]->magic.spellList.first; node; node = node->next) {
        auto spell = static_cast<spell_t*>(node->element);
        if (spell->ID != id) continue;
        // A racial-only/shapeshift spell is not a memorized spell.
        bool memorized = false;
        for (auto entry = stats[player]->inventory.first; entry; entry = entry->next) {
            auto item = static_cast<Item*>(entry->element);
            if (item->type == SPELL_ITEM && item->appearance == static_cast<Uint32>(id)) memorized = true;
        }
        if (!memorized) return false;
        spell->delveImprint = fromBook(book);
        messagePlayer(player, MESSAGE_PROGRESSION, "%s: book rolls replaced.", spell->getSpellName());
        return true;
    }
    return false;
}
void learned(int player, const Item& book) {
    if (!Raid::active() || itemCategory(&book) != SPELLBOOK) return;
    const int id = getSpellIDFromSpellbook(book.type);
    for (auto node = players[player]->magic.spellList.first; node; node = node->next) {
        auto spell = static_cast<spell_t*>(node->element);
        if (spell->ID == id) spell->delveImprint = fromBook(book);
    }
}
int value(Entity* caster, const spell_t* spell, Kind kind, bool usingBook) {
    if (!Raid::active() || !caster || caster->behavior != &actPlayer || !spell) return 0;
    if (usingBook) {
        auto stat = caster->getStats();
        if (!stat || !stat->shield || !valid(fromBook(*stat->shield), spell->ID)) return 0;
        return DelveSpellRules::value(fromBook(*stat->shield), kind);
    }
    if (!valid(spell->delveImprint, spell->ID)) return 0;
    return DelveSpellRules::value(spell->delveImprint, kind);
}
int activeValue(Entity* caster, Kind kind) {
    return caster && caster == currentCaster ? value(caster, currentSpell, kind) : 0;
}
int activePower() { return activeValue(currentCaster, Kind::Power); }
bool activeLegendary(Entity* caster) {
    return Raid::active() && caster && caster == currentCaster && currentSpell
        && valid(currentSpell->delveImprint, currentSpell->ID) && currentSpell->delveImprint.tier == 4;
}
Scope::Scope(Entity* caster, const spell_t* spell) : previousCaster(currentCaster), previousSpell(currentSpell) {
    currentCaster = caster; currentSpell = spell;
}
Scope::~Scope() { currentCaster = previousCaster; currentSpell = previousSpell; }
}
