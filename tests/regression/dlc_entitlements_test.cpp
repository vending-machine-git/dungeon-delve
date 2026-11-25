#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include "dlc_entitlements.hpp"
#define WINDOWS 1
#define BASE_DATA_DIR "owned-game"
using Uint32 = std::uint32_t;
using Sint32 = std::int32_t;
bool enabledDLCPack1=false, enabledDLCPack2=false, enabledDLCPack3=false;
unsigned fakeMask=0, queryCount=0, initialized=0, shutdowns=0, loads=0, unloads=0;
Uint32 clockMs=100;
enum Failure { None, Library, Export, Init, Apps } failure=None;
unsigned assertions=0;
void require(bool value, const char* label) {
    ++assertions;
    if (!value) throw std::runtime_error(label);
}
void appContext() {
    const char* id=std::getenv("SteamAppId");
    require(id && std::string(id)=="371970", "correct context throughout API call");
}
Uint32 SDL_GetTicks() { return clockMs; }
const char* SDL_getenv(const char* name) { return std::getenv(name); }
int SDL_setenv(const char* name,const char* value,int overwrite) { return setenv(name,value,overwrite); }
int _putenv_s(const char* name,const char* value) { return *value ? setenv(name,value,1) : unsetenv(name); }
void printlog(const char*, ...) {}
void* SDL_LoadObject(const char*) { ++loads; return failure==Library ? nullptr : reinterpret_cast<void*>(1); }
void SDL_UnloadObject(void*) { ++unloads; }
bool apiInit() { ++initialized;appContext();return failure!=Init; }
void apiShutdown() { ++shutdowns;appContext(); }
void* apiApps() { appContext();return failure==Apps ? nullptr : reinterpret_cast<void*>(2); }
bool apiInstalled(void*,std::uint32_t app) {
    appContext();++queryCount;
    require(app>=1010820 && app<=1010822,"only official three DLC app ids queried");
    return (fakeMask & (1u<<(app-1010820)))!=0;
}
void* SDL_LoadFunction(void*,const char* name) {
    if (failure==Export) return nullptr;
    if (!std::strcmp(name,"SteamAPI_InitSafe")) return reinterpret_cast<void*>(&apiInit);
    if (!std::strcmp(name,"SteamAPI_Shutdown")) return reinterpret_cast<void*>(&apiShutdown);
    if (!std::strcmp(name,"SteamAPI_SteamApps_v008")) return reinterpret_cast<void*>(&apiApps);
    if (!std::strcmp(name,"SteamAPI_ISteamApps_BIsDlcInstalled")) return reinterpret_cast<void*>(&apiInstalled);
    throw std::runtime_error("unknown export");
}
struct Stat { int playerRace=0, stat_appearance=0; };
struct Challenge {
    bool active=false;int classnum=-1,race=-1;
    bool isActive() const { return active; }
};
struct { struct { Challenge challengeRun; } currentSession; } gameModeManager;
constexpr int NUMCLASSES=26;
bool achievement=false;
bool isAchievementUnlockedForClassUnlock(int) { return achievement; }
#include "dlc_under_test.inc"
unsigned effective() { return (enabledDLCPack1?1:0)|(enabledDLCPack2?2:0)|(enabledDLCPack3?4:0); }
void flags(unsigned mask) { enabledDLCPack1=mask&1;enabledDLCPack2=mask&2;enabledDLCPack3=mask&4; }
int main() {
    try {
        unsigned policyCases=0, characterCases=0, detectorCases=0;
        // Every standalone/Steam combination and every authoritative transition.
        for (unsigned retail=0;retail<8;++retail) for (unsigned steam=0;steam<8;++steam)
        for (unsigned next=0;next<8;++next) {
            DLC::Entitlements state;
            for (unsigned i=0;i<3;++i) if (retail&(1u<<i)) state.standalone(i);
            state.steam(true,steam);require(state.effective()==(retail|steam),"sources union");
            state.steam(false,7);require(state.effective()==(retail|steam),"unavailable retains verified state");
            state.steam(true,next);require(state.effective()==(retail|next),"authoritative negatives replace Steam only");
            state.standalone(99);require(state.effective()==(retail|next),"invalid retail pack ignored");
            DLC::Entitlements peer;require(peer.effective()==0,"no entitlement leak to another local instance");
            ++policyCases;
        }
        // Execute actual production detector for all eight results and failures.
        for (unsigned owned=0;owned<8;++owned) for (unsigned retail=0;retail<8;++retail)
        for (int f=None;f<=Apps;++f) for (int priorEnv=0;priorEnv<2;++priorEnv) {
            localDLC=DLC::Entitlements();flags(0);failure=None;fakeMask=owned;
            for(unsigned i=0;i<3;++i) if(retail&(1u<<i)) recordValidatedStandaloneDLC(i);
            if(priorEnv)setenv("SteamAppId","previous-context",1);else unsetenv("SteamAppId");
            unsigned before=queryCount;
            refreshInstalledSteamDLC(true);
            require(queryCount==before+3,"every pack queried even when standalone already owns it");
            require(effective()==(retail|owned),"actual detector ownership union");
            failure=static_cast<Failure>(f);fakeMask=0;
            const auto oldShutdowns=shutdowns;
            refreshInstalledSteamDLC(true);
            require(effective()==(f==None ? retail : (retail|owned)),"detector failure vs authoritative negatives");
            require(shutdowns-oldShutdowns==((f==None||f==Apps)?1u:0u),"balanced successful API initialization/shutdown");
            const char* id=std::getenv("SteamAppId");
            require(priorEnv ? id && std::string(id)=="previous-context" : id==nullptr,"app context restored on all paths");
            failure=None;fakeMask=owned;refreshInstalledSteamDLC(true);
            require(effective()==(retail|owned),"recovery from every failure without restart");
            ++detectorCases;
        }
        // Real backoff, forced prompt refresh, transient loss and clock wrap.
        failure=Init;clockMs=1000;refreshInstalledSteamDLC(true);
        unsigned before=loads;clockMs=3999;refreshInstalledSteamDLC(false);
        require(loads==before,"unavailable query waits 3 seconds");
        failure=None;fakeMask=3;clockMs=4000;refreshInstalledSteamDLC(false);
        require(loads==before+1,"unavailable query retries after 3 seconds");
        before=loads;clockMs=33999;refreshInstalledSteamDLC(false);require(loads==before,"successful query waits 30 seconds");
        clockMs=34000;refreshInstalledSteamDLC(false);require(loads==before+1,"successful query refreshes after 30 seconds");
        before=loads;refreshInstalledSteamDLC(true);require(loads==before+1,"prompt refresh bypasses backoff");
        clockMs=0xfffffff0u;failure=Library;refreshInstalledSteamDLC(true);before=loads;
        clockMs=100;refreshInstalledSteamDLC(false);require(loads==before,"backoff survives timer wrap");
        clockMs=2984;failure=None;refreshInstalledSteamDLC(false);require(loads==before+1,"retry fires across timer wrap");
        require(loads-unloads>0,"missing DLL paths exercised");
        // Actual native validator, all ownership combinations, classes, races,
        // cosmetic/full traits and achievement availability. Independent oracle.
        const int nativeRace[13]={1,2,3,4,5,6,7,8,13,14,15,16,17};
        for(unsigned mask=0;mask<8;++mask) for(int c=0;c<26;++c)
        for(int race=0;race<18;++race) for(int cosmetic=0;cosmetic<2;++cosmetic)
        for(int ach=0;ach<2;++ach) {
            flags(mask);achievement=ach;Stat stats{race,cosmetic};
            const int classPack=c<13 ? 0 : c<17 ? 1 : c<21 ? 2 : 4;
            const int racePack=race==0 ? 0 : race<=4 ? 1 : race<=12 ? 2 : 4;
            int expected=VALID_OK_CHARACTER;
            auto missing=[](int pack) { return pack==1?INVALID_REQUIREDLC1:pack==2?INVALID_REQUIREDLC2:INVALID_REQUIREDLC3; };
            if (race>=9 && race<=12 && !(mask&2)) expected=INVALID_REQUIREDLC2;
            else if (classPack && !(mask&classPack)) expected=missing(classPack);
            else if (racePack && !(mask&racePack)) expected=missing(racePack);
            else if (race && !cosmetic && c>=13 && race!=nativeRace[c-13] && !ach) expected=INVALID_REQUIRE_ACHIEVEMENT;
            require(isCharacterValidFromDLC(stats,c)==expected,"native class/race/cosmetic/achievement ownership matrix");
            ++characterCases;
        }
        // Native challenge exceptions remain explicit; a challenge for a class
        // does not give away its unrelated race or every class in the same pack.
        flags(0);Stat stats;gameModeManager.currentSession.challengeRun={true,13,1};
        require(isCharacterValidFromDLC(stats,13)==VALID_OK_CHARACTER,"specific challenge class exception retained");
        stats.playerRace=1;require(isCharacterValidFromDLC(stats,13)==VALID_OK_CHARACTER,"specific challenge race exception retained");
        stats.playerRace=2;require(isCharacterValidFromDLC(stats,13)==INVALID_REQUIREDLC1,"challenge cannot unlock unrelated race");
        stats.playerRace=0;require(isCharacterValidFromDLC(stats,14)==INVALID_REQUIREDLC1,"challenge cannot unlock unrelated class");
        std::cout<<"PASS policy_cases="<<policyCases<<" detector_cases="<<detectorCases<<" character_cases="<<characterCases<<" assertions="<<assertions<<"\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<"\n";return 1; }
}
