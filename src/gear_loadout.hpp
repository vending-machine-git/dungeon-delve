#pragma once

#include "main.hpp"
#include "game.hpp"
#include "scores.hpp"

#include <optional>

namespace GearLoadout
{
using ItemRecord = SaveGameInfo::Player::stat_t::item_t;
using Equipment = std::array<std::optional<ItemRecord>, 10>;

struct Loadout
{
	std::string name;
	Uint32 characterClass = 0;
	Uint32 race = 0;
	Uint32 sex = 0;
	Uint32 appearance = 0;
	std::vector<ItemRecord> inventory;
	std::vector<std::pair<std::string, Uint32>> playerEquipment;

	bool serialize(FileInterface* file);
};

enum class LoadResult { Missing, Ready, Error };

LoadResult readStartingLoadout(const std::string& profilePath, const std::string& importPath,
	Loadout& loadout, std::string& error);
bool writeProfile(const std::string& path, const Loadout& loadout, std::string& error);

Loadout capture(int player);
Loadout withoutGear(const Loadout& loadout);
bool applyIdentity(int player, const Loadout& loadout, bool* usedLobbyIdentity = nullptr);
bool applyLocal(int player, const Loadout& loadout);

// Equipment uses ItemEquippableSlot order. Remote backpacks stay on their clients.
Equipment equippedItems(const Loadout& loadout);
bool applyRemoteEquipment(int player, const Equipment& equipment);

constexpr int equipmentPacketSize = 130;
void sendEquipmentToHost(const char* command, int player);
bool receiveEquipment(int player);
}
