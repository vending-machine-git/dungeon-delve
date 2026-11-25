#pragma once
#include <cstdint>
#include <string>

// vending_machine framework Steam invites and relay transport.
//
// The framework runtime is built without the Steamworks SDK. This module loads
// the installed game's own steam_api64.dll and uses its flat C API (plain
// integers and pointers only) for friends-only lobbies, invites, rich presence
// "Join Game" and Steam relayed P2P packets.
//
// Steam peers are presented to the engine's ordinary Direct IP networking as
// virtual addresses in the reserved 240.0.0.0/8 block. Only the lowest socket
// send/receive calls know about Steam, so lobby, Join In Progress and Dungeon
// Delve protocols run unchanged over either transport.

// Include main.hpp (SDL_net types) before this header.

namespace SteamInvites {
// Command line: -no-steam-invites, +connect_lobby <id>, -connect-lobby=<id>.
bool parseArgument(const char* argument);

// Load and initialize Steam. Call before the render window exists so the
// Steam overlay can attach. Safe when Steam is missing; retries later.
void startup();
void shutdown();
// Pump Steam callbacks; cheap, call every frame.
void tick();

bool enabled();
bool active();
// Installed Steam DLC bits (1 Myths, 2 Legends, 4 Deserters) through the
// shared Steam session, or -1 while Steam is not initialized here.
int installedDlcMask();

// Hosting. mode is a short label stored in the lobby ("normal", "delve", "jip").
void hostStarted(const char* mode);
bool hostLobbyReady();
std::uint64_t lobbyId();
// Opens the Steam overlay invite dialog. If the overlay is unavailable, copies
// the invite code to the clipboard. Returns a message suitable for the player.
std::string inviteFriends();
std::string inviteCode();
// Engine networking was closed: leave lobbies, close sessions, reset peers.
void networkClosed();

// Joining. A pending lobby comes from an accepted invite, the Steam friends
// list "Join Game", the command line or a pasted invite code. When the lobby
// is entered and validated the connect address ("steam:<owner>") is ready.
bool parseInviteCode(const char* text, std::uint64_t& lobby);
void requestJoin(std::uint64_t lobby, const char* source);
bool takeConnectAddress(std::string& address, std::string& error);
void cancelJoin();
bool joinInProgress();

// Transport.
bool isSteamAddress(const char* text);
bool resolve(const char* text, IPaddress* out);
bool isVirtual(const IPaddress& address);
int send(UDPsocket sock, int channel, UDPpacket* packet);
int recv(UDPsocket sock, UDPpacket* packet);
}
