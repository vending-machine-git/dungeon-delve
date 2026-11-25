#include "SmokeTestHooks.hpp"
#include "SmokeHooksCommon.hpp"
#include "../bring_gear.hpp"
#include "../mod_framework.hpp"
#include "../gear_loadout.hpp"
#include "../raid.hpp"
#include "../entity.hpp"
#include "../stat.hpp"
#include "../player.hpp"
#include "../net.hpp"
#include "../mod_tools.hpp"

namespace SmokeTestHooks::Gear {
namespace {
Uint32 equipmentHash(int player) {
 const Stat& s = *stats[player];
 Item* equipped[] = {s.weapon,s.shield,s.mask,s.helmet,s.gloves,s.shoes,s.breastplate,s.cloak,s.amulet,s.ring};
 Uint32 hash = 2166136261u;
 for (const Item* item : equipped) {
  Uint32 fields[] = {item ? 1u : 0u, item ? static_cast<Uint32>(item->type) : 0u,
   item ? static_cast<Uint32>(item->status) : 0u, item ? item->appearance : 0u,
   item ? static_cast<Uint32>(item->beatitude) : 0u, item ? static_cast<Uint32>(item->count) : 0u};
  for (Uint32 value : fields) { hash ^= value; hash *= 16777619u; }
 }
 return hash;
}
}
void tick() {
 static bool observed = false;
 if (observed || SmokeHooksCommon::parseEnvInt("BARONY_SMOKE_GEAR",0,0,1) == 0
  || !BringGear::requested() || ::Raid::active() || intro || !players[clientnum]->entity) return;
 if (multiplayer == SERVER && !BringGear::hasLoadout(1)) return;
 observed = true;
 auto gear = GearLoadout::capture(clientnum);
 if (!FileHelper::writeObject("gear-observed.json",EFileFormat::Json_Compact,gear)) {
  printlog("[SMOKE][GEAR] fail reason=snapshot"); return;
 }
 const char* role = multiplayer == SINGLE ? "solo" : (multiplayer == SERVER ? "host" : "client");
 printlog("[SMOKE][GEAR] observed role=%s mode=%d class=%d race=%d level=%d floor=%d saves=%d ac=%d hash=%u",
  role,multiplayer,client_classes[clientnum],stats[clientnum]->playerRace,stats[clientnum]->LVL,
  currentlevel,gameModeManager.allowsSaves()?1:0,AC(stats[clientnum]),equipmentHash(clientnum));
 if (multiplayer == SERVER) printlog("[SMOKE][GEAR] remote ac=%d hash=%u",AC(stats[1]),equipmentHash(1));
 if (SmokeHooksCommon::parseEnvInt("BARONY_SMOKE_GEAR_RESUME",0,0,1) == 0) {
  newItem(TOOL_SKELETONKEY,EXCELLENT,7,1,0,true,&stats[clientnum]->inventory);
 }
 if (SmokeHooksCommon::parseEnvInt("BARONY_SMOKE_RING",0,0,1)) {
  const int last=multiplayer==SERVER?1:clientnum;
  for(int slot=clientnum;slot<=last;++slot) {
   auto* entity=players[slot]->entity;
   const bool ring=stats[slot]->ring && stats[slot]->ring->type==RMF_FIRST_ITEM && stats[slot]->ring->status!=BROKEN;
   bool ok=entity && rmf::hasInfiniteHealth(entity)==ring && rmf::hasInfiniteMana(entity)==ring && rmf::hasNoHunger(entity)==ring;
   if(ok && ring) {
    entity->modHP(-stats[slot]->MAXHP*10);
    stats[slot]->MP=1;entity->safeConsumeMP(2*stats[slot]->MAXMP);
    stats[slot]->HUNGER=0;rmf::enforceResources(entity,stats[slot]);
    ok=stats[slot]->HP==stats[slot]->MAXHP && stats[slot]->MP==stats[slot]->MAXMP && stats[slot]->HUNGER==1500;
   }
   if(!ok) {printlog("[SMOKE][RING] fail slot=%d",slot);return;}
   printlog("[SMOKE][RING] verified slot=%d protected=%d",slot,ring?1:0);
  }
 }
 const int result = saveGame();
 printlog("[SMOKE][GEAR] ordinary_save result=%d",result);
}
}
