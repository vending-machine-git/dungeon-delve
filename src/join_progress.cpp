#include "main.hpp"
#include "game.hpp"
#include "join_progress.hpp"
#include "entity.hpp"
#include "stat.hpp"
#include "player.hpp"
#include "net.hpp"
#include "scores.hpp"
#include "menu.hpp"
#include "draw.hpp"
#include "paths.hpp"
#include "classdescriptions.hpp"
#include "monster.hpp"
#include "mod_tools.hpp"
#include "gear_loadout.hpp"
#include "raid.hpp"
#include "bring_gear.hpp"
#include "ui/MainMenu.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <vector>

namespace JoinProgress {
namespace {
constexpr size_t ChunkBytes = 384;
constexpr size_t MaxBytes = 8 * 1024 * 1024;
constexpr Uint32 TimeoutMs = 30000;
bool enabled = false;
bool liveJoined = false;
Uint32 peerPauseUntil = 0;
std::array<Uint8, NET_PACKET_SIZE> joinRequest{};
int joinRequestLength = 0;
Uint32 nextJoinRetry = 0;

// Only shared value fields cross the wire. Pointers, server-side creature stats,
// inventory ownership, and behavior function addresses are never transferred.
enum Kind { Generic, PlayerHead, MonsterHead, ItemActor, Ladder, Torch, Campfire,
 Door, DoorFrame, Gate, Switch, Chest, ChestLid, Fountain, Gold, Boulder, Portal };

struct Actor {
 Sint32 uid = 0, parent = 0, sprite = 0, kind = 0;
 Uint32 flags = 0;
 std::vector<double> pose;
 std::vector<Sint32> skills;
 std::vector<double> floats;
 bool serialize(FileInterface* f) {
  return f->property("uid",uid) && f->property("parent",parent)
   && f->property("sprite",sprite) && f->property("kind",kind)
   && f->property("flags",flags) && f->property("pose",pose,17)
   && f->property("skills",skills,NUMENTITYSKILLS)
   && f->property("floats",floats,NUMENTITYFSKILLS);
 }
};

struct Roster {
 bool connected = false;
 Sint32 characterClass = 0, race = 0, sex = 0, appearance = 0;
 std::string name;
 std::vector<Sint32> stats;
 bool serialize(FileInterface* f) {
  return f->property("connected",connected) && f->property("class",characterClass)
   && f->property("race",race) && f->property("sex",sex)
   && f->property("appearance",appearance) && f->property("name",name)
   && f->property("stats",stats,14);
 }
};

struct World {
 Uint32 version = 1, key = 0, lobby = 0, seed = 0, elapsed = 0, serverFlags = 0;
 Sint32 floor = 0;
 bool secret = false, dark = false, minotaur = false;
 Uint32 width = 0, height = 0, skybox = 0;
 std::string name;
 std::vector<Sint32> mapFlags, tiles;
 std::vector<std::pair<Sint32,Uint32>> attributes;
 std::vector<Roster> roster;
 std::vector<Actor> actors;
 bool serialize(FileInterface* f) {
  return f->property("version",version) && f->property("key",key)
   && f->property("lobby",lobby) && f->property("seed",seed)
   && f->property("elapsed",elapsed) && f->property("server_flags",serverFlags)
   && f->property("floor",floor) && f->property("secret",secret)
   && f->property("dark",dark) && f->property("minotaur",minotaur)
   && f->property("width",width) && f->property("height",height)
   && f->property("skybox",skybox) && f->property("name",name)
   && f->property("map_flags",mapFlags,16)
   && f->property("tiles",tiles,256*256*MAPLAYERS)
   && f->property("attributes",attributes,256*256*MAPLAYERS)
   && f->property("roster",roster,MAXPLAYERS)
   && f->property("actors",actors,32768);
 }
};

struct Outgoing {
 int slot = -1;
 Uint32 nonce = 0, started = 0, nextBegin = 0;
 std::string bytes;
 Uint32 hash = 0;
 std::vector<bool> acked;
 std::vector<Uint32> sentAt;
 bool requested = false, equipment = false;
} outgoing;

struct Incoming {
 Uint32 nonce = 0, started = 0, total = 0, hash = 0, nextReady = 0;
 std::string bytes;
 std::vector<bool> received;
#ifdef BARONY_SMOKE_TESTS
 std::set<Uint32> testDropped;
#endif
 World world;
 bool complete = false, starting = false, applied = false;
} incoming;
std::array<Uint32, MAXPLAYERS> committed{};
Uint32 bodypartRefreshUntil = 0, nextBodypartRefresh = 0;
std::string queuedError;
struct WaitingJoin { std::vector<Uint8> packet; IPaddress address{}; Uint32 started=0; };
std::vector<WaitingJoin> waitingJoins;

Uint32 checksum(const char* bytes,size_t length) {
 Uint32 hash=2166136261u;
 for(size_t i=0;i<length;++i) { hash^=static_cast<Uint8>(bytes[i]); hash*=16777619u; }
 return hash;
}
bool fromHost() {
 return net_packet->address.host==net_server.host && net_packet->address.port==net_server.port;
}
int fromPlayer() {
 for(int i=1;i<MAXPLAYERS;++i)
  if(!client_disconnected[i] && net_packet->address.host==net_clients[i-1].host
   && net_packet->address.port==net_clients[i-1].port) return i;
 return -1;
}
void sendTo(int slot,const char* command,int length,bool safe=false) {
 memcpy(net_packet->data,command,4);
 net_packet->len=length;
 net_packet->address=slot==0 ? net_server : net_clients[slot-1];
 if(safe) sendPacketSafe(net_sock,-1,net_packet,slot==0 ? 0 : slot-1);
 else sendPacket(net_sock,-1,net_packet,slot==0 ? 0 : slot-1);
}
void control(int slot,const char* command,Uint32 nonce) {
 SDLNet_Write32(nonce,&net_packet->data[4]); sendTo(slot,command,8);
}
void pausePeers(bool pause) {
 for(int i=1;i<MAXPLAYERS;++i) if(!client_disconnected[i] && i!=outgoing.slot) {
  SDLNet_Write32(uniqueGameKey,&net_packet->data[4]); net_packet->data[8]=pause;
  sendTo(i,"JPSE",9,true);
 }
}
void sendHello(int slot) {
 bool locks[MAXPLAYERS]{}; MainMenu::copyJoinSlotLocks(locks);
 SDLNet_Write32(slot,&net_packet->data[4]);
 for(int i=0;i<MAXPLAYERS;++i) {
  auto* p=&net_packet->data[8+i*38];
  p[0]=client_disconnected[i]; p[1]=locks[i]; p[2]=client_classes[i];
  p[3]=stats[i]->sex; p[4]=stats[i]->stat_appearance; p[5]=stats[i]->playerRace;
  memset(p+6,0,32); stringCopy(reinterpret_cast<char*>(p+6),stats[i]->name,32,sizeof(Stat::name));
 }
 sendTo(slot,"HELO",8+MAXPLAYERS*38,true);
}
void rejectAddress(const char* reason,int code=MAXPLAYERS+1) {
 printlog("[JIP] join rejected: %s",reason);
 SDLNet_Write32(code,&net_packet->data[4]); memcpy(net_packet->data,"HELO",4);
 net_packet->len=8; sendPacket(net_sock,-1,net_packet,0);
}
void cancelOutgoing(const char* reason) {
 if(outgoing.slot<0) return;
 printlog("[JIP] sync cancelled slot=%d: %s",outgoing.slot,reason);
 const int slot=outgoing.slot;
 memset(&net_packet->data[4],0,200);
 stringCopy(reinterpret_cast<char*>(&net_packet->data[4]),reason,200,strlen(reason)+1);
 sendTo(slot,"JERR",204,true);
 client_disconnected[slot]=true;
 players[slot]->was_connected_to_game=false;
 // The usual actPlayer disconnect path disposes of this newly reserved body.
 pausePeers(false); outgoing=Outgoing{};
}

int actorKind(Entity* e) {
 if(e->behavior==&actPlayer) return PlayerHead;
 if(e->behavior==&actMonster) return MonsterHead;
 if(e->behavior==&actItem) return ItemActor;
 if(e->behavior==&actLadder) return Ladder;
 if(e->behavior==&actTorch) return Torch;
 if(e->behavior==&actCampfire) return Campfire;
 if(e->behavior==&actDoor) return Door;
 if(e->behavior==&actDoorFrame) return DoorFrame;
 if(e->behavior==&actGate) return Gate;
 if(e->behavior==&actSwitch) return Switch;
 if(e->behavior==&actChest) return Chest;
 if(e->behavior==&actChestLid) return ChestLid;
 if(e->behavior==&actFountain) return Fountain;
 if(e->behavior==&actGoldBag) return Gold;
 if(e->behavior==&actBoulder) return Boulder;
 if(e->behavior==&actPortal) return Portal;
 return Generic;
}
void captureWorld(World& w) {
 w.key=uniqueGameKey; w.lobby=uniqueLobbyKey; w.seed=mapseed;
 w.elapsed=completionTime; w.serverFlags=svFlags; w.floor=currentlevel;
 w.secret=secretlevel; w.dark=darkmap; w.minotaur=minotaurlevel;
 w.width=map.width; w.height=map.height; w.skybox=map.skybox; w.name=map.name;
 w.mapFlags.assign(map.flags,map.flags+16);
 w.tiles.assign(map.tiles,map.tiles+map.width*map.height*MAPLAYERS);
 w.attributes.assign(map.tileAttributes.begin(),map.tileAttributes.end());
 for(int i=0;i<MAXPLAYERS;++i) {
  const Stat& s=*stats[i]; Roster r;
  r.connected=!client_disconnected[i]; r.name=s.name; r.characterClass=client_classes[i];
  r.race=s.playerRace; r.sex=s.sex; r.appearance=s.stat_appearance;
  r.stats={s.HP,s.MAXHP,s.MP,s.MAXMP,s.HUNGER,s.STR,s.DEX,s.CON,s.INT,s.PER,s.CHR,s.EXP,s.LVL,s.GOLD};
  w.roster.push_back(r);
 }
 std::set<Sint32> bodyparts;
 for(auto* n=map.entities->first;n;n=n->next) {
  auto* e=static_cast<Entity*>(n->element);
  for(auto* limb:e->bodyparts) if(limb) bodyparts.insert(limb->getUID());
 }
 for(auto* n=map.entities->first;n;n=n->next) {
  auto* e=static_cast<Entity*>(n->element);
  if(static_cast<Sint32>(e->getUID())<=0 || e->sprite<0 || e->flags[OVERDRAW] || bodyparts.count(e->getUID())) continue;
  Actor a; a.uid=e->getUID(); a.parent=e->parent; a.sprite=e->sprite; a.kind=actorKind(e);
  a.pose={e->x,e->y,e->z,e->sizex,e->sizey,e->scalex,e->scaley,e->scalez,e->yaw,e->pitch,e->roll,
   e->focalx,e->focaly,e->focalz,e->vel_x,e->vel_y,e->vel_z};
  a.skills.assign(e->skill,e->skill+NUMENTITYSKILLS);
  a.floats.assign(e->fskill,e->fskill+NUMENTITYFSKILLS);
  for(int j=0;j<24;++j) if(e->flags[j]) a.flags|=1u<<j;
  w.actors.push_back(std::move(a));
 }
}
bool validWorld(const World& w) {
 if(w.version!=1 || w.width<3 || w.height<3 || w.width>256 || w.height>256
  || w.tiles.size()!=w.width*w.height*MAPLAYERS || w.mapFlags.size()!=16
  || w.roster.size()!=MAXPLAYERS || w.floor<0 || w.floor>255 || w.name.size()>31) return false;
 std::set<Sint32> ids; bool ownBody=false;
 for(const auto& r:w.roster)
  if(r.characterClass<0 || r.characterClass>=NUMCLASSES || r.race<0 || r.race>=RACE_ENUM_END
   || r.sex<0 || r.sex>1 || r.name.size()>=sizeof(Stat::name) || r.stats.size()!=14) return false;
 for(const auto& a:w.actors) {
  if(a.uid<=0 || !ids.insert(a.uid).second || a.sprite<0 || a.kind<Generic || a.kind>Portal
   || a.pose.size()!=17 || a.skills.size()!=NUMENTITYSKILLS || a.floats.size()!=NUMENTITYFSKILLS) return false;
  if(!std::all_of(a.pose.begin(),a.pose.end(),[](double x){return std::isfinite(x) && std::abs(x)<1000000;})
   || !std::all_of(a.floats.begin(),a.floats.end(),[](double x){return std::isfinite(x);})) return false;
  if(a.kind==PlayerHead) {
   if(a.skills[2]<0 || a.skills[2]>=MAXPLAYERS) return false;
   if(a.skills[2]==clientnum) ownBody=true;
  }
 }
 return ownBody;
}
bool spawnPlayer(int slot) {
 Entity* leader=nullptr;
 for(int i=0;i<MAXPLAYERS;++i) if(i!=slot && !client_disconnected[i]
  && players[i]->entity && stats[i]->HP>0) {leader=players[i]->entity;break;}
 if(!leader) return false;
 const int originX=static_cast<int>(leader->x/16),originY=static_cast<int>(leader->y/16);
 int spawnX=-1,spawnY=-1;
 for(int radius=1;radius<9 && spawnX<0;++radius) for(int dx=-radius;dx<=radius && spawnX<0;++dx)
  for(int dy=-radius;dy<=radius;++dy) {
   if(std::abs(dx)+std::abs(dy)!=radius) continue;
   int x=originX+dx,y=originY+dy;
   if(x<1 || y<1 || x>=static_cast<int>(map.width)-1 || y>=static_cast<int>(map.height)-1) continue;
   const int index=MAPLAYERS*(y+x*map.height); const int floor=map.tiles[index];
   if(!floor || map.tiles[index+OBSTACLELAYER] || lavatiles[floor] || swimmingtiles[floor]) continue;
   bool occupied=false;
   for(auto* n=map.entities->first;n;n=n->next) {
    auto* e=static_cast<Entity*>(n->element);
    if(!e->flags[PASSABLE] && std::abs(e->x-(x*16+8))<e->sizex+4
     && std::abs(e->y-(y*16+8))<e->sizey+4) {occupied=true;break;}
   }
   if(!occupied) {spawnX=x;spawnY=y;break;}
  }
 if(spawnX<0) return false;
 players[slot]->init(); players[slot]->was_connected_to_game=true;
 stats[slot]->clearStats(); initClass(slot);
 Entity* e=newEntity(playerHeadSprite(getMonsterFromPlayerRace(stats[slot]->playerRace),
  stats[slot]->sex,stats[slot]->stat_appearance),1,map.entities,map.creatures);
 e->behavior=&actPlayer; e->skill[2]=slot; e->x=spawnX*16+8; e->y=spawnY*16+8; e->z=-1;
 e->sizex=e->sizey=4; e->flags[GENIUS]=true; e->flags[BLOCKSIGHT]=true; e->flags[UPDATENEEDED]=true;
 e->focalx=limbs[HUMAN][0][0]; e->focaly=limbs[HUMAN][0][1]; e->focalz=limbs[HUMAN][0][2];
 players[slot]->entity=e; players[slot]->ghost.initStartRoomLocation(spawnX,spawnY);
 actPlayer(e); // initialize this new body only; existing players/world are untouched
 ++numplayers;
 printlog("[JIP] reserved slot=%d uid=%u x=%d y=%d floor=%d",slot,e->getUID(),spawnX,spawnY,currentlevel);
 return true;
}

bool applyWorld() {
 auto& w=incoming.world;
 if(!validWorld(w) || w.width!=map.width || w.height!=map.height) return false;
 // The normal client startup allocates rendering/navigation buffers. Replace
 // its generated actors and tiles with the host's live state, including things
 // removed or altered since this floor was originally generated.
 list_FreeAll(map.entities);
 TileEntityList.emptyGridEntities();
 for(int i=0;i<MAXPLAYERS;++i) {
  players[i]->entity=nullptr;
  players[i]->hud.arm=nullptr; players[i]->hud.weapon=nullptr;
  players[i]->hud.magicLeftHand=nullptr; players[i]->hud.magicRightHand=nullptr;
  players[i]->hud.magicRangefinder=nullptr;
 }
 std::copy(w.tiles.begin(),w.tiles.end(),map.tiles);
 std::copy(w.mapFlags.begin(),w.mapFlags.end(),map.flags);
 map.tileAttributes.clear(); map.tileAttributes.insert(w.attributes.begin(),w.attributes.end());
 map.skybox=w.skybox; stringCopy(map.name,w.name.c_str(),sizeof(map.name),w.name.size()+1);
 currentlevel=w.floor; secretlevel=w.secret; darkmap=w.dark; minotaurlevel=w.minotaur;
 mapseed=w.seed; uniqueGameKey=w.key; uniqueLobbyKey=w.lobby; completionTime=w.elapsed; svFlags=w.serverFlags;
 for(int i=0;i<MAXPLAYERS;++i) {
  auto& r=w.roster[i]; auto& s=*stats[i]; client_disconnected[i]=!r.connected;
  client_classes[i]=r.characterClass; s.playerRace=r.race; s.sex=static_cast<sex_t>(r.sex);
  s.stat_appearance=r.appearance; stringCopy(s.name,r.name.c_str(),sizeof(s.name),r.name.size()+1);
  s.HP=r.stats[0];s.MAXHP=r.stats[1];s.MP=r.stats[2];s.MAXMP=r.stats[3];s.HUNGER=r.stats[4];
  s.STR=r.stats[5];s.DEX=r.stats[6];s.CON=r.stats[7];s.INT=r.stats[8];s.PER=r.stats[9];
  s.CHR=r.stats[10];s.EXP=r.stats[11];s.LVL=r.stats[12];s.GOLD=r.stats[13];
 }
 Sint32 highest=1;for(const auto& a:w.actors) highest=std::max(highest,a.uid);
 entity_uids=highest+1024;
 for(const auto& a:w.actors) {
  Entity* e=newEntity(a.sprite,1,map.entities,nullptr); e->setUID(a.uid); e->parent=a.parent;
  const auto& p=a.pose;
  e->x=e->new_x=p[0]; e->y=e->new_y=p[1]; e->z=e->new_z=p[2];
  e->sizex=p[3];e->sizey=p[4];e->scalex=p[5];e->scaley=p[6];e->scalez=p[7];
  e->yaw=e->new_yaw=p[8];e->pitch=e->new_pitch=p[9];e->roll=e->new_roll=p[10];
  e->focalx=p[11];e->focaly=p[12];e->focalz=p[13];e->vel_x=p[14];e->vel_y=p[15];e->vel_z=p[16];
  std::copy(a.skills.begin(),a.skills.end(),e->skill); std::copy(a.floats.begin(),a.floats.end(),e->fskill);
  for(int i=0;i<24;++i) e->flags[i]=(a.flags&(1u<<i))!=0;
  SDLNet_Write32(e->skill[2],&net_packet->data[30]);
  switch(a.kind) {
   case PlayerHead:
    e->behavior=&actPlayer; e->skill[0]=0;
    players[e->skill[2]]->entity=e; e->addToCreatureList(map.creatures);
    e->flags[UPDATENEEDED]=e->skill[2]!=clientnum;
    break;
   case MonsterHead: e->behavior=&actMonster; e->skill[3]=0; e->addToCreatureList(map.creatures); break;
   case ItemActor: e->behavior=&actItem; break;
   case Ladder: e->behavior=&actLadder; break;
   case Torch: e->behavior=&actTorch; e->skill[0]=0; e->skill[3]=0; break;
   case Campfire: e->behavior=&actCampfire; break;
   case Door: e->behavior=&actDoor; break;
   case DoorFrame: e->behavior=&actDoorFrame; break;
   case Gate: e->behavior=&actGate; break;
   case Switch: e->behavior=&actSwitch; break;
   case Chest: e->behavior=&actChest; break;
   case ChestLid: e->behavior=&actChestLid; break;
   case Fountain: e->behavior=&actFountain; break;
   case Gold: e->behavior=&actGoldBag; break;
   case Boulder: e->behavior=&actBoulder; break;
   case Portal: e->behavior=&actPortal; break;
   default: clientActions(e); if(!e->behavior) e->behavior=&actEmpty; break;
  }
  e->lastupdateserver=0; e->lastupdate=ticks;
  TileEntityList.addEntity(*e);
 }
 if(!players[clientnum]->entity) return false;
 generatePathMaps(); clearChunks(); createChunks();
 players[clientnum]->ghost.initStartRoomLocation(players[clientnum]->entity->x/16,players[clientnum]->entity->y/16);
 printlog("[JIP] applied slot=%d floor=%d seed=%u tiles=%u actors=%u hash=%u",
  clientnum,currentlevel,mapseed,static_cast<unsigned>(w.tiles.size()),static_cast<unsigned>(w.actors.size()),tileHash());
 return true;
}
void finishOutgoing() {
 const int slot=outgoing.slot;
 if(slot<1) return;
 committed[slot]=outgoing.nonce;
 BringGear::setRemoteLoadoutReady(slot,true);
 control(slot,"JOK!",outgoing.nonce);
 printlog("[JIP] joined slot=%d floor=%d seed=%u hash=%u",slot,currentlevel,mapseed,tileHash());
 messagePlayer(0,MESSAGE_MISC,"%s joined the ongoing game.",stats[slot]->name);
 pausePeers(false); outgoing=Outgoing{};
 bodypartRefreshUntil=SDL_GetTicks()+3000;nextBodypartRefresh=0;
 serverUpdatePlayerStats();
}
} // namespace

bool parseArgument(const char* argument) {
 if(!strcmp(argument,"-join-in-progress")) {enabled=true;return true;}
 return false;
}
bool requested() {return enabled;}
bool joinedLive() {return liveJoined;}
void resetSession() {
 outgoing=Outgoing{};incoming=Incoming{};committed.fill(0);waitingJoins.clear();
 liveJoined=false;peerPauseUntil=0;joinRequestLength=0;bodypartRefreshUntil=0;queuedError.clear();
}
bool paused() {
 if(!enabled || intro) return false;
 return outgoing.slot>=0 || incoming.starting || (peerPauseUntil && SDL_GetTicks()<peerPauseUntil);
}
bool clientStarting() {return multiplayer==CLIENT && incoming.starting;}
unsigned int clientSeed() {return incoming.world.seed;}
void configureClientFloor() {
 if(!clientStarting()) return;
 currentlevel=incoming.world.floor;secretlevel=incoming.world.secret;
 uniqueGameKey=incoming.world.key;uniqueLobbyKey=incoming.world.lobby;
 svFlags=lobbyWindowSvFlags=incoming.world.serverFlags;
}
unsigned int tileHash() {
 return map.tiles ? checksum(reinterpret_cast<const char*>(map.tiles),map.width*map.height*MAPLAYERS*sizeof(Sint32)) : 0;
}
void rememberJoinPacket() {
 if(!enabled || net_packet->len<70 || net_packet->len>NET_PACKET_SIZE) return;
 incoming=Incoming{};liveJoined=false;peerPauseUntil=0;queuedError.clear();
 joinRequestLength=net_packet->len;
 memcpy(joinRequest.data(),net_packet->data,joinRequestLength);
 nextJoinRetry=SDL_GetTicks()+1000;
}
bool repeatHello(int& slot) {
 if(!enabled || !directConnect) return false;
 slot=fromPlayer();
 if(slot<1) return false;
 sendHello(slot); return true;
}
void receiveJoin() {
 if(!enabled || multiplayer!=SERVER || intro || !directConnect || net_packet->len<70) return;
 int existing=-1;
 if(repeatHello(existing)) return;
 if(outgoing.slot>=0 || loading || loadnextlevel || fadeout) {
  auto duplicate=std::find_if(waitingJoins.begin(),waitingJoins.end(),[](const WaitingJoin& w){
   return w.address.host==net_packet->address.host && w.address.port==net_packet->address.port;
  });
  if(duplicate==waitingJoins.end() && waitingJoins.size()<MAXPLAYERS-1) {
   WaitingJoin w;w.address=net_packet->address;w.started=SDL_GetTicks();
   w.packet.assign(net_packet->data,net_packet->data+net_packet->len);waitingJoins.push_back(std::move(w));
   printlog("[JIP] join queued until synchronization/level change finishes");
  }
  return;
 }
 bool livingParty=false;
 for(int i=0;i<MAXPLAYERS;++i) if(!client_disconnected[i] && players[i]->entity && stats[i]->HP>0) livingParty=true;
 if(!livingParty || gameModeManager.getMode()!=GameModeManager_t::GAME_MODE_DEFAULT
  || Raid::requested()) {
  rejectAddress("This host is not ready for a new player.");return;
 }
 if(!(net_packet->data[69]&joinCapability)) {rejectAddress("Enable Join In Progress on both peers.");return;}
 if(net_packet->data[56]!=0 || SDLNet_Read32(&net_packet->data[61])!=0) {
  rejectAddress("Create a fresh joining character; campaign-resume joins use the ordinary lobby.");return;
 }
 const Uint32 characterClass=SDLNet_Read32(&net_packet->data[36]);
 const Uint32 sex=SDLNet_Read32(&net_packet->data[40]);
 const Uint32 appearance=SDLNet_Read32(&net_packet->data[44]);
 if(characterClass>=NUMCLASSES || sex>1 || (appearance&255)>=RACE_ENUM_END
  || ((appearance>>8)&255)>=((appearance&255)==RACE_HUMAN?NUMAPPEARANCES:2)) {
  rejectAddress("Invalid character identity.");return;
 }
 bool locks[MAXPLAYERS]{};MainMenu::copyJoinSlotLocks(locks);
 for(int i=1;i<MAXPLAYERS;++i) if(players[i]->entity) locks[i]=true;
 int slot=-1;
 // The active session is not resumed/restarted; reuse the existing JOIN/HELO
 // identity and capacity validation while its lobby-only save-key check is off.
 const auto oldLoading=loadingsavegame;loadingsavegame=0;
 const auto result=lobbyPlayerJoinRequest(slot,locks);loadingsavegame=oldLoading;
 if(result!=NET_LOBBY_JOIN_DIRECTIP_SUCCESS || slot<1 || slot>=MAXPLAYERS) return;
 BringGear::setRemoteLoadoutReady(slot,false);
 outgoing=Outgoing{};outgoing.slot=slot;outgoing.started=SDL_GetTicks();
 outgoing.nonce=local_rng.getU32();if(!outgoing.nonce) outgoing.nonce=1;
 pausePeers(true);
 if(!spawnPlayer(slot)) {cancelOutgoing("Could not find a safe space near the party.");return;}
 World world;captureWorld(world);
 std::error_code snapshotError;
 std::filesystem::create_directories("jip",snapshotError);
 if(snapshotError) {cancelOutgoing("Could not create the world snapshot directory.");return;}
 if(!FileHelper::writeObject("jip/outgoing.json",EFileFormat::Json_Compact,world)) {
  cancelOutgoing("Could not prepare the current world.");return;
 }
 std::ifstream input("jip/outgoing.json",std::ios::binary);
 outgoing.bytes.assign(std::istreambuf_iterator<char>(input),{});
 if(outgoing.bytes.empty() || outgoing.bytes.size()>MaxBytes) {cancelOutgoing("World snapshot is too large.");return;}
 outgoing.hash=checksum(outgoing.bytes.data(),outgoing.bytes.size());
 const size_t chunks=(outgoing.bytes.size()+ChunkBytes-1)/ChunkBytes;
 outgoing.acked.assign(chunks,false);outgoing.sentAt.assign(chunks,0);
 printlog("[JIP] snapshot slot=%d nonce=%u bytes=%u chunks=%u hash=%u",slot,outgoing.nonce,
  static_cast<unsigned>(outgoing.bytes.size()),static_cast<unsigned>(chunks),tileHash());
}
void receiveRequest() {
 if(!enabled || multiplayer!=SERVER || outgoing.slot<1 || net_packet->len!=5
  || fromPlayer()!=outgoing.slot || net_packet->data[4]!=outgoing.slot) return;
 outgoing.requested=true;
}
void receiveBegin() {
 if(!enabled || multiplayer!=CLIENT || !intro || !fromHost() || net_packet->len!=20) return;
 const Uint32 nonce=SDLNet_Read32(&net_packet->data[4]);
 const Uint32 total=SDLNet_Read32(&net_packet->data[8]);
 const Uint32 length=SDLNet_Read32(&net_packet->data[12]);
 if(!nonce || !length || length>MaxBytes || total!=(length+ChunkBytes-1)/ChunkBytes) return;
 if(incoming.nonce==nonce) return;
 incoming=Incoming{};incoming.nonce=nonce;incoming.total=total;incoming.started=SDL_GetTicks();
 incoming.hash=SDLNet_Read32(&net_packet->data[16]);incoming.bytes.resize(length);
 incoming.received.assign(total,false);
 printlog("[JIP] receiving nonce=%u bytes=%u chunks=%u",nonce,length,total);
}
void receiveData() {
 if(!enabled || multiplayer!=CLIENT || !intro || !fromHost() || incoming.complete
  || net_packet->len<12 || SDLNet_Read32(&net_packet->data[4])!=incoming.nonce || !incoming.nonce) return;
 const Uint32 index=SDLNet_Read32(&net_packet->data[8]);
 if(index>=incoming.total) return;
#ifdef BARONY_SMOKE_TESTS
 if(std::getenv("BARONY_SMOKE_JIP_DELAY") && SDL_GetTicks()-incoming.started<12000) return;
 if(std::getenv("BARONY_SMOKE_JIP_ABORT")) {
  if(incoming.testDropped.insert(index).second && index==0) printlog("[SMOKE][JIP] holding transfer for abort test");
  return;
 }
 if(std::getenv("BARONY_SMOKE_JIP_LOSS") && index%7==0 && incoming.testDropped.insert(index).second) {
  printlog("[SMOKE][JIP] dropped first chunk=%u",index);return;
 }
#endif
 const size_t offset=index*ChunkBytes;
 const size_t length=std::min(ChunkBytes,incoming.bytes.size()-offset);
 if(static_cast<size_t>(net_packet->len)!=12+length) return;
#ifdef BARONY_SMOKE_TESTS
 if(std::getenv("BARONY_SMOKE_JIP_CORRUPT") && index==0 && !incoming.received[index]) {
  net_packet->data[12]^=1;printlog("[SMOKE][JIP] corrupted chunk=0");
 }
#endif
 memcpy(&incoming.bytes[offset],&net_packet->data[12],length);incoming.received[index]=true;
 SDLNet_Write32(incoming.nonce,&net_packet->data[4]);SDLNet_Write32(index,&net_packet->data[8]);sendTo(0,"JACK",12);
 if(!std::all_of(incoming.received.begin(),incoming.received.end(),[](bool x){return x;})) return;
 if(checksum(incoming.bytes.data(),incoming.bytes.size())!=incoming.hash) {queuedError="World transfer failed its checksum.";return;}
 std::error_code snapshotError;
 std::filesystem::create_directories("jip",snapshotError);
 if(snapshotError) {queuedError="Could not create the world snapshot directory.";return;}
 std::ofstream output("jip/incoming.json",std::ios::binary);output.write(incoming.bytes.data(),incoming.bytes.size());output.close();
 if(!output.good() || !FileHelper::readObject("jip/incoming.json",incoming.world) || !validWorld(incoming.world)) {
  queuedError="The host sent an invalid world snapshot.";return;
 }
 incoming.complete=true;printlog("[JIP] transfer complete nonce=%u",incoming.nonce);
}
void receiveAck() {
 if(!enabled || multiplayer!=SERVER || outgoing.slot<1 || net_packet->len!=12
  || fromPlayer()!=outgoing.slot || SDLNet_Read32(&net_packet->data[4])!=outgoing.nonce) return;
 const Uint32 index=SDLNet_Read32(&net_packet->data[8]);
 if(index<outgoing.acked.size()) outgoing.acked[index]=true;
}
void receiveReady() {
 if(!enabled || multiplayer!=SERVER || net_packet->len!=8) return;
 const int player=fromPlayer();if(player<1) return;
 const Uint32 nonce=SDLNet_Read32(&net_packet->data[4]);
 if(nonce==committed[player] && nonce) {control(player,"JOK!",nonce);return;}
 if(player!=outgoing.slot || nonce!=outgoing.nonce) return;
 if(outgoing.equipment) finishOutgoing();
}
void receiveEquipment() {
 if(!enabled || multiplayer!=SERVER || outgoing.slot<1
  || fromPlayer()!=outgoing.slot || net_packet->len!=GearLoadout::equipmentPacketSize
  || SDLNet_Read32(&net_packet->data[4])!=uniqueGameKey || net_packet->data[8]!=outgoing.slot) return;
 if(!outgoing.equipment) {
  outgoing.equipment=GearLoadout::receiveEquipment(outgoing.slot);
  if(outgoing.equipment) printlog("[JIP] equipment staged slot=%d",outgoing.slot);
 }
}
void receiveCommit() {
 if(!enabled || multiplayer!=CLIENT || !fromHost() || net_packet->len!=8
  || !incoming.applied || SDLNet_Read32(&net_packet->data[4])!=incoming.nonce) return;
 incoming.starting=false;liveJoined=true;joinRequestLength=0;
 printlog("[JIP] live slot=%d floor=%d seed=%u uid=%u hash=%u",clientnum,currentlevel,mapseed,
  players[clientnum]->entity?players[clientnum]->entity->getUID():0,tileHash());
}
void receivePause() {
 if(!enabled || multiplayer!=CLIENT || !fromHost() || net_packet->len!=9
  || SDLNet_Read32(&net_packet->data[4])!=uniqueGameKey) return;
 peerPauseUntil=net_packet->data[8]?SDL_GetTicks()+TimeoutMs:0;
}
void receiveRoster() {
 if(!enabled || multiplayer!=CLIENT || !fromHost() || net_packet->len!=41) return;
 const int slot=net_packet->data[4];if(slot<=0 || slot>=MAXPLAYERS || slot==clientnum) return;
 client_disconnected[slot]=false;client_classes[slot]=net_packet->data[5];
 stats[slot]->sex=static_cast<sex_t>(net_packet->data[6]);stats[slot]->stat_appearance=net_packet->data[7];
 stats[slot]->playerRace=net_packet->data[8];
 stringCopy(stats[slot]->name,reinterpret_cast<char*>(&net_packet->data[9]),sizeof(Stat::name),32);
 stats[slot]->clearStats();initClass(slot);
}
void receiveError() {
 if(enabled && multiplayer==CLIENT && fromHost() && net_packet->len>=5) {
  const size_t n=std::min<size_t>(200,net_packet->len-4);
  queuedError.assign(reinterpret_cast<char*>(&net_packet->data[4]),n);
  queuedError.resize(strlen(queuedError.c_str()));
 }
}
bool allowServerPacket(unsigned int command) {
 if(!enabled || outgoing.slot<0) return true;
 switch(command) {
  case 'JOIN':case 'JREQ':case 'JACK':case 'JRDY':case 'JGEA':case 'KPAL':case 'PNGU':case 'PNGR':return true;
  default:return false;
 }
}
void tick() {
 if(!enabled) return;
 const Uint32 now=SDL_GetTicks();
 if(multiplayer==SERVER && !intro && outgoing.slot<0 && !loading && !loadnextlevel && !fadeout && !waitingJoins.empty()) {
  WaitingJoin w=std::move(waitingJoins.front());waitingJoins.erase(waitingJoins.begin());
  net_packet->address=w.address;net_packet->len=w.packet.size();memcpy(net_packet->data,w.packet.data(),w.packet.size());
  if(now-w.started<TimeoutMs) receiveJoin();else rejectAddress("Queued join timed out.");
 }
 if(!queuedError.empty()) {
  const std::string error=std::move(queuedError);queuedError.clear();incoming=Incoming{};
  printlog("[JIP] join failed: %s",error.c_str());MainMenu::showJoinProgressError(error.c_str());return;
 }
 if(multiplayer==SERVER && outgoing.slot>=0) {
  if(now-outgoing.started>TimeoutMs || client_disconnected[outgoing.slot]) {cancelOutgoing("Joining timed out; the ongoing game will continue.");return;}
  if(outgoing.requested) {
   if(now>=outgoing.nextBegin) {
    outgoing.nextBegin=now+500;
    SDLNet_Write32(outgoing.nonce,&net_packet->data[4]);SDLNet_Write32(outgoing.acked.size(),&net_packet->data[8]);
    SDLNet_Write32(outgoing.bytes.size(),&net_packet->data[12]);SDLNet_Write32(outgoing.hash,&net_packet->data[16]);
    sendTo(outgoing.slot,"JBEG",20);
   }
   int budget=24;
   for(size_t i=0;i<outgoing.acked.size() && budget;i++) {
    if(outgoing.acked[i] || (outgoing.sentAt[i] && now-outgoing.sentAt[i]<150)) continue;
    const size_t offset=i*ChunkBytes;const size_t size=std::min(ChunkBytes,outgoing.bytes.size()-offset);
    SDLNet_Write32(outgoing.nonce,&net_packet->data[4]);SDLNet_Write32(i,&net_packet->data[8]);
    memcpy(&net_packet->data[12],outgoing.bytes.data()+offset,size);sendTo(outgoing.slot,"JDAT",12+size);
    outgoing.sentAt[i]=now;--budget;
   }
  }
 }
 if(multiplayer==CLIENT && directConnect) {
  if(intro && !receivedclientnum && joinRequestLength && now>=nextJoinRetry) {
   nextJoinRetry=now+1000;memcpy(net_packet->data,joinRequest.data(),joinRequestLength);
   net_packet->len=joinRequestLength;net_packet->address=net_server;sendPacket(net_sock,-1,net_packet,0);
  }
  if(intro && receivedclientnum && !incoming.complete && now>=nextJoinRetry) {
   nextJoinRetry=now+500;net_packet->data[4]=clientnum;sendTo(0,"JREQ",5);
  }
  if(incoming.complete && !incoming.starting && !incoming.applied && intro) {
   incoming.starting=true;configureClientFloor();loadingsavegame=0;loadinglobbykey=0;
   MainMenu::destroyMainMenu();MainMenu::createDummyMainMenu();
   MainMenu::beginFade(MainMenu::FadeDestination::GameStart);
  }
  if(incoming.starting && !intro && !incoming.applied) {
   if(!applyWorld()) {queuedError="This floor could not be synchronized safely.";return;}
   incoming.applied=true;
  }
  if(incoming.applied && !liveJoined && now>=incoming.nextReady) {
   incoming.nextReady=now+300;
   GearLoadout::sendEquipmentToHost("JGEA",clientnum);
#ifdef BARONY_SMOKE_TESTS
   if(std::getenv("BARONY_SMOKE_JIP_ABORT_EQUIPMENT")) return;
#endif
   control(0,"JRDY",incoming.nonce);
  }
  if(incoming.nonce && !liveJoined && now-incoming.started>TimeoutMs) queuedError="The world synchronization timed out.";
 }
 if(multiplayer==SERVER && now<bodypartRefreshUntil && now>=nextBodypartRefresh && outgoing.slot<0) {
  nextBodypartRefresh=now+500;
  for(auto* n=map.entities->first;n;n=n->next) {
   auto* e=static_cast<Entity*>(n->element);
   if(e->behavior==&actPlayer || e->behavior==&actMonster) {
    if(list_Size(&e->children)<2) continue;
    serverUpdateBodypartIDs(e);
    for(int i=e->behavior==&actPlayer?1:2;i<list_Size(&e->children);++i) {
     auto* child=list_Node(&e->children,i);
     if(child && child->element) serverUpdateEntityBodypart(e,i);
    }
   }
  }
 }
}
} // namespace JoinProgress
