#include "main.hpp"
#include "game.hpp"
#include "entity.hpp"
#include "stat.hpp"
#include "player.hpp"
#include "net.hpp"
#include "raid.hpp"
#include "delve_builds.hpp"
#include "delve_passives.hpp"
#include "delve_effects.hpp"
#include "delve_hud.hpp"
#include "delve_skill_vfx.hpp"
#include "delve_perk_vfx.hpp"
#include "delve_effect_audio.hpp"
#include "engine/audio/sound.hpp"
#include "draw.hpp"
#include <cstring>

namespace DelveEffects {
namespace {
constexpr int eventLength = 48, stateLength = 286, particleLimit = 320;
constexpr std::array<std::array<unsigned char, 3>, 8> pigments{{
    {238,65,75}, {246,139,58}, {249,211,111}, {111,230,156},
    {113,201,255}, {191,139,249}, {242,242,225}, {92,105,129}
}};
std::array<int, 8> strokeModels{{-1,-1,-1,-1,-1,-1,-1,-1}};
struct EventGeometry {
    bool valid = false;
    double sourceX = 0, sourceY = 0, sourceZ = 0, yaw = 0, targetX = 0, targetY = 0, targetZ = 0;
};
EventGeometry receivedGeometry;
struct Mark { unsigned int uid = 0, remaining = 0; int effect = -1; };
struct Actor {
    int characterClass = -1;
    unsigned int uid = 0;
    std::array<View, 10> views{};
    std::array<unsigned int, 10> cooldowns{};
    std::array<std::array<unsigned int, 2>, 4> budgets{};
    std::array<std::array<Mark, 4>, 4> marks{};
#ifdef BARONY_SMOKE_TESTS
    std::array<std::array<unsigned int, 4>, 4> confirmedHits{};
#endif
    unsigned int soundTick = 0, hitSoundTick = 0;
    bool sounded = false, hitSounded = false, concealed = false;
};
std::array<Actor, MAXPLAYERS> actors;
std::array<unsigned int, MAXPLAYERS> bodyGenerations{};
std::array<unsigned int, MAXPLAYERS> sentEvents{}, receivedEvents{}, sentStates{}, receivedStates{};
unsigned int lastSend = 0, lastPulse = 0, generation = 1;
int floor = -1, particles = 0;
#ifdef BARONY_SMOKE_TESTS
bool quiet = false;
bool galleryPose = false;
std::array<std::array<unsigned int, 10>, MAXPLAYERS> counts{};
unsigned int networkEvents = 0, networkStates = 0;
#endif
bool validPlayer(int player) { return player >= 0 && player < MAXPLAYERS; }
bool live(int player) {
    return validPlayer(player) && Raid::active() && !Raid::matchComplete() && !client_disconnected[player]
        && Raid::outcome(player) == Raid::Outcome::Playing && !Raid::protectedPlayer(player)
        && players[player] && players[player]->entity && stats[player] && stats[player]->HP > 0;
}
bool fromHost() {
    return multiplayer == CLIENT && net_packet->address.host == net_server.host
        && net_packet->address.port == net_server.port;
}
bool silent() {
#ifdef BARONY_SMOKE_TESTS
    return quiet;
#else
    return false;
#endif
}
const SkillVFX::Design& authoredDesign(int characterClass, int index) {
    if (index >= 6) return SkillVFX::get(characterClass,index);
    static const auto perks = []() {
        std::array<SkillVFX::Design,156> result{};
        for (int characterClass=0; characterClass<26; ++characterClass) for (int index=0; index<6; ++index)
            result[characterClass*6+index]=SkillVFX::perkDesign(characterClass,DelvePassives::rule(characterClass,index));
        return result;
    }();
    return perks[characterClass*6+index];
}
Profile presentation(int characterClass, int index) {
    auto effect = profile(characterClass, index);
    if (validProfile(characterClass, index)) {
        const auto& design = authoredDesign(characterClass, index);
        effect.primary = 168 + design.primary; effect.secondary = 168 + design.secondary;
        effect.sound = design.sound;
    }
    return effect;
}
bool ensure(int player) {
    if (!live(player)) return false;
    const int characterClass = DelveBuilds::activeClass(player);
    if (characterClass < 0 || characterClass >= 26) return false;
    auto& actor = actors[player];
    const auto uid = players[player]->entity->getUID();
    if (actor.uid != uid || actor.characterClass != characterClass) {
        actor = Actor{}; actor.uid = uid; actor.characterClass = characterClass;
        ++bodyGenerations[player];
    }
    actor.concealed = players[player]->entity->isInvisible() || players[player]->entity->flags[INVISIBLE];
    return true;
}
int effectForGate(SkillVFX::Gate gate) {
    using Gate = SkillVFX::Gate;
    switch (gate) {
        case Gate::Strength: return EFF_POTION_STR;
        case Gate::Speed: return EFF_FAST;
        case Gate::Regeneration: return EFF_HP_REGEN;
        case Gate::Resistance: return EFF_MAGICRESIST;
        case Gate::Reflection: return EFF_MAGICREFLECT;
        case Gate::Sight: return EFF_TELEPATH;
        case Gate::Levitation: return EFF_LEVITATING;
        case Gate::Amplification: return EFF_MAGICAMPLIFY;
        case Gate::Mana: return EFF_MP_REGEN;
        case Gate::Hidden: return EFF_INVISIBLE;
        default: return -1;
    }
}
unsigned int gateRemaining(int player, int index, SkillVFX::Gate gate) {
    if (!validPlayer(player) || index < 6 || !validProfile(actors[player].characterClass, index)) return 0;
    const auto& actor = actors[player];
    const auto& ability = DelveBuilds::option(actor.characterClass, index).ability;
    unsigned int remaining = 0;
    for (int effectIndex = 0; effectIndex < 2; ++effectIndex)
        if (ability.self[effectIndex].id == effectForGate(gate)) remaining = std::max(remaining, actor.budgets[index - 6][effectIndex]);
    return remaining;
}
SkillVFX::Layer perkLayer(const SkillVFX::Design& design, int layerIndex, bool building) {
    auto layer=design.layers[layerIndex];
    if (building && layer.stage==SkillVFX::Stage::Active) {
        layer.shape=design.layers[0].shape; layer.motion=SkillVFX::Motion::Pulse;
        layer.anchor=SkillVFX::Anchor::Hands; layer.size=2.2; layer.tone=1;
    }
    return layer;
}
bool clearStroke(double originX, double originY, double endX, double endY) {
    const double distance = std::hypot(endX - originX, endY - originY);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / 3)));
    for (int step = 0; step <= steps; ++step) {
        const double fraction = step / static_cast<double>(steps);
        const int tileX = static_cast<int>(std::floor((originX + (endX - originX) * fraction) / 16));
        const int tileY = static_cast<int>(std::floor((originY + (endY - originY) * fraction) / 16));
        if (tileX < 0 || tileY < 0 || tileX >= map.width || tileY >= map.height
            || map.tiles[OBSTACLELAYER + MAPLAYERS * (tileY + tileX * map.height)]) return false;
    }
    return true;
}
bool updateStroke(Entity& particle) {
    using namespace SkillVFX;
    const int player = particle.skill[6], characterClass = particle.skill[7], index = particle.skill[8];
    if (!live(player) || !validProfile(characterClass, index) || actors[player].uid != static_cast<unsigned int>(particle.skill[13])
        || actors[player].characterClass != characterClass || bodyGenerations[player] != static_cast<unsigned int>(particle.skill[20])) return false;
    auto caster = players[player]->entity;
    const auto& design = authoredDesign(characterClass, index);
    const bool building=index<6 && actors[player].views[index].stacks>0 && !actors[player].views[index].charges;
    const auto layer = perkLayer(design,particle.skill[9],particle.skill[21]!=0);
    const auto stage = static_cast<Stage>(particle.skill[11]);
    const bool hidden = actors[player].concealed || caster->isInvisible() || caster->flags[INVISIBLE];
    const bool targetEffect = stage == Stage::Hit || stage == Stage::Control;
    if (stage == Stage::Active && player == clientnum) return false;
    if (hidden && player != clientnum) return false;
    if (!targetEffect && particle.skill[12] && player != clientnum) return false;
    if (stage == Stage::Active && (index >= 6 ? !gateRemaining(player,index,layer.gate)
        : !actors[player].views[index].remaining && !actors[player].views[index].charges)) return false;
    if (index<6 && stage==Stage::Active && building!=(particle.skill[21]!=0)) return false;
    auto target = particle.skill[14] ? uidToEntity(static_cast<unsigned int>(particle.skill[14])) : nullptr;
    if (targetEffect && (!target || target->isInvisible() || target->flags[INVISIBLE])) return false;
    if (stage == Stage::Control && players[clientnum] && players[clientnum]->entity == target) return false;
    if (stage == Stage::Control) {
        bool marked = false;
        for (const auto& mark : actors[player].marks[index - 6])
            marked |= mark.uid == target->getUID() && mark.remaining;
        if (!marked) return false;
    }
    const int age = particle.skill[1] - particle.skill[0];
    if (age < layer.delay) { particle.flags[INVISIBLE] = true; return true; }
    const double progress = stage == Stage::Active || stage == Stage::Control
        ? std::fmod((ticks + index * 3) / 75.0, 1.0)
        : (age - layer.delay) / static_cast<double>(std::max(1, particle.skill[1] - layer.delay));
    auto authored = layer;
    const auto& ability = DelveBuilds::option(characterClass, index).ability;
    if (authored.size < 0) authored.size = ability.range;
    auto stroke = geometry(authored, particle.skill[10], progress);
    double originX = particle.fskill[1], originY = particle.fskill[2], originZ = particle.fskill[3], yaw = particle.fskill[4];
    double pitch = 0;
    bool cameraAligned = false;
    // Keep private glyphs inside room headroom; matching size/width scaling preserves screen size.
    const double viewScale = layer.anchor == Anchor::Link ? 1 : .25;
    const auto& camera = cameras[clientnum];
    // Native voxel translation is -entity.z * 2 - 1; camera Z uses twice that scale.
    const Point eye{camera.x * 16, camera.y * 16, (camera.z - 1) / 2};
    const auto cameraOrigin = [&](double forward) { return cameraPoint(eye,camera.ang,camera.vang,{forward,0,0}); };
    const auto alignToCamera = [&](double forward) {
        const auto origin = cameraOrigin(forward * viewScale);
        originX = origin.forward; originY = origin.side; originZ = origin.height;
        yaw = camera.ang; pitch = camera.vang; cameraAligned = true;
    };
    const bool follows = stage == Stage::Active || (layer.anchor != Anchor::Link && ability.range == 0);
    if (follows) { originX = caster->x; originY = caster->y; originZ = caster->z; yaw = caster->yaw; }
    if (targetEffect && layer.anchor == Anchor::Target) {
        originX = stage == Stage::Control ? target->x : particle.fskill[5];
        originY = stage == Stage::Control ? target->y : particle.fskill[6];
        originZ = stage == Stage::Control ? target->z : particle.fskill[7];
        yaw = particle.fskill[4];
        const bool overhead = layer.shape == Shape::Sleep || layer.shape == Shape::Confusion || layer.shape == Shape::Fang
            || layer.shape == Shape::Skull || layer.shape == Shape::Note;
        const bool grounded = layer.shape == Shape::Root || layer.shape == Shape::Web || layer.shape == Shape::Icicle
            || layer.shape == Shape::Snowflake || layer.shape == Shape::Steps || layer.shape == Shape::Chain;
        originZ += overhead ? -11 : grounded ? 5 : -3;
        if (players[clientnum] && players[clientnum]->entity == target) {
            alignToCamera(16);
            for (auto point : {&stroke.first,&stroke.second}) { point->forward*=.35; point->side*=.35; point->height*=.35; }
        }
    } else if (layer.anchor == Anchor::Hands) {
        if (player == clientnum) {
            alignToCamera(16);
            for (auto point : {&stroke.first, &stroke.second}) {
                const double scale = ability.range > 0 ? .7 : stage == Stage::Active ? .45 : .42;
                point->forward *= scale; point->side *= scale; point->height *= scale;
            }
        } else {
            originX += 5 * std::cos(yaw); originY += 5 * std::sin(yaw); originZ -= 3;
        }
    } else if (layer.anchor == Anchor::Feet) originZ += 6;
    else if (layer.anchor == Anchor::Body) {
        originZ -= 3;
        if (player == clientnum) {
            alignToCamera(18);
            for (auto point : {&stroke.first,&stroke.second}) { point->forward *= .3; point->side *= .3; point->height *= .3; }
        }
    }
    if (layer.anchor == Anchor::Link) {
        if (player == clientnum) alignToCamera(14);
        else { originX += 5*std::cos(yaw); originY += 5*std::sin(yaw); originZ -= 3; }
    }
    const auto world = [&](Point point) {
        if (layer.anchor == Anchor::Link) {
            double amount = point.forward;
            if (layer.shape == Shape::Arrow || layer.shape == Shape::Spear) amount = (amount + .7) / 1.7;
            if (layer.motion == Motion::Contract) amount = 1 - amount;
            double targetX = particle.fskill[5], targetY = particle.fskill[6], targetZ = particle.fskill[7] - 3;
            if (players[clientnum] && players[clientnum]->entity == target) {
                const auto endpoint = cameraOrigin(10);
                targetX = endpoint.forward; targetY = endpoint.side; targetZ = endpoint.height;
            }
            const double direction = std::atan2(targetY - originY, targetX - originX);
            return Point{originX + (targetX - originX) * std::clamp(amount,0.0,1.0) - std::sin(direction)*point.side*5,
                originY + (targetY - originY) * std::clamp(amount,0.0,1.0) + std::cos(direction)*point.side*5,
                originZ + (targetZ - originZ) * std::clamp(amount,0.0,1.0) + point.height*5};
        }
        if (cameraAligned) return cameraPoint({originX,originY,originZ},yaw,pitch,
            {point.forward * viewScale,point.side * viewScale,point.height * viewScale});
        return Point{originX + point.forward*std::cos(yaw) - point.side*std::sin(yaw),
            originY + point.forward*std::sin(yaw) + point.side*std::cos(yaw), originZ + point.height};
    };
    const auto first = world(stroke.first), second = world(stroke.second);
    const bool unobstructed = clearStroke(caster->x,caster->y,first.forward,first.side)
        && clearStroke(caster->x,caster->y,second.forward,second.side);
    particle.flags[INVISIBLE] = !unobstructed;
    if (!unobstructed) return true;
    particle.x = (first.forward + second.forward) / 2; particle.y = (first.side + second.side) / 2;
    particle.z = (first.height + second.height) / 2;
    const double differenceX = second.forward - first.forward, differenceY = second.side - first.side, differenceZ = second.height - first.height;
    const double length = std::sqrt(differenceX*differenceX + differenceY*differenceY + differenceZ*differenceZ);
    particle.yaw = std::atan2(differenceY,differenceX); particle.pitch = std::atan2(differenceZ,std::hypot(differenceX,differenceY));
    const double fade = stage == Stage::Active || stage == Stage::Control ? 1.0 : std::min(1.0, particle.skill[0] / 9.0);
    const double width = std::max(.025, layer.width * stroke.width * fade * (player==clientnum ? .75 : 1))
        * (cameraAligned ? viewScale : 1);
    particle.scalex = stroke.bead ? width * 2.6 : length * 2 + width;
    particle.scaley = particle.scalez = width * (stroke.bead ? 2.6 : 2);
    // Native voxel meshes have a -1 model-Z center; cancel it after width scaling.
    particle.focalz = particle.scaley / 2;
    return true;
}
void particleBehavior(Entity* particle) {
    const bool current = static_cast<unsigned int>(particle->skill[4]) == generation;
    if (!current || !Raid::active() || loading || intro || Raid::matchComplete() || particle->skill[0] <= 0) {
        if (current) particles = std::max(0, particles - 1);
        list_RemoveNode(particle->mynode); return;
    }
    if (gamePaused) return;
    if (!particle->skill[19]) --particle->skill[0];
    if (!updateStroke(*particle)) {
        particles = std::max(0, particles - 1); list_RemoveNode(particle->mynode);
    }
}
void authoredBurst(int player, int index, Entity& anchor, Phase phase, bool sustained) {
    using namespace SkillVFX;
    if (phase == Phase::Ready || strokeModels[0] < 0) return;
    const auto& actor = actors[player];
    const auto& design = authoredDesign(actor.characterClass, index);
    const auto stage = phase == Phase::Hit ? (sustained ? Stage::Control : Stage::Hit) : sustained ? Stage::Active : Stage::Cast;
    const bool recipient = players[clientnum] && players[clientnum]->entity == &anchor;
    if (stage == Stage::Active && player == clientnum) return;
    if (stage == Stage::Control && recipient) return;
    auto caster = players[player]->entity;
    const bool building=index<6 && actor.views[index].stacks>0 && !actor.views[index].charges;
    if (sustained) for (auto node = map.entities->first; node;) {
        auto next = node->next; auto particle = static_cast<Entity*>(node->element); node = next;
        if (particle->behavior != &particleBehavior || !particle->skill[5] || particle->skill[6] != player
            || particle->skill[8] != index || particle->skill[11] != static_cast<int>(stage)
            || static_cast<unsigned int>(particle->skill[14]) != (phase == Phase::Hit ? anchor.getUID() : 0)) continue;
        if (static_cast<unsigned int>(particle->skill[4])==generation) particles=std::max(0,particles-1);
        list_RemoveNode(particle->mynode);
    }
    for (int layerIndex = 0; layerIndex < design.count; ++layerIndex) {
        const auto layer = perkLayer(design,layerIndex,building);
        if (layer.stage != stage || (stage == Stage::Active && index >= 6 && !gateRemaining(player,index,layer.gate))) continue;
        if (building && stage==Stage::Cast && layerIndex==1) continue;
        if (sustained && particles >= particleLimit - 80) break;
        const int pigment = layer.tone == 2 ? Ivory : layer.tone == 1 ? design.secondary : design.primary;
        const int count = segments(layer.shape);
        for (int ordinal = 0; ordinal < count && particles < particleLimit; ++ordinal) {
            auto particle = newEntity(strokeModels[pigment], 1, map.entities, nullptr);
            if (!particle) continue;
            particle->flags[PASSABLE] = particle->flags[NOUPDATE] = particle->flags[UNCLICKABLE] = particle->flags[BRIGHT] = true;
            particle->sizex = particle->sizey = 0; particle->lightBonus = vec4(.12f,.12f,.12f,0.f);
            const bool privateGlyph = (player == clientnum && (layer.anchor == Anchor::Hands || layer.anchor == Anchor::Body))
                || (recipient && layer.anchor == Anchor::Target);
            particle->skill[0] = particle->skill[1] = sustained ? 13 : privateGlyph ? (stage == Stage::Hit ? 12 : 16)
                : (stage == Stage::Hit ? 28 : 38);
            particle->skill[4] = generation; particle->skill[5] = 1; particle->skill[6] = player;
            particle->skill[7] = actor.characterClass; particle->skill[8] = index; particle->skill[9] = layerIndex;
            particle->skill[10] = ordinal; particle->skill[11] = static_cast<int>(stage); particle->skill[12] = actor.concealed;
            particle->skill[13] = caster->getUID(); particle->skill[14] = phase == Phase::Hit ? anchor.getUID() : 0;
            particle->skill[20] = bodyGenerations[player];
            particle->skill[21] = building;
#ifdef BARONY_SMOKE_TESTS
            if (galleryPose && !sustained) { particle->skill[19] = 1; particle->skill[0] = std::max(1,particle->skill[1] / 2); }
#endif
            particle->fskill[1] = caster->x; particle->fskill[2] = caster->y; particle->fskill[3] = caster->z; particle->fskill[4] = caster->yaw;
            particle->fskill[5] = anchor.x; particle->fskill[6] = anchor.y; particle->fskill[7] = anchor.z;
            if (receivedGeometry.valid && !sustained) {
                particle->fskill[1] = receivedGeometry.sourceX; particle->fskill[2] = receivedGeometry.sourceY;
                particle->fskill[3] = receivedGeometry.sourceZ; particle->fskill[4] = receivedGeometry.yaw;
                particle->fskill[5] = receivedGeometry.targetX; particle->fskill[6] = receivedGeometry.targetY; particle->fskill[7] = receivedGeometry.targetZ;
            }
            particle->behavior = &particleBehavior;
            if (multiplayer != CLIENT) --entity_uids;
            particle->setUID(-3); ++particles;
            if (!updateStroke(*particle)) { --particles; list_RemoveNode(particle->mynode); }
        }
    }
}
void clearTransient(int player, int index) {
    for (auto node = map.entities->first; node;) {
        auto next = node->next; auto particle = static_cast<Entity*>(node->element); node = next;
        if (particle->behavior != &particleBehavior || !particle->skill[5] || particle->skill[6] != player || (index >= 0 && particle->skill[8] != index)
            || (particle->skill[11] != static_cast<int>(SkillVFX::Stage::Cast) && particle->skill[11] != static_cast<int>(SkillVFX::Stage::Hit))) continue;
        if (static_cast<unsigned int>(particle->skill[4])==generation) particles=std::max(0,particles-1);
        list_RemoveNode(particle->mynode);
    }
}
bool visible(Entity* entity, int owner, bool impactEvent) {
    if (!entity || entity->flags[INVISIBLE] || entity->isInvisible()) return false;
    if (!impactEvent && owner == clientnum) return true;
    return true;
}
void burst(int player, int index, Entity* anchor, Phase phase, bool sustained = false) {
    if (silent() || !validPlayer(player) || !anchor || !validProfile(actors[player].characterClass, index)) return;
    const bool hit = phase == Phase::Hit;
    const bool privateCast = !hit && player == clientnum && actors[player].concealed;
    if (!hit && actors[player].concealed && !privateCast) return;
    if (!visible(anchor, player, hit) && !privateCast) return;
    authoredBurst(player,index,*anchor,phase,sustained);
}
void broadcast(int length, int only = -1) {
    if (silent()) return;
    for (int peer = 1; peer < MAXPLAYERS; ++peer) {
        if (client_disconnected[peer] || !players[peer] || players[peer]->isLocalPlayer() || (only >= 0 && only != peer)) continue;
        net_packet->len = length; net_packet->address = net_clients[peer - 1];
        sendPacketSafe(net_sock, -1, net_packet, peer - 1);
    }
}
void impactSound(int player, int index, Entity* target) {
    if (silent() || !target || target->isInvisible() || target->flags[INVISIBLE] || !validPlayer(player) || index < 6) return;
    auto& actor = actors[player];
    if ((actor.concealed && player != clientnum) || (actor.hitSounded && ticks-actor.hitSoundTick < TICKS_PER_SECOND/4)) return;
    const auto& ability = DelveBuilds::option(actor.characterClass,index).ability;
    int sound = ability.physical ? 28 : 169;
    if (actor.characterClass == 16 && index == 7) sound = 249;
    else if (actor.characterClass == 22) sound = 153;
    else if (ability.leech) sound = 174;
    else if (ability.control == EFF_FEAR) sound = 687;
    else if (ability.control == EFF_ASLEEP) sound = actor.characterClass == 21 ? 689 : 683;
    else for (const auto& layer : SkillVFX::get(actor.characterClass,index).layers) {
        if (layer.stage != SkillVFX::Stage::Hit) continue;
        if (layer.shape == SkillVFX::Shape::Arrow) { sound=72; break; }
        if (layer.shape == SkillVFX::Shape::Snowflake || layer.shape == SkillVFX::Shape::Icicle) { sound=172; break; }
        if (!ability.physical && layer.shape == SkillVFX::Shape::Lightning) { sound=173; break; }
    }
    playSoundEntityLocal(target,sound,40); actor.hitSounded=true; actor.hitSoundTick=ticks;
    FeedbackAudio::play(authoredDesign(actor.characterClass,index),actor.characterClass,index,true,*target,40);
}
void event(int player, int index, Phase phase, Entity* target = nullptr, unsigned int duration = 0) {
    if (!ensure(player) || !validProfile(actors[player].characterClass, index)) return;
    auto& actor = actors[player]; auto anchor = target ? target : players[player]->entity;
#ifdef BARONY_SMOKE_TESTS
    if (phase != Phase::Ready) ++counts[player][index];
#endif
    auto& indicator = actor.views[index];
    indicator.flash = TICKS_PER_SECOND;
    if (!indicator.remaining) indicator.phase = phase;
    if (phase != Phase::Ready || player == clientnum) burst(player, index, anchor, phase);
    if (phase == Phase::Hit) impactSound(player,index,anchor);
    const bool ownerOnly = players[player]->entity->isInvisible() || players[player]->entity->flags[INVISIBLE];
    if (!silent() && phase != Phase::Hit && (phase != Phase::Ready || player == clientnum) && (!ownerOnly || player == clientnum)
        && (!actor.sounded || ticks - actor.soundTick >= TICKS_PER_SECOND / 4)) {
        playSoundEntityLocal(anchor, phase == Phase::Ready ? 251 : presentation(actor.characterClass, index).sound,
            phase == Phase::Ready ? 24 : (index < 6 ? 36 : 64));
        FeedbackAudio::play(authoredDesign(actor.characterClass,index),actor.characterClass,index,false,*anchor,phase==Phase::Ready?20:index<6?36:64,phase==Phase::Ready);
        actor.soundTick = ticks; actor.sounded = true;
    }
    std::memcpy(net_packet->data, "DFXE", 4); SDLNet_Write32(uniqueGameKey, net_packet->data + 4);
    SDLNet_Write32(++sentEvents[player], net_packet->data + 8); SDLNet_Write32(currentlevel, net_packet->data + 12);
    net_packet->data[16] = player; net_packet->data[17] = actor.characterClass;
    net_packet->data[18] = index; net_packet->data[19] = static_cast<int>(phase);
    SDLNet_Write32(actor.uid, net_packet->data + 20); SDLNet_Write32(target ? target->getUID() : 0, net_packet->data + 24);
    SDLNet_Write16(anchor->x, net_packet->data + 28); SDLNet_Write16(anchor->y, net_packet->data + 30);
    SDLNet_Write16(static_cast<Sint16>(anchor->z), net_packet->data + 32);
    SDLNet_Write16(static_cast<unsigned int>(anchor->yaw * 1000) % 6284, net_packet->data + 34);
    SDLNet_Write16(std::min<unsigned int>(duration, 60000), net_packet->data + 36);
    if (index<6) { net_packet->data[36]=std::clamp(indicator.charges,0,255); net_packet->data[37]=std::clamp(indicator.stacks,0,255); }
    const auto source = players[player]->entity;
    SDLNet_Write16(source->x, net_packet->data + 38); SDLNet_Write16(source->y, net_packet->data + 40);
    SDLNet_Write16(static_cast<Sint16>(source->z), net_packet->data + 42);
    SDLNet_Write16(static_cast<unsigned int>(source->yaw * 1000) % 6284, net_packet->data + 44);
    net_packet->data[46] = ownerOnly ? 1 : 0; net_packet->data[47] = 0;
#ifdef BARONY_SMOKE_TESTS
    net_packet->data[47] = galleryPose ? 1 : 0;
#endif
    broadcast(eventLength, phase == Phase::Ready || ownerOnly ? player : -1);
}
void sendState(int player) {
    const auto& actor = actors[player];
    if (!validProfile(actor.characterClass, 0) || !actor.uid) return;
    std::memcpy(net_packet->data, "DFXS", 4); SDLNet_Write32(uniqueGameKey, net_packet->data + 4);
    SDLNet_Write32(++sentStates[player], net_packet->data + 8); SDLNet_Write32(currentlevel, net_packet->data + 12);
    net_packet->data[16] = player; net_packet->data[17] = actor.characterClass; SDLNet_Write32(actor.uid, net_packet->data + 18);
    for (int index = 0; index < 10; ++index) {
        const auto& indicator = actor.views[index]; const int offset = 22 + index * 12;
        SDLNet_Write16(std::min<unsigned int>(indicator.remaining, 60000), net_packet->data + offset);
        SDLNet_Write16(std::min<unsigned int>(indicator.total, 60000), net_packet->data + offset + 2);
        SDLNet_Write16(indicator.flash, net_packet->data + offset + 4);
        net_packet->data[offset + 6] = static_cast<int>(indicator.phase);
        net_packet->data[offset + 7] = std::clamp(indicator.charges, 0, 255);
        net_packet->data[offset + 8] = std::clamp(indicator.stacks, 0, 255);
        net_packet->data[offset + 9] = actor.concealed ? 1 : 0;
        SDLNet_Write16(std::min<unsigned int>(actor.cooldowns[index], 60000), net_packet->data + offset + 10);
    }
    for (int ability = 0; ability < 4; ++ability) for (int target = 0; target < 4; ++target) {
        const auto& mark = actor.marks[ability][target]; const int offset = 142 + (ability * 4 + target) * 8;
        SDLNet_Write32(mark.uid, net_packet->data + offset); SDLNet_Write16(std::min<unsigned int>(mark.remaining, 60000), net_packet->data + offset + 4);
        net_packet->data[offset + 6] = mark.effect < 0 ? 255 : mark.effect; net_packet->data[offset + 7] = 0;
    }
    for (int ability = 0; ability < 4; ++ability) for (int effect = 0; effect < 2; ++effect)
        SDLNet_Write16(std::min<unsigned int>(actor.budgets[ability][effect],60000), net_packet->data + 270 + (ability * 2 + effect) * 2);
    broadcast(stateLength);
}
bool packet(int length, bool state) {
    if (!Raid::active() || intro || loading || Raid::matchComplete() || !fromHost() || net_packet->len != length
        || SDLNet_Read32(net_packet->data + 4) != uniqueGameKey || SDLNet_Read32(net_packet->data + 12) != static_cast<unsigned int>(currentlevel)) return false;
    const int player = net_packet->data[16], characterClass = net_packet->data[17];
    if (!live(player) || !validProfile(characterClass, 0)) return false;
    const auto uid = SDLNet_Read32(net_packet->data + (state ? 18 : 20));
    if (uid != players[player]->entity->getUID()) return false;
    if (player == clientnum && characterClass != DelveBuilds::activeClass(player)) return false;
    const auto serial = SDLNet_Read32(net_packet->data + 8);
    return serial && serial > (state ? receivedStates[player] : receivedEvents[player]);
}
}
void prepareRender(Entity& entity, int player) {
    if (player != clientnum || entity.behavior != &particleBehavior || !entity.skill[5]
        || static_cast<unsigned int>(entity.skill[4]) != generation) return;
    const int characterClass = entity.skill[7], index = entity.skill[8];
    if (!validProfile(characterClass,index)) return;
    const auto anchor = authoredDesign(characterClass,index).layers[entity.skill[9]].anchor;
    const bool ownCue = entity.skill[6] == player && (anchor == SkillVFX::Anchor::Hands
        || anchor == SkillVFX::Anchor::Body || anchor == SkillVFX::Anchor::Link);
    const bool recipientCue = players[player] && players[player]->entity
        && static_cast<unsigned int>(entity.skill[14]) == players[player]->entity->getUID();
    // Refresh after native camera smoothing, before culling, without advancing cosmetic time.
    if ((ownCue || recipientCue) && !updateStroke(entity)) entity.flags[INVISIBLE] = true;
}
void createModels() {
    strokeModels.fill(-1);
    std::array<voxel_t*,8> generated{};
    for (int pigment = 0; pigment < 8; ++pigment) {
        generated[pigment] = static_cast<voxel_t*>(std::calloc(1,sizeof(voxel_t)));
        if (generated[pigment]) generated[pigment]->data = static_cast<Uint8*>(std::calloc(1,1));
        if (!generated[pigment] || !generated[pigment]->data) {
            for (auto model : generated) if (model) { std::free(model->data); std::free(model); }
            printlog("[DELVE-FX] procedural stroke models unavailable"); return;
        }
        auto model = generated[pigment]; model->sizex = model->sizey = model->sizez = 1;
        for (int channel = 0; channel < 3; ++channel) model->palette[0][channel] = pigments[pigment][channel];
    }
    auto expanded = static_cast<voxel_t**>(std::realloc(models,sizeof(voxel_t*)*(nummodels+8)));
    if (!expanded) {
        for (auto model : generated) { std::free(model->data); std::free(model); }
        printlog("[DELVE-FX] procedural stroke model allocation failed"); return;
    }
    models = expanded;
    for (int pigment = 0; pigment < 8; ++pigment) { strokeModels[pigment] = nummodels + pigment; models[nummodels + pigment] = generated[pigment]; }
    nummodels += 8;
    printlog("[DELVE-FX] initialized eight original procedural stroke models");
}
unsigned int color(int characterClass, int index) {
    if (validProfile(characterClass,index)) {
        const auto& rgb = pigments[authoredDesign(characterClass,index).primary];
        return makeColorRGB(rgb[0],rgb[1],rgb[2]);
    }
    const auto effect = profile(characterClass, index);
    switch (effect.primary) {
        case 168: return makeColorRGB(245, 105, 100);
        case 169: return makeColorRGB(247, 168, 90);
        case 170: return makeColorRGB(242, 211, 115);
        case 171: return makeColorRGB(126, 224, 161);
        case 172: return makeColorRGB(125, 195, 247);
        case 173: return makeColorRGB(203, 156, 246);
        case 175: return makeColorRGB(180, 162, 220);
        default: return makeColorRGB(231, 231, 221);
    }
}
void reset() {
    FeedbackAudio::stop();
    actors = {}; sentEvents = {}; receivedEvents = {}; sentStates = {}; receivedStates = {};
    lastSend = lastPulse = ticks; floor = currentlevel; ++generation; particles = 0;
#ifdef BARONY_SMOKE_TESTS
    counts = {}; networkEvents = networkStates = 0;
#endif
}
void resetPlayer(int player) {
    if (validPlayer(player)) { actors[player] = Actor{}; ++bodyGenerations[player]; }
}
View view(int player, int index) {
    if (!Raid::active() || !validPlayer(player) || index < 0 || index >= 10) return {};
    return actors[player].views[index];
}
void activated(int player, int index) {
    if (multiplayer != SERVER || index < 6 || !ensure(player) || !validProfile(actors[player].characterClass, index)) return;
    auto& actor = actors[player]; auto& indicator = actor.views[index]; indicator = {};
    const auto& ability = DelveBuilds::option(actor.characterClass, index).ability;
    for (int effectIndex = 0; effectIndex < 2; ++effectIndex) {
        const auto& effect = ability.self[effectIndex];
        const unsigned int remaining = effect.id >= 0 && stats[player]->getEffectActive(effect.id)
            ? std::max(0, stats[player]->EFFECTS_TIMERS[effect.id]) : 0;
        actor.budgets[index - 6][effectIndex] = remaining;
        indicator.remaining = std::max(indicator.remaining, remaining);
    }
    indicator.total = indicator.remaining; indicator.phase = indicator.remaining ? Phase::Active : Phase::Cast;
    actor.cooldowns[index] = ability.cooldown * TICKS_PER_SECOND;
    event(player, index, Phase::Cast);
    sendState(player);
}
void impact(int player, int index, Entity& target, bool controlled) {
    if (multiplayer != SERVER || index < 6 || !ensure(player) || !validProfile(actors[player].characterClass, index)) return;
    auto& actor = actors[player];
#ifdef BARONY_SMOKE_TESTS
    if (index >= 6) for (auto& uid : actor.confirmedHits[index - 6]) if (!uid || uid == target.getUID()) { uid = target.getUID(); break; }
#endif
    if (controlled && index >= 6 && target.getStats()) {
        const int effect = DelveBuilds::option(actor.characterClass, index).ability.control;
        if (effect >= 0 && target.getStats()->getEffectActive(effect)) {
            for (auto& mark : actor.marks[index - 6]) if (!mark.uid || mark.uid == target.getUID()) {
                mark = {target.getUID(), static_cast<unsigned int>(std::max(0, target.getStats()->EFFECTS_TIMERS[effect])), effect};
                auto& indicator = actor.views[index];
                if (mark.remaining > indicator.remaining) { indicator.remaining = mark.remaining; indicator.phase = Phase::Control; }
                indicator.total = std::max(indicator.total, indicator.remaining);
                break;
            }
        }
    }
    event(player, index, Phase::Hit, &target, controlled ? TICKS_PER_SECOND : 0);
    if (controlled) sendState(player);
}
void passive(int player, int index, unsigned int remaining, int charges, int stacks, unsigned int cooldown, bool triggered) {
    if (multiplayer != SERVER || index < 0 || index >= 6 || !ensure(player)) return;
    auto& actor = actors[player]; auto& indicator = actor.views[index];
    const bool armed = charges > 0 || stacks > 0;
    const bool changed = triggered || cooldown > actor.cooldowns[index] || charges != indicator.charges || stacks > indicator.stacks;
    indicator.charges=charges; indicator.stacks=stacks;
    if (changed) {
        indicator.total = std::max(remaining, 1u); indicator.remaining = remaining;
        indicator.phase = armed ? Phase::Armed : (remaining ? Phase::Active : Phase::Proc);
        event(player, index, armed ? Phase::Armed : Phase::Proc);
    } else indicator.remaining = std::min(indicator.remaining, remaining);
    const bool ready = actor.cooldowns[index] && !cooldown && !armed;
    indicator.charges = charges; indicator.stacks = stacks; actor.cooldowns[index] = cooldown;
    if (ready) event(player, index, Phase::Ready);
    if (charges > 0) indicator.phase = Phase::Armed;
    else if (indicator.remaining) indicator.phase = Phase::Active;
    else if (!indicator.flash) indicator.phase = Phase::None;
}
void tick(unsigned int delta) {
    FeedbackAudio::update(Raid::active()&&!intro&&!loading&&!Raid::matchComplete(),gamePaused);
    if (!Raid::active() || intro || loading || Raid::matchComplete()) {
        for (auto& actor : actors) { actor.views = {}; actor.budgets = {}; actor.marks = {}; }
        return;
    }
    if (floor != currentlevel) {
        floor = currentlevel; for (auto& actor : actors) { actor.views = {}; actor.budgets = {}; actor.marks = {}; }
        ++generation; particles = 0; lastPulse = ticks;
    }
    if (gamePaused) return;
    const bool pulse = ticks - lastPulse >= 12;
    if (pulse) lastPulse = ticks;
    for (int player = 0; player < MAXPLAYERS; ++player) {
        auto& actor = actors[player];
        if (!live(player)) { actor.views = {}; actor.budgets = {}; actor.marks = {}; continue; }
        if (!actor.uid || actor.uid != players[player]->entity->getUID()) continue;
        if (multiplayer == SERVER) actor.concealed = players[player]->entity->isInvisible() || players[player]->entity->flags[INVISIBLE];
        for (int index = 0; index < 10; ++index) {
            auto& indicator = actor.views[index];
            indicator.flash -= std::min(indicator.flash, delta);
            if (multiplayer == SERVER && index >= 6 && validProfile(actor.characterClass, index)) {
                const auto& ability = DelveBuilds::option(actor.characterClass, index).ability;
                indicator.remaining = 0;
                for (int effectIndex = 0; effectIndex < 2; ++effectIndex) {
                    auto& budget = actor.budgets[index - 6][effectIndex]; budget -= std::min(budget, delta);
                    const auto& effect = ability.self[effectIndex];
                    if (effect.id < 0 || !stats[player]->getEffectActive(effect.id)) budget = 0;
                    else budget = std::min(budget, static_cast<unsigned int>(std::max(0, stats[player]->EFFECTS_TIMERS[effect.id])));
                    indicator.remaining = std::max(indicator.remaining, budget);
                }
                const auto selfRemaining = indicator.remaining;
                for (auto& mark : actor.marks[index - 6]) {
                    mark.remaining -= std::min(mark.remaining, delta);
                    auto target = mark.uid ? uidToEntity(mark.uid) : nullptr;
                    auto targetStats = target ? target->getStats() : nullptr;
                    if (!targetStats || targetStats->HP <= 0 || mark.effect < 0 || !targetStats->getEffectActive(mark.effect)) mark = {};
                    else mark.remaining = std::min(mark.remaining, static_cast<unsigned int>(std::max(0, targetStats->EFFECTS_TIMERS[mark.effect])));
                    indicator.remaining = std::max(indicator.remaining, mark.remaining);
                }
                if (indicator.remaining) indicator.phase = selfRemaining ? Phase::Active : Phase::Control;
                const auto oldCooldown = actor.cooldowns[index]; actor.cooldowns[index] -= std::min(actor.cooldowns[index], delta);
                const auto build = DelveBuilds::activeSelection(player);
                for (int slot = 0; slot < 2; ++slot) if (build[slot + 2] == index) actor.cooldowns[index] = DelveBuilds::cooldownRemaining(player, slot);
                if (oldCooldown && !actor.cooldowns[index]) event(player, index, Phase::Ready);
            } else {
                indicator.remaining -= std::min(indicator.remaining, delta);
                if (index >= 6) for (auto& budget : actor.budgets[index - 6]) budget -= std::min(budget,delta);
            }
            if (!indicator.remaining && !indicator.flash && !indicator.charges && !indicator.stacks) indicator.phase = Phase::None;
            if ((indicator.remaining || indicator.charges) && indicator.phase != Phase::Control && pulse) burst(player, index, players[player]->entity, indicator.phase, true);
            if (index >= 6) for (auto& mark : actor.marks[index - 6]) {
                if (multiplayer != SERVER) mark.remaining -= std::min(mark.remaining, delta);
                if (mark.remaining && pulse) burst(player, index, uidToEntity(mark.uid), Phase::Hit, true);
            }
        }
    }
    if (multiplayer == SERVER && ticks - lastSend >= 10) {
        lastSend = ticks; for (int player = 0; player < MAXPLAYERS; ++player) sendState(player);
    }
}
void receiveEvent() {
    if (!packet(eventLength, false)) return;
    const int player = net_packet->data[16], characterClass = net_packet->data[17], index = net_packet->data[18];
    const auto phase = static_cast<Phase>(net_packet->data[19]);
    if (!validProfile(characterClass, index) || phase == Phase::None || phase > Phase::Ready || phase == Phase::Active) return;
    bool gallery = false;
#ifdef BARONY_SMOKE_TESTS
    gallery = net_packet->data[47] == 1 && std::getenv("BARONY_EFFECTS_CAPTURE");
#endif
    if (net_packet->data[46] > 1 || (net_packet->data[47] && !gallery) || SDLNet_Read16(net_packet->data + 34) >= 6284
        || SDLNet_Read16(net_packet->data + 44) >= 6284 || SDLNet_Read16(net_packet->data + 28) >= map.width*16
        || SDLNet_Read16(net_packet->data + 30) >= map.height*16 || SDLNet_Read16(net_packet->data + 38) >= map.width*16
        || SDLNet_Read16(net_packet->data + 40) >= map.height*16
        || std::abs(static_cast<Sint16>(SDLNet_Read16(net_packet->data + 32))) > 128
        || std::abs(static_cast<Sint16>(SDLNet_Read16(net_packet->data + 42))) > 128) return;
    const auto targetUid = SDLNet_Read32(net_packet->data + 24);
    if (phase == Phase::Hit && index < 6) return;
    auto anchor = targetUid ? uidToEntity(targetUid) : players[player]->entity;
    if (phase == Phase::Hit && !targetUid) return;
    receivedEvents[player] = SDLNet_Read32(net_packet->data + 8);
    auto& actor = actors[player];
    if (actor.uid != players[player]->entity->getUID() || actor.characterClass != characterClass) { actor = Actor{}; ++bodyGenerations[player]; }
    actor.uid = players[player]->entity->getUID(); actor.characterClass = characterClass;
    actor.concealed = net_packet->data[46] != 0;
    auto& indicator = actor.views[index]; indicator.flash = TICKS_PER_SECOND;
    if (index<6) { indicator.charges=net_packet->data[36]; indicator.stacks=net_packet->data[37]; }
    if (!indicator.remaining) indicator.phase = phase;
    receivedGeometry = {true, static_cast<double>(SDLNet_Read16(net_packet->data + 38)), static_cast<double>(SDLNet_Read16(net_packet->data + 40)),
        static_cast<double>(static_cast<Sint16>(SDLNet_Read16(net_packet->data + 42))), SDLNet_Read16(net_packet->data + 44) / 1000.0,
        static_cast<double>(SDLNet_Read16(net_packet->data + 28)), static_cast<double>(SDLNet_Read16(net_packet->data + 30)),
        static_cast<double>(static_cast<Sint16>(SDLNet_Read16(net_packet->data + 32)))};
#ifdef BARONY_SMOKE_TESTS
    galleryPose = gallery;
    if (gallery && phase == Phase::Cast) clearTransient(player,-1);
#endif
    burst(player, index, anchor, phase); receivedGeometry.valid = false;
    if (phase == Phase::Hit) impactSound(player,index,anchor);
#ifdef BARONY_SMOKE_TESTS
    galleryPose = false;
#endif
    const bool hidden = actor.concealed || players[player]->entity->isInvisible() || players[player]->entity->flags[INVISIBLE];
    if (!silent() && anchor && phase != Phase::Hit && (!hidden || player == clientnum) && (phase != Phase::Ready || player == clientnum)
        && (!actor.sounded || ticks - actor.soundTick >= TICKS_PER_SECOND / 4)) {
        playSoundEntityLocal(anchor, phase == Phase::Ready ? 251 : presentation(characterClass, index).sound,
            phase == Phase::Ready ? 24 : (index < 6 ? 36 : 64));
        FeedbackAudio::play(authoredDesign(characterClass,index),characterClass,index,false,*anchor,phase==Phase::Ready?20:index<6?36:64,phase==Phase::Ready);
        actor.soundTick = ticks; actor.sounded = true;
    }
#ifdef BARONY_SMOKE_TESTS
    ++networkEvents;
#endif
}
void receiveState() {
    if (!packet(stateLength, true)) return;
    const int player = net_packet->data[16];
    for (int index = 0; index < 10; ++index) {
        const int offset = 22 + index * 12;
        if (net_packet->data[offset + 6] > static_cast<int>(Phase::Control)
            || SDLNet_Read16(net_packet->data + offset) > SDLNet_Read16(net_packet->data + offset + 2)
            || SDLNet_Read16(net_packet->data + offset + 4) > TICKS_PER_SECOND
            || net_packet->data[offset + 9] > 1 || net_packet->data[offset + 9] != net_packet->data[31]) return;
    }
    for (int ordinal = 0; ordinal < 16; ++ordinal) {
        const int offset = 142 + ordinal * 8;
        if (SDLNet_Read32(net_packet->data + offset) && (net_packet->data[offset + 6] >= NUMEFFECTS
            || SDLNet_Read16(net_packet->data + offset + 4) > 60000)) return;
    }
    for (int ordinal = 0; ordinal < 8; ++ordinal)
        if (SDLNet_Read16(net_packet->data + 270 + ordinal*2) > 60000) return;
    auto& actor = actors[player];
    if (actor.uid != SDLNet_Read32(net_packet->data + 18) || actor.characterClass != net_packet->data[17]) { actor = Actor{}; ++bodyGenerations[player]; }
    actor.characterClass = net_packet->data[17]; actor.uid = SDLNet_Read32(net_packet->data + 18);
    actor.concealed = net_packet->data[31] != 0;
    for (int index = 0; index < 10; ++index) {
        const int offset = 22 + index * 12; auto& indicator = actor.views[index];
        indicator.remaining = SDLNet_Read16(net_packet->data + offset); indicator.total = SDLNet_Read16(net_packet->data + offset + 2);
        indicator.flash = SDLNet_Read16(net_packet->data + offset + 4); indicator.phase = static_cast<Phase>(net_packet->data[offset + 6]);
        indicator.charges = net_packet->data[offset + 7]; indicator.stacks = net_packet->data[offset + 8];
        actor.cooldowns[index] = SDLNet_Read16(net_packet->data + offset + 10);
    }
    for (int ability = 0; ability < 4; ++ability) for (int target = 0; target < 4; ++target) {
        const int offset = 142 + (ability * 4 + target) * 8;
        actor.marks[ability][target] = {SDLNet_Read32(net_packet->data + offset), SDLNet_Read16(net_packet->data + offset + 4), net_packet->data[offset + 6]};
    }
    for (int ability = 0; ability < 4; ++ability) for (int effect = 0; effect < 2; ++effect)
        actor.budgets[ability][effect] = SDLNet_Read16(net_packet->data + 270 + (ability*2+effect)*2);
    receivedStates[player] = SDLNet_Read32(net_packet->data + 8);
#ifdef BARONY_SMOKE_TESTS
    ++networkStates;
#endif
}
void drawHUD(int player) {
    if (player != clientnum || !live(player) || intro || loading || fadeout || gamePaused || !players[player]->shootmode || players[player]->gui_mode != GUI_MODE_NONE) return;
    const auto build = DelveBuilds::activeSelection(player); const int characterClass = DelveBuilds::activeClass(player);
    const double scale = std::max(.75, std::min(xres / 1280.0, yres / 720.0));
    const SDL_Rect viewport{0, 0, xres, yres}; int row = 0;
    for (int selection : {2, 3, 0, 1}) {
        const int index = build[selection]; if (!validProfile(characterClass, index)) continue;
        const auto indicator = view(player, index);
        if (!indicator.remaining && !indicator.flash && !indicator.charges && !indicator.stacks) continue;
        const int panelWidth = std::lround(258 * scale), panelHeight = std::lround(36 * scale);
        const int left = xres - panelWidth - std::lround(14 * scale), top = std::lround(98 * scale) + row++ * (panelHeight + 4);
        const auto tint = color(characterClass, index);
        if (auto image = Image::get("*#images/system/white.png")) {
            image->drawColor(nullptr, {left, top, panelWidth, panelHeight}, viewport, makeColor(18, 21, 27, 215));
            image->drawColor(nullptr, {left, top, 3, panelHeight}, viewport, tint);
            const double fraction = indicator.remaining ? indicator.remaining / static_cast<double>(std::max(1u, indicator.total)) : indicator.charges ? 1.0 : indicator.flash / static_cast<double>(TICKS_PER_SECOND);
            image->drawColor(nullptr, {left + 3, top + panelHeight - 3, std::max(1, int((panelWidth - 3) * fraction)), 2}, viewport, tint);
        }
        const auto& option = DelveBuilds::option(characterClass, index);
        const auto font = scale < 1 ? "lang/en.ttf#11#1" : "lang/en.ttf#12#1";
        if (auto name = Text::get(option.name, font, tint, 0xff000000))
            name->draw({0, 0, 0, 0}, {left + 9, top + 3, int(name->getWidth()), int(name->getHeight())}, viewport);
        char label[96];
        if (indicator.charges && !indicator.remaining) snprintf(label, sizeof(label), "ARMED / %d CHARGE%s", indicator.charges, indicator.charges == 1 ? "" : "S");
        else if (indicator.stacks && !indicator.charges && indicator.remaining) snprintf(label, sizeof(label), "COMBO %d  /  %.1fs", indicator.stacks, indicator.remaining / double(TICKS_PER_SECOND));
        else if (indicator.stacks && !indicator.charges) snprintf(label, sizeof(label), "COMBO %d", indicator.stacks);
        else if (indicator.remaining && index >= 6) {
            std::string state;
            if (indicator.phase == Phase::Control) {
                switch (option.ability.control) {
                    case EFF_SLOW: state="TARGET SLOWED"; break;
                    case EFF_WEBBED: state="TARGET BOUND"; break;
                    case EFF_ASLEEP: state="TARGET ASLEEP"; break;
                    case EFF_CONFUSED: state="TARGET CONFUSED"; break;
                    case EFF_FEAR: state="TARGET AFRAID"; break;
                    case EFF_PARALYZED: state="TARGET PARALYZED"; break;
                    default: state="TARGET EFFECT"; break;
                }
            } else {
                constexpr std::array<const char*,11> labels{{"","MIGHT","HASTE","MENDING","RESIST","REFLECT","SIGHT","FLOAT","SPELLPOWER","MANA REGEN","HIDDEN"}};
                for (int gate = 1; gate < 11; ++gate) if (gateRemaining(player,index,static_cast<SkillVFX::Gate>(gate))) {
                    if (!state.empty()) state += "/";
                    state += labels[gate];
                }
            }
            snprintf(label,sizeof(label),"%s  %.1fs",state.empty()?"ACTIVE":state.c_str(),indicator.remaining/double(TICKS_PER_SECOND));
        }
        else if (indicator.remaining) snprintf(label, sizeof(label), "%s  %.1fs%s", indicator.phase == Phase::Armed ? "ARMED" : indicator.phase == Phase::Control ? "TARGET EFFECT" : "ACTIVE", indicator.remaining / double(TICKS_PER_SECOND), indicator.charges ? "  / CHARGED" : "");
        else snprintf(label, sizeof(label), "%s", indicator.phase == Phase::Ready ? "READY" : indicator.phase == Phase::Hit ? "HIT CONFIRMED" : index < 6 ? "PERK TRIGGERED" : "CAST");
        if (auto status = Text::get(label, "lang/en.ttf#10#1", DelveHUD::ivory, 0xff000000))
            status->draw({0, 0, 0, 0}, {left + 9, top + panelHeight / 2, int(status->getWidth()), int(status->getHeight())}, viewport);
    }
}
#ifdef BARONY_SMOKE_TESTS
void smokeQuiet(bool enabled) { quiet = enabled; }
unsigned int smokeCount(int player, int index) { return validPlayer(player) && index >= 0 && index < 10 ? counts[player][index] : 0; }
bool smokeNetworkConfirmed() { return networkEvents > 0 && networkStates > 0; }
void smokeAudit(Entity& host, Entity& guest, const std::function<void(const std::string&, bool)>& check) {
    const auto savedActors = actors;
    const auto savedReceivedEvents = receivedEvents, savedReceivedStates = receivedStates;
    const auto savedCounts = counts;
    const auto savedNetworkEvents = networkEvents, savedNetworkStates = networkStates;
    const auto savedMultiplayer = multiplayer, savedClient = clientnum;
    const auto savedAddress = net_packet->address; const int savedLength = net_packet->len;
    std::array<unsigned char, stateLength> savedBytes{};
    std::copy(net_packet->data, net_packet->data + stateLength, savedBytes.begin());
    const bool savedPause = gamePaused;
    const int characterClass = DelveBuilds::activeClass(0);
#if !defined(USE_FMOD) && !defined(USE_OPENAL)
    if (FeedbackAudio::device) SDL_LockAudioDevice(FeedbackAudio::device);
    const auto savedVoices=FeedbackAudio::voices;
    const auto savedVolume=FeedbackAudio::volume;
    const auto savedAudioPause=FeedbackAudio::paused;
    FeedbackAudio::voices={};
    const auto clip=std::make_shared<const std::vector<float>>(256,.5f);
    FeedbackAudio::voices[0]={clip,0,.4f,.4f}; FeedbackAudio::volume=1; FeedbackAudio::paused=false;
    std::array<float,32> audioSamples{};
    const auto mix=[&]() { FeedbackAudio::callback(nullptr,reinterpret_cast<Uint8*>(audioSamples.data()),audioSamples.size()*sizeof(float)); };
    mix(); check("effects_fallback_audio_mixer_is_audible",audioSamples[0]>0 && audioSamples[1]>0);
    FeedbackAudio::volume=0; const auto beforeMute=FeedbackAudio::voices[0].cursor; mix();
    check("effects_audio_mute_silences_without_stale_queue",FeedbackAudio::voices[0].cursor>beforeMute
        && std::all_of(audioSamples.begin(),audioSamples.end(),[](float sample){return sample==0;}));
    FeedbackAudio::paused=true; const auto beforePause=FeedbackAudio::voices[0].cursor; mix();
    check("effects_audio_pause_freezes_cursor",FeedbackAudio::voices[0].cursor==beforePause
        && std::all_of(audioSamples.begin(),audioSamples.end(),[](float sample){return sample==0;}));
    FeedbackAudio::paused=false; FeedbackAudio::volume=1;
    for (auto& voice:FeedbackAudio::voices) voice={clip,0,1,1}; mix();
    check("effects_audio_overlapping_cues_do_not_clip",std::all_of(audioSamples.begin(),audioSamples.end(),[](float sample){return std::abs(sample)<=.85f;}));
    FeedbackAudio::voices=savedVoices; FeedbackAudio::volume=savedVolume; FeedbackAudio::paused=savedAudioPause;
    if (FeedbackAudio::device) SDL_UnlockAudioDevice(FeedbackAudio::device);
#endif
    bool modelPrimitives = true;
    for (int sprite : strokeModels) modelPrimitives &= sprite >= 0 && sprite < static_cast<int>(nummodels)
        && models[sprite] && models[sprite]->sizex == 1 && models[sprite]->sizey == 1 && models[sprite]->sizez == 1
        && polymodels[sprite].numfaces == 12;
    check("effects_eight_original_procedural_primitives", modelPrimitives);
    int buff = 6;
    while (buff < 10 && DelveBuilds::option(characterClass, buff).ability.self[0].id < 0) ++buff;
    check("effects_buff_profile_available", buff < 10);
    if (buff < 10) {
        const auto& ability = DelveBuilds::option(characterClass, buff).ability;
        std::array<Uint8, 2> strengths{}; std::array<int, 2> timers{};
        for (int effectIndex = 0; effectIndex < 2; ++effectIndex) {
            const auto& effect = ability.self[effectIndex]; if (effect.id < 0) continue;
            strengths[effectIndex] = stats[0]->getEffectActive(effect.id); timers[effectIndex] = stats[0]->EFFECTS_TIMERS[effect.id];
            host.setEffect(effect.id, true, effect.ticks, false);
        }
        activated(0, buff); const auto duration = view(0, buff).remaining;
        gamePaused = true; tick(10);
        check("effects_pause_freezes_active_duration", view(0, buff).remaining == duration);
        gamePaused = false;
        if (ability.self[0].id >= 0 && ability.self[1].id >= 0) {
            host.setEffect(ability.self[0].id,false,0,false); tick(1);
            check("effects_components_expire_independently", actors[0].budgets[buff-6][0] == 0
                && actors[0].budgets[buff-6][1] > 0 && view(0,buff).remaining > 0);
        }
        for (const auto& effect : ability.self) if (effect.id >= 0) host.setEffect(effect.id, false, 0, false);
        tick(1); check("effects_cleanse_cancels_active_visual", view(0, buff).remaining == 0);
        for (const auto& effect : ability.self) if (effect.id >= 0) host.setEffect(effect.id, true, effect.ticks, false);
        activated(0, buff); tick(duration);
        check("effects_other_buffs_cannot_extend_skill_visual", view(0, buff).remaining == 0);
        activated(0, buff); const int health = stats[0]->HP; stats[0]->HP = 0; tick(1); stats[0]->HP = health;
        check("effects_death_clears_active_visual", view(0, buff).remaining == 0 && view(0, buff).flash == 0);
        const auto invisible = stats[0]->getEffectActive(EFF_INVISIBLE); const int invisibleTimer = stats[0]->EFFECTS_TIMERS[EFF_INVISIBLE];
        host.setEffect(EFF_INVISIBLE, true, 100, false); ensure(0);
        const int before = particles; const bool savedQuiet = quiet; quiet = false; clientnum = 1;
        burst(0, buff, &host, Phase::Active, true); quiet = savedQuiet;
        check("effects_invisibility_does_not_spawn_revealing_particles", particles == before && actors[0].concealed);
        clientnum = 0; quiet = false;
        burst(0, buff, &host, Phase::Cast);
        check("effects_invisible_owner_has_private_cast_particles", particles > before);
        bool briefCast = false, boundedCast = true;
        for (auto node=map.entities->first;node;node=node->next) {
            auto particle=static_cast<Entity*>(node->element);
            if (particle->behavior!=&particleBehavior || particle->skill[6]!=0 || particle->skill[8]!=buff
                || particle->skill[11]!=static_cast<int>(SkillVFX::Stage::Cast)) continue;
            const auto anchor=authoredDesign(characterClass,buff).layers[particle->skill[9]].anchor;
            if (anchor==SkillVFX::Anchor::Hands || anchor==SkillVFX::Anchor::Body) {
                briefCast=true; boundedCast &= particle->skill[1]<=16;
            }
        }
        check("effects_first_person_casts_end_within_320ms",briefCast && boundedCast);
        activated(0,buff);
        const int beforeLocalBuff=particles;
        burst(0,buff,&host,Phase::Active,true);
        check("effects_active_buffs_keep_owner_view_clear",view(0,buff).remaining>0 && particles==beforeLocalBuff);
        bool generationGuard = false;
        for (auto node = map.entities->first; node; node = node->next) {
            auto particle = static_cast<Entity*>(node->element);
            if (particle->behavior != &particleBehavior || !particle->skill[5]) continue;
            const auto savedGeneration = bodyGenerations[0]; ++bodyGenerations[0];
            generationGuard = !updateStroke(*particle); bodyGenerations[0] = savedGeneration; break;
        }
        check("effects_same_body_reset_invalidates_old_choreography", generationGuard);
        check("effects_geometry_cannot_cross_world_boundary", !clearStroke(host.x,host.y,-16,-16));
        const int beforeStale=particles;
        auto stale=newEntity(strokeModels[0],1,map.entities,nullptr);
        if (stale) {
            --entity_uids; stale->setUID(-3); stale->behavior=&particleBehavior;
            stale->skill[4]=generation-1; stale->skill[5]=1; stale->skill[6]=MAXPLAYERS-1;
            stale->skill[8]=9; stale->skill[11]=static_cast<int>(SkillVFX::Stage::Cast);
            clearTransient(MAXPLAYERS-1,9);
        }
        check("effects_old_generation_cleanup_preserves_current_budget",stale && particles==beforeStale);
        host.setEffect(EFF_INVISIBLE,false,0,false); ensure(0);
        const int beforePerk=particles;
        actors[0].views[0].charges=1;
        burst(0,0,&host,Phase::Armed);
        check("effects_perks_emit_authored_trigger_and_reward_geometry",particles>beforePerk);
        bool chargeSpent=false;
        const int beforeLocalPerk=particles;
        burst(0,0,&host,Phase::Armed,true);
        check("effects_armed_perks_keep_owner_view_clear",particles==beforeLocalPerk && actors[0].views[0].charges==1);
        clientnum=1;
        burst(0,0,&host,Phase::Armed,true);
        check("effects_remote_armed_perk_cues_retained",particles>beforeLocalPerk);
        actors[0].views[0].charges=0;
        for (auto node=map.entities->first;node;node=node->next) {
            auto particle=static_cast<Entity*>(node->element);
            if (particle->behavior==&particleBehavior && particle->skill[5] && particle->skill[8]==0
                && particle->skill[11]==static_cast<int>(SkillVFX::Stage::Active)) chargeSpent|=!updateStroke(*particle);
        }
        check("effects_spent_perk_charge_stops_sustained_geometry",chargeSpent);
        clientnum=0;
        clearTransient(0,0);
        actors[0].views[0].stacks=1; actors[0].views[0].remaining=100;
        burst(0,0,&host,Phase::Armed);
        bool prematureReward=false;
        for (auto node=map.entities->first;node;node=node->next) {
            auto particle=static_cast<Entity*>(node->element);
            prematureReward|=particle->behavior==&particleBehavior && particle->skill[5] && particle->skill[8]==0
                && particle->skill[9]==1 && particle->skill[11]==static_cast<int>(SkillVFX::Stage::Cast);
        }
        check("effects_unfinished_combo_never_displays_reward_proc",!prematureReward);
        actors[0].views[0].stacks=0; actors[0].views[0].remaining=0;
        for (int repeat = 0; repeat < 40; ++repeat) burst(0, buff, &host, Phase::Cast);
        check("effects_particle_budget_bounded", particles == particleLimit);
        bool cosmetic = true;
        for (auto node = map.entities->first; node;) {
            auto next = node->next; auto particle = static_cast<Entity*>(node->element); node = next;
            if (particle->behavior != &particleBehavior) continue;
            cosmetic &= particle->flags[NOUPDATE] && particle->flags[PASSABLE] && particle->flags[UNCLICKABLE] && particle->getUID() == static_cast<Uint32>(-3);
            list_RemoveNode(particle->mynode);
        }
        particles = 0; quiet = savedQuiet;
        check("effects_particles_never_collide_or_network_as_entities", cosmetic);
        host.setEffect(EFF_INVISIBLE, invisible, invisibleTimer, false);
        for (int effectIndex = 0; effectIndex < 2; ++effectIndex) {
            const auto& effect = ability.self[effectIndex];
            if (effect.id >= 0) host.setEffect(effect.id, strengths[effectIndex], timers[effectIndex], false);
        }
    }
    int controlSkill = 6;
    while (controlSkill < 10 && DelveBuilds::option(characterClass, controlSkill).ability.control < 0) ++controlSkill;
    if (controlSkill < 10) {
        const int effect = DelveBuilds::option(characterClass, controlSkill).ability.control;
        const auto strength = stats[1]->getEffectActive(effect); const int timer = stats[1]->EFFECTS_TIMERS[effect];
        guest.setEffect(effect, true, 100, false); const int health = stats[1]->HP;
        impact(0, controlSkill, guest, true);
        check("effects_confirmed_control_has_target_timer", view(0, controlSkill).remaining == 100 && view(0, controlSkill).phase == Phase::Control);
        check("effects_control_feedback_does_not_apply_extra_damage", stats[1]->HP == health);
        const bool previousQuiet=quiet; quiet=false; clientnum=1;
        const int beforeLocalControl=particles;
        burst(0,controlSkill,&guest,Phase::Hit,true);
        check("effects_control_keeps_recipient_view_clear",particles==beforeLocalControl && view(0,controlSkill).remaining==100);
        clientnum=0;
        burst(0,controlSkill,&guest,Phase::Hit,true);
        check("effects_remote_target_control_cues_retained",particles>beforeLocalControl);
        for (auto node=map.entities->first;node;) {
            auto next=node->next; auto particle=static_cast<Entity*>(node->element); node=next;
            if (particle->behavior!=&particleBehavior || particle->skill[11]!=static_cast<int>(SkillVFX::Stage::Control)) continue;
            --particles; list_RemoveNode(particle->mynode);
        }
        quiet=previousQuiet;
        guest.setEffect(effect, false, 0, false); tick(1);
        check("effects_target_cleanse_removes_timer_and_mark", view(0, controlSkill).remaining == 0);
        guest.setEffect(effect, strength, timer, false);
    }
    actors = {}; receivedEvents = {}; receivedStates = {};
    multiplayer = CLIENT; clientnum = 1;
    auto statePacket = [&]() {
        std::memset(net_packet->data, 0, stateLength); std::memcpy(net_packet->data, "DFXS", 4);
        SDLNet_Write32(uniqueGameKey, net_packet->data + 4); SDLNet_Write32(100, net_packet->data + 8);
        SDLNet_Write32(currentlevel, net_packet->data + 12); net_packet->data[16] = 0; net_packet->data[17] = characterClass;
        SDLNet_Write32(host.getUID(), net_packet->data + 18);
        const int offset = 22 + 6 * 12;
        SDLNet_Write16(50, net_packet->data + offset); SDLNet_Write16(100, net_packet->data + offset + 2);
        net_packet->data[offset + 6] = static_cast<int>(Phase::Active);
        net_packet->len = stateLength; net_packet->address = net_server;
    };
    statePacket(); --net_packet->len; receiveState(); check("effects_truncated_packet_rejected", receivedStates[0] == 0);
    statePacket(); net_packet->address.port ^= 1; receiveState(); check("effects_spoofed_endpoint_rejected", receivedStates[0] == 0);
    statePacket(); SDLNet_Write32(uniqueGameKey + 1, net_packet->data + 4); receiveState(); check("effects_stale_session_rejected", receivedStates[0] == 0);
    statePacket(); SDLNet_Write32(currentlevel + 1, net_packet->data + 12); receiveState(); check("effects_old_floor_rejected", receivedStates[0] == 0);
    statePacket(); SDLNet_Write32(guest.getUID(), net_packet->data + 18); receiveState(); check("effects_wrong_body_rejected", receivedStates[0] == 0);
    statePacket(); net_packet->data[16] = MAXPLAYERS; receiveState(); check("effects_invalid_owner_rejected", receivedStates[0] == 0);
    statePacket(); net_packet->data[17] = 26; receiveState(); check("effects_invalid_class_rejected", receivedStates[0] == 0);
    statePacket(); net_packet->data[22 + 6 * 12 + 6] = 255; receiveState(); check("effects_invalid_phase_rejected", receivedStates[0] == 0);
    statePacket(); net_packet->data[31] = 2; receiveState(); check("effects_invalid_state_concealment_rejected", receivedStates[0] == 0);
    statePacket(); SDLNet_Write16(60001,net_packet->data+270); receiveState(); check("effects_invalid_component_budget_rejected", receivedStates[0] == 0);
    statePacket(); SDLNet_Write16(101, net_packet->data + 22 + 6 * 12); receiveState(); check("effects_invalid_duration_rejected", receivedStates[0] == 0);
    statePacket(); SDLNet_Write32(guest.getUID(), net_packet->data + 142); net_packet->data[148] = 255; receiveState(); check("effects_invalid_target_status_rejected", receivedStates[0] == 0);
    statePacket(); receiveState(); check("effects_valid_state_accepted", receivedStates[0] == 100 && view(0, 6).remaining == 50);
    SDLNet_Write16(1, net_packet->data + 22 + 6 * 12); receiveState(); check("effects_replayed_state_rejected", view(0, 6).remaining == 50);
    multiplayer = SERVER; receivedStates[0] = 0; statePacket(); receiveState(); check("effects_server_rejects_client_state", receivedStates[0] == 0);
    multiplayer = CLIENT;
    auto eventPacket = [&]() {
        std::memset(net_packet->data, 0, eventLength); std::memcpy(net_packet->data, "DFXE", 4);
        SDLNet_Write32(uniqueGameKey, net_packet->data + 4); SDLNet_Write32(50, net_packet->data + 8);
        SDLNet_Write32(currentlevel, net_packet->data + 12); net_packet->data[16] = 0; net_packet->data[17] = characterClass;
        net_packet->data[18] = 7; net_packet->data[19] = static_cast<int>(Phase::Cast);
        SDLNet_Write32(host.getUID(), net_packet->data + 20); net_packet->len = eventLength; net_packet->address = net_server;
    };
    eventPacket(); net_packet->data[18] = 10; receiveEvent(); check("effects_invalid_skill_event_rejected", receivedEvents[0] == 0);
    eventPacket(); net_packet->data[19] = 255; receiveEvent(); check("effects_invalid_event_phase_rejected", receivedEvents[0] == 0);
    eventPacket(); net_packet->data[18]=0; net_packet->data[19]=static_cast<int>(Phase::Hit); receiveEvent(); check("effects_passive_cannot_forge_target_hit",receivedEvents[0]==0);
    eventPacket(); net_packet->data[46] = 2; receiveEvent(); check("effects_invalid_concealment_flag_rejected", receivedEvents[0] == 0);
    eventPacket(); net_packet->data[47] = 2; receiveEvent(); check("effects_invalid_gallery_flag_rejected", receivedEvents[0] == 0);
    eventPacket(); SDLNet_Write16(map.width*16,net_packet->data+38); receiveEvent(); check("effects_out_of_world_origin_rejected", receivedEvents[0] == 0);
    eventPacket(); SDLNet_Write16(1000,net_packet->data+42); receiveEvent(); check("effects_out_of_world_height_rejected", receivedEvents[0] == 0);
    eventPacket(); receiveEvent(); check("effects_valid_event_accepted", receivedEvents[0] == 50 && view(0, 7).flash == TICKS_PER_SECOND);
    actors[0].views[7].flash = 1; receiveEvent(); check("effects_replayed_event_rejected", view(0, 7).flash == 1);
    resetPlayer(0); check("effects_redeploy_clears_visuals", actors[0].uid == 0 && view(0, 7).flash == 0);
    actors = savedActors; receivedEvents = savedReceivedEvents; receivedStates = savedReceivedStates;
    counts = savedCounts; networkEvents = savedNetworkEvents; networkStates = savedNetworkStates;
    multiplayer = savedMultiplayer; clientnum = savedClient; gamePaused = savedPause;
    std::copy(savedBytes.begin(), savedBytes.end(), net_packet->data); net_packet->len = savedLength; net_packet->address = savedAddress;
}
void smokeShowcase(int characterClass, int index) {
    if (!validProfile(characterClass, index) || !players[0] || !players[0]->entity) return;
    clearTransient(0,index); galleryPose = !std::getenv("BARONY_EFFECTS_MOTION");
    event(0, index, Phase::Cast);
    for (auto uid : actors[0].confirmedHits[index - 6]) if (auto target = uid ? uidToEntity(uid) : nullptr) event(0,index,Phase::Hit,target);
    galleryPose = false;
}
#endif
}
