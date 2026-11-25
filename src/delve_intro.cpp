#include "delve_intro.hpp"
#include "main.hpp"
#include "raid.hpp"
#include "net.hpp"
#include "engine/audio/sound.hpp"
#include "rapidjson/document.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <iterator>
#include <vector>
#include <physfs.h>

namespace DelveIntro {
namespace {
SDL_AudioDeviceID device = 0;
Uint8* narration = nullptr;
Uint32 narrationBytes = 0, cursor = 0;
SDL_AudioSpec format{};
std::vector<Cue> cues;
std::atomic<Uint64> playedFrames{0};
std::atomic<int> gain{SDL_MIX_MAXVOLUME};
Uint64 began = 0;
double duration = 0;
bool running = false;

void audio(void*, Uint8* output, int bytes) {
    SDL_memset(output, 0, bytes);
    const auto count = std::min<Uint32>(bytes, narrationBytes - cursor);
    if (count) {
        SDL_MixAudioFormat(output, narration + cursor, AUDIO_S16LSB, count, gain.load());
        cursor += count;
        playedFrames.fetch_add(count / 4);
    }
}
}

void stop() {
    if (device) {
        SDL_CloseAudioDevice(device);
        device = 0;
#ifdef USE_FMOD
        if (fmod_system && music_group) music_group->setMute(false);
#endif
    }
    if (narration) SDL_FreeWAV(narration);
    narration = nullptr; narrationBytes = cursor = 0;
    if (running) printlog("[DELVE-INTRO] stopped frames=%llu", static_cast<unsigned long long>(playedFrames.load()));
    running = false;
}

bool prepare(int floor) {
    stop(); cues.clear();
    if (!Raid::requested() || floor < 1 || floor > 3) return false;
    const std::string file = floor == 1 ? "data/rmf/intro.json" : "data/rmf/intro-floor-" + std::to_string(floor) + ".json";
    const char* mount = PHYSFS_getRealDir(file.c_str());
    if (!mount) return false;
    std::ifstream input(std::string(mount) + PHYSFS_getDirSeparator() + file, std::ios::binary);
    const std::string json{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    rapidjson::Document document;
    document.Parse(json.c_str());
    if (document.HasParseError() || !document.IsObject() || !document.HasMember("audio") || !document["audio"].IsString()
        || !document.HasMember("duration_seconds") || !document["duration_seconds"].IsNumber()
        || !document.HasMember("cues") || !document["cues"].IsArray()) return false;
    if (document.HasMember("floor") && (!document["floor"].IsInt() || document["floor"].GetInt() != floor)) return false;
    duration = document["duration_seconds"].GetDouble();
    if (!std::isfinite(duration) || duration <= 0 || duration > 120) return false;
    double previousEnd = 0;
    for (const auto& entry : document["cues"].GetArray()) {
        if (!entry.IsObject() || !entry.HasMember("start") || !entry["start"].IsNumber()
            || !entry.HasMember("end") || !entry["end"].IsNumber()
            || !entry.HasMember("text") || !entry["text"].IsString()) return false;
        Cue cue{entry["start"].GetDouble(), entry["end"].GetDouble(), entry["text"].GetString()};
        if (!std::isfinite(cue.start) || !std::isfinite(cue.end) || cue.start < previousEnd
            || cue.end <= cue.start || cue.end > duration || cue.text.size() > 1024) return false;
        previousEnd = cue.end; cues.push_back(cue);
    }
    if (cues.empty()) return false;
    const std::string name = document["audio"].GetString();
    if (name.rfind("music/rmf/", 0) != 0 || name.find("..") != std::string::npos) return false;
    mount = PHYSFS_getRealDir(name.c_str());
    if (!mount) return false;
    const std::string path = std::string(mount) + PHYSFS_getDirSeparator() + name;
    if (!SDL_LoadWAV(path.c_str(), &format, &narration, &narrationBytes)
        || format.format != AUDIO_S16LSB || format.channels != 2 || format.freq != 48000
        || !narrationBytes || std::abs(narrationBytes / 4.0 / format.freq - duration) > 0.05) {
        printlog("[DELVE-INTRO] invalid/missing PCM narration: %s", SDL_GetError());
        stop(); return false;
    }
    printlog("[DELVE-INTRO] prepared cues=%u duration=%.3f", static_cast<unsigned>(cues.size()), duration);
    return true;
}

void setVolume(double volume) {
    gain.store(static_cast<int>(SDL_MIX_MAXVOLUME * std::clamp(volume, 0.0, 1.0)));
}

void start() {
    if (running || !narration || cues.empty()) return;
    cursor = 0; playedFrames.store(0); began = SDL_GetTicks64(); running = true;
    if (!no_sound) {
        if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO)) SDL_InitSubSystem(SDL_INIT_AUDIO);
        format.callback = audio; format.userdata = nullptr; format.samples = 1024;
        device = SDL_OpenAudioDevice(nullptr, 0, &format, nullptr, 0);
        if (device) {
#ifdef USE_FMOD
            if (fmod_system && music_group) music_group->setMute(true);
#endif
            SDL_PauseAudioDevice(device, 0);
        }
        else printlog("[DELVE-INTRO] audio unavailable; using subtitle clock: %s", SDL_GetError());
    }
    printlog("[DELVE-INTRO] playing audio=%d gain=%d", device != 0, gain.load());
}

bool active() { return running; }
double position() {
    return !running ? 0 : device ? playedFrames.load() / static_cast<double>(format.freq) : (SDL_GetTicks64() - began) / 1000.0;
}
bool finished() {
    return running && position() >= duration && (SDL_GetTicks64() - began) / 1000.0 >= duration + 0.15;
}
int cueIndex() {
    if (!running) return -1;
    const double now = position();
    for (int index = 0; index < static_cast<int>(cues.size()); ++index)
        if (now >= cues[index].start && now < cues[index].end) return index;
    return -1;
}
void showSubtitle() {
    const auto current = cue();
    if (!current) return;
    const Uint32 color = makeColorRGB(255, 0, 255);
    if (cueIndex() == 0)
        messageLocalPlayersColor(color, MESSAGE_WORLD, "%s", Language::get(537));
    std::string dialogue = current->text;
    std::replace(dialogue.begin(), dialogue.end(), '\n', ' ');
    messageLocalPlayersColor(color, MESSAGE_WORLD, "\"%s\"", dialogue.c_str());
}
const Cue* cue() {
    const int index = cueIndex();
    return index >= 0 ? &cues[index] : nullptr;
}
}
