// Local multi-instance stand-in for steam_api64.dll used ONLY by smoke tests.
// It exports the flat functions the framework's Steam invite module loads and
// emulates friends-only lobbies, invites, rich presence and P2P packets between
// game processes on one machine. Lobbies live in a shared directory; packets
// travel over loopback UDP. Never ship this file or its build output.
//
// Environment:
//   MOCK_STEAM_ID       this process's 64-bit user id (required)
//   MOCK_STEAM_DIR      shared state directory (required)
//   MOCK_STEAM_OVERLAY  0 to report the overlay as unavailable (default 1)
//   MOCK_STEAM_DROP     drop every Nth unreliable packet sent (default 0 = none)
//   MOCK_STEAM_DLC      installed DLC bit mask (default 0)
#include <winsock2.h>
#include <windows.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

typedef std::uint64_t u64;
#define EXPORT extern "C" __declspec(dllexport)

#pragma pack(push, 8)
struct CallbackMsg { int user; int id; unsigned char* param; int size; };
struct LobbyCreated { int result; u64 lobby; };
struct LobbyEnter { u64 lobby; std::uint32_t permissions; bool locked; std::uint32_t response; };
struct GameLobbyJoinRequested { u64 lobby; u64 friendId; };
struct GameRichPresenceJoinRequested { u64 friendId; char connect[256]; };
struct P2PSessionRequest { u64 remote; };
#pragma pack(pop)
// Matches the MSVC CCallbackBase the real DLL calls: slot 1 is Run(void*).
struct CallbackObject { void** vtable; std::uint8_t flags; int id; };
typedef void (*RunFn)(CallbackObject*, void*);

namespace {
struct Callback { int id; std::vector<unsigned char> data; };
struct Packet { u64 from; int channel; std::vector<unsigned char> data; };

u64 self = 0;
std::string dir;
bool overlay = true;
int dropEvery = 0;
unsigned dlcMask = 0;
SOCKET sock = INVALID_SOCKET;
std::deque<Callback> callbacks;
std::vector<CallbackObject*> registered;
std::map<u64, std::vector<unsigned char>> results;
std::deque<Packet> inbox;
std::set<u64> accepted;
std::set<u64> requested;
std::set<u64> lobbies; // lobbies this process is in
u64 nextCall = 1;
unsigned sends = 0;
int dummy[8];

std::string id(u64 v) { return std::to_string(static_cast<unsigned long long>(v)); }
std::string lobbyDir(u64 lobby) { return dir + "\\lobby-" + id(lobby); }
unsigned short portFor(u64 user) { return static_cast<unsigned short>(41000 + (user % 2000)); }

void log(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    std::ofstream out(dir + "\\mock-" + id(self) + ".log", std::ios::app);
    out << GetTickCount() << " " << buf << "\n";
}

template <typename T> void push(int cbid, const T& value) {
    Callback cb;
    cb.id = cbid;
    cb.data.resize(sizeof(T));
    std::memcpy(cb.data.data(), &value, sizeof(T));
    callbacks.push_back(cb);
}

template <typename T> u64 complete(int cbid, const T& value) {
    const u64 call = (self << 8) ^ (nextCall++);
    std::vector<unsigned char> data(sizeof(T));
    std::memcpy(data.data(), &value, sizeof(T));
    results[call] = data;
    (void)cbid;
    return call;
}

bool exists(const std::string& path) { return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES; }
std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
void writeFile(const std::string& path, const std::string& text) {
    const std::string tmp = path + ".tmp" + id(self);
    { std::ofstream out(tmp, std::ios::binary | std::ios::trunc); out << text; }
    MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
}

std::map<std::string, std::string> lobbyData(u64 lobby) {
    std::map<std::string, std::string> data;
    std::istringstream in(readFile(lobbyDir(lobby) + "\\data.txt"));
    std::string line;
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq != std::string::npos) data[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return data;
}

std::vector<u64> members(u64 lobby) {
    std::vector<u64> list;
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA((lobbyDir(lobby) + "\\member-*").c_str(), &found);
    if (h == INVALID_HANDLE_VALUE) return list;
    do {
        const char* name = found.cFileName + 7;
        if (!std::strstr(name, ".tmp")) list.push_back(std::strtoull(name, nullptr, 10));
    } while (FindNextFileA(h, &found));
    FindClose(h);
    return list;
}

void poll() {
    if (sock == INVALID_SOCKET) return;
    unsigned char buf[65536];
    for (;;) {
        const int n = recv(sock, reinterpret_cast<char*>(buf), sizeof(buf), 0);
        if (n < 12) break;
        Packet p;
        std::memcpy(&p.from, buf, 8);
        std::memcpy(&p.channel, buf + 8, 4);
        p.data.assign(buf + 12, buf + n);
        if (!accepted.count(p.from) && !requested.count(p.from)) {
            requested.insert(p.from);
            push(1202, P2PSessionRequest{p.from});
            log("session request from %s", id(p.from).c_str());
        }
        inbox.push_back(std::move(p));
    }
    // Test harness delivers invites and Join Game requests as files.
    const std::string invite = dir + "\\invite-" + id(self) + ".txt";
    if (exists(invite)) {
        const u64 lobby = std::strtoull(readFile(invite).c_str(), nullptr, 10);
        if (!lobby) return; // still being written; retry next frame
        DeleteFileA(invite.c_str());
        auto data = lobbyData(lobby);
        push(333, GameLobbyJoinRequested{lobby, std::strtoull(data["__owner"].c_str(), nullptr, 10)});
        log("invite delivered lobby=%s", id(lobby).c_str());
    }
    const std::string joinGame = dir + "\\joingame-" + id(self) + ".txt";
    if (exists(joinGame)) {
        GameRichPresenceJoinRequested request{};
        const std::string text = readFile(joinGame);
        if (text.empty()) return;
        DeleteFileA(joinGame.c_str());
        std::strncpy(request.connect, text.c_str(), sizeof(request.connect) - 1);
        push(337, request);
        log("join game delivered %s", text.c_str());
    }
}
} // namespace

EXPORT bool SteamAPI_InitSafe() {
    const char* idText = std::getenv("MOCK_STEAM_ID");
    const char* dirText = std::getenv("MOCK_STEAM_DIR");
    if (!idText || !dirText) return false;
    self = std::strtoull(idText, nullptr, 10);
    dir = dirText;
    if (const char* v = std::getenv("MOCK_STEAM_OVERLAY")) overlay = std::atoi(v) != 0;
    if (const char* v = std::getenv("MOCK_STEAM_DROP")) dropEvery = std::atoi(v);
    if (const char* v = std::getenv("MOCK_STEAM_DLC")) dlcMask = static_cast<unsigned>(std::atoi(v));
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(portFor(self));
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    }
    u_long nonblocking = 1;
    ioctlsocket(sock, FIONBIO, &nonblocking);
    log("init id=%s port=%u overlay=%d", id(self).c_str(), portFor(self), overlay ? 1 : 0);
    return true;
}
EXPORT void SteamAPI_Shutdown() {
    for (u64 lobby : lobbies) DeleteFileA((lobbyDir(lobby) + "\\member-" + id(self)).c_str());
    if (sock != INVALID_SOCKET) closesocket(sock);
    sock = INVALID_SOCKET;
    log("shutdown");
}
EXPORT void SteamAPI_RegisterCallback(CallbackObject* object, int cbid) {
    object->flags |= 1;
    object->id = cbid;
    registered.push_back(object);
}
EXPORT void SteamAPI_UnregisterCallback(CallbackObject* object) {
    for (auto it = registered.begin(); it != registered.end();) it = *it == object ? registered.erase(it) : it + 1;
    object->flags &= ~1;
}
EXPORT void SteamAPI_RunCallbacks() {
    poll();
    while (!callbacks.empty()) {
        Callback cb = callbacks.front();
        callbacks.pop_front();
        for (CallbackObject* object : std::vector<CallbackObject*>(registered)) {
            if (object->id == cb.id) reinterpret_cast<RunFn>(object->vtable[1])(object, cb.data.data());
        }
    }
}
EXPORT bool SteamAPI_ISteamUtils_IsAPICallCompleted(void*, u64 call, bool* failed) {
    if (failed) *failed = false;
    return results.count(call) != 0;
}
EXPORT bool SteamAPI_ISteamUtils_GetAPICallResult(void*, u64 call, void* out, int size, int, bool* failed) {
    auto it = results.find(call);
    if (it == results.end() || static_cast<int>(it->second.size()) > size) return false;
    std::memcpy(out, it->second.data(), it->second.size());
    if (failed) *failed = false;
    results.erase(it);
    return true;
}

EXPORT void* SteamAPI_SteamMatchmaking_v009() { return &dummy[0]; }
EXPORT void* SteamAPI_SteamFriends_v017() { return &dummy[1]; }
EXPORT void* SteamAPI_SteamUser_v023() { return &dummy[2]; }
EXPORT void* SteamAPI_SteamNetworking_v006() { return &dummy[3]; }
EXPORT void* SteamAPI_SteamUtils_v010() { return &dummy[4]; }
EXPORT void* SteamAPI_SteamApps_v008() { return &dummy[5]; }

EXPORT u64 SteamAPI_ISteamMatchmaking_CreateLobby(void*, int type, int maxMembers) {
    static unsigned counter = 0;
    const u64 lobby = 109775240000000000ull + (self % 100000) * 100 + (++counter);
    CreateDirectoryA(lobbyDir(lobby).c_str(), nullptr);
    writeFile(lobbyDir(lobby) + "\\data.txt", "__owner=" + id(self) + "\n__max=" + std::to_string(maxMembers)
        + "\n__type=" + std::to_string(type) + "\n__joinable=1\n");
    writeFile(lobbyDir(lobby) + "\\member-" + id(self), "1");
    lobbies.insert(lobby);
    log("create lobby=%s type=%d max=%d", id(lobby).c_str(), type, maxMembers);
    return complete(513, LobbyCreated{1, lobby});
}
EXPORT u64 SteamAPI_ISteamMatchmaking_JoinLobby(void*, u64 lobby) {
    LobbyEnter entered{lobby, 0, false, 1};
    auto data = lobbyData(lobby);
    if (!exists(lobbyDir(lobby) + "\\data.txt") || data["__closed"] == "1") entered.response = 2; // doesn't exist
    else if (data["__joinable"] != "1") entered.response = 3; // not allowed
    else if (!exists(lobbyDir(lobby) + "\\member-" + id(self))
        && static_cast<int>(members(lobby).size()) >= std::atoi(data["__max"].c_str())) entered.response = 4; // full
    if (entered.response == 1) {
        writeFile(lobbyDir(lobby) + "\\member-" + id(self), "1");
        lobbies.insert(lobby);
    }
    log("join lobby=%s response=%u", id(lobby).c_str(), entered.response);
    return complete(504, entered);
}
EXPORT void SteamAPI_ISteamMatchmaking_LeaveLobby(void*, u64 lobby) {
    DeleteFileA((lobbyDir(lobby) + "\\member-" + id(self)).c_str());
    lobbies.erase(lobby);
    auto data = lobbyData(lobby);
    if (data["__owner"] == id(self)) {
        std::string text = readFile(lobbyDir(lobby) + "\\data.txt");
        writeFile(lobbyDir(lobby) + "\\data.txt", text + "__closed=1\n");
    }
    log("leave lobby=%s", id(lobby).c_str());
}
EXPORT bool SteamAPI_ISteamMatchmaking_SetLobbyData(void*, u64 lobby, const char* key, const char* value) {
    auto data = lobbyData(lobby);
    if (data["__owner"] != id(self)) return false;
    data[key] = value;
    std::string text;
    for (auto& kv : data) text += kv.first + "=" + kv.second + "\n";
    writeFile(lobbyDir(lobby) + "\\data.txt", text);
    log("set lobby=%s %s=%s", id(lobby).c_str(), key, value);
    return true;
}
EXPORT const char* SteamAPI_ISteamMatchmaking_GetLobbyData(void*, u64 lobby, const char* key) {
    static std::string value;
    value = lobbyData(lobby)[key];
    return value.c_str();
}
EXPORT u64 SteamAPI_ISteamMatchmaking_GetLobbyOwner(void*, u64 lobby) {
    return std::strtoull(lobbyData(lobby)["__owner"].c_str(), nullptr, 10);
}
EXPORT int SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(void*, u64 lobby) { return static_cast<int>(members(lobby).size()); }
EXPORT u64 SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex(void*, u64 lobby, int index) {
    auto list = members(lobby);
    return index >= 0 && index < static_cast<int>(list.size()) ? list[index] : 0;
}
EXPORT bool SteamAPI_ISteamMatchmaking_SetLobbyJoinable(void*, u64 lobby, bool joinable) {
    log("joinable lobby=%s value=%d", id(lobby).c_str(), joinable ? 1 : 0);
    return SteamAPI_ISteamMatchmaking_SetLobbyData(nullptr, lobby, "__joinable", joinable ? "1" : "0");
}

EXPORT void SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog(void*, u64 lobby) {
    writeFile(dir + "\\overlay-invite-" + id(self) + ".txt", id(lobby));
    log("overlay invite dialog lobby=%s", id(lobby).c_str());
}
EXPORT bool SteamAPI_ISteamFriends_SetRichPresence(void*, const char* key, const char* value) {
    writeFile(dir + "\\presence-" + id(self) + "-" + key + ".txt", value ? value : "");
    log("rich presence %s=%s", key, value ? value : "");
    return true;
}
EXPORT void SteamAPI_ISteamFriends_ClearRichPresence(void*) {
    DeleteFileA((dir + "\\presence-" + id(self) + "-connect.txt").c_str());
    DeleteFileA((dir + "\\presence-" + id(self) + "-status.txt").c_str());
    log("rich presence cleared");
}
EXPORT u64 SteamAPI_ISteamUser_GetSteamID(void*) { return self; }

EXPORT bool SteamAPI_ISteamNetworking_SendP2PPacket(void*, u64 target, const void* data, std::uint32_t size, int mode, int channel) {
    if (sock == INVALID_SOCKET || size > 60000) return false;
    accepted.insert(target); // sending implicitly accepts the session
    ++sends;
    if (dropEvery > 0 && mode == 0 && sends % dropEvery == 0) return true;
    std::vector<unsigned char> buf(12 + size);
    std::memcpy(buf.data(), &self, 8);
    std::memcpy(buf.data() + 8, &channel, 4);
    std::memcpy(buf.data() + 12, data, size);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(portFor(target));
    return sendto(sock, reinterpret_cast<const char*>(buf.data()), static_cast<int>(buf.size()), 0,
        reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == static_cast<int>(buf.size());
}
static std::deque<Packet>::iterator nextReadable(int channel) {
    for (auto it = inbox.begin(); it != inbox.end(); ++it) {
        if (it->channel == channel && accepted.count(it->from)) return it;
    }
    return inbox.end();
}
EXPORT bool SteamAPI_ISteamNetworking_IsP2PPacketAvailable(void*, std::uint32_t* size, int channel) {
    poll();
    auto it = nextReadable(channel);
    if (it == inbox.end()) return false;
    if (size) *size = static_cast<std::uint32_t>(it->data.size());
    return true;
}
EXPORT bool SteamAPI_ISteamNetworking_ReadP2PPacket(void*, void* dest, std::uint32_t capacity, std::uint32_t* size, u64* remote, int channel) {
    auto it = nextReadable(channel);
    if (it == inbox.end()) return false;
    const std::uint32_t n = static_cast<std::uint32_t>(it->data.size());
    std::memcpy(dest, it->data.data(), n < capacity ? n : capacity);
    if (size) *size = n;
    if (remote) *remote = it->from;
    inbox.erase(it);
    return true;
}
EXPORT bool SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser(void*, u64 user) {
    if (!accepted.count(user)) log("accept session %s", id(user).c_str());
    accepted.insert(user);
    return true;
}
EXPORT bool SteamAPI_ISteamNetworking_CloseP2PSessionWithUser(void*, u64 user) {
    accepted.erase(user);
    requested.erase(user);
    log("close session %s", id(user).c_str());
    return true;
}
EXPORT bool SteamAPI_ISteamNetworking_AllowP2PPacketRelay(void*, bool) { return true; }
EXPORT bool SteamAPI_ISteamUtils_IsOverlayEnabled(void*) { return overlay; }
EXPORT bool SteamAPI_ISteamApps_BIsDlcInstalled(void*, std::uint32_t app) {
    return (app == 1010820 && (dlcMask & 1)) || (app == 1010821 && (dlcMask & 2)) || (app == 1010822 && (dlcMask & 4));
}
