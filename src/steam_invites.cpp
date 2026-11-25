#include "main.hpp"
#include "game.hpp"
#include "net.hpp"
#include "files.hpp"
#include "raid.hpp"
#include "join_progress.hpp"
#include "steam_invites.hpp"
#include "player.hpp"
#include "entity.hpp"
#include "interface/interface.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

// See steam_invites.hpp. Every Steam type crossing the flat API is a plain
// integer, pointer, bool or a callback struct laid out with Steam's Windows
// packing (8). No Steamworks SDK headers are used or redistributed.

namespace SteamInvites {
namespace {
#if defined(WINDOWS) && !defined(STEAMWORKS) && !defined(USE_EOS)
constexpr bool kSupported = true;
#else
constexpr bool kSupported = false;
#endif

constexpr int kChannel = 7;               // retail Barony uses channel 0
constexpr int kInviteProtocol = 1;
constexpr std::uint32_t kVirtualNet = 0xF0000000u; // 240.0.0.0/8, never routable
constexpr Uint16 kVirtualPort = 27015;
constexpr Uint32 kPendingSessionMs = 15000;
constexpr int kMaxUnreliable = 1200;

typedef std::uint64_t u64;
typedef std::uint64_t SteamAPICall;

#pragma pack(push, 8)
struct LobbyCreated { int result; u64 lobby; };
struct LobbyEnter { u64 lobby; std::uint32_t permissions; bool locked; std::uint32_t response; };
struct GameLobbyJoinRequested { u64 lobby; u64 friendId; };
struct GameRichPresenceJoinRequested { u64 friendId; char connect[256]; };
struct P2PSessionRequest { u64 remote; };
struct P2PSessionConnectFail { u64 remote; unsigned char error; };
#pragma pack(pop)
static_assert(sizeof(LobbyCreated) == 16, "LobbyCreated_t layout");
static_assert(sizeof(LobbyEnter) == 24, "LobbyEnter_t layout");
static_assert(sizeof(GameRichPresenceJoinRequested) == 264, "GameRichPresenceJoinRequested_t layout");

enum : int {
    kLobbyEnter = 504, kLobbyCreated = 513, kGameLobbyJoinRequested = 333,
    kRichPresenceJoinRequested = 337, kP2PSessionRequest = 1202,
    kP2PSessionConnectFail = 1203,
};

// steam_api64.dll is built with MSVC, so a registered callback is a
// CCallbackBase: vtable pointer, flags byte, callback id. MSVC places the
// overloaded virtual Run(void*, bool, SteamAPICall_t) before Run(void*), then
// GetCallbackSizeBytes(). On x64 every slot uses the one Windows calling
// convention with `this` first, so plain functions fill the table. Probed
// against the installed DLL: registered callbacks dispatch through slot 1.
// (Its ManualDispatch exports never return callbacks for this process.)
struct CallbackObject { void** vtable; std::uint8_t flags; int id; };
static_assert(sizeof(CallbackObject) == 16, "CCallbackBase layout");
void handleCallback(int id, const void* param);
void callbackRunResult(CallbackObject*, void*, bool, u64) {}
void callbackRun(CallbackObject* self, void* param) { if (self && param) handleCallback(self->id, param); }
int callbackSize(CallbackObject* self) {
    switch (self ? self->id : 0) {
    case kGameLobbyJoinRequested: return sizeof(GameLobbyJoinRequested);
    case kRichPresenceJoinRequested: return sizeof(GameRichPresenceJoinRequested);
    case kP2PSessionRequest: return sizeof(P2PSessionRequest);
    case kP2PSessionConnectFail: return sizeof(P2PSessionConnectFail);
    default: return 0;
    }
}
void* callbackTable[3] = {
    reinterpret_cast<void*>(&callbackRunResult),
    reinterpret_cast<void*>(&callbackRun),
    reinterpret_cast<void*>(&callbackSize),
};
constexpr int kCallbackIds[] = {kGameLobbyJoinRequested, kRichPresenceJoinRequested, kP2PSessionRequest, kP2PSessionConnectFail};
CallbackObject callbackObjects[sizeof(kCallbackIds) / sizeof(kCallbackIds[0])];

struct Api {
    bool (*initSafe)() = nullptr;
    void (*shutdown)() = nullptr;
    void (*registerCallback)(CallbackObject*, int) = nullptr;
    void (*unregisterCallback)(CallbackObject*) = nullptr;
    void (*runCallbacks)() = nullptr;
    bool (*callCompleted)(void*, SteamAPICall, bool*) = nullptr;
    bool (*callResult)(void*, SteamAPICall, void*, int, int, bool*) = nullptr;
    void* (*matchmakingIface)() = nullptr;
    void* (*friendsIface)() = nullptr;
    void* (*userIface)() = nullptr;
    void* (*networkingIface)() = nullptr;
    void* (*utilsIface)() = nullptr;
    void* (*appsIface)() = nullptr;
    SteamAPICall (*createLobby)(void*, int, int) = nullptr;
    SteamAPICall (*joinLobby)(void*, u64) = nullptr;
    void (*leaveLobby)(void*, u64) = nullptr;
    bool (*setLobbyData)(void*, u64, const char*, const char*) = nullptr;
    const char* (*getLobbyData)(void*, u64, const char*) = nullptr;
    u64 (*getLobbyOwner)(void*, u64) = nullptr;
    int (*numMembers)(void*, u64) = nullptr;
    u64 (*memberAt)(void*, u64, int) = nullptr;
    bool (*setJoinable)(void*, u64, bool) = nullptr;
    void (*inviteDialog)(void*, u64) = nullptr;
    bool (*setRichPresence)(void*, const char*, const char*) = nullptr;
    void (*clearRichPresence)(void*) = nullptr;
    u64 (*steamId)(void*) = nullptr;
    bool (*sendP2P)(void*, u64, const void*, std::uint32_t, int, int) = nullptr;
    bool (*p2pAvailable)(void*, std::uint32_t*, int) = nullptr;
    bool (*readP2P)(void*, void*, std::uint32_t, std::uint32_t*, u64*, int) = nullptr;
    bool (*acceptP2P)(void*, u64) = nullptr;
    bool (*closeP2P)(void*, u64) = nullptr;
    bool (*allowRelay)(void*, bool) = nullptr;
    bool (*overlayEnabled)(void*) = nullptr;
    bool (*dlcInstalled)(void*, std::uint32_t) = nullptr;
};

enum class JoinState { Idle, Joining, Ready, Connecting, Connected, Failed };

struct State {
    bool disabled = false;
    bool noOverlay = false;
    bool active = false;
    bool loggedUnavailable = false;
    void* library = nullptr;
    Uint32 nextAttempt = 0;
    Uint32 lastTick = 0;
    Api api;
    bool callbacksRegistered = false;
    void* matchmaking = nullptr;
    void* friends = nullptr;
    void* user = nullptr;
    void* networking = nullptr;
    void* utils = nullptr;
    void* apps = nullptr;
    u64 self = 0;

    // hosting
    bool hostRequested = false;
    std::string hostMode;
    SteamAPICall createCall = 0;
    u64 hostLobby = 0;
    bool closedForGame = false;

    // joining
    JoinState join = JoinState::Idle;
    SteamAPICall joinCall = 0;
    u64 joinLobby = 0;
    u64 joinOwner = 0;
    u64 argumentLobby = 0;
    bool expectArgumentLobby = false;
    std::string joinError;

    std::vector<u64> peers;
    std::map<u64, Uint32> pendingSessions;
    std::vector<unsigned char> scratch;
} s;

std::string toString(u64 value) { return std::to_string(static_cast<unsigned long long>(value)); }

bool parseU64(const char* text, u64& value) {
    if (!text) return false;
    while (*text == ' ' || *text == '\t') ++text;
    if (*text < '0' || *text > '9') return false;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(text, &end, 10);
    while (end && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) ++end;
    if (!end || *end != '\0' || parsed == 0) return false;
    value = static_cast<u64>(parsed);
    return true;
}

const char* localMode() {
    if (Raid::requested()) return "delve";
    if (JoinProgress::requested()) return "jip";
    return "normal";
}

const char* modeName(const std::string& mode) {
    if (mode == "delve") return "Dungeon Delve";
    if (mode == "jip") return "Join In Progress";
    return "a normal campaign";
}

template <typename T> bool load(T& target, const char* name) {
    target = reinterpret_cast<T>(SDL_LoadFunction(s.library, name));
    return target != nullptr;
}

std::string libraryPath() {
#ifdef BARONY_SMOKE_TESTS
    // Smoke builds can substitute a local multi-instance Steam stand-in.
    if (const char* path = std::getenv("BARONY_SMOKE_STEAM_API")) return path;
#endif
    // The launcher passes -datadir=<installed game>; use that game's DLL.
    if (datadir[0]) return std::string(datadir) + "/steam_api64.dll";
    return std::string(BASE_DATA_DIR) + "/steam_api64.dll";
}

bool loadLibrary() {
    if (s.library) return true;
    const std::string path = libraryPath();
    s.library = SDL_LoadObject(path.c_str());
    if (!s.library) return false;
    Api& a = s.api;
    bool ok = load(a.initSafe, "SteamAPI_InitSafe") && load(a.shutdown, "SteamAPI_Shutdown")
        && load(a.registerCallback, "SteamAPI_RegisterCallback")
        && load(a.unregisterCallback, "SteamAPI_UnregisterCallback")
        && load(a.runCallbacks, "SteamAPI_RunCallbacks")
        && load(a.callCompleted, "SteamAPI_ISteamUtils_IsAPICallCompleted")
        && load(a.callResult, "SteamAPI_ISteamUtils_GetAPICallResult")
        && load(a.matchmakingIface, "SteamAPI_SteamMatchmaking_v009")
        && load(a.friendsIface, "SteamAPI_SteamFriends_v017")
        && load(a.userIface, "SteamAPI_SteamUser_v023")
        && load(a.networkingIface, "SteamAPI_SteamNetworking_v006")
        && load(a.utilsIface, "SteamAPI_SteamUtils_v010")
        && load(a.appsIface, "SteamAPI_SteamApps_v008")
        && load(a.createLobby, "SteamAPI_ISteamMatchmaking_CreateLobby")
        && load(a.joinLobby, "SteamAPI_ISteamMatchmaking_JoinLobby")
        && load(a.leaveLobby, "SteamAPI_ISteamMatchmaking_LeaveLobby")
        && load(a.setLobbyData, "SteamAPI_ISteamMatchmaking_SetLobbyData")
        && load(a.getLobbyData, "SteamAPI_ISteamMatchmaking_GetLobbyData")
        && load(a.getLobbyOwner, "SteamAPI_ISteamMatchmaking_GetLobbyOwner")
        && load(a.numMembers, "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers")
        && load(a.memberAt, "SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex")
        && load(a.setJoinable, "SteamAPI_ISteamMatchmaking_SetLobbyJoinable")
        && load(a.inviteDialog, "SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog")
        && load(a.setRichPresence, "SteamAPI_ISteamFriends_SetRichPresence")
        && load(a.clearRichPresence, "SteamAPI_ISteamFriends_ClearRichPresence")
        && load(a.steamId, "SteamAPI_ISteamUser_GetSteamID")
        && load(a.sendP2P, "SteamAPI_ISteamNetworking_SendP2PPacket")
        && load(a.p2pAvailable, "SteamAPI_ISteamNetworking_IsP2PPacketAvailable")
        && load(a.readP2P, "SteamAPI_ISteamNetworking_ReadP2PPacket")
        && load(a.acceptP2P, "SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser")
        && load(a.closeP2P, "SteamAPI_ISteamNetworking_CloseP2PSessionWithUser")
        && load(a.allowRelay, "SteamAPI_ISteamNetworking_AllowP2PPacketRelay")
        && load(a.overlayEnabled, "SteamAPI_ISteamUtils_IsOverlayEnabled")
        && load(a.dlcInstalled, "SteamAPI_ISteamApps_BIsDlcInstalled");
    if (!ok) {
        printlog("[STEAM-INVITE] %s lacks a required flat API export; Steam invites disabled.", path.c_str());
        SDL_UnloadObject(s.library);
        s.library = nullptr;
        s.disabled = true;
        return false;
    }
    return true;
}

// Steam injects its overlay only into processes it launches itself. The
// launcher starts this runtime directly, so load Steam's own overlay module
// (from the installed Steam client) before the render window exists.
void attachOverlay() {
#ifdef WINDOWS
    static bool attempted = false;
    if (attempted || s.noOverlay) return;
    attempted = true;
    if (GetModuleHandleA("gameoverlayrenderer64.dll")) {
        printlog("[STEAM-INVITE] Steam overlay already present.");
        return;
    }
#ifdef BARONY_SMOKE_TESTS
    if (std::getenv("BARONY_SMOKE_STEAM_API") && !std::getenv("BARONY_SMOKE_REAL_STEAM_INVITES")) return; // stand-in only
#endif
    char steamPath[MAX_PATH] = {};
    DWORD size = sizeof(steamPath);
    if (RegGetValueA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath", RRF_RT_REG_SZ, nullptr, steamPath, &size) != ERROR_SUCCESS) {
        printlog("[STEAM-INVITE] Steam install path unknown; overlay unavailable.");
        return;
    }
    SDL_setenv("SteamOverlayGameId", "371970", 1);
    const std::string path = std::string(steamPath) + "/gameoverlayrenderer64.dll";
    if (LoadLibraryA(path.c_str())) printlog("[STEAM-INVITE] attached Steam overlay module.");
    else printlog("[STEAM-INVITE] could not load %s; overlay unavailable.", path.c_str());
#endif
}

void tryInitialize(bool force) {
    if (!kSupported || s.disabled || s.active) return;
    const Uint32 now = SDL_GetTicks();
    if (!force && s.nextAttempt && static_cast<Sint32>(now - s.nextAttempt) < 0) return;
    s.nextAttempt = now + 3000;
    if (!loadLibrary()) {
        if (!s.loggedUnavailable && !s.disabled) printlog("[STEAM-INVITE] steam_api64.dll unavailable; Direct IP only.");
        s.loggedUnavailable = true;
        return;
    }
    // Isolated profiles have no steam_appid.txt; identify as Barony for this process.
    SDL_setenv("SteamAppId", "371970", 1);
    SDL_setenv("SteamGameId", "371970", 1);
    if (force) attachOverlay();
    if (!s.api.initSafe()) {
        if (!s.loggedUnavailable) printlog("[STEAM-INVITE] Steam client unavailable; retrying. Direct IP still works.");
        s.loggedUnavailable = true;
        return;
    }
    Api& a = s.api;
    s.matchmaking = a.matchmakingIface();
    s.friends = a.friendsIface();
    s.user = a.userIface();
    s.networking = a.networkingIface();
    s.utils = a.utilsIface();
    s.apps = a.appsIface();
    if (!s.matchmaking || !s.friends || !s.user || !s.networking || !s.utils) {
        printlog("[STEAM-INVITE] Steam interfaces unavailable; retrying.");
        a.shutdown();
        return;
    }
    s.self = a.steamId(s.user);
    a.allowRelay(s.networking, true);
    for (size_t i = 0; i < sizeof(kCallbackIds) / sizeof(kCallbackIds[0]); ++i) {
        callbackObjects[i].vtable = callbackTable;
        callbackObjects[i].flags = 0;
        callbackObjects[i].id = 0;
        a.registerCallback(&callbackObjects[i], kCallbackIds[i]);
    }
    s.callbacksRegistered = true;
    s.active = true;
    printlog("[STEAM-INVITE] ready user=%s overlay=%d", toString(s.self).c_str(),
        s.utils && a.overlayEnabled(s.utils) ? 1 : 0);
}

u64 peerAt(const IPaddress& address) {
    const std::uint32_t host = SDL_SwapBE32(address.host);
    const std::uint32_t index = host & 0x00FFFFFFu;
    if (index == 0 || index > s.peers.size()) return 0;
    return s.peers[index - 1];
}

IPaddress addressFor(u64 peer) {
    auto it = std::find(s.peers.begin(), s.peers.end(), peer);
    std::uint32_t index;
    if (it == s.peers.end()) {
        s.peers.push_back(peer);
        index = static_cast<std::uint32_t>(s.peers.size());
    } else {
        index = static_cast<std::uint32_t>(it - s.peers.begin()) + 1;
    }
    IPaddress address;
    address.host = SDL_SwapBE32(kVirtualNet | index);
    address.port = SDL_SwapBE16(kVirtualPort);
    return address;
}

bool lobbyMember(u64 lobby, u64 user) {
    if (!lobby || !user) return false;
    const int count = s.api.numMembers(s.matchmaking, lobby);
    for (int i = 0; i < count; ++i) {
        if (s.api.memberAt(s.matchmaking, lobby, i) == user) return true;
    }
    return false;
}

// A remote may exchange packets with us if it belongs to our hosted lobby,
// is the owner of the lobby we joined, or already holds a virtual address.
bool trusted(u64 remote) {
    if (!remote || remote == s.self) return false;
    if (s.hostLobby && lobbyMember(s.hostLobby, remote)) return true;
    if (s.joinOwner && remote == s.joinOwner) return true;
    return std::find(s.peers.begin(), s.peers.end(), remote) != s.peers.end();
}

void setHostPresence(bool joinable) {
    if (!s.friends) return;
    if (joinable && s.hostLobby) {
        const std::string connect = "+connect_lobby " + toString(s.hostLobby);
        s.api.setRichPresence(s.friends, "connect", connect.c_str());
        s.api.setRichPresence(s.friends, "status", "Hosting with vending_machine framework");
    } else {
        s.api.clearRichPresence(s.friends);
    }
}

void failJoin(const std::string& error) {
    printlog("[STEAM-INVITE] join failed: %s", error.c_str());
    if (s.joinLobby) s.api.leaveLobby(s.matchmaking, s.joinLobby);
    s.joinLobby = 0;
    s.joinOwner = 0;
    s.joinCall = 0;
    s.join = JoinState::Failed;
    s.joinError = error;
}

void lobbyCreated(bool ioFailure, const LobbyCreated& created) {
    s.createCall = 0;
    if (ioFailure || created.result != 1 || !created.lobby) {
        printlog("[STEAM-INVITE] lobby creation failed result=%d io=%d; Direct IP hosting continues.", created.result, ioFailure ? 1 : 0);
        return;
    }
    if (!s.hostRequested) {
        s.api.leaveLobby(s.matchmaking, created.lobby);
        return;
    }
    s.hostLobby = created.lobby;
    Api& a = s.api;
    const std::string protocol = std::to_string(static_cast<int>(Raid::protocolVersion));
    a.setLobbyData(s.matchmaking, s.hostLobby, "ver", ("rmf-" + protocol).c_str()); // retail rejects
    a.setLobbyData(s.matchmaking, s.hostLobby, "rmf", std::to_string(kInviteProtocol).c_str());
    a.setLobbyData(s.matchmaking, s.hostLobby, "rmf_protocol", protocol.c_str());
    a.setLobbyData(s.matchmaking, s.hostLobby, "rmf_mode", s.hostMode.c_str());
    a.setLobbyData(s.matchmaking, s.hostLobby, "rmf_host", toString(s.self).c_str());
    a.setJoinable(s.matchmaking, s.hostLobby, true);
    setHostPresence(true);
    s.closedForGame = false;
    printlog("[STEAM-INVITE] lobby ready id=%s mode=%s", toString(s.hostLobby).c_str(), s.hostMode.c_str());
}

void lobbyEntered(bool ioFailure, const LobbyEnter& entered) {
    if (s.join != JoinState::Joining || entered.lobby != s.joinLobby) return;
    if (ioFailure || entered.response != 1) {
        failJoin("The Steam lobby is full, closed or no longer exists.");
        return;
    }
    Api& a = s.api;
    const char* invite = a.getLobbyData(s.matchmaking, s.joinLobby, "rmf");
    if (!invite || !*invite) {
        failJoin("This invite is for unmodded Barony. Ask the host to start from the vending_machine framework launcher.");
        return;
    }
    if (std::atoi(invite) != kInviteProtocol) {
        failJoin("The host's Steam invite support is a different version. Update both players to the same framework release.");
        return;
    }
    const char* protocol = a.getLobbyData(s.matchmaking, s.joinLobby, "rmf_protocol");
    if (!protocol || std::atoi(protocol) != Raid::protocolVersion) {
        failJoin("The host runs a different framework runtime (protocol " + std::string(protocol ? protocol : "?")
            + ", yours " + std::to_string(static_cast<int>(Raid::protocolVersion)) + "). Use the same release on every computer.");
        return;
    }
    const std::string mode = a.getLobbyData(s.matchmaking, s.joinLobby, "rmf_mode");
    if (mode != localMode()) {
        failJoin(std::string("The host is playing ") + modeName(mode) + ". Start " + modeName(mode)
            + " from the launcher, then accept the invite again.");
        return;
    }
    const u64 owner = a.getLobbyOwner(s.matchmaking, s.joinLobby);
    if (!owner || owner == s.self) {
        failJoin("That Steam lobby belongs to you.");
        return;
    }
    s.joinOwner = owner;
    a.acceptP2P(s.networking, owner);
    s.join = JoinState::Ready;
    printlog("[STEAM-INVITE] lobby entered id=%s host=%s mode=%s", toString(s.joinLobby).c_str(), toString(owner).c_str(), mode.c_str());
}

// Steam completes lobby calls asynchronously; poll them through ISteamUtils.
template <typename T> bool pollCall(SteamAPICall& call, int id, T& result, bool& ioFailure) {
    if (!call) return false;
    ioFailure = false;
    if (!s.api.callCompleted(s.utils, call, &ioFailure)) return false;
    result = T{};
    if (!s.api.callResult(s.utils, call, &result, sizeof(result), id, &ioFailure)) ioFailure = true;
    call = 0; // each result is read once
    return true;
}

void pollCallResults() {
    bool ioFailure = false;
    LobbyCreated created{};
    if (pollCall(s.createCall, kLobbyCreated, created, ioFailure)) lobbyCreated(ioFailure, created);
    LobbyEnter entered{};
    const u64 joining = s.joinLobby;
    if (pollCall(s.joinCall, kLobbyEnter, entered, ioFailure)) {
        entered.lobby = joining;
        lobbyEntered(ioFailure, entered);
    }
}

void handleCallback(int id, const void* param) {
#ifdef BARONY_SMOKE_TESTS
    if (std::getenv("BARONY_SMOKE_STEAM_TRACE")) printlog("[STEAM-INVITE] trace callback id=%d", id);
#endif
    switch (id) {
    case kGameLobbyJoinRequested:
        {
            GameLobbyJoinRequested request;
            std::memcpy(&request, param, sizeof(request));
            requestJoin(request.lobby, "invite");
        }
        break;
    case kRichPresenceJoinRequested:
        {
            GameRichPresenceJoinRequested request;
            std::memcpy(&request, param, sizeof(request));
            request.connect[sizeof(request.connect) - 1] = '\0';
            const char* text = std::strstr(request.connect, "+connect_lobby");
            u64 lobby = 0;
            if (text && parseU64(text + std::strlen("+connect_lobby"), lobby)) requestJoin(lobby, "join-game");
            else printlog("[STEAM-INVITE] ignored Join Game request without a lobby.");
        }
        break;
    case kP2PSessionRequest:
        {
            P2PSessionRequest request;
            std::memcpy(&request, param, sizeof(request));
            if (trusted(request.remote)) {
                s.api.acceptP2P(s.networking, request.remote);
                printlog("[STEAM-INVITE] accepted relay session %s", toString(request.remote).c_str());
            } else {
                s.pendingSessions[request.remote] = SDL_GetTicks();
            }
        }
        break;
    case kP2PSessionConnectFail:
        {
            P2PSessionConnectFail fail;
            std::memcpy(&fail, param, sizeof(fail));
            printlog("[STEAM-INVITE] relay session with %s failed error=%d", toString(fail.remote).c_str(), fail.error);
        }
        break;
    default:
        break;
    }
}

#ifdef BARONY_SMOKE_TESTS
// Smoke-only: press the host's invite control once the lobby is ready (the
// lobby card callback in menus, the /invite command in game) and report what
// each peer sees once gameplay runs over the relay.
void smokeTick() {
    static bool clicked = false;
    static Uint32 lastReport = 0;
    const char* click = std::getenv("BARONY_SMOKE_STEAM_INVITE_CLICK");
    // "1": first chance (menu or game); "game": only once gameplay is running.
    const bool wanted = click && (!std::strcmp(click, "1") || (!std::strcmp(click, "game") && !intro));
    if (!clicked && wanted && multiplayer == SERVER && s.hostLobby && !s.closedForGame) {
        clicked = true;
        if (intro) {
            const std::string message = inviteFriends();
            printlog("[SMOKE][STEAM] invite button result='%s'", message.c_str());
        } else {
            consoleCommand("/invite");
            printlog("[SMOKE][STEAM] invite command issued in game");
        }
    }
    const char* report = std::getenv("BARONY_SMOKE_STEAM_REPORT");
    if (!report || intro || multiplayer == SINGLE || SDL_GetTicks() - lastReport < 1000) return;
    lastReport = SDL_GetTicks();
    std::string text = "{\"clientnum\":" + std::to_string(clientnum) + ",\"multiplayer\":" + std::to_string(multiplayer)
        + ",\"key\":" + std::to_string(uniqueGameKey) + ",\"level\":" + std::to_string(currentlevel)
        + ",\"virtual_server\":" + (isVirtual(net_server) ? "true" : "false")
        + ",\"steam_peers\":" + std::to_string(s.peers.size()) + ",\"lobby_closed\":" + (s.closedForGame ? "true" : "false")
        + ",\"connected\":[";
    for (int i = 0; i < MAXPLAYERS; ++i) text += std::string(i ? "," : "") + (client_disconnected[i] ? "false" : "true");
    text += "],\"bodies\":[";
    for (int i = 0; i < MAXPLAYERS; ++i) text += std::string(i ? "," : "") + (players[i] && players[i]->entity ? std::to_string(players[i]->entity->getUID()) : "0");
    text += "]}\n";
    if (FILE* out = std::fopen(report, "wb")) {
        std::fwrite(text.data(), 1, text.size(), out);
        std::fclose(out);
    }
}
#endif

void pump() {
    if (!s.active) return;
    s.lastTick = SDL_GetTicks();
    s.api.runCallbacks();
    pollCallResults();
    // Lobby membership can lag the first relay packet; recheck briefly.
    const Uint32 now = SDL_GetTicks();
    for (auto it = s.pendingSessions.begin(); it != s.pendingSessions.end();) {
        if (trusted(it->first)) {
            s.api.acceptP2P(s.networking, it->first);
            printlog("[STEAM-INVITE] accepted relay session %s", toString(it->first).c_str());
            it = s.pendingSessions.erase(it);
        } else if (now - it->second > kPendingSessionMs) {
            printlog("[STEAM-INVITE] ignored relay session from non-member %s", toString(it->first).c_str());
            it = s.pendingSessions.erase(it);
        } else {
            ++it;
        }
    }
    // Only Join In Progress accepts arrivals once gameplay has started.
    if (s.hostLobby && !intro && !s.closedForGame && !JoinProgress::requested()) {
        s.api.setJoinable(s.matchmaking, s.hostLobby, false);
        setHostPresence(false);
        s.closedForGame = true;
        printlog("[STEAM-INVITE] lobby closed to new arrivals for gameplay");
    }
    if (s.argumentLobby) {
        const u64 lobby = s.argumentLobby;
        s.argumentLobby = 0;
        requestJoin(lobby, "command-line");
    }
#ifdef BARONY_SMOKE_TESTS
    smokeTick();
#endif
}
} // namespace

bool parseArgument(const char* argument) {
    if (!argument) return false;
    if (s.expectArgumentLobby) {
        s.expectArgumentLobby = false;
        u64 lobby = 0;
        if (parseU64(argument, lobby)) {
            s.argumentLobby = lobby;
            return true;
        }
    }
    if (!std::strcmp(argument, "-no-steam-invites")) {
        s.disabled = true;
        return true;
    }
    if (!std::strcmp(argument, "-no-steam-overlay")) {
        s.noOverlay = true;
        return true;
    }
    if (!std::strcmp(argument, "+connect_lobby")) {
        s.expectArgumentLobby = true;
        return true;
    }
    if (!std::strncmp(argument, "-connect-lobby=", 15)) {
        u64 lobby = 0;
        if (parseInviteCode(argument + 15, lobby)) s.argumentLobby = lobby;
        return true;
    }
    return false;
}

void startup() {
#ifdef BARONY_SMOKE_TESTS
    // Automated suites must never advertise lobbies on the owner's real Steam
    // account. Opt in with the local stand-in or an explicit real-Steam check.
    if (!std::getenv("BARONY_SMOKE_STEAM_API") && !std::getenv("BARONY_SMOKE_REAL_STEAM_INVITES")) s.disabled = true;
#endif
    if (s.disabled) {
        printlog("[STEAM-INVITE] disabled by -no-steam-invites.");
        return;
    }
    tryInitialize(true);
}

void shutdown() {
    if (!s.active) return;
    networkClosed();
    if (s.callbacksRegistered) {
        for (auto& object : callbackObjects) s.api.unregisterCallback(&object);
        s.callbacksRegistered = false;
    }
    s.api.shutdown();
    s.active = false;
}

void tick() {
    if (!kSupported || s.disabled) return;
    if (!s.active) {
        tryInitialize(false);
        return;
    }
    pump();
}

bool enabled() { return kSupported && !s.disabled; }
bool active() { return s.active; }

int installedDlcMask() {
    if (!s.active || !s.apps) return -1;
    return (s.api.dlcInstalled(s.apps, 1010820) ? 1 : 0)
        | (s.api.dlcInstalled(s.apps, 1010821) ? 2 : 0)
        | (s.api.dlcInstalled(s.apps, 1010822) ? 4 : 0);
}

void hostStarted(const char* mode) {
    if (!s.active) {
        printlog("[STEAM-INVITE] hosting without Steam invites (Steam unavailable).");
        return;
    }
    if (s.hostLobby || s.createCall) return;
    s.hostRequested = true;
    s.hostMode = mode ? mode : localMode();
    s.createCall = s.api.createLobby(s.matchmaking, 1 /* friends only */, MAXPLAYERS);
    printlog("[STEAM-INVITE] creating lobby mode=%s call=%llu", s.hostMode.c_str(), static_cast<unsigned long long>(s.createCall));
}

bool hostLobbyReady() { return s.active && s.hostLobby != 0 && !s.closedForGame; }
std::uint64_t lobbyId() { return s.hostLobby; }

std::string inviteCode() { return s.hostLobby ? "steam:" + toString(s.hostLobby) : std::string(); }

std::string inviteFriends() {
    if (!s.active) return "Steam is not running. Start Steam, then host again to invite friends.";
    if (!s.hostLobby) return s.createCall ? "The Steam lobby is still being created. Try again in a moment." : "Host a lobby to invite Steam friends.";
    if (s.closedForGame) return "This game no longer accepts new players.";
    const std::string code = inviteCode();
    SDL_SetClipboardText(code.c_str());
    const bool overlay = s.utils && s.api.overlayEnabled(s.utils);
    if (overlay) {
        s.api.inviteDialog(s.friends, s.hostLobby);
        printlog("[STEAM-INVITE] opened overlay invite dialog");
        return std::string();
    }
    printlog("[STEAM-INVITE] overlay unavailable; copied invite code");
    return "The Steam overlay is unavailable, so the invite code " + code
        + " was copied. Friends can use Join Game from your Steam profile, or paste the code into Direct IP.";
}

void networkClosed() {
    if (!s.active) return;
    if (s.join == JoinState::Connecting) {
        // connectToServer closes old interfaces before it resolves our peer.
        return;
    }
    if (s.hostLobby) {
        s.api.leaveLobby(s.matchmaking, s.hostLobby);
        setHostPresence(false);
        printlog("[STEAM-INVITE] left hosted lobby %s", toString(s.hostLobby).c_str());
    }
    if (s.joinLobby && s.join != JoinState::Joining) s.api.leaveLobby(s.matchmaking, s.joinLobby);
    for (u64 peer : s.peers) s.api.closeP2P(s.networking, peer);
    s.hostRequested = false;
    s.hostLobby = 0;
    s.createCall = 0;
    s.closedForGame = false;
    if (s.join != JoinState::Joining) {
        s.join = s.join == JoinState::Failed ? JoinState::Failed : JoinState::Idle;
        s.joinLobby = 0;
        s.joinCall = 0;
    }
    s.joinOwner = 0;
    s.peers.clear();
    s.pendingSessions.clear();
}

bool parseInviteCode(const char* text, std::uint64_t& lobby) {
    if (!text) return false;
    while (*text == ' ' || *text == '\t') ++text;
    if (!SDL_strncasecmp(text, "steam:", 6)) text += 6;
    else if (!std::strncmp(text, "+connect_lobby", 14)) text += 14;
    else return false;
    return parseU64(text, lobby);
}

void requestJoin(std::uint64_t lobby, const char* source) {
    if (!lobby) return;
    printlog("[STEAM-INVITE] join requested lobby=%s source=%s", toString(lobby).c_str(), source ? source : "?");
    if (!s.active) {
        s.join = JoinState::Failed;
        s.joinError = "Steam is not running. Start Steam, then accept the invite again.";
        return;
    }
    if (!intro || multiplayer != SINGLE) {
        s.join = JoinState::Failed;
        s.joinError = "Leave your current game or lobby, then accept the invite again.";
        return;
    }
    if (s.hostLobby && lobby == s.hostLobby) {
        s.join = JoinState::Failed;
        s.joinError = "That Steam lobby belongs to you.";
        return;
    }
    if (s.joinLobby && s.joinLobby != lobby) s.api.leaveLobby(s.matchmaking, s.joinLobby);
    s.joinLobby = lobby;
    s.joinOwner = 0;
    s.join = JoinState::Joining;
    s.joinError.clear();
    s.joinCall = s.api.joinLobby(s.matchmaking, lobby);
    if (!s.joinCall) failJoin("Steam could not start joining that lobby.");
}

bool takeConnectAddress(std::string& address, std::string& error) {
    if (s.join == JoinState::Failed) {
        error = s.joinError;
        address.clear();
        s.join = JoinState::Idle;
        s.joinError.clear();
        return true;
    }
    if (s.join != JoinState::Ready) return false;
    s.join = JoinState::Connecting;
    address = "steampeer:" + toString(s.joinOwner);
    error.clear();
    return true;
}

void cancelJoin() {
    if (s.active && s.joinLobby) s.api.leaveLobby(s.matchmaking, s.joinLobby);
    s.joinLobby = 0;
    s.joinOwner = 0;
    s.joinCall = 0;
    s.join = JoinState::Idle;
    s.joinError.clear();
}

bool joinInProgress() { return s.join == JoinState::Joining || s.join == JoinState::Ready || s.join == JoinState::Connecting; }

bool isSteamAddress(const char* text) {
    return text && (!SDL_strncasecmp(text, "steam:", 6) || !std::strncmp(text, "steampeer:", 10));
}

bool resolve(const char* text, IPaddress* out) {
    if (!text || !out || std::strncmp(text, "steampeer:", 10)) return false;
    u64 peer = 0;
    if (!s.active || !parseU64(text + 10, peer)) return false;
    if (s.join == JoinState::Connecting) s.join = JoinState::Connected;
    *out = addressFor(peer);
    printlog("[STEAM-INVITE] relaying to host %s", toString(peer).c_str());
    return true;
}

bool isVirtual(const IPaddress& address) {
    return (SDL_SwapBE32(address.host) & 0xFF000000u) == kVirtualNet;
}

int send(UDPsocket sock, int channel, UDPpacket* packet) {
    if (!packet || !isVirtual(packet->address)) return SDLNet_UDP_Send(sock, channel, packet);
    if (!s.active) return 0;
    const u64 peer = peerAt(packet->address);
    if (!peer || packet->len <= 0) return 0;
    const int mode = packet->len <= kMaxUnreliable ? 0 /* unreliable */ : 2 /* reliable */;
    return s.api.sendP2P(s.networking, peer, packet->data, static_cast<std::uint32_t>(packet->len), mode, kChannel) ? 1 : 0;
}

int recv(UDPsocket sock, UDPpacket* packet) {
    if (sock) {
        const int result = SDLNet_UDP_Recv(sock, packet);
        if (result != 0) return result;
    }
    if (!s.active || !packet) return 0;
    if (SDL_GetTicks() - s.lastTick >= 10) pump();
    std::uint32_t size = 0;
    int guard = 0;
    while (guard++ < 64 && s.api.p2pAvailable(s.networking, &size, kChannel)) {
        u64 remote = 0;
        std::uint32_t read = 0;
        if (size == 0 || size > static_cast<std::uint32_t>(packet->maxlen)) {
            s.scratch.resize(std::max<std::uint32_t>(size, 1));
            s.api.readP2P(s.networking, s.scratch.data(), static_cast<std::uint32_t>(s.scratch.size()), &read, &remote, kChannel);
            continue; // oversized for the engine's packet buffer
        }
        if (!s.api.readP2P(s.networking, packet->data, static_cast<std::uint32_t>(packet->maxlen), &read, &remote, kChannel)) continue;
        if (!trusted(remote)) continue;
        packet->len = static_cast<int>(read);
        packet->channel = -1;
        packet->status = static_cast<int>(read);
        packet->address = addressFor(remote);
        return 1;
    }
    return 0;
}
}
