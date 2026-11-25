#pragma once
#include "delve_effect_audio_rules.hpp"
#include <memory>
#include <vector>

namespace MainMenu { double dungeonNarrationVolume(); }

namespace DelveEffects::FeedbackAudio {
#if !defined(USE_FMOD) && !defined(USE_OPENAL)
struct Voice {
    std::shared_ptr<const std::vector<float>> clip;
    size_t cursor=0;
    float left=0,right=0;
};
inline SDL_AudioDeviceID device=0;
inline std::array<Voice,8> voices{};
inline std::array<std::shared_ptr<const std::vector<float>>,780> clips{};
inline float volume=0;
inline bool paused=false;
inline Uint64 retryAfter=0;
inline void callback(void*,Uint8* output,int bytes) {
    auto samples=reinterpret_cast<float*>(output);
    std::fill(samples,samples+bytes/sizeof(float),0);
    if (paused) return;
    for (int frame=0;frame<bytes/static_cast<int>(sizeof(float)*2);++frame) {
        float left=0,right=0;
        for (auto& voice:voices) {
            if (!voice.clip || voice.cursor>=voice.clip->size()) continue;
            const float sample=(*voice.clip)[voice.cursor++]; left+=sample*voice.left; right+=sample*voice.right;
        }
        samples[frame*2]=.85f*std::tanh(left*volume);
        samples[frame*2+1]=.85f*std::tanh(right*volume);
    }
}
inline void stop() {
    if (device) { SDL_CloseAudioDevice(device); device=0; }
    voices={};
}
inline void update(bool active,bool pause) {
    if (!active || no_sound) { stop(); return; }
    if (!device) return;
    SDL_LockAudioDevice(device); paused=pause;
    volume=static_cast<float>(MainMenu::dungeonNarrationVolume()); SDL_UnlockAudioDevice(device);
}
inline void play(const SkillVFX::Design& design,int characterClass,int index,bool impact,Entity& anchor,int strength,bool ready=false) {
    if (no_sound || gamePaused || MainMenu::dungeonNarrationVolume()<=0 || !players[clientnum] || !players[clientnum]->entity) return;
    const auto listener=players[clientnum]->entity;
    const double distance=std::hypot(anchor.x-listener->x,anchor.y-listener->y);
    if (distance>192) return;
    if (!device) {
        if (SDL_GetTicks64()<retryAfter) return;
        if (!(SDL_WasInit(SDL_INIT_AUDIO)&SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO)!=0) {
            retryAfter=SDL_GetTicks64()+5000; printlog("[DELVE-FX] skill audio initialization unavailable: %s",SDL_GetError()); return;
        }
        SDL_AudioSpec desired{},obtained{}; desired.freq=22050; desired.format=AUDIO_F32SYS;
        desired.channels=2; desired.samples=256; desired.callback=callback;
        device=SDL_OpenAudioDevice(nullptr,0,&desired,&obtained,0);
        if (!device) { retryAfter=SDL_GetTicks64()+5000; printlog("[DELVE-FX] skill audio device unavailable: %s",SDL_GetError()); return; }
        SDL_PauseAudioDevice(device,0);
        printlog("[DELVE-FX] original SDL skill-audio fallback active");
    }
    const int key=(characterClass*10+index)*3+(ready?2:impact?1:0);
    auto clip=clips[key];
    if (!clip) {
        const auto cue=ready?Sonic::Cue{Sonic::Family::Metal,560,.14,0,false}:Sonic::cue(design,characterClass,index,impact);
        auto generated=std::make_shared<std::vector<float>>(static_cast<size_t>(cue.duration*22050));
        unsigned int random=0x39f2b17u+key*1987u;
        for (size_t frame=0;frame<generated->size();++frame) {
            random^=random<<13; random^=random>>17; random^=random<<5;
            const double noise=(random&65535)/32767.5-1;
            (*generated)[frame]=static_cast<float>(Sonic::sample(cue,frame/22050.0,noise));
        }
        clips[key]=clip=generated;
    }
    const double pan=std::clamp(std::sin(std::atan2(anchor.y-listener->y,anchor.x-listener->x)-listener->yaw),-1.0,1.0);
    const double gain=.32*(strength/64.0)/(1+distance/48);
    SDL_LockAudioDevice(device);
    auto slot=&voices[0];
    for (auto& voice:voices) if (!voice.clip || voice.cursor>=voice.clip->size()) { slot=&voice; break; }
    *slot={clip,0,static_cast<float>(gain*std::sqrt((1-pan)/2)),static_cast<float>(gain*std::sqrt((1+pan)/2))};
    volume=static_cast<float>(MainMenu::dungeonNarrationVolume()); paused=false;
    SDL_UnlockAudioDevice(device);
}
#else
inline void stop() {}
inline void update(bool,bool) {}
inline void play(const SkillVFX::Design&,int,int,bool,Entity&,int,bool=false) {}
#endif
}
