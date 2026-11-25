#pragma once
#include <cstdlib>
#include <string>
#include <algorithm>
namespace SmokeHooksCommon {
inline std::string parseEnvString(const char* name, const char* fallback) {
 const char* value = std::getenv(name); return value ? value : fallback;
}
inline int parseEnvInt(const char* name, int fallback, int low, int high) {
 const char* value = std::getenv(name); return value ? std::clamp(std::atoi(value),low,high) : fallback;
}
}
