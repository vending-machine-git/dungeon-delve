#include "gear_loadout.hpp"
#include "rapidjson/document.h"

#include "classdescriptions.hpp"
#include "menu.hpp"
#include "stat.hpp"
#include "net.hpp"

#include <filesystem>
#include <fstream>
#include <cstring>
#include <limits>

namespace GearLoadout
{
namespace
{
constexpr Uint32 NoItem = std::numeric_limits<Uint32>::max();
constexpr Uint32 MaxInventory = 1024;

struct Slot
{
	const char* name;
	Item* Stat::* item;
};

constexpr Slot Slots[] = {
	{ "weapon", &Stat::weapon },
	{ "shield", &Stat::shield },
	{ "mask", &Stat::mask },
	{ "helmet", &Stat::helmet },
	{ "gloves", &Stat::gloves },
	{ "shoes", &Stat::shoes },
	{ "breastplate", &Stat::breastplate },
	{ "cloak", &Stat::cloak },
	{ "amulet", &Stat::amulet },
	{ "ring", &Stat::ring },
};
static_assert(std::size(Slots) == std::tuple_size<Equipment>::value);
static_assert(std::size(Slots) == NO_EQUIP);

struct Profile
{
	std::string format;
	Uint32 version = 0;
	Loadout loadout;

	bool serialize(FileInterface* file)
	{
		return file->property("format", format)
			&& file->property("version", version)
			&& format == "BaronyGearLoadout" && version == 1
			&& file->property("character", loadout);
	}
};

bool validIdentity(const Loadout& loadout)
{
	return loadout.characterClass < NUMCLASSES && loadout.race < RACE_ENUM_END
		&& loadout.sex <= FEMALE && loadout.name.size() < sizeof(Stat::name)
		&& loadout.name.find('\0') == std::string::npos
		&& loadout.appearance < (loadout.race == RACE_HUMAN ? NUMAPPEARANCES : 2);
}

bool canUseIdentity(const Loadout& loadout)
{
	SaveGameInfo::Player character;
	character.char_class = loadout.characterClass;
	character.race = loadout.race;
	character.stats.statscore_appearance = loadout.appearance;
	return character.isCharacterValidFromDLC() == VALID_OK_CHARACTER;
}

bool validItem(const ItemRecord& item)
{
	return item.type < NUMITEMS && item.type != SPELL_ITEM && item.status <= EXCELLENT
		&& item.count > 0 && item.count <= std::numeric_limits<Sint16>::max()
		&& item.beatitude >= -100 && item.beatitude <= 100;
}

bool validLoadout(const Loadout& loadout)
{
	if ( loadout.inventory.size() > MaxInventory || loadout.playerEquipment.size() > std::size(Slots)
		|| !std::all_of(loadout.inventory.begin(), loadout.inventory.end(), validItem) )
	{
		return false;
	}
	std::vector<bool> equipped(loadout.inventory.size(), false);
	std::array<bool, std::size(Slots)> usedSlots{};
	for ( const auto& entry : loadout.playerEquipment )
	{
		const auto slot = std::find_if(std::begin(Slots), std::end(Slots),
			[&](const Slot& candidate) { return entry.first == candidate.name; });
		if ( slot == std::end(Slots) || usedSlots[slot - Slots] )
		{
			return false;
		}
		usedSlots[slot - Slots] = true;
		const Uint32 index = entry.second;
		if ( index == NoItem )
		{
			continue;
		}
		if ( index >= equipped.size() || equipped[index] )
		{
			return false;
		}
		equipped[index] = true;
	}
	return true;
}

// The campaign serializer tolerates missing/wrongly typed properties for old
// saves. Imports must reject those records rather than inventing default gear.
bool readBoundedJson(const std::string& path, size_t limit, rapidjson::Document& document)
{
 std::error_code error;
 const auto size = std::filesystem::file_size(path, error);
 if (error || size == 0 || size > limit) return false;
 std::ifstream input(path, std::ios::binary);
 std::string bytes(static_cast<size_t>(size), '\0');
 if (!input.read(bytes.data(), bytes.size())) return false;
 document.Parse(bytes.data(), bytes.size());
 return !document.HasParseError() && document.IsObject();
}
bool validInventoryJson(const rapidjson::Value& value)
{
 if (!value.IsObject() || !value.HasMember("inventory") || !value["inventory"].IsArray()
  || value["inventory"].Size() > MaxInventory || !value.HasMember("player_equipment")
  || !value["player_equipment"].IsArray() || value["player_equipment"].Size() > std::size(Slots)) return false;
 for (const auto& item : value["inventory"].GetArray()) {
  if (!item.IsObject()) return false;
  for (const char* key : {"type", "status", "appearance"})
   if (!item.HasMember(key) || !item[key].IsUint()) return false;
  for (const char* key : {"beatitude", "count", "x", "y"})
   if (!item.HasMember(key) || !item[key].IsInt()) return false;
  if (!item.HasMember("identified") || !item["identified"].IsBool()) return false;
 }
 for (const auto& slot : value["player_equipment"].GetArray())
  if (!slot.IsObject() || !slot.HasMember("first") || !slot["first"].IsString()
   || !slot.HasMember("second") || !slot["second"].IsUint()) return false;
 return true;
}
bool validStringJson(const rapidjson::Value& value, const char* key)
{
 return value.HasMember(key) && value[key].IsString()
  && std::strlen(value[key].GetString()) == value[key].GetStringLength();
}
bool readProfile(const std::string& path, Profile& profile)
{
	std::error_code error;
	const auto size = std::filesystem::file_size(path, error);
 rapidjson::Document document;
 if (error || size == 0 || size > 1024 * 1024 || !readBoundedJson(path, 1024 * 1024, document)
  || !document.HasMember("character") || !validInventoryJson(document["character"])) return false;
 const auto& character = document["character"];
 if (!validStringJson(character, "name")) return false;
 for (const char* key : {"class", "race", "sex", "appearance"})
  if (!character.HasMember(key) || !character[key].IsUint()) return false;
 return FileHelper::readObject(path.c_str(), profile) && validLoadout(profile.loadout);
}

bool importSave(const std::string& path, Loadout& loadout)
{
 rapidjson::Document document;
 if (!readBoundedJson(path, 8 * 1024 * 1024, document)
  || !document.HasMember("player_num") || !document["player_num"].IsInt()
  || !document.HasMember("players") || !document["players"].IsArray()) return false;
 const int playerIndex = document["player_num"].GetInt();
 if (playerIndex < 0 || static_cast<unsigned>(playerIndex) >= document["players"].Size()
  || document["players"].Size() > MAXPLAYERS) return false;
 const auto& character = document["players"][playerIndex];
 if (!character.IsObject() || !character.HasMember("stats") || !validInventoryJson(character["stats"])) return false;
 for (const char* key : {"char_class", "race"})
  if (!character.HasMember(key) || !character[key].IsUint()) return false;
 const auto& characterStats = character["stats"];
 if (!validStringJson(characterStats, "name") || !characterStats.HasMember("sex")
  || !characterStats["sex"].IsUint() || !characterStats.HasMember("appearance")
  || !characterStats["appearance"].IsUint()) return false;
 if (document.HasMember("players_connected")) {
  const auto& connected = document["players_connected"];
  if (!connected.IsArray()) return false;
  for (const auto& entry : connected.GetArray())
   if (!entry.IsInt() || (entry.GetInt() != 0 && entry.GetInt() != 1)) return false;
 }
	SaveGameInfo save;
	save.magic_cookie.clear();
	save.player_num = -1;
	if ( !FileHelper::readObject(path.c_str(), save) || save.magic_cookie != "BARONYJSONSAVE"
		|| save.game_version < 0 || save.player_num < 0
		|| static_cast<size_t>(save.player_num) >= save.players.size()
		|| (!save.players_connected.empty() && (static_cast<size_t>(save.player_num) >= save.players_connected.size()
			|| !save.players_connected[save.player_num])) )
	{
		return false;
	}

	const auto& player = save.players[save.player_num];
	loadout.name = player.stats.name;
	loadout.characterClass = player.char_class;
	loadout.race = player.race;
	loadout.sex = player.stats.sex;
	loadout.appearance = player.stats.statscore_appearance;

	const auto& inventory = player.stats.inventory;
	std::vector<Uint32> newIndices(inventory.size(), NoItem);
	for ( size_t i = 0; i < inventory.size(); ++i )
	{
		if ( inventory[i].type != SPELL_ITEM )
		{
			newIndices[i] = static_cast<Uint32>(loadout.inventory.size());
			loadout.inventory.push_back(inventory[i]);
		}
	}
	for ( const auto& slot : player.stats.player_equipment )
	{
		if ( slot.second == NoItem || slot.second < newIndices.size() )
		{
			loadout.playerEquipment.emplace_back(slot.first,
				slot.second == NoItem ? NoItem : newIndices[slot.second]);
		}
		else
		{
			return false;
		}
	}
	return validLoadout(loadout);
}

bool flushAndReplace(const std::filesystem::path& temporary, const std::filesystem::path& path)
{
#ifdef WINDOWS
	const HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if ( file == INVALID_HANDLE_VALUE )
	{
		return false;
	}
	const bool flushed = FlushFileBuffers(file) != 0;
	CloseHandle(file);
	return flushed && MoveFileExW(temporary.c_str(), path.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	const int file = open(temporary.c_str(), O_WRONLY);
	if ( file < 0 )
	{
		return false;
	}
	const bool flushed = fsync(file) == 0;
	close(file);
	std::error_code error;
	if ( flushed )
	{
		std::filesystem::rename(temporary, path, error);
	}
	return flushed && !error;
#endif
}

Stat* getStats(int player)
{
	return player >= 0 && player < MAXPLAYERS ? stats[player] : nullptr;
}

ItemRecord toRecord(const Item& item)
{
	return ItemRecord(item.type, item.status, item.appearance, item.beatitude,
		item.count, item.identified, item.x, item.y);
}

Item* restoreItem(const ItemRecord& item, list_t* inventory)
{
	Item* restored = newItem(static_cast<ItemType>(item.type), static_cast<Status>(item.status),
		static_cast<Sint16>(item.beatitude), static_cast<Sint16>(item.count),
		item.appearance, item.identified, inventory);
	restored->x = item.x;
	restored->y = item.y;
	return restored;
}
}

bool Loadout::serialize(FileInterface* file)
{
	return file->property("name", name)
		&& file->property("class", characterClass)
		&& file->property("race", race)
		&& file->property("sex", sex)
		&& file->property("appearance", appearance)
		&& file->property("inventory", inventory, MaxInventory)
		&& file->property("player_equipment", playerEquipment, static_cast<Uint32>(std::size(Slots)));
}

LoadResult readStartingLoadout(const std::string& profilePath, const std::string& importPath,
	Loadout& loadout, std::string& error)
{
	loadout = {};
	error.clear();
	std::error_code fileError;
	const bool haveProfile = !profilePath.empty() && std::filesystem::exists(profilePath, fileError);
	if ( fileError )
	{
		error = "Unable to check raid profile: " + profilePath;
		return LoadResult::Error;
	}
	if ( haveProfile )
	{
		Profile profile;
		if ( !readProfile(profilePath, profile) )
		{
			error = "Unable to read raid profile: " + profilePath;
			return LoadResult::Error;
		}
		loadout = std::move(profile.loadout);
	}
	else if ( !importPath.empty() )
	{
		if ( !importSave(importPath, loadout) )
		{
			error = "Unable to import character from Barony save: " + importPath;
			return LoadResult::Error;
		}
	}
	else
	{
		return LoadResult::Missing;
	}
	if ( !validIdentity(loadout) )
	{
		error = "The raid character has an invalid name, appearance, sex, class, or race.";
		return LoadResult::Error;
	}
	return LoadResult::Ready;
}

bool writeProfile(const std::string& pathString, const Loadout& loadout, std::string& error)
{
	error.clear();
	const std::filesystem::path path(pathString);
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	if ( path.empty() || extension == ".baronysave" || !validIdentity(loadout) || !validLoadout(loadout) )
	{
		error = "Invalid raid character or raid profile path: " + pathString;
		return false;
	}
	std::error_code fileError;
	if ( path.has_parent_path() )
	{
		std::filesystem::create_directories(path.parent_path(), fileError);
		if ( fileError )
		{
			error = "Unable to create raid profile directory: " + path.parent_path().string();
			return false;
		}
	}
	auto temporary = path;
	temporary += ".tmp";
	const auto temporaryPath = temporary.string();
	Profile profile{ "BaronyGearLoadout", 1, loadout };
	Profile verification;
	if ( !FileHelper::writeObject(temporaryPath.c_str(), EFileFormat::Json_Compact, profile)
		|| !readProfile(temporaryPath, verification) || !flushAndReplace(temporary, path) )
	{
		std::filesystem::remove(temporary, fileError);
		error = "Unable to save raid profile: " + pathString;
		return false;
	}
	return true;
}

Loadout capture(int player)
{
	Loadout loadout;
	const Stat* character = getStats(player);
	if ( !character )
	{
		return loadout;
	}
	loadout.name = character->name;
	loadout.characterClass = client_classes[player];
	loadout.race = character->playerRace;
	loadout.sex = character->sex;
	loadout.appearance = character->stat_appearance;
	std::unordered_map<const Item*, Uint32> itemIndices;
	for ( const node_t* node = character->inventory.first; node; node = node->next )
	{
		const auto* item = static_cast<const Item*>(node->element);
		if ( item->type != SPELL_ITEM )
		{
			itemIndices.emplace(item, static_cast<Uint32>(loadout.inventory.size()));
			loadout.inventory.push_back(toRecord(*item));
		}
	}
	for ( const auto& slot : Slots )
	{
		const auto item = itemIndices.find(character->*(slot.item));
		if ( item != itemIndices.end() )
		{
			loadout.playerEquipment.emplace_back(slot.name, item->second);
		}
	}
	return loadout;
}

Loadout withoutGear(const Loadout& loadout)
{
	Loadout empty;
	empty.name = loadout.name;
	empty.characterClass = loadout.characterClass;
	empty.race = loadout.race;
	empty.sex = loadout.sex;
	empty.appearance = loadout.appearance;
	return empty;
}

bool applyIdentity(int player, const Loadout& loadout, bool* usedLobbyIdentity)
{
	Stat* character = getStats(player);
	if ( !character || !validIdentity(loadout) )
	{
		return false;
	}
	stringCopy(character->name, loadout.name.c_str(), sizeof(character->name), loadout.name.size());
	const bool available = canUseIdentity(loadout);
	if ( usedLobbyIdentity )
	{
		*usedLobbyIdentity = !available;
	}
	if ( !available )
	{
		return true;
	}
	client_classes[player] = loadout.characterClass;
	character->playerRace = loadout.race;
	character->sex = static_cast<sex_t>(loadout.sex);
	character->stat_appearance = loadout.appearance;
	return true;
}

bool applyLocal(int player, const Loadout& loadout)
{
	Stat* character = getStats(player);
	if ( !character || !players[player] || !players[player]->isLocalPlayer() || !validLoadout(loadout) )
	{
		return false;
	}

	// Replace starter gear while retaining spell entries created by initClass.
	character->freePlayerEquipment();
	for ( auto* node = character->inventory.first; node; )
	{
		auto* current = node;
		node = node->next;
		if ( static_cast<Item*>(current->element)->type != SPELL_ITEM )
		{
			list_RemoveNode(current);
		}
	}
	players[player]->inventoryUI.appraisal.appraisalProgressionItems.clear();
	players[player]->paperDoll.clear();

	std::vector<Item*> restored;
	restored.reserve(loadout.inventory.size());
	for ( const auto& item : loadout.inventory )
	{
		restored.push_back(restoreItem(item, &character->inventory));
	}
	for ( const auto& slot : Slots )
	{
		const auto index = std::find_if(loadout.playerEquipment.begin(), loadout.playerEquipment.end(),
			[&](const auto& entry) { return entry.first == slot.name; });
		if ( index != loadout.playerEquipment.end() && index->second != NoItem )
		{
			character->*(slot.item) = restored[index->second];
		}
	}
	players[player]->paperDoll.updateSlots();
	return true;
}

Equipment equippedItems(const Loadout& loadout)
{
	Equipment gear;
	for ( size_t i = 0; i < std::size(Slots); ++i )
	{
		const auto index = std::find_if(loadout.playerEquipment.begin(), loadout.playerEquipment.end(),
			[&](const auto& entry) { return entry.first == Slots[i].name; });
		if ( index != loadout.playerEquipment.end() && index->second < loadout.inventory.size() )
		{
			gear[i].emplace(loadout.inventory[index->second]);
		}
	}
	return gear;
}

bool applyRemoteEquipment(int player, const Equipment& equipment)
{
	Stat* character = getStats(player);
	if ( !character || !players[player] || players[player]->isLocalPlayer()
		|| !std::all_of(equipment.begin(), equipment.end(),
			[](const auto& item) { return !item || validItem(*item); }) )
	{
		return false;
	}
	character->freePlayerEquipment();
	for ( size_t i = 0; i < equipment.size(); ++i )
	{
		if ( equipment[i] )
		{
			character->*(Slots[i].item) = restoreItem(*equipment[i], nullptr);
		}
	}
	return true;
}

void sendEquipmentToHost(const char* command, int player)
{
 const auto equipment = equippedItems(capture(player));
 memcpy(net_packet->data, command, 4);
 SDLNet_Write32(uniqueGameKey, &net_packet->data[4]);
 net_packet->data[8] = player;
 net_packet->data[9] = 0;
 for (size_t i = 0; i < equipment.size(); ++i) {
  auto* data = &net_packet->data[10 + i * 12];
  memset(data, 0, 12);
  if (const auto& item = equipment[i]) {
   data[0] = 1;
   SDLNet_Write16(item->type, data + 1); data[3] = item->status;
   SDLNet_Write32(item->appearance, data + 4);
   SDLNet_Write16(static_cast<Sint16>(item->beatitude), data + 8);
   SDLNet_Write16(item->count, data + 10);
  }
 }
 net_packet->address = net_server;
 net_packet->len = equipmentPacketSize;
 sendPacketSafe(net_sock, -1, net_packet, 0);
}

bool receiveEquipment(int player)
{
 if (net_packet->len != equipmentPacketSize) return false;
 Equipment equipment;
 for (size_t i = 0; i < equipment.size(); ++i) {
  const auto* data = &net_packet->data[10 + i * 12];
  if (data[0] > 1) return false;
  if (data[0]) equipment[i].emplace(SDLNet_Read16(data + 1), data[3], SDLNet_Read32(data + 4),
   static_cast<Sint16>(SDLNet_Read16(data + 8)), SDLNet_Read16(data + 10), false, 0, 0);
 }
 return applyRemoteEquipment(player, equipment);
}

}
