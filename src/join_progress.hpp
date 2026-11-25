#pragma once
#include <string>

namespace JoinProgress {
constexpr unsigned char joinCapability = 0x08;
bool parseArgument(const char* argument);
bool requested();
bool paused();
bool clientStarting();
void configureClientFloor();
unsigned int clientSeed();
void tick();
void rememberJoinPacket();
bool repeatHello(int& slot);
void receiveJoin();
void receiveRequest();
void receiveBegin();
void receiveData();
void receiveAck();
void receiveReady();
void receiveEquipment();
void receiveCommit();
void receivePause();
void receiveRoster();
void receiveError();
bool allowServerPacket(unsigned int command);
bool joinedLive();
unsigned int tileHash();
void resetSession();
}

namespace MainMenu {
void copyJoinSlotLocks(bool* locks);
void showJoinProgressError(const char* error);
}
