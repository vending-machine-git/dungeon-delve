// Reyes Mod Framework 0.1: native extension points for data-driven equipment.
#pragma once
#include <rapidjson/document.h>
#include <string>
class Entity;
class Stat;
namespace rmf {
extern bool selfTestRequested;
void tickSelfTest(Entity* entity, Stat* stats);
constexpr int kApiVersion = 1;
constexpr int kCustomItemSlots = 16;
void appendCustomItems(rapidjson::Document& itemsDocument);
void applyCustomItemNames();
std::string itemDescription(int itemType);
bool hasEquipmentTrait(const Stat* stats, const char* trait);
bool hasInfiniteHealth(const Entity* entity);
bool hasInfiniteMana(const Entity* entity);
bool hasNoHunger(const Entity* entity);
void enforceResources(Entity* entity, Stat* stats);
}
