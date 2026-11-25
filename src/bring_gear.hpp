#pragma once
#include <string>
namespace BringGear {
constexpr unsigned char joinCapability = 0x04;
bool parseArgument(const char* argument);
bool requested();
bool prepareCharacter(std::string& error);
void deployLocalCharacter();
void tick();
void receiveLoadout();
void receiveRequest();
bool hasLoadout(int player);
void setRemoteLoadoutReady(int player, bool ready);
}
