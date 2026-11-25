#pragma once

#include <string>

class Entity;
class Frame;
class Item;

namespace Raid
{
	constexpr unsigned char joinCapability = 0x02;
	constexpr unsigned int startFlag = 1u << 31;
	constexpr unsigned char protocolVersion = 34;
	enum class Outcome : unsigned char { Playing, Descended, PendingExtraction, Extracted, Slain, Abandoned };

	bool parseArgument(const char* argument);
	bool requested();
	bool menuAvailable();
	void selectFromMenu(bool enabled);
	bool active();
	bool concealedFromObserver(const Entity& entity, int observer);
	int experienceGain(int normalAmount);
	bool neutralShopkeeperPair(const Entity& first, const Entity& second);
	bool isExtracted(int player);
	bool hasLoadout(int player);
	bool prepareCharacter(int player, std::string& error);
	void restoreLocalIdentity();
	void begin(bool enabled);
	void deployLocalCharacter();
	void placeSpawnMarkers();
	void useExit(Entity& exit);
	void tick();
	bool stormBoulder(const Entity& entity);
	bool tickStormBoulder(Entity& entity);
	bool controlGate(Entity& gate);
	void spawnStormWave(int secondsOvertime);
	void scatterDeathLoot(int player, double x, double y, const Item& item);
	void receiveLoadout();
	void receiveEquipmentRequest();
	void receiveExtraction();
	void receiveExtractionResult();
	int loadFloor(int floor, unsigned int seed, int* checkMapHash);
	bool canSpawn(int player);
	bool protectedPlayer(int player);
	bool holdPlayer(Entity& player);
	bool instantIdentification();
	void fillChest(Entity& chest);
	void fillShop(Entity& merchant);
	void dropEnemyLoot(Entity& enemy);
	void openedChest(Entity& chest, int player);
	void recordKill(Entity& killer, Entity& victim);
	bool applyControl(Entity& target, Entity* source, int effect, int duration, bool updateClients = false, unsigned char strength = 1);
	double pvpRaceMultiplier(Entity* target, Entity* source, double nativeMultiplier);
	void useShrine(Entity& shrine);
	void receiveState();
	void drawHUD(int player);
	void updateResultsWindow(Frame& window, int player);
	Outcome outcome(int player);
	int floorSecondsRemaining();
	bool matchComplete();
	bool revenant(int player);
	bool revenantRat(int player);
	bool ratReturn(int player);
	bool algernon(int player);
	void ateCheese(int player);
	bool neutralRevenantPair(const Entity& first, const Entity& second);
	bool requestRevenant(int player, int choice);
	void receiveRevenantRequest();
	void receiveLootScore();
#ifdef BARONY_SMOKE_TESTS
	bool smokeEnemyLootRules(int player);
	void setSmokeElapsed(int seconds);
	void smokeResetControl();
	void smokeReturnLives();
#endif
}
