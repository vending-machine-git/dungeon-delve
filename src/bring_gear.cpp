#include "main.hpp"
#include "game.hpp"
#include "bring_gear.hpp"
#include "gear_loadout.hpp"
#include "join_progress.hpp"
#include "raid.hpp"
#include "stat.hpp"
#include "mod_tools.hpp"
#include "net.hpp"
#include "player.hpp"
#include "ui/MainMenu.hpp"
#include <array>
#include <string_view>

namespace BringGear {
namespace {
bool enabled = false;
std::string importPath;
GearLoadout::Loadout imported;
bool prepared = false;
bool haveImport = false;
bool pendingEquipment = false;
bool failed = false;
Uint32 nextRequestTick = 0;
std::array<bool, MAXPLAYERS> received{};
}

bool parseArgument(const char* argument) {
 const std::string_view value(argument);
 constexpr std::string_view prefix = "-gear-import=";
 if (value == "-bring-gear") { enabled = true; return true; }
 if (value.substr(0,prefix.size()) == prefix) {
  importPath = value.substr(prefix.size()); enabled = true; return true;
 }
 return false;
}

bool requested() { return enabled; }
bool hasLoadout(int player) { return player >= 0 && player < MAXPLAYERS && received[player]; }

void setRemoteLoadoutReady(int player, bool ready) {
 if (player > 0 && player < MAXPLAYERS) received[player] = ready;
}

bool prepareCharacter(std::string& error) {
 error.clear();
 if (!enabled) return true;
 prepared = false; haveImport = false; failed = false; pendingEquipment = false;
 received.fill(false);
 nextRequestTick = 0;
 if (Raid::requested() || gameModeManager.getMode() != GameModeManager_t::GAME_MODE_DEFAULT) {
  error = "Bring Your Gear is a separate mod for normal games. Launch it without Extraction Raid.";
  return false;
 }
 // Resuming a normal campaign uses that campaign's current inventory.
 if (loadingsavegame) { prepared = true; return true; }
 if (!importPath.empty()) {
  const auto result = GearLoadout::readStartingLoadout("", importPath, imported, error);
  if (result != GearLoadout::LoadResult::Ready) return false;
  haveImport = true;
 }
 prepared = true;
 return true;
}

void deployLocalCharacter() {
 if (!enabled || Raid::active()) return;
 // Quickstart enters doNewGame directly, bypassing the lobby preparation hook.
 if (!prepared) {
  std::string error;
  if (!prepareCharacter(error)) { printlog("[GEAR] import failed: %s",error.c_str()); failed = true; return; }
 }
 if (!loadingsavegame && haveImport) {
  if (!GearLoadout::applyLocal(clientnum, imported)) {
   printlog("[GEAR] could not apply imported inventory"); failed = true; return;
  }
  conductGameChallenges[CONDUCT_MODDED] = 1;
  Mods::disableSteamAchievements = true;
  printlog("[GEAR] imported slot=%d stacks=%u class=%d race=%d level=%d floor=%d",
   clientnum, static_cast<unsigned>(imported.inventory.size()), client_classes[clientnum],
   stats[clientnum]->playerRace, stats[clientnum]->LVL, currentlevel);
 } else {
  printlog("[GEAR] ordinary %s inventory slot=%d",loadingsavegame ? "resumed" : "starter",clientnum);
 }
 // The host requests equipment once its own game initialization has finished.
 pendingEquipment = false;
 prepared = false;
}

void tick() {
 if ((!enabled && !JoinProgress::requested()) || Raid::active()) return;
 if (failed) {
  if (!fadeout) {
   messagePlayer(clientnum, MESSAGE_MISC, "Could not import gear. Check the selected save and log.");
   MainMenu::destroyMainMenu(); MainMenu::createDummyMainMenu();
   MainMenu::beginFade(MainMenu::FadeDestination::RootMainMenu);
   failed = false;
  }
  return;
 }
 if (enabled && multiplayer == SERVER && !JoinProgress::paused() && ticks >= nextRequestTick) {
  nextRequestTick = ticks + TICKS_PER_SECOND;
  for (int player = 1; player < MAXPLAYERS; ++player) {
   if (client_disconnected[player] || received[player]) continue;
   memcpy(net_packet->data,"GREQ",4);
   SDLNet_Write32(uniqueGameKey,&net_packet->data[4]);
   net_packet->address = net_clients[player-1]; net_packet->len = 8;
   sendPacket(net_sock,-1,net_packet,player-1);
  }
 }
 if (pendingEquipment) {
  pendingEquipment = false;
  GearLoadout::sendEquipmentToHost("GKIT",clientnum);
 }
}

void receiveRequest() {
 if ((!enabled && !JoinProgress::requested()) || Raid::requested() || multiplayer != CLIENT || net_packet->len != 8
  || SDLNet_Read32(&net_packet->data[4]) != uniqueGameKey
  || net_packet->address.host != net_server.host || net_packet->address.port != net_server.port) return;
 pendingEquipment = true;
}

void receiveLoadout() {
 if (!enabled || Raid::requested() || multiplayer != SERVER
  || net_packet->len != GearLoadout::equipmentPacketSize
  || SDLNet_Read32(&net_packet->data[4]) != uniqueGameKey) return;
 const int player = net_packet->data[8];
 if (player <= 0 || player >= MAXPLAYERS || client_disconnected[player] || received[player]) return;
 const auto& address = net_clients[player-1];
 if (net_packet->address.host != address.host || net_packet->address.port != address.port) return;
 received[player] = GearLoadout::receiveEquipment(player);
 if (received[player]) printlog("[GEAR] remote equipment synchronized slot=%d",player);
}
}
