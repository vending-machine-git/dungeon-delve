#pragma once
#include "delve_effect_rules.hpp"
#include <functional>
#include <string>
class Entity;
namespace DelveEffects {
enum class Phase { None, Cast, Active, Armed, Proc, Hit, Ready, Control };
struct View {
    unsigned int remaining = 0, total = 0, flash = 0;
    Phase phase = Phase::None;
    int charges = 0, stacks = 0;
};
void reset();
void createModels();
void resetPlayer(int player);
void activated(int player, int index);
void impact(int player, int index, Entity& target, bool controlled);
void passive(int player, int index, unsigned int remaining, int charges, int stacks, unsigned int cooldown, bool triggered = false);
void tick(unsigned int delta);
void drawHUD(int player);
void prepareRender(Entity& entity, int player);
void receiveEvent();
void receiveState();
View view(int player, int index);
unsigned int color(int characterClass, int index);
#ifdef BARONY_SMOKE_TESTS
void smokeQuiet(bool enabled);
unsigned int smokeCount(int player, int index);
void smokeAudit(Entity& host, Entity& guest, const std::function<void(const std::string&, bool)>& check);
bool smokeNetworkConfirmed();
void smokeShowcase(int characterClass, int index);
#endif
}
