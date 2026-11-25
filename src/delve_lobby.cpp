#include "delve_lobby.hpp"
#include "draw.hpp"
#include "entity.hpp"
#include "player.hpp"
#include "raid.hpp"
#include "engine/audio/sound.hpp"
#include "ui/Frame.hpp"
#include "ui/MainMenu.hpp"
#include <atomic>
#include <cstring>
#include <limits>
#include <map>
#include <tuple>
#include <vector>
#include <physfs.h>

namespace DelveLobby {
namespace {
SDL_AudioDeviceID device = 0;
Uint8* music = nullptr;
Uint32 musicBytes = 0, cursor = 0;
bool attempted = false;
std::atomic<int> gain{0};
std::atomic<Uint64> frames{0};

// The callback only copies/mixes preloaded PCM. No IO or allocations on this thread.
void audio(void*, Uint8* output, int bytes) {
    SDL_memset(output, 0, bytes);
    int offset = 0;
    while (offset < bytes && musicBytes) {
        const int count = std::min<Uint32>(bytes - offset, musicBytes - cursor);
        SDL_MixAudioFormat(output + offset, music + cursor, AUDIO_S16LSB, count, gain.load());
        offset += count;
        cursor = (cursor + count) % musicBytes;
    }
    frames.fetch_add(bytes / 4);
}

// A private scratch list keeps preview geometry out of the world and UID allocator.
list_t props{};
Entity& scratch() { static Entity entity(0, 0, &props, nullptr); return entity; }

Shader& tavernShader() {
    static Shader shader;
    static bool ready = false;
    if (!ready) {
        static const char vertex[] = R"(
            in vec3 iPosition; in vec3 iColor; in vec3 iNormal;
            uniform mat4 uModel; uniform mat4 uProjView;
            out vec3 Color; out vec3 Normal; out vec3 World;
            void main() {
                vec4 p = uModel * vec4(iPosition, 1.0);
                World = p.xyz; Color = iColor;
                Normal = normalize(mat3(uModel) * iNormal);
                gl_Position = uProjView * p;
            })";
        static const char fragment[] = R"(
            in vec3 Color; in vec3 Normal; in vec3 World;
            uniform mat4 uColorRemap;
            uniform vec4 uLightFactor, uLightColor, uColorAdd, uCameraPos;
            uniform float uTime;
            out vec4 FragColor;
            vec3 lamp(vec3 position, vec3 color, float radius, float power) {
                vec3 p = vec3(position.x*2.0, -position.z*2.0-1.0, position.y*2.0);
                vec3 delta = p - World;
                float distance = length(delta);
                float falloff = max(0.0, 1.0-distance/(radius*2.0));
                float diffuse = .20 + .80*max(0.0,dot(normalize(Normal),normalize(delta)));
                return color * power * falloff * falloff * diffuse;
            }
            void main() {
                vec3 base = uColorRemap[0].rgb*Color.r + uColorRemap[1].rgb*Color.g + uColorRemap[2].rgb*Color.b;
                float flame = .94 + .045*sin(uTime*8.1) + .035*sin(uTime*13.7);
                vec3 light = vec3(.075,.085,.115);
                light += lamp(vec3(169,128,-5), vec3(1.0,.40,.10), 58.0, 5.4*flame);
                light += lamp(vec3(118,117,-7), vec3(1.0,.64,.27), 31.0, 3.5*flame);
                light += lamp(vec3(121,141,-7), vec3(1.0,.60,.24), 29.0, 3.2*(2.0-flame));
                light += lamp(vec3(158,79,-6), vec3(1.0,.48,.18), 36.0, 2.8*flame);
                light += lamp(vec3(176,171,-16), vec3(.22,.46,1.0), 53.0, 3.3);
                light += lamp(vec3(149,128,-22), vec3(1.0,.66,.32), 37.0, 1.8*flame);
                float haze = clamp((length(uCameraPos.xyz-World)-90.0)/350.0,0.0,.18);
                vec3 lit = mix(base*light*uLightColor.rgb, vec3(.035,.045,.075), haze);
                FragColor = vec4(lit,1.0)*uLightFactor + uColorAdd;
            })";
        shader.init("delveTavernShader");
        shader.compile(vertex, sizeof(vertex), Shader::Type::Vertex);
        shader.compile(fragment, sizeof(fragment), Shader::Type::Fragment);
        shader.bindAttribLocation("iPosition", 0);
        shader.bindAttribLocation("iColor", 1);
        shader.bindAttribLocation("iNormal", 2);
        shader.link();
        ready = true;
    }
    return shader;
}

struct Bounds {
    real_t minX=1e9, maxX=-1e9, minY=1e9, maxY=-1e9, minZ=1e9, maxZ=-1e9;
};
Bounds propBounds(int model, real_t sx=1, real_t sy=1, real_t sz=1, real_t yaw=0, real_t pitch=0) {
    using Key=std::tuple<int,real_t,real_t,real_t,real_t,real_t>;
    static std::map<Key,Bounds> cache;
    const Key key{model,sx,sy,sz,yaw,pitch};
    if (auto it=cache.find(key); it!=cache.end()) return it->second;
    mat4x4_t m,t,i;
    rotate_mat(&m,&t,360-yaw*180/PI,&i.y);t=m;
    rotate_mat(&m,&t,360-pitch*180/PI,&i.z);t=m;
    vec4_t scale=vec4(sx,sz,sy,0);
    scale_mat(&m,&t,&scale);
    Bounds bounds;
    if (model>=0 && model<nummodels) {
        const auto& mesh=polymodels[model];
        for (uint64_t f=0;f<mesh.numfaces;++f) for (int p=0;p<3;++p) {
            const auto& v=mesh.faces[f].vertex[p];
            vec4_t position=vec4(v.x,-v.z,v.y,1), transformed;
            mul_mat_vec4(&transformed,&m,&position);
            const real_t x=transformed.x/2,y=transformed.z/2,z=-transformed.y/2;
            bounds.minX=std::min(bounds.minX,x);bounds.maxX=std::max(bounds.maxX,x);
            bounds.minY=std::min(bounds.minY,y);bounds.maxY=std::max(bounds.maxY,y);
            bounds.minZ=std::min(bounds.minZ,z);bounds.maxZ=std::max(bounds.maxZ,z);
        }
    }
    cache[key]=bounds;return bounds;
}
void prop(view_t& view, int model, real_t x, real_t y, real_t z,
          real_t sx=1, real_t sy=1, real_t sz=1, real_t yaw=0, real_t pitch=0) {
    auto& e=scratch();
    e.sprite=model;e.x=x;e.y=y;e.z=z;
    e.scalex=sx;e.scaley=sz;e.scalez=sy;
    e.yaw=yaw;e.pitch=pitch;e.roll=0;
    e.focalx=e.focaly=e.focalz=0;e.flags[BRIGHT]=true;
    glDrawVoxelLit(&view,&e,REALCOLORS,tavernShader(),Vector4{1,1,1,1});
}
real_t restingPitch(int model,real_t sx,real_t sy,real_t sz,real_t yaw,real_t pitch) {
    if (model==69 || model==73 || model==58 || model==99 || model==101) {
        const auto flat=propBounds(model,sx,sy,sz,yaw,0);
        const auto turned=propBounds(model,sx,sy,sz,yaw,PI/2);
        return flat.maxZ-flat.minZ <= turned.maxZ-turned.minZ ? 0 : PI/2;
    }
    return pitch;
}
void supported(view_t& view,int model,real_t x,real_t y,real_t surface,
               real_t sx=1,real_t sy=1,real_t sz=1,real_t yaw=0,real_t pitch=0) {
    pitch=restingPitch(model,sx,sy,sz,yaw,pitch);
    const auto box=propBounds(model,sx,sy,sz,yaw,pitch);
    prop(view,model,x,y,surface-box.maxZ,sx,sy,sz,yaw,pitch);
}
void seatedPart(view_t& view, const Entity& original, const Entity& root,
                real_t x, real_t y, real_t yaw, int part, real_t chairZ) {
    if (original.flags[INVISIBLE]) return;
    // Copy only rendering fields; real character transforms/inventories stay untouched.
    auto& e = scratch();
    e.sprite = original.sprite;
    const real_t turn = yaw - root.yaw;
    const real_t dx = original.x - root.x, dy = original.y - root.y;
    e.x = x + dx * cos(turn) - dy * sin(turn);
    e.y = y + dx * sin(turn) + dy * cos(turn);
    e.z = original.z - root.z + chairZ - 1.5;
    e.yaw = original.yaw + turn;
    e.pitch = original.pitch; e.roll = original.roll;
    e.focalx = original.focalx; e.focaly = original.focaly; e.focalz = original.focalz;
    e.scalex = original.scalex; e.scaley = original.scaley; e.scalez = original.scalez;
    if (part == 2 || part == 3) {
        e.pitch = -PI / 2.5; // legs extend forward from the chair, beneath the table
        e.z -= 1.5;
        e.x -= cos(yaw); e.y -= sin(yaw);
    } else if (part == 4 || part == 5) {
        e.pitch = -PI / 8;
    }
    e.flags[BRIGHT] = true;
    glDrawVoxelLit(&view, &e, REALCOLORS, tavernShader(), Vector4{1,1,1,1});
}
}

void draw(SDL_Rect pos, const std::array<bool, 4>& occupied) {
    if (!Raid::requested() || !intro) return;
    static view_t view{};
    const auto previousFov = ::fov;
    ::fov = 48;
    view.x = 5.1; view.y = 8; view.z = -19;
    view.ang = 0; view.vang = PI / 20;
    view.winx = pos.x; view.winy = pos.y + yres - Frame::virtualScreenY;
    view.winw = pos.w; view.winh = pos.h;
    GL_CHECK_ERR(glClear(GL_DEPTH_BUFFER_BIT));
    glBeginCamera(&view, false, map);
    auto& shader = tavernShader();
    shader.bind();
    GL_CHECK_ERR(glUniformMatrix4fv(shader.uniform("uProjView"), 1, false, (float*)&view.projview));
    GL_CHECK_ERR(glUniform1f(shader.uniform("uTime"), ticks / (float)TICKS_PER_SECOND));

    // Native installed voxel props, arranged as an original tavern. No map is loaded.
    for (int x = 64; x <= 192; x += 16)
        for (int y = 48; y <= 208; y += 16)
            prop(view, 674, x, y, 13);
    for (int y = 48; y <= 208; y += 16)
        for (int z = -40; z <= 8; z += 16) {
            if (y == 128 && z >= -8) continue; // recessed hearth
            prop(view, 621, 177, y, z, 1, 1, 1, 0, PI / 2);
        }
    // Enclosing side walls give the alcove depth and leave the corners in shadow.
    for (int x = 112; x <= 176; x += 16)
        for (int z = -40; z <= 8; z += 16) {
            prop(view, 621, x, 65, z, 1, 1, 1, PI / 2, PI / 2);
            prop(view, 621, x, 191, z, 1, 1, 1, PI / 2, PI / 2);
        }
    // Tall dark timber beams and flanking stone columns frame the candlelit table.
    for (int y : {64, 96, 160, 192})
        prop(view, 674, 174, y, -14, 3.7, .18, 5, 0, PI / 2);
    prop(view, 674, 174, 128, -38, .25, 8, 4, 0, PI / 2);
    const real_t floorTop=13+propBounds(674).minZ;
    supported(view,271,159,78,floorTop,1,1.3,1);
    supported(view,271,159,178,floorTop,1,1.3,1);
    supported(view,94,159,78,floorTop-propBounds(271,1,1.3,1).maxZ+propBounds(271,1,1.3,1).minZ,.6,.6,.6);
    supported(view,94,159,178,floorTop-propBounds(271,1,1.3,1).maxZ+propBounds(271,1,1.3,1).minZ,.6,.6,.6);
    supported(view,1138,170,94,floorTop);
    supported(view,1138,170,162,floorTop);
    supported(view,162,173,128,floorTop,1.2,1.2,1);
    // Mantel and stone jambs; hanging banners and the old iron chandelier.
    prop(view, 621, 172, 128, -18, .75, 2.1, 2.0);
    prop(view, 621, 174, 116, -3, 2.0, .3, 5, 0, PI / 2);
    prop(view, 621, 174, 140, -3, 2.0, .3, 5, 0, PI / 2);
    prop(view, 1836, 175, 102, -17, .43, .43, .43, PI);
    prop(view, 1837, 175, 153, -17, .43, .43, .43, PI);
    prop(view, 1354, 149, 128, -24, .65, .65, .65);
    // Shelves of bottles/books and adventurers' stores, all installed native models.
    for (int level = 0; level < 3; ++level) {
        const real_t z = 1 - level * 8;
        prop(view, 674, 169, 83, z, .65, 1.7, 1.5);
        for (int bottle = 0; bottle < 5; ++bottle)
            supported(view, 50 + (level*2+bottle)%8, 168, 73+bottle*5, z+propBounds(674,.65,1.7,1.5).minZ, .48,.48,.48);
        supported(view,73,165,91,z+propBounds(674,.65,1.7,1.5).minZ,.55,.55,.55,PI/5,PI/2);
    }
    supported(view,1315,133,76,floorTop,.8,.8,.8);
    supported(view,1315,144,77,floorTop,.7,.7,.7,PI/9);
    supported(view,1150,143,88,floorTop,.65,.65,.65);
    supported(view,1138,170,82,floorTop,.8,.8,.8);
    supported(view,1644,171,166,floorTop,1.1,1.1,1.1,PI/2);
    const real_t chestZ=floorTop-propBounds(188,1.15,1.15,1.15).maxZ;
    prop(view,188,144,180,chestZ,1.15,1.15,1.15);
    prop(view,216,144,180,chestZ-4,1.15,1.15,1.15);
    supported(view,1850,159,184,floorTop-propBounds(271,1,1.3,1).maxZ+propBounds(271,1,1.3,1).minZ,.45,.45,.45);
    supported(view,73,172,129,-18+propBounds(621,.75,2.1,2).minZ,.65,.65,.65,PI/8,PI/2);
    supported(view,78,172,120,-18+propBounds(621,.75,2.1,2).minZ,.45,.45,.45);

    static const real_t seatX[4] = {126, 145, 145, 126};
    static const real_t seatY[4] = {99, 116, 140, 157};
    static const real_t facing[4] = {PI / 2, PI * .88, PI * 1.12, -PI / 2};
    for (int p = 0; p < 4; ++p) {
        supported(view,272,seatX[p],seatY[p],floorTop,1.15,1.15,1.05,facing[p]);
        if (!occupied[p]) continue;
        auto* root = Player::getPlayerInteractEntity(p);
        if (!root) continue;
        const real_t chairZ=floorTop-propBounds(272,1.15,1.15,1.05,facing[p]).maxZ;
        seatedPart(view, *root, *root, seatX[p], seatY[p], facing[p], -1,chairZ);
        int part = 0;
        for (node_t* node = root->children.first; node; node = node->next, ++part) {
            // The first child is the first-person weapon; held gear rests off camera.
            if (part == 0 || part == 6 || part == 7) continue;
            seatedPart(view, *static_cast<Entity*>(node->element), *root,
                       seatX[p], seatY[p], facing[p], part,chairZ);
        }
    }
    // Mesh-derived support planes keep props on the floor/tabletop.
    const real_t tableBottom=floorTop-propBounds(271,3.2,6,1).maxZ;
    const real_t tableTop=tableBottom+propBounds(271,3.2,6,1).minZ;
    prop(view,271,121,128,tableBottom,3.2,6,1);
    const auto table=propBounds(271,3.2,6,1);
    std::vector<Bounds> settings;
    bool clear=true;
    auto tableProp=[&](int model,real_t x,real_t y,real_t sx=1,real_t sy=1,real_t sz=1,real_t yaw=0,real_t pitch=0) {
        pitch=restingPitch(model,sx,sy,sz,yaw,pitch);
        auto box=propBounds(model,sx,sy,sz,yaw,pitch);
        // Keep the entire mesh inside the table, with a visible edge margin.
        x=std::clamp(x,121+table.minX+.5-box.minX,121+table.maxX-.5-box.maxX);
        y=std::clamp(y,128+table.minY+.5-box.minY,128+table.maxY-.5-box.maxY);
        box.minX+=x;box.maxX+=x;box.minY+=y;box.maxY+=y;
        for(const auto& other:settings) {
            if(box.minX<other.maxX+.15 && box.maxX+.15>other.minX
                && box.minY<other.maxY+.15 && box.maxY+.15>other.minY)clear=false;
        }
        settings.push_back(box);
        supported(view,model,x,y,tableTop,sx,sy,sz,yaw,pitch);
    };
    tableProp(94,110,132,.55,.55,.55);
    tableProp(1839,121,115,.30,.30,.30);
    tableProp(1839,124,141,.30,.30,.30);
    // No full pie used as a plate: smaller food sits directly on the wood.
    tableProp(99,115,106,.45,.45,.45);
    tableProp(101,132,118,.45,.45,.45);
    tableProp(102,131,149,.50,.50,.50);
    tableProp(99,116,153,.45,.45,.45);
    tableProp(105,112,114,.50,.50,.65);
    tableProp(105,135,127,.50,.50,.65);
    tableProp(105,135,143,.50,.50,.65);
    tableProp(795,114,141,.45,.45,.45);
    tableProp(73,122,128,1.1,1.1,.50,.15,PI/2);
    tableProp(58,126,132,.55,.55,.55,PI/3,PI/2);
    tableProp(50,110,149,.4,.4,.4);
    static bool layoutLogged=false;
    if(!layoutLogged) {
        layoutLogged=true;
        printlog("[DELVE-LOBBY] support floor=%.3f table=%.3f",floorTop,tableTop);
        printlog("[DELVE-LOBBY] check %s tabletop_support_and_clearance props=%d",clear ? "PASS" : "FAIL",int(settings.size()));
    }
    auto& flame = scratch();
    flame.sprite = SPRITE_FLAME;
    flame.flags[SPRITE] = true;
    flame.scalex = flame.scaley = flame.scalez = 1.1 + .08 * sin(ticks * .19);
    flame.focalx = flame.focaly = flame.focalz = 0;
    flame.yaw = flame.pitch = flame.roll = 0;
    flame.x = 118; flame.y = 117; flame.z = -8;
    view.luminance = .25;
    glDrawSprite(&view, &flame, REALCOLORS);
    flame.x = 121; flame.y = 141;
    glDrawSprite(&view, &flame, REALCOLORS);
    for (int n = 0; n < 32; ++n) {
        const real_t phase = fmod(ticks*.015 + n*.618, 1.0);
        flame.x = 170 + 1.4 * sin(n * 2.1);
        flame.y = 128 + 3.1 * cos(n * 2.1) * (1-phase);
        flame.z = 7 - phase*16;
        flame.scalex = flame.scaley = flame.scalez = 2.3*(1-phase)+.45;
        glDrawSprite(&view, &flame, REALCOLORS);
    }
    flame.flags[SPRITE] = false;
    if (drawingGui) GL_CHECK_ERR(glEnable(GL_BLEND));
    glEndCamera(&view, false, map);
    ::fov = previousFov;
}

void stopMusic() {
    if (device) {
        SDL_CloseAudioDevice(device); // joins callback before freeing PCM
        device = 0;
#ifdef USE_FMOD
        if (fmod_system && music_group) music_group->setMute(false);
#endif
        printlog("[DELVE-LOBBY] music stopped frames=%llu", (unsigned long long)frames.load());
    }
    if (music) SDL_FreeWAV(music);
    music = nullptr; musicBytes = cursor = 0;
    attempted = false;
}

void tickMusic(bool visible, real_t volume) {
    if (!visible || !Raid::requested() || !intro) { stopMusic(); return; }
    gain.store(static_cast<int>(SDL_MIX_MAXVOLUME * std::clamp<real_t>(volume, 0, 1)));
    if (attempted || no_sound) return;
    attempted = true;
    const char* name = "music/rmf/embers-before-the-delve.wav";
    const char* mount = PHYSFS_getRealDir(name);
    if (!mount) { printlog("[DELVE-LOBBY] music asset missing: %s", name); return; }
    std::string path = std::string(mount) + PHYSFS_getDirSeparator() + name;
    SDL_AudioSpec spec{};
    if (!SDL_LoadWAV(path.c_str(), &spec, &music, &musicBytes)
        || spec.format != AUDIO_S16LSB || spec.channels != 2) {
        printlog("[DELVE-LOBBY] could not load stereo PCM music: %s", SDL_GetError());
        return;
    }
    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        printlog("[DELVE-LOBBY] audio unavailable: %s", SDL_GetError()); return;
    }
    spec.callback = audio; spec.userdata = nullptr; spec.samples = 1024;
    device = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
    if (!device) { printlog("[DELVE-LOBBY] audio device unavailable: %s", SDL_GetError()); return; }
#ifdef USE_FMOD
    if (fmod_system && music_group) music_group->setMute(true);
#endif
    cursor = 0; frames.store(0);
    SDL_PauseAudioDevice(device, 0);
    printlog("[DELVE-LOBBY] music playing rate=%d bytes=%u gain=%d", spec.freq, musicBytes, gain.load());
}
#ifdef BARONY_SMOKE_TESTS
bool musicPlaying() { return device != 0; }
Uint64 musicFrames() { return frames.load(); }
#endif
}
