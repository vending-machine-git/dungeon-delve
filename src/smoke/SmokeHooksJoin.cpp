#include "SmokeTestHooks.hpp"
#include "SmokeHooksCommon.hpp"
#include "../join_progress.hpp"
#include "../gear_loadout.hpp"
#include "../mod_framework.hpp"
#include "../entity.hpp"
#include "../game.hpp"
#include "../stat.hpp"
#include "../player.hpp"
#include "../items.hpp"
#include "../net.hpp"
#include "../scores.hpp"
#include <cmath>
#include <fstream>
#include <filesystem>

namespace SmokeTestHooks::Join {
namespace {
struct Fixture {
 Uint32 host=0, removed=0, loot=0, key=0, seed=0, hash=0;
 Sint32 floor=0;
 std::string name;
 bool serialize(FileInterface* f) {
  return f->property("host",host) && f->property("removed",removed) && f->property("loot",loot)
   && f->property("key",key) && f->property("seed",seed) && f->property("hash",hash) && f->property("floor",floor)
   && f->property("name",name);
 }
};
int marked() {
 int count=0;
 for(auto* n=stats[clientnum]->inventory.first;n;n=n->next) {
  auto* item=static_cast<Item*>(n->element);
  if(item->type==TOOL_SKELETONKEY && item->beatitude==9) count+=item->count;
 }
 return count;
}
Uint32 equipmentHash(int player) {
 const Stat& s=*stats[player];
 Item* equipped[]={s.weapon,s.shield,s.mask,s.helmet,s.gloves,s.shoes,s.breastplate,s.cloak,s.amulet,s.ring};
 Uint32 hash=2166136261u;
 for(const Item* item:equipped) {
  Uint32 fields[]={item?1u:0u,item?static_cast<Uint32>(item->type):0u,
   item?static_cast<Uint32>(item->status):0u,item?item->appearance:0u,
   item?static_cast<Uint32>(item->beatitude):0u,item?static_cast<Uint32>(item->count):0u};
  for(Uint32 value:fields) {hash^=value;hash*=16777619u;}
 }
 return hash;
}
void observe(const char* path,const Fixture& fixture) {
 auto gear=GearLoadout::capture(clientnum);
 FileHelper::writeObject((std::string(path)+"-gear.json").c_str(),EFileFormat::Json_Compact,gear);
 std::ofstream f(path);
 f << "{\"floor\":" << currentlevel << ",\"seed\":" << mapseed
  << ",\"key\":" << uniqueGameKey << ",\"host\":" << (players[0]->entity?players[0]->entity->getUID():0)
  << ",\"own\":" << (players[clientnum]->entity?players[clientnum]->entity->getUID():0)
  << ",\"slot\":" << clientnum << ",\"hash\":" << JoinProgress::tileHash()
  << ",\"removed_present\":" << (fixture.removed && uidToEntity(fixture.removed)?"true":"false")
  << ",\"loot_present\":" << (uidToEntity(fixture.loot)?"true":"false")
  << ",\"host_gold\":" << stats[0]->GOLD << ",\"marked\":" << marked() << "}";
}
}
void tick() {
 static bool prepared=false, observed=false, done=false, repositioned=false;
 static bool sawPause=false,abortObserved=false,advancing=false;
 static bool killedHost=false,slotFreeObserved=false;
 static int observedFloor=-1;
 static Uint32 liveAt=0,nextClick=0;
 static Fixture fixture;
 const std::string sync=SmokeHooksCommon::parseEnvString("BARONY_SMOKE_JIP_SYNC","");
 if(sync.empty() || !JoinProgress::requested() || intro) return;
 if(currentlevel!=observedFloor) {
  observedFloor=currentlevel;std::ofstream floorFile("jip-floor.json");
  floorFile << "{\"floor\":" << currentlevel << ",\"seed\":" << mapseed << ",\"key\":" << uniqueGameKey << "}";
 }
 static std::array<Uint32,MAXPLAYERS> previous{};
 std::array<Uint32,MAXPLAYERS> ids{};
 for(int i=0;i<MAXPLAYERS;++i) if(players[i]->entity) ids[i]=players[i]->entity->getUID();
 if(ids!=previous) {
  previous=ids;std::ofstream peers("jip-peers.json");
  peers << "{\"slot\":" << clientnum << ",\"marked\":" << marked() << ",\"players\":[";
  for(int i=0;i<MAXPLAYERS;++i) {if(i) peers << ',';peers << ids[i];}peers << "]}";
 }
 if(multiplayer==SERVER) {
  static std::array<bool,MAXPLAYERS> equipmentObserved{};
  if(!JoinProgress::paused()) for(int i=1;i<MAXPLAYERS;++i) {
   if(!equipmentObserved[i] && !client_disconnected[i] && players[i]->entity) {
    equipmentObserved[i]=true;std::ofstream out("jip-equipment-slot"+std::to_string(i)+".json");
    out << "{\"hash\":" << equipmentHash(i) << "}";
   }
  }
  if(JoinProgress::paused()) sawPause=true;
  if(!slotFreeObserved && abortObserved && !JoinProgress::paused() && client_disconnected[1] && !players[1]->entity) {
   slotFreeObserved=true;int bags=0;for(auto* node=map.entities->first;node;node=node->next) {
    auto* item=static_cast<Entity*>(node->element);
    if(item->behavior==&actItem && item->skill[10]==TOOL_PLAYER_LOOT_BAG) ++bags;
   }
   std::ofstream out("jip-slot-free.json");out << "{\"loot_bags\":" << bags << "}";
  }
  if(sawPause && !JoinProgress::paused() && client_disconnected[1] && !abortObserved) {
   abortObserved=true;observe("jip-aborted.json",fixture);
   printlog("[SMOKE][JIP] resumed after aborted transfer");
  }
  if(!prepared) {
   if(!players[0]->entity) return;
   prepared=true;
   stats[0]->GOLD=4321;
   fixture.host=players[0]->entity->getUID();fixture.key=uniqueGameKey;fixture.seed=mapseed;fixture.floor=currentlevel;
   fixture.name=stats[0]->name;
   int x=static_cast<int>(players[0]->entity->x/16),y=static_cast<int>(players[0]->entity->y/16);
   auto& ceiling=map.tiles[MAPLAYERS*(y+x*map.height)+MAPLAYERS-1];ceiling=ceiling?0:1;
   for(auto* n=map.entities->first;n;n=n->next) {
    auto* e=static_cast<Entity*>(n->element);
    if(e->behavior==&actItem || e->behavior==&actTorch) {
     fixture.removed=e->getUID();list_RemoveNode(e->mynode);break;
    }
   }
   Item* item=newItem(TOOL_SKELETONKEY,EXCELLENT,9,1,0,true,nullptr);
   Entity* loot=dropItemMonster(item,players[0]->entity,stats[0]);
   if(!loot) {printlog("[SMOKE][JIP] fail reason=loot");return;}
   fixture.loot=loot->getUID();loot->vel_x=loot->vel_y=0;
   fixture.hash=JoinProgress::tileHash();
   FileHelper::writeObject((sync+"/fixture.json").c_str(),EFileFormat::Json_Compact,fixture);
   printlog("[SMOKE][JIP] ongoing floor=%d seed=%u key=%u host=%u hash=%u loot=%u removed=%u",
    currentlevel,mapseed,uniqueGameKey,fixture.host,fixture.hash,fixture.loot,fixture.removed);
  }
  if(!JoinProgress::paused() && players[1]->entity && !client_disconnected[1]) {
   if(!liveAt) liveAt=ticks;
   if(!observed) {observed=true;observe("jip-host-observed.json",fixture);}
   if(!repositioned && ticks-liveAt>TICKS_PER_SECOND) {
    if(Entity* loot=uidToEntity(fixture.loot)) {
     players[1]->entity->teleport(loot->x/16,loot->y/16);repositioned=true;
    }
   }
  }
  if(!killedHost && std::filesystem::exists(sync+"/host-die") && players[0]->entity) {
   killedHost=true;players[0]->entity->modHP(-stats[0]->HP);
  }
  if(killedHost && !players[0]->entity && stats[0]->HP<=0) std::ofstream(sync+"/host-dead") << "1";
  if(!advancing && std::filesystem::exists(sync+"/advance") && !JoinProgress::paused()) {
   Entity* ladder=nullptr;
   for(auto* n=map.entities->first;n;n=n->next) {
    auto* e=static_cast<Entity*>(n->element);
    if(e->behavior==&actLadder && e->skill[3]==0) {ladder=e;break;}
   }
   if(!ladder) {printlog("[SMOKE][JIP] fail reason=no_ladder");return;}
   for(int i=0;i<MAXPLAYERS;++i) if(!client_disconnected[i] && players[i]->entity) {
    players[i]->entity->x=ladder->x;players[i]->entity->y=ladder->y;
   }
   selectedEntity[0]=ladder;inrange[0]=true;actLadder(ladder);advancing=true;
   printlog("[SMOKE][JIP] ordinary ladder used after late join");
  }
 } else if(multiplayer==CLIENT && JoinProgress::joinedLive() && players[clientnum]->entity) {
  static int ringFloor=-1;
  if(SmokeHooksCommon::parseEnvInt("BARONY_SMOKE_RING",0,0,1) && ringFloor!=currentlevel && !JoinProgress::paused()) {
   ringFloor=currentlevel;auto* entity=players[clientnum]->entity;
   bool ok=stats[clientnum]->ring && stats[clientnum]->ring->type==RMF_FIRST_ITEM
    && rmf::hasInfiniteHealth(entity) && rmf::hasInfiniteMana(entity) && rmf::hasNoHunger(entity);
   if(ok) {
    entity->modHP(-stats[clientnum]->MAXHP*10);stats[clientnum]->MP=1;
    entity->safeConsumeMP(stats[clientnum]->MAXMP*2);stats[clientnum]->HUNGER=0;
    rmf::enforceResources(entity,stats[clientnum]);
    ok=stats[clientnum]->HP==stats[clientnum]->MAXHP && stats[clientnum]->MP==stats[clientnum]->MAXMP && stats[clientnum]->HUNGER==1500;
   }
   std::ofstream out("jip-ring-floor"+std::to_string(currentlevel)+".json");out << "{\"pass\":" << (ok?"true":"false") << "}";
   if(!ok) printlog("[SMOKE][JIP] fail reason=ring");
  }

  if(!observed) {
   if(!FileHelper::readObject((sync+"/fixture.json").c_str(),fixture)) return;
   observed=true;observe("jip-observed.json",fixture);
   printlog("[SMOKE][JIP] observed slot=%d floor=%d host=%u hash=%u",clientnum,currentlevel,
    players[0]->entity?players[0]->entity->getUID():0,JoinProgress::tileHash());
  }
  if(!done && marked()==1) {
   done=true;observe("jip-done.json",fixture);saveGame();
   printlog("[SMOKE][JIP] pickup marked=1 slot=%d",clientnum);
  }
  Entity* loot=uidToEntity(fixture.loot);
  Entity* own=players[clientnum]->entity;
  if(clientnum==1 && !done && loot && ticks>=nextClick && std::hypot(own->x-loot->x,own->y-loot->y)<=TOUCHRANGE) {
   nextClick=ticks+TICKS_PER_SECOND/4;
   net_packet->data[4]=clientnum;SDLNet_Write32(fixture.loot,&net_packet->data[5]);
   memcpy(net_packet->data,"CKIR",4);net_packet->len=9;net_packet->address=net_server;
   sendPacketSafe(net_sock,-1,net_packet,0);
  }
 }
}
}
