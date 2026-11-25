#include "main.hpp"
#include "game.hpp"
#include "entity.hpp"
#include "stat.hpp"
#include "items.hpp"
#include "net.hpp"
#include "player.hpp"
#include "monster.hpp"
#include "raid.hpp"
#include "raid_battle_rules.hpp"
#include "paths.hpp"
#include "delve_rarity.hpp"
#include "rapidjson/document.h"
#include "engine/audio/sound.hpp"
#include <random>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <array>
#include <deque>
#include <set>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <limits>
#include <map>
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "files.hpp"
#ifdef BARONY_SMOKE_TESTS
#include "collision.hpp"
#endif

namespace Raid {
namespace {
rapidjson::Document world;
int worldFloor = 0;
bool nativeGenerating = false;
std::array<unsigned int, MAXPLAYERS> deathDropOrdinals{};
struct GateControl { std::vector<Uint32> levers; bool startsOpen; };
std::map<int,GateControl> gateControls;
constexpr int shopsPerFloor=3, shopStockSize=16;
constexpr int kioskRadius=2, kioskClearingRadius=3;
constexpr int kioskShelfWall=55, kioskFloor=9, kioskCeiling=30;
const ItemType gear[3][6] = {
    {IRON_SWORD,STEEL_HALBERD,IRON_BREASTPIECE,STEEL_HELM,SPELLBOOK_FIREBALL,SPELLBOOK_HEALING},
    {CRYSTAL_SWORD,CRYSTAL_BREASTPIECE,RING_MAGICRESISTANCE,AMULET_MAGICREFLECTION,MAGICSTAFF_LIGHTNING,SPELLBOOK_LIGHTNING},
    {ARTIFACT_SWORD,ARTIFACT_BOW,ARTIFACT_BREASTPIECE,ARTIFACT_BOOTS,SCROLL_ENCHANTWEAPON,SPELLBOOK_EXTRAHEALING}};
const ItemType magic[3][4] = {
    {SPELLBOOK_FIREBALL,SPELLBOOK_HEALING,SPELLBOOK_MAGICMISSILE,MAGICSTAFF_COLD},
    {SPELLBOOK_LIGHTNING,MAGICSTAFF_LIGHTNING,RING_MAGICRESISTANCE,AMULET_MAGICREFLECTION},
    {SPELLBOOK_EXTRAHEALING,SCROLL_ENCHANTWEAPON,SCROLL_ENCHANTARMOR,MAGICSTAFF_LIGHTNING}};
const std::vector<ItemType> equipmentPools[3][10] = {
    {
        {IRON_SWORD,IRON_AXE,IRON_MACE,STEEL_HALBERD,SHORTBOW,CROSSBOW},
        {LEATHER_BREASTPIECE,IRON_BREASTPIECE,STEEL_BREASTPIECE,TUNIC,SILVER_DOUBLET},
        {HAT_PHRYGIAN,HAT_HOOD,HAT_WIZARD,HAT_JESTER,HAT_FEZ,HAT_HOOD_RED,HAT_HOOD_SILVER,LEATHER_HELM,IRON_HELM,STEEL_HELM},
        {CLOAK,CLOAK_BLACK,CLOAK_SILVER,CLOAK_PROTECTION,CLOAK_BACKPACK},
        {RING_ADORNMENT,RING_SLOWDIGESTION,RING_PROTECTION,RING_WARNING,RING_STRENGTH,RING_CONSTITUTION},
        {AMULET_POISONRESISTANCE,AMULET_WATERBREATHING,AMULET_MAGICREFLECTION},
        {LEATHER_BOOTS,SUEDE_BOOTS,IRON_BOOTS,LEATHER_BOOTS_SPEED,IRON_BOOTS_WATERWALKING},
        {GLOVES,SUEDE_GLOVES,BRACERS,GAUNTLETS,GLOVES_DEXTERITY,BRACERS_CONSTITUTION},
        {WOODEN_SHIELD,BRONZE_SHIELD,IRON_SHIELD,STEEL_SHIELD},
        {MASK_BANDIT,MASK_EYEPATCH,MASK_MASQUERADE,MONOCLE}
    },
    {
        {STEEL_SWORD,STEEL_AXE,STEEL_MACE,CRYSTAL_SWORD,LONGBOW,HEAVY_CROSSBOW,TOOL_WHIP},
        {STEEL_BREASTPIECE,CRYSTAL_BREASTPIECE,SILVER_DOUBLET,MACHINIST_APRON},
        {HAT_WIZARD,HAT_JESTER,HAT_FEZ,HAT_HOOD_RED,HAT_HOOD_SILVER,STEEL_HELM,CRYSTAL_HELM,PUNISHER_HOOD},
        {CLOAK_PROTECTION,CLOAK_INVISIBILITY,CLOAK_MAGICREFLECTION,CLOAK_BACKPACK,CLOAK_SILVER},
        {RING_PROTECTION,RING_STRENGTH,RING_CONSTITUTION,RING_MAGICRESISTANCE,RING_INVISIBILITY,RING_LEVITATION,RING_REGENERATION},
        {AMULET_POISONRESISTANCE,AMULET_WATERBREATHING,AMULET_MAGICREFLECTION},
        {STEEL_BOOTS,CRYSTAL_BOOTS,LEATHER_BOOTS_SPEED,IRON_BOOTS_WATERWALKING,STEEL_BOOTS_FEATHER,STEEL_BOOTS_LEVITATION},
        {GLOVES_DEXTERITY,BRACERS_CONSTITUTION,GAUNTLETS_STRENGTH,CRYSTAL_GLOVES},
        {STEEL_SHIELD,STEEL_SHIELD_RESISTANCE,CRYSTAL_SHIELD},
        {MASK_BANDIT,MASK_EYEPATCH,MASK_MASQUERADE,MASK_SHAMAN,MONOCLE}
    },
    {
        {CRYSTAL_SWORD,CRYSTAL_BATTLEAXE,ARTIFACT_SWORD,ARTIFACT_AXE,ARTIFACT_MACE,ARTIFACT_BOW,COMPOUND_BOW},
        {CRYSTAL_BREASTPIECE,ARTIFACT_BREASTPIECE,STEEL_BREASTPIECE,MACHINIST_APRON},
        {CRYSTAL_HELM,ARTIFACT_HELM,HAT_WIZARD,HAT_JESTER,HAT_FEZ,PUNISHER_HOOD},
        {CLOAK_PROTECTION,CLOAK_INVISIBILITY,CLOAK_MAGICREFLECTION,CLOAK_BACKPACK,CLOAK_SILVER},
        {RING_MAGICRESISTANCE,RING_INVISIBILITY,RING_LEVITATION,RING_REGENERATION,RING_STRENGTH,RING_CONSTITUTION,RING_PROTECTION},
        {AMULET_POISONRESISTANCE,AMULET_WATERBREATHING,AMULET_MAGICREFLECTION},
        {CRYSTAL_BOOTS,ARTIFACT_BOOTS,LEATHER_BOOTS_SPEED,IRON_BOOTS_WATERWALKING,STEEL_BOOTS_LEVITATION,STEEL_BOOTS_FEATHER},
        {CRYSTAL_GLOVES,ARTIFACT_GLOVES,GLOVES_DEXTERITY,BRACERS_CONSTITUTION,GAUNTLETS_STRENGTH},
        {CRYSTAL_SHIELD,STEEL_SHIELD_RESISTANCE,STEEL_SHIELD},
        {MASK_BANDIT,MASK_EYEPATCH,MASK_MASQUERADE,MASK_SHAMAN,MONOCLE}
    }
};
ItemType randomEquipment(int tier, std::mt19937& rng, int slot = -1) {
    const auto& pool = equipmentPools[std::clamp(tier, 0, 2)][slot < 0 ? rng() % 10 : slot];
    return pool[rng() % pool.size()];
}
const std::array<Monster,12> patrols[3] = {
    {RAT,RAT,RAT,RAT,BAT_SMALL,SKELETON,SKELETON,SKELETON,SKELETON,SKELETON,SLIME,SLIME},
    // Jungle: melee goblins replace most skeletons, so only one slot can roll a bow.
    {RAT,RAT,RAT,BAT_SMALL,SKELETON,SLIME,SLIME,GOBLIN,GOBLIN,GOBLIN,GOBLIN,GHOUL},
    // Crystal Caves: mid-tier insectoids, scorpions and spiders. Ranged pressure stays
    // limited to insectoid acid spray, spider webs and a 1-in-10 insectoid bow.
    {RAT,BAT_SMALL,SLIME,SLIME,SPIDER,SPIDER,SCORPION,SCORPION,INSECTOID,INSECTOID,GOBLIN,GHOUL}};
ItemType randomMagic(int tier, std::mt19937& rng) {
    const auto type = magic[std::clamp(tier, 0, 2)][rng() % 4];
    return items[type].category == SPELLBOOK && rng() % 4 == 0 ? MAGICSTAFF_COLD : type;
}
ItemType randomEnemyLoot(int floor, std::mt19937& rng) {
    const int category = rng() % 100;
    if (category < 55) return randomEquipment(floor - 1, rng);
    if (category < 80) return rng() % 3 ? POTION_HEALING : POTION_RESTOREMAGIC;
    if (category < 92) {
        const auto type = randomMagic(floor - 1, rng);
        return items[type].category == SCROLL ? MAGICSTAFF_LIGHTNING : type;
    }
    return GEM_DIAMOND;
}
// Every floor must also be survivable after a next-floor level-one redeployment.
const Monster guardians[]={SKELETON,GOBLIN,TROLL};
const int guardianHealth[]={60,90,140};
const int guardianLevels[]={3,6,12};
constexpr int enemySpawnClearance=12, encounterSeparation=3, guardianClearance=4;
constexpr int harderReplacements[]={0,5,9};
std::vector<Monster> encounterRoster(int floor,int count,unsigned seed) {
    std::mt19937 rng(seed ^ (floor*83492791u) ^ 0x68bc21ebu);
    std::vector<Monster> roster;
    roster.reserve(count);
    while(roster.size()<static_cast<size_t>(count)) {
        auto bag=patrols[floor-1];
        std::shuffle(bag.begin(),bag.end(),rng);
        for(auto mob:bag) {
            if(roster.size()==static_cast<size_t>(count))break;
            roster.push_back(mob);
        }
    }
    std::shuffle(roster.begin(),roster.end(),rng);
    std::array<int,NUMMONSTERS> remaining{};
    for(auto mob:roster)++remaining[mob];
    int replaced=0;
    for(auto& mob:roster) {
        if(replaced>=harderReplacements[floor-1])break;
        if((mob==RAT || mob==BAT_SMALL || mob==SLIME) && remaining[mob]>1) {
            --remaining[mob];mob=TROLL;++replaced;
        }
    }
    return roster;
}
bool point(const rapidjson::Value& v,int size) {
    return v.IsObject() && v.HasMember("x") && v["x"].IsInt() && v["x"].GetInt()>0 && v["x"].GetInt()<size-1 &&
        v.HasMember("y") && v["y"].IsInt() && v["y"].GetInt()>=8 && v["y"].GetInt()<size-1;
}
unsigned seedAt(int x,int y) { return mapseed ^ (x*73856093u) ^ (y*19349663u) ^ (worldFloor*83492791u); }
ItemType delveGuardWeapon(Monster type, int roll) {
    if(type==GOBLIN)return roll<6 || roll==9 ? BRONZE_AXE : roll<8 ? IRON_MACE : IRON_AXE;
    if(type==INSECTOID)return roll==0 ? SHORTBOW : roll<5 ? IRON_SPEAR : roll<8 ? STEEL_SWORD : STEEL_HALBERD;
    return roll<2 ? (roll==0 ? SHORTBOW : CROSSBOW) : BRONZE_SWORD;
}
void configureDelveGuard(Entity& guard, unsigned seed) {
    auto stats=guard.getStats();
    if(!stats)return;
    stats->setAttribute("delve_loot_seed", std::to_string(seed));
    stats->MISC_FLAGS[STAT_FLAG_DISABLE_MINIBOSS]=1;
    if(stats->type!=SKELETON && stats->type!=GOBLIN && stats->type!=INSECTOID)return;
    if(stats->type==INSECTOID) {
        // No default inventory: removes native thrown-dagger stacks.
        for(int slot=ITEM_SLOT_INV_1;slot<=ITEM_SLOT_INV_6;slot+=ITEM_SLOT_NUMPROPERTIES)stats->EDITOR_ITEMS[slot]=0;
        // Native insectoids are level-25 late-game threats; Delve uses a mid-tier profile
        // a step above scorpions so floor 3 stays survivable after redeployment.
        stats->MAXHP=stats->HP=stats->OLDHP=85;stats->RANDOM_MAXHP=stats->RANDOM_HP=10;
        stats->STR=8;stats->RANDOM_STR=2;stats->DEX=7;stats->CON=8;stats->RANDOM_CON=0;
        stats->PER=8;stats->RANDOM_PER=0;stats->LVL=10;stats->RANDOM_LVL=0;
    }
    std::mt19937 rng(seed ^ 0xa511e9b3u);
    const int roll=rng()%10;
    const ItemType weapon=delveGuardWeapon(stats->type,roll);
    const int beatitude=-1+static_cast<int>(rng()%(stats->type==GOBLIN ? 3:2));
    stats->weapon=newItem(weapon,WORN,beatitude,1,rng(),false,nullptr);
}
}
// The room generator reads only the owner's installed official room sets.
bool nativeGeneration() { return nativeGenerating; }
bool prepareDungeonBase() {
    if(!nativeGenerating)return true;
    char name[32],filename[256];int flags[MAPFLAGS];unsigned skybox=map.skybox;
    const unsigned oldWidth=map.width,oldHeight=map.height;
    const std::vector<Sint32> baseTiles(map.tiles,map.tiles+map.width*map.height*MAPLAYERS);
    std::memcpy(name,map.name,sizeof(name));std::memcpy(filename,map.filename,sizeof(filename));
    std::memcpy(flags,map.flags,sizeof(flags));
    auto base="delve"+std::to_string(worldFloor);
    auto path=std::filesystem::absolute(physfsFormatMapName(base.c_str())).string();int hash=0;
    if(loadMap(path.c_str(),&map,map.entities,map.creatures,&hash)<0)return false;
    std::memcpy(map.name,name,sizeof(name));std::memcpy(map.filename,filename,sizeof(filename));
    std::memcpy(map.flags,flags,sizeof(flags));map.skybox=skybox;
    // Expand the native biome background at runtime; room templates stay at their normal scale.
    // This preserves natural open terrain linking official Mines/Caves rooms.
    for(unsigned x=0;x<map.width;++x)for(unsigned y=0;y<map.height;++y) {
        unsigned sx=std::min(oldWidth-1,x*oldWidth/map.width),sy=std::min(oldHeight-1,y*oldHeight/map.height);
        for(int layer=0;layer<MAPLAYERS;++layer)
            map.tiles[layer+MAPLAYERS*(y+x*map.height)]=baseTiles[layer+MAPLAYERS*(sy+sx*oldHeight)];
    }
    return true;
}
bool configureWorld(int floor) {
    deathDropOrdinals.fill(0);
    gateControls.clear();
    worldFloor=0;world.SetObject();
    auto file=PHYSFS_openRead("data/rmf/raid.json");if(!file)return false;
    auto length=PHYSFS_fileLength(file);std::string json;
    if(length>0 && length<65536) { json.resize(length);PHYSFS_readBytes(file,json.data(),length); }
    PHYSFS_close(file);rapidjson::Document d;d.Parse(json.c_str());
    if(d.HasParseError() || !d.IsObject() || !d.HasMember("layout_version") || !d["layout_version"].IsInt() ||
        d["layout_version"].GetInt()!=4 || !d.HasMember("layouts") || !d["layouts"].IsArray() ||
        floor<1 || floor>3 || d["layouts"].Size()!=3)return false;
    auto& w=d["layouts"][floor-1];
    const char* themes[]={"mine","swamp","caves"};int sizes[]={90,90,90};
    if(!w.IsObject() || !w.HasMember("size") || !w["size"].IsInt() || w["size"].GetInt()!=sizes[floor-1] ||
        !w.HasMember("theme") || !w["theme"].IsString() || std::strcmp(w["theme"].GetString(),themes[floor-1]))return false;
    for(auto key:{"chest_target","drop_target","guard_target"})
        if(!w.HasMember(key) || !w[key].IsInt() || w[key].GetInt()<4 || w[key].GetInt()>128)return false;
    world.CopyFrom(w,world.GetAllocator());worldFloor=floor;
    return true;
}
namespace {
using Cell=std::pair<int,int>;
int manhattan(Cell a,Cell b){return std::abs(a.first-b.first)+std::abs(a.second-b.second);}
bool doorway(int sprite) {
    return sprite==2 || sprite==3 || sprite==19 || sprite==20 || sprite==113 || sprite==114 || sprite==217 || sprite==218;
}
bool gateSprite(int sprite) {return sprite==19 || sprite==20 || sprite==113 || sprite==114;}
Cell doorStep(int sprite) {
    return sprite==2 || sprite==20 || sprite==114 || sprite==218 ? Cell{1,0}:Cell{0,1};
}
Entity* raw(int sprite,Cell p) {
    auto e=newEntity(sprite,1,map.entities,nullptr);e->x=p.first*16;e->y=p.second*16;
    setSpriteAttributes(e,nullptr,nullptr);return e;
}
bool blocking(Entity& e) {
    switch(e.sprite) {
        case 14:case 21:case 35:case 59:case 60:case 108:case 109:case 110:return true;
        case 179:return e.colliderHasCollision==1;
        default:return false;
    }
}
bool adaptOfficial(unsigned seed) {
    gateControls.clear();
    const int size=map.width;
    if(size!=world["size"].GetInt() || map.height!=size)return false;
    auto& allocator=world.GetAllocator();
    for(auto key:{"chests","drops","encounters","gold","spawns","descents","portals","views","kiosks","doors"})
        world.AddMember(rapidjson::StringRef(key),rapidjson::Value(rapidjson::kArrayType),allocator);
    auto addPoint=[&](const char* key,Cell p,const char* kind=nullptr,int role=-1) {
        rapidjson::Value v(rapidjson::kObjectType);v.AddMember("x",p.first,allocator);v.AddMember("y",p.second,allocator);
        if(kind)v.AddMember("kind",rapidjson::Value(kind,allocator),allocator);
        if(role>=0){v.AddMember("role",role,allocator);v.AddMember("facing",0,allocator);}
        world[key].PushBack(v,allocator);
    };
    std::vector<Cell> nativeGuards;std::set<Cell> occupied;
    int removedSpellTraps=0,removedNativeChests=0;
    const int nativeShopTiles=shoparea ? std::count(shoparea,shoparea+size*size,true):0;
    if(nativeShopTiles){printlog("[DELVE] generation rejected: native full shop");return false;}
    world.AddMember("native_shop_tiles",nativeShopTiles,allocator);
    for(auto node=map.entities->first;node;) {
        auto next=node->next;auto e=static_cast<Entity*>(node->element);
        Cell p{static_cast<int>(e->x)/16,static_cast<int>(e->y)/16};
        auto mob=editorSpriteTypeToMonster(e->sprite);
        bool monster=e->sprite==10 || (mob!=NOTHING && mob!=SHOPKEEPER);
        bool exit=e->sprite==1 || e->sprite==11 || e->sprite==45 || e->sprite==46 || e->sprite==63 || e->sprite==161;
        const bool spellTrap=worldFloor==2 && (e->sprite==65 || e->sprite==120);
        // Native cache templates include adjacent rows and doorway blockers.
        // Rebuild the complete cache budget under one placement policy.
        if(monster || mob==SHOPKEEPER || exit || p.second<9 || spellTrap || e->sprite==21) {
            if(spellTrap)++removedSpellTraps;
            if(e->sprite==21)++removedNativeChests;
            if(monster)nativeGuards.push_back(p);
            list_RemoveNode(node);
        } else occupied.insert(p);
        node=next;
    }
    world.AddMember("removed_spell_traps",removedSpellTraps,allocator);
    world.AddMember("replaced_native_chests",removedNativeChests,allocator);
    std::vector<bool> dry(size*size,false),safe(size*size,false);
    auto inside=[&](Cell p){return p.first>1 && p.second>=10 && p.first<size-2 && p.second<size-2;};
    auto index=[&](Cell p){return p.second+p.first*size;};
    for(int x=2;x<size-2;++x)for(int y=10;y<size-2;++y) {
        int off=MAPLAYERS*(y+x*size),f=map.tiles[off];
        dry[y+x*size]=f>0 && !map.tiles[off+OBSTACLELAYER] && !swimmingtiles[f] && !lavatiles[f];
    }
    // Preserve native barrier models. Only structurally broken doorways go;
    // valid rooms receive rewards and gates receive reachable controls below.
    int removedDoors=0,clearedDoorProps=0;
    auto sanitizeDoors=[&]() {
        for(auto node=map.entities->first;node;) {
            auto next=node->next;auto e=static_cast<Entity*>(node->element);
            if(doorway(e->sprite)) {
                Cell p{static_cast<int>(e->x)/16,static_cast<int>(e->y)/16},step=doorStep(e->sprite);
                Cell a{p.first-step.first,p.second-step.second},b{p.first+step.first,p.second+step.second};
                if(!inside(p) || !inside(a) || !inside(b) || !dry[index(p)] || !dry[index(a)] || !dry[index(b)]) {
                    occupied.erase(p);list_RemoveNode(node);++removedDoors;
                } else {
                    if(gateSprite(e->sprite))e->gateDisableOpening=0;
                    else {
                        e->doorForceLockedUnlocked=2;e->doorDisableLockpicks=0;e->doorDisableOpening=0;
                        if(e->sprite==217 || e->sprite==218)e->doorUnlockWhenPowered=0;
                    }
                }
            }
            node=next;
        }
    };
    sanitizeDoors();
    std::set<Cell> doorClearance,doorCells;
    auto reserveDoors=[&]() {
        doorClearance.clear();
        doorCells.clear();
        for(auto node=map.entities->first;node;node=node->next) {
            auto e=static_cast<Entity*>(node->element);if(!doorway(e->sprite))continue;
            Cell p{static_cast<int>(e->x)/16,static_cast<int>(e->y)/16};
            doorCells.insert(p);
            for(int dx=-2;dx<=2;++dx)for(int dy=-2;dy<=2;++dy)doorClearance.insert({p.first+dx,p.second+dy});
        }
    };
    reserveDoors();
    for(auto node=map.entities->first;node;) {
        auto next=node->next;auto e=static_cast<Entity*>(node->element);
        Cell p{static_cast<int>(e->x)/16,static_cast<int>(e->y)/16};
        if(blocking(*e) && doorClearance.count(p)) {
            occupied.erase(p);list_RemoveNode(node);++clearedDoorProps;
        }
        node=next;
    }
    safe=dry;
    for(auto node=map.entities->first;node;node=node->next) {
        auto e=static_cast<Entity*>(node->element);Cell p{static_cast<int>(e->x)/16,static_cast<int>(e->y)/16};
        if(inside(p) && blocking(*e))safe[index(p)]=false;
    }
    const Cell sides[]={{1,0},{-1,0},{0,1},{0,-1}};
    auto flood=[&](Cell start,const std::vector<bool>& terrain) {
        std::vector<int> dist(size*size,-1);std::deque<Cell> queue;
        if(!inside(start) || !terrain[index(start)])return dist;
        dist[index(start)]=0;queue.push_back(start);
        while(!queue.empty()) {
            auto p=queue.front();queue.pop_front();
            for(auto side:sides) {
                Cell q{p.first+side.first,p.second+side.second};
                if(inside(q) && terrain[index(q)] && dist[index(q)]<0) {
                    dist[index(q)]=dist[index(p)]+1;queue.push_back(q);
                }
            }
        }
        return dist;
    };
    std::vector<int> component(size*size,-1);int largest=0;
    for(int x=2;x<size-2;++x)for(int y=10;y<size-2;++y)if(safe[y+x*size] && component[y+x*size]<0) {
        auto d=flood({x,y},safe);int count=std::count_if(d.begin(),d.end(),[](int n){return n>=0;});
        for(int i=0;i<size*size;++i)if(d[i]>=0)component[i]=0;
        if(count>largest){largest=count;for(int i=0;i<size*size;++i)if(component[i]==1)component[i]=0;for(int i=0;i<size*size;++i)if(d[i]>=0)component[i]=1;}
    }
    printlog("[DELVE] native component floor=%d largest=%d",worldFloor,largest);
    if(largest<150){printlog("[DELVE] generation rejected: small component");return false;}
    for(int i=0;i<size*size;++i)safe[i]=component[i]==1;
    std::vector<Cell> candidates;
    auto roomy=[&](Cell p) {
        for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy) {
            Cell q{p.first+dx,p.second+dy};
            if(!inside(q) || !dry[index(q)])return false;
        }
        return true;
    };
    for(int x=2;x<size-2;++x)for(int y=10;y<size-2;++y)
        if(safe[y+x*size] && !occupied.count({x,y}))candidates.push_back({x,y});
    std::vector<Cell> spawns;
    Cell corners[]={{4,12},{size-5,size-5},{size-5,12},{4,size-5}};
    auto pick=[&](Cell target,bool room,int clearance=0) {
        Cell best{-1,-1};int score=std::numeric_limits<int>::max();
        for(auto p:candidates) {
            if(!safe[index(p)] || occupied.count(p) || (room && !roomy(p)))continue;
            bool close=false;for(auto s:spawns)if(clearance && manhattan(p,s)<clearance)close=true;
            if(close)continue;
            int value=manhattan(p,target);
            if(value<score){score=value;best=p;}
        }
        return best;
    };
    printlog("[DELVE] native candidates=%u roomy=%d",(unsigned)candidates.size(),(int)std::count_if(candidates.begin(),candidates.end(),roomy));
    for(auto corner:corners) {
        auto p=pick(corner,true,size/4);
        if(p.first<0){printlog("[DELVE] generation rejected: no separated start %u",(unsigned)spawns.size());return false;}
        spawns.push_back(p);occupied.insert(p);raw(1,p);addPoint("spawns",p);
    }
    // Supply caches belong to separate starting rooms; hazards cannot occupy their immediate exits.
    for(auto node=map.entities->first;node;) {
        auto next=node->next;auto e=static_cast<Entity*>(node->element);Cell p{static_cast<int>(e->x)/16,static_cast<int>(e->y)/16};
        bool hazard=e->sprite==33 || e->sprite==38 || e->sprite==64 || e->sprite==65 || (e->sprite>=102 && e->sprite<=105);
        if(hazard && std::any_of(spawns.begin(),spawns.end(),[&](Cell s){return manhattan(p,s)<5;})) {
            occupied.erase(p);list_RemoveNode(node);
        }
        node=next;
    }
    auto core=pick({size/2,(size+10)/2},true,enemySpawnClearance);
    if(core.first<0){printlog("[DELVE] generation rejected: no core");return false;}
    occupied.insert(core);
    rapidjson::Value boss(rapidjson::kObjectType);boss.AddMember("x",core.first,allocator);boss.AddMember("y",core.second,allocator);
    boss.AddMember("monster",static_cast<int>(guardians[worldFloor-1]),allocator);
    boss.AddMember("hp",guardianHealth[worldFloor-1],allocator);
    boss.AddMember("level",guardianLevels[worldFloor-1],allocator);
    world.AddMember("boss",boss,allocator);
    // Pick three widely separated sites around triangular map anchors. The clear
    // outer ring preserves local routes before caches and other fixtures are placed.
    const int minimumSpacing=size/4;
    const int radius=(size-10)*3/10, middleY=(size+10)/2;
    const std::array<Cell,3> anchors{{{size/2,middleY-radius},
        {size/2-radius,middleY+radius/2},{size/2+radius,middleY+radius/2}}};
    auto distanceSquared=[](Cell a,Cell b){const int dx=a.first-b.first,dy=a.second-b.second;return dx*dx+dy*dy;};
    std::array<Cell,3> chosen{};int bestScore=std::numeric_limits<int>::max();
    bool carveKiosks=false;
    for(int pass=0;pass<2 && bestScore==std::numeric_limits<int>::max();++pass) {
        std::vector<Cell> sites;
        for(auto p:candidates) {
            bool clear=manhattan(p,core)>=6;
            for(auto spawn:spawns)if(manhattan(p,spawn)<8)clear=false;
            for(int dx=-kioskClearingRadius;dx<=kioskClearingRadius && clear;++dx)for(int dy=-kioskClearingRadius;dy<=kioskClearingRadius;++dy) {
                Cell q{p.first+dx,p.second+dy};
                if(!inside(q) || q==core || doorCells.count(q) || (!pass && (!safe[index(q)] || occupied.count(q)))){clear=false;break;}
            }
            if(clear)sites.push_back(p);
        }
        std::array<std::vector<Cell>,3> nearest;
        for(int n=0;n<shopsPerFloor;++n) {
            nearest[n]=sites;
            std::stable_sort(nearest[n].begin(),nearest[n].end(),[&](Cell a,Cell b){return distanceSquared(a,anchors[n])<distanceSquared(b,anchors[n]);});
            if(nearest[n].size()>64)nearest[n].resize(64);
        }
        for(auto a:nearest[0])for(auto b:nearest[1])for(auto c:nearest[2]) {
            const int ab=distanceSquared(a,b),ac=distanceSquared(a,c),bc=distanceSquared(b,c);
            const int shortest=std::min({ab,ac,bc}),longest=std::max({ab,ac,bc});
            if(shortest<minimumSpacing*minimumSpacing || longest>4*shortest)continue;
            const int score=distanceSquared(a,anchors[0])+distanceSquared(b,anchors[1])+distanceSquared(c,anchors[2])+2*(longest-shortest);
            if(score<bestScore){chosen={a,b,c};bestScore=score;}
        }
        if(pass && bestScore!=std::numeric_limits<int>::max())carveKiosks=true;
        printlog("[DELVE] kiosk_sites floor=%d clearing=%d candidates=%u selected=%d",worldFloor,pass,(unsigned)sites.size(),bestScore!=std::numeric_limits<int>::max());
    }
    if(bestScore==std::numeric_limits<int>::max()){printlog("[DELVE] generation rejected: no three spaced kiosk sites");return false;}
    world.AddMember("kiosk_minimum_spacing",minimumSpacing,allocator);
    world.AddMember("kiosk_clearings",carveKiosks ? shopsPerFloor:0,allocator);
    for(int ordinal=0;ordinal<shopsPerFloor;++ordinal) {
        const auto best=chosen[ordinal];
        const int surface=map.tiles[MAPLAYERS*index(best)];
        if(carveKiosks) {
            // Narrow native rooms may have no empty 7x7 squares. Open a small
            // dry clearing instead; its outer ring bypasses every new kiosk wall
            // and preserves the original connected routes through this area.
            for(auto node=map.entities->first;node;) {
                auto next=node->next;auto entity=static_cast<Entity*>(node->element);
                if(std::abs(static_cast<int>(entity->x)/16-best.first)<=kioskClearingRadius &&
                    std::abs(static_cast<int>(entity->y)/16-best.second)<=kioskClearingRadius)list_RemoveNode(node);
                node=next;
            }
            for(int dx=-kioskClearingRadius;dx<=kioskClearingRadius;++dx)for(int dy=-kioskClearingRadius;dy<=kioskClearingRadius;++dy) {
                Cell q{best.first+dx,best.second+dy};const int offset=MAPLAYERS*index(q);
                map.tiles[offset]=surface;map.tiles[offset+OBSTACLELAYER]=0;
                dry[index(q)]=safe[index(q)]=true;
            }
        }
        for(int dx=-kioskClearingRadius;dx<=kioskClearingRadius;++dx)for(int dy=-kioskClearingRadius;dy<=kioskClearingRadius;++dy) {
            Cell q{best.first+dx,best.second+dy};occupied.insert(q);
            if(std::abs(dx)>kioskRadius || std::abs(dy)>kioskRadius)continue;
            const int offset=MAPLAYERS*index(q);
            const bool entrance=dx==0 && dy==kioskRadius;
            const bool perimeter=std::abs(dx)==kioskRadius || std::abs(dy)==kioskRadius;
            map.tiles[offset]=kioskFloor;
            map.tiles[offset+OBSTACLELAYER]=perimeter && !entrance ?
                (dy==kioskRadius ? 54:kioskShelfWall):0;
            map.tiles[offset+2]=kioskCeiling;
            safe[index(q)]=!perimeter || entrance;
        }
        // Furnish the left alcove, leaving the central trading lane and the
        // outer bypass completely clear. Disable native random table extras.
        auto table=raw(59,{best.first-1,best.second});
        table->furnitureDir=0;table->furnitureTableRandomItemChance=0;table->furnitureTableSpawnChairs=0;
        auto chair=raw(60,{best.first-1,best.second+1});chair->furnitureDir=6;
        const std::array<Cell,4> torchSites{{{best.first-1,best.second-1},{best.first+1,best.second-1},
            {best.first-1,best.second+3},{best.first+1,best.second+3}}};
        std::array<Uint32,4> torchUIDs{};
        for(unsigned n=0;n<torchSites.size();++n)torchUIDs[n]=raw(5,torchSites[n])->getUID();
        const Cell merchantSite{best.first,best.second+1};
        auto merchant=raw(35,merchantSite);merchant->yaw=PI/2;
        safe[index({best.first-1,best.second})]=safe[index({best.first-1,best.second+1})]=safe[index(merchantSite)]=false;
        addPoint("kiosks",best);
        auto& kiosk=world["kiosks"][static_cast<rapidjson::SizeType>(ordinal)];
        kiosk.AddMember("width",5,allocator);kiosk.AddMember("height",5,allocator);
        kiosk.AddMember("entrance_x",best.first,allocator);kiosk.AddMember("entrance_y",best.second+2,allocator);
        kiosk.AddMember("approach_x",best.first,allocator);kiosk.AddMember("approach_y",best.second+3,allocator);
        kiosk.AddMember("merchant_x",merchantSite.first,allocator);kiosk.AddMember("merchant_y",merchantSite.second,allocator);
        kiosk.AddMember("shopkeeper_uid",merchant->getUID(),allocator);
        kiosk.AddMember("table_uid",table->getUID(),allocator);kiosk.AddMember("chair_uid",chair->getUID(),allocator);
        rapidjson::Value torches(rapidjson::kArrayType);
        for(auto uid:torchUIDs)torches.PushBack(uid,allocator);
        kiosk.AddMember("torch_uids",torches,allocator);
        printlog("[DELVE] kiosk floor=%d ordinal=%d center=%d,%d footprint=5x5 entrance=%d,%d shelves=1 torches=4 table=1 chair=1",worldFloor,ordinal,best.first,best.second,best.first,best.second+2);
    }
    // Fallback kiosk clearings can replace part of an old room boundary.
    // Re-evaluate its doors using the finished terrain, not stale templates.
    for(int x=2;x<size-2;++x)for(int y=10;y<size-2;++y) {
        int off=MAPLAYERS*(y+x*size),f=map.tiles[off];
        dry[y+x*size]=f>0 && !map.tiles[off+OBSTACLELAYER] && !swimmingtiles[f] && !lavatiles[f];
    }
    sanitizeDoors();reserveDoors();
    auto coreDistances=flood(core,safe);
    auto fresh=[&](Cell target,bool room=false) {
        auto p=pick(target,room);
        if(p.first>=0)occupied.insert(p);
        return p;
    };
    auto niche=[&](Cell p) {
        int open=0;
        for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy) {
            Cell q{p.first+dx,p.second+dy};if(inside(q) && dry[index(q)])++open;
        }
        return open>=5;
    };
    auto canBlock=[&](Cell p) {
        if(!safe[index(p)] || doorClearance.count(p))return false;
        std::vector<Cell> neighbors;
        for(auto side:sides) {
            Cell q{p.first+side.first,p.second+side.second};if(inside(q) && safe[index(q)])neighbors.push_back(q);
        }
        if(neighbors.empty())return false;
        // A short local bypass proves this object cannot sever the surrounding route.
        // Conservatively skip tight sites instead of flooding the whole dungeon per candidate.
        std::set<Cell> seen;std::deque<Cell> queue;seen.insert(neighbors.front());queue.push_back(neighbors.front());
        while(!queue.empty()) {
            auto q=queue.front();queue.pop_front();
            for(auto side:sides) {
                Cell next{q.first+side.first,q.second+side.second};
                if(next==p || std::abs(next.first-p.first)>2 || std::abs(next.second-p.second)>2 ||
                    !inside(next) || !safe[index(next)] || seen.count(next))continue;
                seen.insert(next);queue.push_back(next);
            }
        }
        return std::all_of(neighbors.begin(),neighbors.end(),[&](Cell q){return seen.count(q)!=0;});
    };
    auto spacedCache=[&](Cell p) {
        for(auto& c:world["chests"].GetArray())
            if(manhattan(p,{c["x"].GetInt(),c["y"].GetInt()})<4)return false;
        return true;
    };
    auto blockingSpot=[&](Cell target) {
        Cell best{-1,-1};int distance=10000;
        for(auto p:candidates) {
            int d=manhattan(p,target);
            if(d<distance && !occupied.count(p) && niche(p) && canBlock(p) && spacedCache(p)){distance=d;best=p;}
        }
        if(best.first>=0)occupied.insert(best);return best;
    };
    auto cache=[&](Cell p,int role) {raw(21,p);occupied.insert(p);safe[index(p)]=false;addPoint("chests",p,nullptr,role);};
    for(auto spawn:spawns) {
        Cell best{-1,-1};int distance=10000;
        for(auto p:candidates) {
            if(occupied.count(p) || manhattan(p,spawn)>5 || manhattan(p,spawn)<2 || !canBlock(p) || !spacedCache(p))continue;
            int d=manhattan(p,spawn);if(d<distance){best=p;distance=d;}
        }
        if(best.first<0){printlog("[DELVE] generation rejected: missing start cache");return false;}
        cache(best,0);
    }
    printlog("[DELVE] native starts and starter caches ready");
    int ordinal=0;
    for(int n=0;n<3;++n) {
        auto p=blockingSpot({core.first+(n-1)*3,core.second+3});
        if(p.first<0){printlog("[DELVE] generation rejected: no relic site");return false;}cache(p,4);
    }
    std::mt19937 rng(seed^0xd31fe123u);std::shuffle(candidates.begin(),candidates.end(),rng);
    for(auto p:candidates) {
        if(world["chests"].Size()>=world["chest_target"].GetInt())break;
        if(occupied.count(p) || !niche(p) || !canBlock(p) || !spacedCache(p))continue;
        cache(p,1+(ordinal++%3));
    }
    if(world["chests"].Size()<world["chest_target"].GetInt()){printlog("[DELVE] generation rejected: only %u cache sites",world["chests"].Size());return false;}
    // Endpoints use clear room tiles in the connected dungeon, never a sealed native vault.
    int descents=BattleRules::descentCount;
    Cell targets[]={{size/2,14},{size/2,size-7},{7,(size+10)/2},{size-7,(size+10)/2}};
    for(int n=0;n<descents;++n) {
        auto p=fresh(targets[n]);if(p.first<0){printlog("[DELVE] generation rejected: missing descent");return false;}raw(11,p);addPoint("descents",p);
    }
    for(int n=0;n<BattleRules::victoryPortalCount;++n) {
        auto p=fresh({core.first+(n==1 ? -5: n==2 ? 5:0),core.second-4+(n%2)*7});
        if(p.first<0){printlog("[DELVE] generation rejected: missing portal");return false;}raw(45,p);addPoint("portals",p);
    }
    int fountains=0;
    for(auto node=map.entities->first;node;node=node->next) {
        auto e=static_cast<Entity*>(node->element);fountains+=e->sprite==14;
    }
    while(fountains<3) {auto p=blockingSpot(targets[fountains]);if(p.first<0)return false;raw(14,p);safe[index(p)]=false;++fountains;}
    auto away=[&](Cell p){
        if(manhattan(p,core)<guardianClearance ||
            !std::all_of(spawns.begin(),spawns.end(),[&](Cell s){return manhattan(p,s)>=enemySpawnClearance;}))return false;
        for(auto& encounter:world["encounters"].GetArray())
            if(manhattan(p,{encounter["x"].GetInt(),encounter["y"].GetInt()})<encounterSeparation)return false;
        return true;
    };
    std::shuffle(nativeGuards.begin(),nativeGuards.end(),rng);
    for(auto p:nativeGuards) {
        if(world["encounters"].Size()>=world["guard_target"].GetInt())break;
        if(!inside(p) || !safe[index(p)] || occupied.count(p) || !away(p))continue;
        occupied.insert(p);addPoint("encounters",p,"patrol");
    }
    for(auto p:candidates) {
        if(world["encounters"].Size()>=world["guard_target"].GetInt())break;
        if(occupied.count(p) || !safe[index(p)] || !roomy(p) || !away(p))continue;
        occupied.insert(p);addPoint("encounters",p,"patrol");
    }
    if(world["encounters"].Size()!=world["guard_target"].GetInt()) {
        printlog("[DELVE] generation rejected: insufficient separated encounter sites");return false;
    }
    const auto roster=encounterRoster(worldFloor,world["encounters"].Size(),seed);
    for(size_t index=0;index<roster.size();++index) {
        auto& encounter=world["encounters"][static_cast<rapidjson::SizeType>(index)];
        const Monster mob=roster[index];
        const char* kind=mob==RAT || mob==BAT_SMALL ? "vermin" : mob==TROLL || mob==CRYSTALGOLEM ? "elite" : "patrol";
        encounter["kind"].SetString(kind,allocator);
        encounter.AddMember("monster",static_cast<int>(mob),allocator);
    }
    const char* kinds[]={"medicine","gear","food","magic","tool","treasure"};
    for(auto p:candidates) {
        if(world["drops"].Size()>=world["drop_target"].GetInt())break;
        if(occupied.count(p) || !safe[index(p)] || !roomy(p))continue;
        occupied.insert(p);addPoint("drops",p,kinds[world["drops"].Size()%6]);
    }
    for(int n=0;n<6;++n) {
        auto p=fresh({core.first+(n%3-1)*5,core.second+(n/3 ? 5:-5)},false);
        if(p.first<0)return false;
        addPoint("gold",p);world["gold"][n].AddMember("scale",n%3+1,allocator);
    }
    // Insert four disconnected cells in the reserved strip, matching park().
    const int surface=map.tiles[MAPLAYERS*(spawns[0].second+spawns[0].first*size)];
    const int wall=worldFloor==1 ? 20:worldFloor==2 ? 37:156;
    for(int x=0;x<size;++x)for(int y=0;y<9;++y) {
        int off=MAPLAYERS*(y+x*size);map.tiles[off]=surface;map.tiles[off+1]=wall;map.tiles[off+2]=8;
    }
    for(int n=0;n<4;++n) {
        int x=3+n*8;
        for(int dx=-1;dx<=2;++dx)for(int y=2;y<=5;++y)map.tiles[MAPLAYERS*(y+(x+dx)*size)+OBSTACLELAYER]=0;
        raw(12,{x+1,3});
    }
    // Reject a placement if added furniture/caches closed a required route.
    for(auto node=map.entities->first;node;node=node->next) {
        auto e=static_cast<Entity*>(node->element);Cell p{static_cast<int>(e->x)/16,static_cast<int>(e->y)/16};
        if(inside(p) && blocking(*e))safe[index(p)]=false;
    }
    auto reachable=flood(spawns[0],safe);
    auto reached=[&](Cell p){return inside(p) && reachable[index(p)]>=0;};
    for(auto p:spawns)if(!reached(p)){printlog("[DELVE] generation rejected: disconnected start");return false;}
    if(!reached(core)){printlog("[DELVE] generation rejected: disconnected core");return false;}
    for(auto& kiosk:world["kiosks"].GetArray()) {
        if(!reached({kiosk["entrance_x"].GetInt(),kiosk["entrance_y"].GetInt()}) ||
            !reached({kiosk["approach_x"].GetInt(),kiosk["approach_y"].GetInt()})) {
            printlog("[DELVE] generation rejected: inaccessible kiosk");return false;
        }
    }
    for(auto key:{"descents","portals","drops","encounters","gold"})for(auto& v:world[key].GetArray())
        if(!reached({v["x"].GetInt(),v["y"].GetInt()})){printlog("[DELVE] generation rejected: disconnected %s",key);return false;}
    auto chestApproachable=[&](const rapidjson::Value& chest,const std::vector<int>& distances) {
        Cell p{chest["x"].GetInt(),chest["y"].GetInt()};
        for(auto side:sides) {
            Cell q{p.first+side.first,p.second+side.second};
            if(inside(q) && distances[index(q)]>=0)return true;
        }
        return false;
    };
    for(auto& chest:world["chests"].GetArray())if(!chestApproachable(chest,reachable)) {
        printlog("[DELVE] generation rejected: inaccessible cache");return false;
    }
    // Plan the challenge first, then supply its reward. Native gate rows form a
    // single barrier, and keep their gate models with a dedicated native lever.
    // Reassign a loose reward from this floor's budget when a valid room is empty.
    auto mainAccess=safe;
    std::vector<Cell> lockApproaches;
    std::vector<Entity*> barriers;
    for(auto node=map.entities->first;node;) {
        auto next=node->next;auto e=static_cast<Entity*>(node->element);
        if(!doorway(e->sprite)){node=next;continue;}
        Cell p{static_cast<int>(e->x)/16,static_cast<int>(e->y)/16},step=doorStep(e->sprite);
        Cell a{p.first-step.first,p.second-step.second},b{p.first+step.first,p.second+step.second};
        Cell jambA{p.first-step.second,p.second-step.first},jambB{p.first+step.second,p.second+step.first};
        if(!reached(p) || !reached(a) || !reached(b) || (!gateSprite(e->sprite) &&
            (!map.tiles[MAPLAYERS*index(jambA)+OBSTACLELAYER] || !map.tiles[MAPLAYERS*index(jambB)+OBSTACLELAYER]))) {
            list_RemoveNode(node);++removedDoors;node=next;continue;
        }
        barriers.push_back(e);
        node=next;
    }
    auto cell=[](Entity* e){return Cell{static_cast<int>(e->x)/16,static_cast<int>(e->y)/16};};
    std::stable_sort(barriers.begin(),barriers.end(),[&](Entity* a,Entity* b){
        if(gateSprite(a->sprite)!=gateSprite(b->sprite))return gateSprite(a->sprite);
        return cell(a)<cell(b);
    });
    std::vector<std::vector<Entity*>> groups;
    std::set<Entity*> grouped;
    for(auto e:barriers) {
        if(grouped.count(e))continue;
        std::vector<Entity*> group{e};grouped.insert(e);
        if(gateSprite(e->sprite))for(unsigned i=0;i<group.size();++i)for(auto other:barriers) {
            if(grouped.count(other) || !gateSprite(other->sprite) || doorStep(e->sprite)!=doorStep(other->sprite))continue;
            auto a=cell(group[i]),b=cell(other),step=doorStep(e->sprite);
            if(manhattan(a,b)==1 && (a.first-b.first)*step.first+(a.second-b.second)*step.second==0) {
                group.push_back(other);grouped.insert(other);
            }
        }
        groups.push_back(group);
    }
    int rewardLocks=0,woodLocks=0,preservedGates=0,generatedRewards=0;
    std::set<int> usedDropRewards;
    std::set<Cell> reservedLeverCells;
    for(auto key:{"chests","descents","portals"})for(auto& v:world[key].GetArray())
        reservedLeverCells.insert({v["x"].GetInt(),v["y"].GetInt()});
    for(auto& kiosk:world["kiosks"].GetArray()) {
        reservedLeverCells.insert({kiosk["entrance_x"].GetInt(),kiosk["entrance_y"].GetInt()});
        reservedLeverCells.insert({kiosk["approach_x"].GetInt(),kiosk["approach_y"].GetInt()});
    }
    for(unsigned groupID=0;groupID<groups.size();++groupID) {
        auto& group=groups[groupID];const bool gate=gateSprite(group.front()->sprite);
        const auto open=flood(spawns[0],mainAccess);
        for(auto e:group)mainAccess[index(cell(e))]=false;
        const auto closed=flood(spawns[0],mainAccess);
        bool essential=closed[index(core)]<0;
        for(auto s:spawns)if(closed[index(s)]<0)essential=true;
        for(auto approach:lockApproaches)if(closed[index(approach)]<0)essential=true;
        for(auto key:{"descents","portals"})for(auto& v:world[key].GetArray())
            if(closed[index({v["x"].GetInt(),v["y"].GetInt()})]<0)essential=true;
        for(auto& kiosk:world["kiosks"].GetArray())
            if(closed[index({kiosk["approach_x"].GetInt(),kiosk["approach_y"].GetInt()})]<0 ||
                closed[index({kiosk["entrance_x"].GetInt(),kiosk["entrance_y"].GetInt()})]<0)essential=true;
        int vaultTiles=0;for(int i=0;i<size*size;++i)if(open[i]>=0 && closed[i]<0)++vaultTiles;
        const bool optional=!essential && open[index(cell(group.front()))]>=0 && vaultTiles>=5 && vaultTiles<=120 && (gate || woodLocks<3);
        auto groupDistance=[&](Cell p){int d=10000;for(auto e:group)d=std::min(d,manhattan(p,cell(e)));return d;};
        int reward=-1;const char* rewardKind="none";bool generatedReward=false;
        if(optional) {
            for(rapidjson::SizeType i=0;i<world["chests"].Size();++i) {
                auto& c=world["chests"][i];
                if(c["role"].GetInt()!=0 && chestApproachable(c,open) && !chestApproachable(c,closed)) {
                    reward=i;rewardKind="chest";break;
                }
            }
            if(reward<0)for(rapidjson::SizeType i=0;i<world["drops"].Size();++i) {
                auto& d=world["drops"][i];Cell p{d["x"].GetInt(),d["y"].GetInt()};
                if(!usedDropRewards.count(i) && open[index(p)]>=0 && closed[index(p)]<0 && groupDistance(p)>=3) {
                    reward=i;rewardKind="drop";break;
                }
            }
            // Move an ordinary main-route cache into the empty room. The exact
            // cache budget is unchanged and the chest entity follows its record.
            if(reward<0) {
                Cell destination{-1,-1};int best=-1;
                for(auto p:candidates) {
                    int d=groupDistance(p);
                    if(d<=best || d<3 || occupied.count(p) || open[index(p)]<0 || closed[index(p)]>=0 || !niche(p) || !canBlock(p))continue;
                    destination=p;best=d;
                }
                int source=-1;
                if(destination.first>=0)for(int role:{2,1,3}) {
                    for(rapidjson::SizeType i=0;source<0 && i<world["chests"].Size();++i) {
                        auto& c=world["chests"][i];
                        if(c["role"].GetInt()!=role || !chestApproachable(c,closed))continue;
                        bool spaced=true;
                        for(rapidjson::SizeType j=0;j<world["chests"].Size();++j)if(j!=i &&
                            manhattan(destination,{world["chests"][j]["x"].GetInt(),world["chests"][j]["y"].GetInt()})<4)spaced=false;
                        if(spaced)source=static_cast<int>(i);
                    }
                    if(source>=0)break;
                }
                Entity* moved=nullptr;
                if(source>=0) {
                    auto& c=world["chests"][static_cast<rapidjson::SizeType>(source)];Cell from{c["x"].GetInt(),c["y"].GetInt()};
                    for(auto node=map.entities->first;node && !moved;node=node->next) {
                        auto e=static_cast<Entity*>(node->element);
                        if(e->sprite==21 && cell(e)==from)moved=e;
                    }
                    if(moved) {
                        moved->x=destination.first*16;moved->y=destination.second*16;
                        occupied.erase(from);occupied.insert(destination);reservedLeverCells.insert(destination);
                        safe[index(from)]=mainAccess[index(from)]=true;
                        safe[index(destination)]=mainAccess[index(destination)]=false;
                        c["x"].SetInt(destination.first);c["y"].SetInt(destination.second);
                        reward=source;rewardKind="chest";generatedReward=true;++generatedRewards;
                    }
                }
            }
            if(reward<0) {
                Cell destination{-1,-1};int best=-1;
                for(auto p:candidates) {
                    int d=groupDistance(p);
                    if(d>best && d>=3 && safe[index(p)] && open[index(p)]>=0 && closed[index(p)]<0 && !occupied.count(p)) {
                        destination=p;best=d;
                    }
                }
                if(destination.first>=0)for(rapidjson::SizeType i=0;i<world["drops"].Size();++i) {
                    auto& d=world["drops"][i];Cell p{d["x"].GetInt(),d["y"].GetInt()};
                    if(usedDropRewards.count(i) || closed[index(p)]<0 ||
                        (std::strcmp(d["kind"].GetString(),"gear") && std::strcmp(d["kind"].GetString(),"treasure")))continue;
                    occupied.erase(p);occupied.insert(destination);
                    d["x"].SetInt(destination.first);d["y"].SetInt(destination.second);
                    reward=i;rewardKind="drop";generatedReward=true;++generatedRewards;break;
                }
            }
        }
        if(reward>=0) {
            ++rewardLocks;if(!gate)++woodLocks;
            if(!std::strcmp(rewardKind,"drop"))usedDropRewards.insert(reward);
            for(auto e:group)for(auto side:sides) {
                Cell p=cell(e),q{p.first+side.first,p.second+side.second};
                if(inside(q) && closed[index(q)]>=0){lockApproaches.push_back(q);break;}
            }
        }
        // Vanilla-style gate control: gates start closed (inverted native gates
        // start open) and a lever sits a few tiles away against a wall, off the
        // doorway lane. A reward gate is controlled only from outside. Any other
        // gate that splits the floor gets a lever on each side, and every lever
        // toggles it, so closing a gate can never strand a player.
        std::vector<Cell> leverSites;
        if(gate) {
            const Cell first=cell(group.front()),step=doorStep(group.front()->sprite);
            auto pickLever=[&](const std::vector<int>& region) {
                Cell site{-1,-1};int best=1<<30;
                for(int pass=0;pass<2 && site.first<0;++pass)for(auto p:candidates) {
                    const int d=groupDistance(p);
                    if(occupied.count(p) || !safe[index(p)] || region[index(p)]<0 || d<1 || (pass==0 && (d<3 || d>8)) || d>12)continue;
                    bool wall=false;
                    for(auto side:sides){Cell q{p.first+side.first,p.second+side.second};if(!inside(q) || !dry[index(q)])wall=true;}
                    bool lane=false;
                    for(auto e:group){Cell g=cell(e);if(d<=3 && (step.first ? p.second==g.second:p.first==g.first))lane=true;}
                    const int score=std::abs(d-5)*2+(wall ? 0:4)+(lane ? 6:0)+(pass ? 20:0);
                    if(score<best){best=score;site=p;}
                }
                // Corridors and small pockets may have no room candidates; use any
                // free floor tile on that side, nearest the gate.
                if(site.first<0)for(int x=0;x<size;++x)for(int y=0;y<size;++y) {
                    Cell p{x,y};
                    if(!inside(p) || region[index(p)]<0 || occupied.count(p) || doorCells.count(p) || !safe[index(p)])continue;
                    const int score=groupDistance(p);
                    if(score>=1 && score<best){best=score;site=p;}
                }
                // Tiny pockets between gates: share a tile with a passable prop or
                // loose loot, never with chests, exits, kiosks or another lever.
                if(site.first<0)for(int x=0;x<size;++x)for(int y=0;y<size;++y) {
                    Cell p{x,y};
                    if(!inside(p) || region[index(p)]<0 || doorCells.count(p) || !safe[index(p)] || reservedLeverCells.count(p))continue;
                    const int score=groupDistance(p);
                    if(score>=1 && score<best){best=score;site=p;}
                }
                if(site.first<0)printlog("[DELVE] gate side without lever site at %d,%d (%d tiles)",first.first,first.second,
                    static_cast<int>(std::count_if(region.begin(),region.end(),[](int v){return v>=0;})));
                return site;
            };
            if(reward>=0) leverSites.push_back(pickLever(closed));
            else {
                const auto sideA=flood({first.first-step.first,first.second-step.second},mainAccess);
                const auto sideB=flood({first.first+step.first,first.second+step.second},mainAccess);
                const bool split=sideA[index({first.first+step.first,first.second+step.second})]<0;
                for(auto* region:{&sideA,&sideB}) {
                    if(region==&sideB && !split)break;
                    if(std::none_of(region->begin(),region->end(),[](int v){return v>=0;}))continue;
                    leverSites.push_back(pickLever(*region));
                }
            }
            if(leverSites.empty() || std::any_of(leverSites.begin(),leverSites.end(),[](Cell p){return p.first<0;})) {
                printlog("[DELVE] generation rejected: no gate control site");return false;
            }
        }
        if(reward<0)for(auto e:group)mainAccess[index(cell(e))]=safe[index(cell(e))];
        std::vector<Uint32> leverIDs;
        for(auto site:leverSites) {
            leverIDs.push_back(raw(17,site)->getUID());occupied.insert(site);reservedLeverCells.insert(site);
            if(open[index(site)]>=0)lockApproaches.push_back(site);
        }
        const bool startsOpen=gate && reward<0 && (group.front()->sprite==113 || group.front()->sprite==114);
        for(auto e:group) {
            Cell p=cell(e),step=doorStep(e->sprite);
            if(gate){gateControls[index(p)]={leverIDs,startsOpen};++preservedGates;}
            else e->doorForceLockedUnlocked=reward>=0 ? 1:2;
            addPoint("doors",p);
            auto& door=world["doors"][world["doors"].Size()-1];
            door.AddMember("axis",step.first ? 0:1,allocator);door.AddMember("group",groupID,allocator);
            door.AddMember("kind",rapidjson::StringRef(gate ? "gate":e->sprite==217 || e->sprite==218 ? "iron":"wood"),allocator);
            door.AddMember("locked",reward>=0,allocator);door.AddMember("reward_index",reward,allocator);
            door.AddMember("reward_kind",rapidjson::StringRef(rewardKind),allocator);
            door.AddMember("reward_generated",generatedReward,allocator);
            door.AddMember("vault_tiles",reward>=0 ? vaultTiles:0,allocator);
            if(gate) {
                door.AddMember("lever_uid",leverIDs.front(),allocator);
                door.AddMember("lever_x",leverSites.front().first,allocator);door.AddMember("lever_y",leverSites.front().second,allocator);
                rapidjson::Value levers(rapidjson::kArrayType);
                for(size_t i=0;i<leverSites.size();++i) {
                    rapidjson::Value v(rapidjson::kObjectType);
                    v.AddMember("uid",leverIDs[i],allocator);v.AddMember("x",leverSites[i].first,allocator);v.AddMember("y",leverSites[i].second,allocator);
                    levers.PushBack(v,allocator);
                }
                door.AddMember("levers",levers,allocator);
                door.AddMember("starts_open",startsOpen,allocator);
            }
        }
    }
    world.AddMember("removed_invalid_doors",removedDoors,allocator);
    world.AddMember("converted_native_gates",0,allocator);
    world.AddMember("preserved_native_gates",preservedGates,allocator);
    world.AddMember("generated_room_rewards",generatedRewards,allocator);
    world.AddMember("cleared_door_props",clearedDoorProps,allocator);
    world.AddMember("reward_locks",rewardLocks,allocator);
    printlog("[DELVE] placement_audit floor=%d native_shop_tiles=%d replaced_chests=%d invalid_doors=%d preserved_gates=%d reward_rooms=%d generated_rewards=%d",
        worldFloor,nativeShopTiles,removedNativeChests,removedDoors,preservedGates,rewardLocks,generatedRewards);
    // Capture points and a straight combat lane are smoke-only diagnostics.
#ifdef BARONY_SMOKE_TESTS
    Cell lane{-1,-1};
    for(auto p:candidates) {
        bool good=true;for(int dx=0;dx<=4;++dx) {
            Cell q{p.first+dx,p.second};
            if(!reached(q) || occupied.count(q))good=false;
        }
        if(good){lane=p;break;}
    }
    if(lane.first<0){printlog("[DELVE] generation rejected: no test lane");return false;}
    rapidjson::Value combat(rapidjson::kObjectType);combat.AddMember("x",lane.first,allocator);combat.AddMember("y",lane.second,allocator);
    world.AddMember("combat_lane",combat,allocator);
    Cell viewTargets[]={{spawns[0].first+2,spawns[0].second},{size/3,size/2},{core.first-4,core.second},{size*3/4,size*3/4}};
    for(int n=0;n<4;++n) {
        if(n==1 && !world["kiosks"].Empty()) {
            auto& kiosk=world["kiosks"][0];
            addPoint("views",{kiosk["approach_x"].GetInt(),kiosk["approach_y"].GetInt()});
            world["views"][n].AddMember("yaw",-PI/2,allocator);
            continue;
        }
        auto p=pick(viewTargets[n],true);if(p.first<0)return false;addPoint("views",p);
        int longest=-1,dir=0;
        for(int d=0;d<4;++d) {
            Cell step=d==0 ? Cell{1,0}:d==1 ? Cell{0,1}:d==2 ? Cell{-1,0}:Cell{0,-1};
            int count=0;for(int k=1;k<=12;++k) {
                Cell q{p.first+k*step.first,p.second+k*step.second};
                if(!inside(q) || !dry[index(q)])break;++count;
            }
            if(count>longest){longest=count;dir=d;}
        }
        world["views"][n].AddMember("yaw",dir*PI/2,allocator);
    }
#endif
    std::uint64_t fingerprint=1469598103934665603ull;
    for(unsigned i=0;i<map.width*map.height*MAPLAYERS;++i){fingerprint^=static_cast<unsigned>(map.tiles[i]);fingerprint*=1099511628211ull;}
    world.AddMember("seed",seed,allocator);world.AddMember("geometry_fingerprint",rapidjson::Value().SetUint64(fingerprint),allocator);
    world.AddMember("reachable_tiles",largest,allocator);
    int spellTraps=0;
    for(auto node=map.entities->first;node;node=node->next) {
        auto entity=static_cast<Entity*>(node->element);
        if(entity->sprite==65 || entity->sprite==120)++spellTraps;
    }
    world.AddMember("spell_traps",spellTraps,allocator);
#ifdef BARONY_SMOKE_TESTS
    rapidjson::Value diagram(rapidjson::kArrayType);
    rapidjson::Value walkable(rapidjson::kArrayType);
    for (int y=0;y<size;++y) {
        std::string row;
        for (int x=0;x<size;++x) {
            const int offset=MAPLAYERS*(y+x*size),surface=map.tiles[offset];
            row += map.tiles[offset+OBSTACLELAYER] ? '#' : !surface ? ' ' : lavatiles[surface] ? '^' : swimmingtiles[surface] ? '~' : '.';
        }
        diagram.PushBack(rapidjson::Value(row.c_str(),allocator),allocator);
        row.clear();for(int x=0;x<size;++x)row+=safe[y+x*size] ? '.':'#';
        walkable.PushBack(rapidjson::Value(row.c_str(),allocator),allocator);
    }
    world.AddMember("diagram_tiles",diagram,allocator);
    world.AddMember("diagram_walkable",walkable,allocator);
    rapidjson::Value blockers(rapidjson::kArrayType);
    for(auto node=map.entities->first;node;node=node->next) {
        auto e=static_cast<Entity*>(node->element);if(!blocking(*e))continue;
        rapidjson::Value v(rapidjson::kObjectType);
        v.AddMember("x",static_cast<int>(e->x)/16,allocator);v.AddMember("y",static_cast<int>(e->y)/16,allocator);
        v.AddMember("sprite",e->sprite,allocator);blockers.PushBack(v,allocator);
    }
    world.AddMember("static_blockers",blockers,allocator);
#endif
    rapidjson::Value uids(rapidjson::kArrayType);
    for(auto node=map.entities->first;node;node=node->next) {
        auto e=static_cast<Entity*>(node->element);
        if(e->sprite!=11 && e->sprite!=45 && e->sprite!=14 && e->sprite!=21 && e->sprite!=35)continue;
        rapidjson::Value v(rapidjson::kArrayType);v.PushBack(e->sprite,allocator);v.PushBack((int)e->x/16,allocator);v.PushBack((int)e->y/16,allocator);v.PushBack(e->getUID(),allocator);uids.PushBack(v,allocator);
    }
    world.AddMember("interaction_uids",uids,allocator);
    rapidjson::StringBuffer buffer;rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);world.Accept(writer);
    std::ofstream output("delve-generated-floor"+std::to_string(worldFloor)+".json",std::ios::binary|std::ios::trunc);
    output.write(buffer.GetString(),buffer.GetSize());
    printlog("[DELVE] native_layout floor=%d theme=%s size=%d chests=%u drops=%u reachable=%d fingerprint=%llu",
        worldFloor,world["theme"].GetString(),size,world["chests"].Size(),world["drops"].Size(),largest,(unsigned long long)fingerprint);
    return true;
}
}
int generateOfficialFloor(unsigned seed,int* checkMapHash) {
#ifdef BARONY_SMOKE_TESTS
    const auto seedVariable="BARONY_SMOKE_DELVE_FLOOR"+std::to_string(worldFloor)+"_SEED";
    if(const char* forced=std::getenv(seedVariable.c_str())) {
        seed=static_cast<unsigned>(std::strtoul(forced,nullptr,10));
        printlog("[SMOKE][DELVE] generation_seed floor=%d seed=%u",worldFloor,seed);
    }
#endif
    rapidjson::Document spec;spec.CopyFrom(world,spec.GetAllocator());
    for(int attempt=0;attempt<4;++attempt) {
        world.CopyFrom(spec,world.GetAllocator());
        char theme[32];std::strncpy(theme,world["theme"].GetString(),sizeof(theme)-1);theme[31]=0;
        Uint32 began=SDL_GetTicks();
        nativeGenerating=true;minotaurlevel=false;darkmap=false;
        int result=generateDungeon(theme,seed+attempt*0x9e3779b9u,std::make_tuple(0,0,0,1));
        nativeGenerating=false;mapseed=seed;
        Uint32 generated=SDL_GetTicks();
        if(result>=0 && adaptOfficial(seed)) {
            printlog("[DELVE] generation_time floor=%d native_ms=%u adaptation_ms=%u",worldFloor,generated-began,SDL_GetTicks()-generated);
            if(checkMapHash)*checkMapHash=-1;
            return result;
        }
        printlog("[DELVE] retry native layout floor=%d attempt=%d",worldFloor,attempt+1);
    }
    printlog("[DELVE] could not generate a connected native dungeon");worldFloor=0;mainloop=0;return -1;
}


bool controlGate(Entity& gate) {
    if(!active() || !worldFloor)return false;
    const int key=static_cast<int>(gate.y)/16+(static_cast<int>(gate.x)/16)*map.height;
    auto control=gateControls.find(key);if(control==gateControls.end())return false;
    gate.gateInverted=0;
    if(multiplayer!=CLIENT) {
        // Every lever on the gate toggles it, so either side can open or close it.
        bool pulled=false;
        for(auto uid:control->second.levers) {
            auto base=uidToEntity(uid);
            auto lever=base ? uidToEntity(base->parent):nullptr;
            if(lever && lever->behavior==&actSwitch && lever->skill[0])pulled=!pulled;
        }
        const int status=control->second.startsOpen ? !pulled:pulled;
        if(gate.gateStatus!=status) {
            gate.gateStatus=status;
            if(multiplayer==SERVER)serverUpdateEntitySkill(&gate,3);
            playSoundEntity(&gate,status ? 81:82,64);
        }
    } else if(!gate.gateInit)gate.gateStatus=control->second.startsOpen;
    return true;
}

void configureChest(Entity& chest) {
    const int x=static_cast<int>(chest.x)/16,y=static_cast<int>(chest.y)/16;
    chest.chestMimicChance=0;chest.skill[20]=0;chest.chestLocked=0;
    if(!worldFloor)return;
    for(auto& c:world["chests"].GetArray()) if(c["x"].GetInt()==x && c["y"].GetInt()==y) {
        chest.skill[20]=c["role"].GetInt();chest.yaw=c["facing"].GetInt();
        // Valuable vaults can be locked. Starting supplies and ordinary caches stay accessible.
        chest.chestLocked=worldFloor>1 && chest.skill[20]>=3 ? 65 : 0;return;
    }
}
void populateWorld() {
    const int floor=currentlevel;
    if(multiplayer!=SERVER || floor!=worldFloor || floor<1 || floor>3)return;
    Entity* anchor=nullptr;
    for(int p=0;p<MAXPLAYERS;++p)if(players[p] && players[p]->entity){anchor=players[p]->entity;break;}
    if(!anchor)return;
    int count=0,dropped=0;
    for(auto& v:world["encounters"].GetArray()) {
        int x=v["x"].GetInt(),y=v["y"].GetInt();
        const Monster mob=static_cast<Monster>(v["monster"].GetInt());
        if(auto guard=summonMonster(mob,x*16+8,y*16+8)) {
            configureDelveGuard(*guard,seedAt(x,y));++count;
        }
    }
    for(auto& v:world["drops"].GetArray()) {
        int x=v["x"].GetInt(),y=v["y"].GetInt();std::mt19937 rng(seedAt(x,y));
        auto kind=v["kind"].GetString();ItemType type=randomEquipment(floor-1,rng);int qty=1;
        if(!std::strcmp(kind,"medicine"))type=rng()%3 ? POTION_HEALING : POTION_RESTOREMAGIC;
        else if(!std::strcmp(kind,"food")) { type=FOOD_BREAD;qty=2; }
        else if(!std::strcmp(kind,"tool")) { const ItemType tools[]={TOOL_LOCKPICK,TOOL_TOWEL,TOOL_BEARTRAP};type=tools[rng()%3]; }
        else if(!std::strcmp(kind,"magic"))type=randomMagic(floor-1,rng);
        else if(!std::strcmp(kind,"treasure")) { type=GEM_DIAMOND;qty=floor; }
        auto item=newItem(type,SERVICABLE,floor-1,qty,rng(),instantIdentification(),nullptr);
        const auto rarityRoll=rng();
        DelveRarity::rollLoot(item,floor,rarityRoll,rng());
        if(auto drop=dropItemMonster(item,anchor,nullptr)) {
            drop->x=x*16+8;drop->y=y*16+8;drop->vel_x=drop->vel_y=0;++dropped;
        }
    }
    for(auto& v:world["gold"].GetArray()) {
        auto gold=newEntity(130,1,map.entities,nullptr);gold->behavior=&actGoldBag;
        gold->x=v["x"].GetInt()*16+8;gold->y=v["y"].GetInt()*16+8;
        gold->sizex=gold->sizey=3;gold->z=6;gold->goldAmount=50*floor*floor*v["scale"].GetInt();
        gold->flags[PASSABLE]=true;gold->flags[UPDATENEEDED]=true;
    }
    if(auto boss=summonMonster(guardians[floor-1],world["boss"]["x"].GetInt()*16+8,world["boss"]["y"].GetInt()*16+8)) {
        configureDelveGuard(*boss,seedAt(world["boss"]["x"].GetInt(),world["boss"]["y"].GetInt()));
        if(auto s=boss->getStats()) {
            std::strcpy(s->name,floor==1 ? "Outpost Warden" : floor==2 ? "Jungle Warden" : "Sanctum Guardian");
            s->MAXHP=s->HP=s->OLDHP=guardianHealth[floor-1];s->LVL=guardianLevels[floor-1];s->GOLD=200*floor;
            s->RANDOM_HP=s->RANDOM_MAXHP=s->RANDOM_LVL=0;
        }
    }
    printlog("[DELVE] floor_ready floor=%d size=%ux%u monsters=%d chests=%u loose=%d seed=%u",
        floor,map.width,map.height,count+1,world["chests"].Size(),dropped,mapseed);
}
#ifdef BARONY_SMOKE_TESTS
bool smokeKioskAudit() {
    if(!active() || multiplayer!=SERVER || worldFloor<1 || !world.HasMember("kiosks") || world["kiosks"].Size()!=shopsPerFloor)return false;
    if(world["native_shop_tiles"].GetInt()!=0 || world["chests"].Size()!=world["chest_target"].GetInt())return false;
    int actualChests=0,actualDoors=0,actualGates=0;
    for(auto node=map.entities->first;node;node=node->next) {
        auto e=static_cast<Entity*>(node->element);
        if(e->behavior!=&actChest && e->behavior!=&actDoor && e->behavior!=&actIronDoor && e->behavior!=&actGate)continue;
        const Cell p{static_cast<int>(e->x)/16,static_cast<int>(e->y)/16};
        bool found=false;
        for(auto& v:world[e->behavior==&actChest ? "chests":"doors"].GetArray()) {
            if(p!=Cell{v["x"].GetInt(),v["y"].GetInt()})continue;
            if((e->behavior==&actDoor || e->behavior==&actIronDoor) && (!e->doorInit || e->doorLocked!=v["locked"].GetBool() ||
                e->doorDisableLockpicks || e->doorDisableOpening))return false;
            if(e->behavior==&actGate) {
                if(std::strcmp(v["kind"].GetString(),"gate") || !e->gateInit ||
                    e->gateStatus!=v["starts_open"].GetBool() || e->flags[PASSABLE]!=v["starts_open"].GetBool())return false;
                if(!v.HasMember("levers") || v["levers"].Empty() || (v["locked"].GetBool() && v["levers"].Size()!=1))return false;
                for(auto& control:v["levers"].GetArray()) {
                    auto base=uidToEntity(control["uid"].GetUint()),lever=base ? uidToEntity(base->parent):nullptr;
                    if(!lever || lever->behavior!=&actSwitch || !lever->flags[PASSABLE])return false;
                    const int oldPower=lever->skill[0],oldStatus=e->gateStatus;
                    lever->toggleSwitch();controlGate(*e);
                    const bool toggled=e->gateStatus!=oldStatus;
                    lever->toggleSwitch();controlGate(*e);
                    if(!toggled || lever->skill[0]!=oldPower || e->gateStatus!=oldStatus)return false;
                }
                ++actualGates;
            }
            found=true;break;
        }
        if(!found)return false;
        if(e->behavior==&actChest)++actualChests;else ++actualDoors;
    }
    if(actualChests!=world["chests"].Size() || actualDoors!=world["doors"].Size())return false;
    if(actualGates!=world["preserved_native_gates"].GetInt())return false;
    std::set<unsigned> rewardGroups;
    for(auto& door:world["doors"].GetArray()) {
        if(!door["locked"].GetBool() || !rewardGroups.insert(door["group"].GetUint()).second)continue;
        const bool chest=!std::strcmp(door["reward_kind"].GetString(),"chest");
        const auto& reward=world[chest ? "chests":"drops"][door["reward_index"].GetInt()];
        bool present=false;
        for(auto node=map.entities->first;node;node=node->next) {
            auto e=static_cast<Entity*>(node->element);
            if(e->behavior!=(chest ? &actChest:&actItem) ||
                std::abs(e->x-(reward["x"].GetInt()*16+8))>4 || std::abs(e->y-(reward["y"].GetInt()*16+8))>4)continue;
            if(!chest || (e->getChestInventoryList() && e->getChestInventoryList()->first)){present=true;break;}
        }
        if(!present)return false;
    }
    if(rewardGroups.size()!=world["reward_locks"].GetInt())return false;
    printlog("[SMOKE][DELVE] placement_native_audit floor=%d chests=%d doors=%d reward_locks=%d preserved_gates=%d generated_rewards=%d native_levers_toggle=1",
        worldFloor,actualChests,actualDoors,world["reward_locks"].GetInt(),actualGates,world["generated_room_rewards"].GetInt());
    int pairs=0;
    std::set<std::string> inventories;
    int merchants=0;
    for(auto node=map.entities->first;node;node=node->next) {
        auto entity=static_cast<Entity*>(node->element);
        if(entity->behavior==&actMonster && entity->getStats() && entity->getStats()->type==SHOPKEEPER)++merchants;
    }
    if(merchants!=shopsPerFloor)return false;
    for(auto& kiosk:world["kiosks"].GetArray()) {
        const int x=kiosk["x"].GetInt(),y=kiosk["y"].GetInt();
        auto merchant=uidToEntity(kiosk["shopkeeper_uid"].GetUint());
        const int merchantX=kiosk["merchant_x"].GetInt(),merchantY=kiosk["merchant_y"].GetInt();
        if(kiosk["width"].GetInt()!=5 || kiosk["height"].GetInt()!=5 || !merchant || merchant->behavior!=&actMonster ||
            !merchant->getStats() || merchant->getStats()->type!=SHOPKEEPER || !merchant->getStats()->inventory.first ||
            static_cast<int>(merchant->x)/16!=merchantX || static_cast<int>(merchant->y)/16!=merchantY)return false;
        for(auto& other:world["kiosks"].GetArray()) {
            if(&other==&kiosk)continue;
            const int dx=x-other["x"].GetInt(),dy=y-other["y"].GetInt();
            if(dx*dx+dy*dy<(map.width/4)*(map.width/4))return false;
        }
        int stockCount=0;std::set<std::pair<int,int>> stockSlots;std::string stockKey;
        if(merchant->monsterStoreType!=9)return false;
        for(auto node=merchant->getStats()->inventory.first;node;node=node->next) {
            auto item=static_cast<Item*>(node->element);
            if(!item->identified || !DelveRarity::eligible(*item) || item->count<1 || item->count>3 ||
                item->x<0 || item->x>=Player::ShopGUI_t::MAX_SHOP_X || item->y<0 || item->y>=Player::ShopGUI_t::MAX_SHOP_Y ||
                !stockSlots.insert({item->x,item->y}).second)return false;
            if(DelveRarity::equipment(*item) || itemCategory(item)==GEM) {
                const auto expected=static_cast<Status>(std::min(static_cast<int>(DelveRarity::stored(*item))+1,static_cast<int>(EXCELLENT)));
                if(item->status!=expected)return false;
            }
            stockKey+=std::to_string(item->type)+":"+std::to_string(item->appearance)+":"+std::to_string(item->count)+";";
            ++stockCount;
        }
        if(stockCount!=shopStockSize || !inventories.insert(stockKey).second)return false;
        if(merchant->monsterPathBoundaryXStart!=merchantX*16 || merchant->monsterPathBoundaryXEnd!=merchantX*16 ||
            merchant->monsterPathBoundaryYStart!=merchantY*16 || merchant->monsterPathBoundaryYEnd!=merchantY*16)return false;
        int walls=0;
        for(int dx=-kioskRadius;dx<=kioskRadius;++dx)for(int dy=-kioskRadius;dy<=kioskRadius;++dy) {
            const int offset=MAPLAYERS*(y+dy+(x+dx)*map.height);
            const bool blocked=(std::abs(dx)==kioskRadius || std::abs(dy)==kioskRadius) && !(dx==0 && dy==kioskRadius);
            const int expectedWall=blocked ? (dy==kioskRadius ? 54:kioskShelfWall):0;
            if(map.tiles[offset]!=kioskFloor || map.tiles[offset+2]!=kioskCeiling || map.tiles[offset+OBSTACLELAYER]!=expectedWall)return false;
            walls+=blocked;
        }
        if(walls!=15)return false;
        auto table=uidToEntity(kiosk["table_uid"].GetUint()),chair=uidToEntity(kiosk["chair_uid"].GetUint());
        if(!table || !chair || table->behavior!=&actFurniture || chair->behavior!=&actFurniture ||
            table->sprite!=271 || chair->sprite!=272 || table->furnitureType!=FURNITURE_TABLE || chair->furnitureType!=FURNITURE_CHAIR ||
            table->x!=(x-1)*16+8 || table->y!=y*16+8 || chair->x!=table->x || chair->y!=(y+1)*16+8 ||
            table->flags[PASSABLE] || chair->flags[PASSABLE] || std::abs(chair->yaw-3*PI/2)>.001)return false;
        if(kiosk["torch_uids"].Size()!=4)return false;
        for(auto& uid:kiosk["torch_uids"].GetArray()) {
            auto torch=uidToEntity(uid.GetUint());
            if(!torch || torch->behavior!=&actTorch || !torch->light || !torch->flags[PASSABLE])return false;
            const int wallX=static_cast<int>(torch->x-std::cos(torch->yaw)*8)/16;
            const int wallY=static_cast<int>(torch->y-std::sin(torch->yaw)*8)/16;
            if(!map.tiles[MAPLAYERS*(wallY+wallX*map.height)+OBSTACLELAYER])return false;
        }
        // The full-tile entrance and approach must remain physically clear.
        for(auto node=map.entities->first;node;node=node->next) {
            auto entity=static_cast<Entity*>(node->element);
            if(entity->flags[PASSABLE] || entity->behavior==&actPlayer || entity->behavior==&actMonster)continue;
            for(int dy=2;dy<=3;++dy)if(std::abs(entity->x-(x*16+8))<entity->sizex+4 &&
                std::abs(entity->y-((y+dy)*16+8))<entity->sizey+4)return false;
        }
        merchant->setEffect(EFF_ASLEEP,false,0,true);
        for(auto node=map.entities->first;node;node=node->next) {
            auto mob=static_cast<Entity*>(node->element);
            if(mob==merchant || mob->behavior!=&actMonster || !mob->getStats())continue;
            if(merchant->checkEnemy(mob) || mob->checkEnemy(merchant) || merchant->checkFriend(mob) || mob->checkFriend(merchant) ||
                merchant->monsterAlertBeforeHit(mob) || mob->monsterAlertBeforeHit(merchant))return false;
            const auto oldMerchantTarget=merchant->monsterTarget,oldMobTarget=mob->monsterTarget;
            const auto oldMerchantState=merchant->monsterState,oldMobState=mob->monsterState;
            merchant->monsterReleaseAttackTarget(true);mob->monsterReleaseAttackTarget(true);
            merchant->monsterAcquireAttackTarget(*mob,MONSTER_STATE_PATH,true);
            mob->monsterAcquireAttackTarget(*merchant,MONSTER_STATE_PATH,true);
            const bool peaceful=!merchant->monsterTarget && !mob->monsterTarget;
            merchant->monsterTarget=oldMerchantTarget;mob->monsterTarget=oldMobTarget;
            merchant->monsterState=oldMerchantState;mob->monsterState=oldMobState;
            if(!peaceful)return false;
            ++pairs;
        }
        auto player=players[0] ? players[0]->entity:nullptr;
        if(!player || neutralShopkeeperPair(*merchant,*player) || neutralShopkeeperPair(*player,*merchant))return false;
        const double oldX=player->x,oldY=player->y,oldYaw=player->yaw;
        const auto oldHit=hit;
        player->x=kiosk["approach_x"].GetInt()*16+8;player->y=kiosk["approach_y"].GetInt()*16+8;player->yaw=-PI/2;
        lineTrace(player,player->x,player->y,player->yaw,TOUCHRANGE,0,true);
        const bool canTrade=hit.entity==merchant;
        hit=oldHit;player->x=oldX;player->y=oldY;player->yaw=oldYaw;
        if(!canTrade)return false;
    }
    printlog("[SMOKE][DELVE] kiosk_audit floor=%d buildings=%u neutral_pairs=%d footprint=5x5 shelves=11 torches=4 table=1 chair=1 clear_entrance=1 trade_los=1 stock_slots=%d distinct_stock=%u",worldFloor,world["kiosks"].Size(),pairs,shopStockSize,(unsigned)inventories.size());
    return pairs>0;
}
bool smokePopulationAudit() {
    int ranged=0;
    for(int roll=0;roll<10;++roll) {
        const auto weapon=delveGuardWeapon(SKELETON,roll);
        ranged+=weapon==SHORTBOW || weapon==CROSSBOW;
    }
    if(ranged!=2)return false;
    printlog("[SMOKE][DELVE] check PASS skeleton_ranged_two_of_ten");
    ranged=0;
    for(int roll=0;roll<10;++roll) {
        const auto weapon=delveGuardWeapon(INSECTOID,roll);
        ranged+=weapon==SHORTBOW || weapon==LONGBOW || weapon==COMPOUND_BOW || weapon==CROSSBOW;
        if(delveGuardWeapon(GOBLIN,roll)==SHORTBOW)return false;
    }
    if(ranged!=1)return false;
    printlog("[SMOKE][DELVE] check PASS insectoid_ranged_one_of_ten goblin_melee_only");
    const int targets[]={56,56,56};
    for(int floor=1;floor<=3;++floor)for(unsigned seed=0;seed<1000;++seed) {
        const auto roster=encounterRoster(floor,targets[floor-1],seed);
        if(roster.size()!=targets[floor-1] || roster!=encounterRoster(floor,targets[floor-1],seed))return false;
        std::array<int,NUMMONSTERS> weights{},counts{};
        for(auto mob:patrols[floor-1])++weights[mob];
        for(auto mob:roster)++counts[mob];
        for(int mob=0;mob<NUMMONSTERS;++mob) {
            int minimum=targets[floor-1]/12*weights[mob];
            int maximum=minimum+(targets[floor-1]%12 ? weights[mob]:0);
            if(mob==TROLL) {minimum+=harderReplacements[floor-1];maximum+=harderReplacements[floor-1];}
            if(mob==RAT || mob==BAT_SMALL || mob==SLIME)minimum=std::max(1,minimum-harderReplacements[floor-1]);
            if(counts[mob]<minimum || counts[mob]>maximum)return false;
        }
        if(!counts[RAT] || !counts[BAT_SMALL])return false;
        if(counts[DEMON] || counts[INCUBUS] || counts[VAMPIRE] || counts[CRYSTALGOLEM])return false;
        if(floor==1 && (!counts[SKELETON] || counts[GOBLIN] || counts[GHOUL] || counts[TROLL] || counts[INSECTOID]))return false;
        if(floor==2 && (!counts[SKELETON] || counts[GOBLIN]<3*counts[SKELETON] || !counts[GHOUL] || counts[TROLL]!=5 || counts[INSECTOID]))return false;
        if(floor==3 && (counts[SKELETON] || !counts[INSECTOID] || !counts[SCORPION] || !counts[SPIDER] || !counts[TROLL]))return false;
    }
    return true;
}
#endif
void fillShop(Entity& merchant) {
    if(!active() || multiplayer!=SERVER || merchant.behavior!=&actMonster)return;
    auto stock=merchant.getStats();if(!stock || stock->type!=SHOPKEEPER)return;
    const int floor=std::clamp(currentlevel,1,3);
    std::mt19937 rng(seedAt(static_cast<int>(merchant.x)/16,static_cast<int>(merchant.y)/16)^0x73686f70u);
    list_FreeAll(&stock->inventory);
    merchant.monsterStoreType=9; // General store: normal gold-based native trading.
    stock->MISC_FLAGS[STAT_FLAG_MYSTERIOUS_SHOPKEEP]=0;
    std::array<int,5> rarities{};
    for(int slot=0;slot<shopStockSize;++slot) {
        const auto type=randomEnemyLoot(floor,rng);
        const auto category=items[type].category;
        const int count=category==POTION ? 1+rng()%3 : 1;
        auto item=newItem(type,EXCELLENT,0,count,rng(),true,&stock->inventory);
        const auto rarityRoll=rng();
        DelveRarity::rollLoot(item,floor,rarityRoll,rng());
        item->x=slot%Player::ShopGUI_t::MAX_SHOP_X;item->y=slot/Player::ShopGUI_t::MAX_SHOP_X;
        ++rarities[static_cast<int>(DelveRarity::stored(*item))];
    }
    printlog("[DELVE] shop_stock floor=%d uid=%u slots=%d rarity=%d,%d,%d,%d,%d",floor,merchant.getUID(),shopStockSize,rarities[0],rarities[1],rarities[2],rarities[3],rarities[4]);
}
void fillChest(Entity& chest) {
    auto inventory=chest.getChestInventoryList();if(!inventory)return;
    int role=std::clamp(chest.skill[20],0,4),floor=std::clamp(currentlevel,1,3);
    int tier=std::min(2,floor-1+(role==4 ? 1 : 0));
    std::mt19937 rng(seedAt(static_cast<int>(chest.x)/16,static_cast<int>(chest.y)/16));
    auto add=[&](ItemType type,int qty=1,bool bless=false) {
        auto item=newItem(type,EXCELLENT,bless ? (tier ? 1+rng()%(tier==2 ? 5:3):0):0,qty,rng(),instantIdentification(),inventory);
        const auto rarityRoll=rng();
        DelveRarity::rollLoot(item,floor,rarityRoll,rng());
        return item;
    };
    switch(role) {
        case 0: // Camp supplies: one upgrade, sustenance and tools, not a full armory.
            add(randomEquipment(floor-1,rng),1,true);add(POTION_HEALING,2);add(POTION_RESTOREMAGIC);
            add(FOOD_BREAD,3);add(FOOD_BLOOD);add(TOOL_METAL_SCRAP,4);
            add(TOOL_LOCKPICK,3);add(TOOL_TORCH,2);add(TOOL_BEARTRAP);break;
        case 1:
            for(int n=0;n<4;++n)add(randomEquipment(floor-1,rng),1,true);
            add(POTION_HEALING,2);add(TOOL_LOCKPICK,2);break;
        case 2:
            for(int n=0;n<4;++n)add(randomMagic(floor-1,rng),1,true);
            add(POTION_RESTOREMAGIC,3);add(POTION_HEALING);add(GEM_DIAMOND,floor);break;
        case 3:
            for(int n=0;n<3;++n)add(randomEquipment(tier,rng),1,true);
            add(GEM_DIAMOND,floor*3);add(SCROLL_ENCHANTWEAPON);add(POTION_HEALING,2);add(TOOL_LOCKPICK);break;
        default: // Guarded relics: guaranteed advanced gear and enchantments.
            for(int n=0;n<4;++n)add(randomEquipment(tier,rng),1,true);
            add(GEM_DIAMOND,floor*3);add(SCROLL_ENCHANTWEAPON);add(SCROLL_ENCHANTARMOR);add(POTION_HEALING,3);break;
    }
    const int accessorySlot = 2 + (static_cast<int>(chest.x / 16) + static_cast<int>(chest.y / 16) + role) % 6;
    add(randomEquipment(floor-1,rng,accessorySlot),1,true);
}

void dropEnemyLoot(Entity& enemy) {
    if (!active() || multiplayer != SERVER || enemy.behavior != &actMonster) return;
    auto stats = enemy.getStats();
    if (!stats || stats->HP > 0 || stats->leader_uid || stats->getAttribute("delve_loot_seed").empty()
        || !stats->getAttribute("delve_loot_dropped").empty()) return;
    stats->setAttribute("delve_loot_dropped", "1");
    for (auto item : {stats->helmet, stats->breastplate, stats->gloves, stats->shoes, stats->shield,
        stats->weapon, stats->cloak, stats->amulet, stats->ring, stats->mask})
        if (item && (!item->ownerUid || item->ownerUid == enemy.getUID())) item->isDroppable = false;
    for (auto node = stats->inventory.first; node; node = node->next)
        if (auto item = static_cast<Item*>(node->element); !item->ownerUid || item->ownerUid == enemy.getUID())
            item->isDroppable = false;
    const int floor = std::clamp(currentlevel, 1, 3);
    std::mt19937 rng(static_cast<unsigned>(std::stoul(stats->getAttribute("delve_loot_seed"))) ^ 0x7c4a9d13u);
    const auto type = randomEnemyLoot(floor, rng);
    auto item = newItem(type, SERVICABLE, 0, 1, rng(), instantIdentification(), nullptr);
    const auto rarityRoll = rng();
    DelveRarity::rollLoot(item, floor, rarityRoll, rng());
    if (auto drop = dropItemMonster(item, &enemy, nullptr)) {
        drop->flags[USERFLAG1] = true;
        printlog("[DELVE] enemy_loot floor=%d enemy=%u type=%d rarity=%d", floor, enemy.getUID(), type,
            static_cast<int>(DelveRarity::roll(floor, rarityRoll)));
    }
}

#ifdef BARONY_SMOKE_TESTS
bool smokeEnemyLootRules(int player) {
    for (int floor = 1; floor <= 3; ++floor) {
        std::set<ItemType> types;
        int originalBooks = 0, reducedBooks = 0;
        for (unsigned seed = 0; seed < 10000; ++seed) {
            std::mt19937 rng(seed), repeat(seed), baseline(seed), reduced(seed);
            const auto type = randomEnemyLoot(floor, rng);
            if (type != randomEnemyLoot(floor, repeat)) return false;
            types.insert(type);
            auto item = newItem(type, SERVICABLE, 0, 1, rng(), true, nullptr);
            const auto roll = rng();
            DelveRarity::rollLoot(item, floor, roll, rng());
            const bool valid = DelveRarity::eligible(*item) && DelveRarity::stored(*item) == DelveRarity::roll(floor, roll);
            free(item);
            if (!valid) return false;
            originalBooks += items[magic[floor - 1][baseline() % 4]].category == SPELLBOOK;
            reducedBooks += items[randomMagic(floor - 1, reduced)].category == SPELLBOOK;
        }
        if (types.size() < 30 || reducedBooks < originalBooks * 70 / 100 || reducedBooks > originalBooks * 80 / 100) return false;
        printlog("[SMOKE][DELVE] loot_distribution floor=%d types=%u books_before=%d books_after=%d",
            floor, static_cast<unsigned>(types.size()), originalBooks, reducedBooks);
    }
    auto source = players[player]->entity;
    auto enemy = summonMonster(SKELETON, source->x + 32, source->y);
    if (!enemy || !enemy->getStats()) return false;
    configureDelveGuard(*enemy, 12345);
    auto stats = enemy->getStats();
    auto scavenged = newItem(IRON_SWORD, SERVICABLE, 0, 1, 0, true, &stats->inventory);
    scavenged->ownerUid = source->getUID();
    const int before = list_Size(map.entities);
    dropEnemyLoot(*enemy);
    if (list_Size(map.entities) != before) return false;
    stats->HP = 0;
    stats->leader_uid = source->getUID();
    dropEnemyLoot(*enemy);
    if (list_Size(map.entities) != before) return false;
    stats->leader_uid = 0;
    dropEnemyLoot(*enemy);
    if (list_Size(map.entities) != before + 1 || !scavenged->isDroppable
        || !stats->weapon || stats->weapon->isDroppable) return false;
    auto drop = static_cast<Entity*>(map.entities->last->element);
    if (drop->behavior != &actItem || drop->skill[13] != 1 || drop->skill[15] != instantIdentification()) return false;
    const auto count = list_Size(map.entities);
    dropEnemyLoot(*enemy);
    return list_Size(map.entities) == count;
}
#endif

void scatterDeathLoot(int player, double x, double y, const Item& item) {
    if (!active() || multiplayer != SERVER || player < 0 || player >= MAXPLAYERS || itemCategory(&item) == SPELL_CAT) return;
    const unsigned int ordinal = deathDropOrdinals[player]++;
    const double angle = ordinal * (PI * (3.0 - std::sqrt(5.0)));
    auto drop = newEntity(-1, 1, map.entities, nullptr);
    drop->behavior = &actItem;
    drop->x = x; drop->y = y; drop->z = -2;
    drop->sizex = drop->sizey = 4;
    drop->yaw = angle;
    drop->vel_x = std::cos(angle) * 1.6;
    drop->vel_y = std::sin(angle) * 1.6;
    drop->vel_z = -0.7;
    drop->flags[INVISIBLE] = true;
    drop->flags[PASSABLE] = drop->flags[UPDATENEEDED] = drop->flags[USERFLAG1] = true;
    drop->skill[10] = item.type; drop->skill[11] = item.status;
    drop->skill[12] = item.beatitude; drop->skill[13] = item.count;
    drop->skill[14] = item.appearance; drop->skill[15] = item.identified;
    printlog("[DELVE] death_scatter slot=%d ordinal=%u type=%d count=%d", player, ordinal, item.type, item.count);
}
bool stormBoulder(const Entity& entity) {
    return active() && entity.behavior == &actBoulder && entity.skill[16] == BattleRules::stormMarker;
}
void spawnStormWave(int overtime) {
    if (!active() || multiplayer != SERVER || gamePaused || matchComplete() || overtime % BattleRules::waveInterval(overtime)) return;
    int count = 0;
    for (auto node = map.entities->first; node; node = node->next)
        if (stormBoulder(*static_cast<Entity*>(node->element))) ++count;
    std::mt19937 rng(mapseed ^ (static_cast<unsigned int>(overtime) * 0x9e3779b9u));
    int spawned = 0;
    for (int ordinal = 0; ordinal < BattleRules::waveSize(overtime) && count < BattleRules::maximumStormBoulders; ++ordinal, ++count) {
        const int edge = rng() % 4;
        const int tileX = 1 + rng() % (map.width - 2);
        const int tileY = BattleRules::dungeonTop + 1 + rng() % (map.height - BattleRules::dungeonTop - 2);
        auto boulder = newEntity(245, 1, map.entities, nullptr);
        boulder->behavior = &actBoulder;
        boulder->skill[2] = -16; boulder->skill[16] = BattleRules::stormMarker;
        boulder->x = (edge == 0 ? 1 : edge == 1 ? map.width - 2 : tileX) * 16 + 8;
        boulder->y = (edge == 2 ? BattleRules::dungeonTop + 1 : edge == 3 ? map.height - 2 : tileY) * 16 + 8;
        boulder->yaw = edge == 0 ? 0 : edge == 1 ? PI : edge == 2 ? PI / 2 : 3 * PI / 2;
        boulder->vel_x = std::cos(boulder->yaw) * 2.5;
        boulder->vel_y = std::sin(boulder->yaw) * 2.5;
        boulder->z = 0; boulder->sizex = boulder->sizey = 7;
        boulder->flags[UPDATENEEDED] = boulder->flags[PASSABLE] = true;
        ++spawned;
    }
    printlog("[DELVE] boulder_wave floor=%d overtime=%d spawned=%d interval=%d", currentlevel, overtime, spawned, BattleRules::waveInterval(overtime));
}
bool tickStormBoulder(Entity& boulder) {
    if (!stormBoulder(boulder) || multiplayer == CLIENT) return false;
    if (gamePaused || loading || matchComplete()) return true;
    boulder.x += boulder.vel_x; boulder.y += boulder.vel_y; boulder.roll += 0.12;
    const int tileX = static_cast<int>(boulder.x / 16), tileY = static_cast<int>(boulder.y / 16);
    if (tileX < 1 || tileY <= BattleRules::dungeonTop || tileX >= map.width - 1 || tileY >= map.height - 1) {
        list_RemoveNode(boulder.mynode); return true;
    }
    bool walls = false;
    for (int x = std::max(1, static_cast<int>((boulder.x - 7) / 16)); x <= std::min<int>(map.width - 2, (boulder.x + 7) / 16); ++x)
        for (int y = std::max(BattleRules::dungeonTop + 1, static_cast<int>((boulder.y - 7) / 16)); y <= std::min<int>(map.height - 2, (boulder.y + 7) / 16); ++y) {
            auto& wall = map.tiles[OBSTACLELAYER + y * MAPLAYERS + x * MAPLAYERS * map.height];
            if (!wall) continue;
            wall = 0; walls = true;
            for (int player = 1; player < MAXPLAYERS; ++player) {
                if (client_disconnected[player] || players[player]->isLocalPlayer()) continue;
                std::memcpy(net_packet->data, "WALD", 4);
                SDLNet_Write16(x, net_packet->data + 4); SDLNet_Write16(y, net_packet->data + 6);
                net_packet->address = net_clients[player - 1]; net_packet->len = 8;
                sendPacketSafe(net_sock, -1, net_packet, player - 1);
            }
        }
    if (walls) generatePathMaps();
    for (auto node = map.entities->first; node;) {
        auto next = node->next;
        auto target = static_cast<Entity*>(node->element);
        if (target != &boulder && (!target->parent || target->behavior == &actPlayer || target->behavior == &actMonster) && !stormBoulder(*target)
            && target->y > BattleRules::dungeonTop * 16 && target->behavior
            && target->behavior != &actLadder && target->behavior != &actPortal
            && std::abs(target->x - boulder.x) < target->sizex + 7
            && std::abs(target->y - boulder.y) < target->sizey + 7) {
            if (target->behavior == &actPlayer || target->behavior == &actMonster) {
                auto character = target->getStats();
                if (character && character->HP > 0 && (target->behavior != &actPlayer || !protectedPlayer(target->skill[2]))) {
                    target->modHP(-100000);
                    target->setObituary("crushed by the boulder storm");
                }
            } else if (!target->flags[PASSABLE] || target->behavior == &actBoulder || target->behavior == &actChest || target->behavior == &actDoor
                || target->behavior == &actFurniture || target->behavior == &actFountain || target->behavior == &actSink || target->behavior == &actColumn) {
                list_RemoveNode(target->mynode);
            }
        }
        node = next;
    }
    return true;
}

void useShrine(Entity& shrine) {
    if (!active() || multiplayer != SERVER || shrine.skill[24] > (int)ticks) return;
    for (int p = 0; p < MAXPLAYERS; ++p) {
        if (client_disconnected[p] || outcome(p) != Outcome::Playing || !players[p]->entity || stats[p]->HP <= 0 ||
            (selectedEntity[p] != &shrine && client_selected[p] != &shrine) || std::hypot(players[p]->entity->x-shrine.x,players[p]->entity->y-shrine.y) > TOUCHRANGE) continue;
        auto player = players[p]->entity;
        switch ((shrine.skill[20]-1)%3) {
            case 0: player->modHP(30); messagePlayer(p,MESSAGE_MISC,"Shrine of Healing: +30 health."); break;
            case 1: player->setEffect(EFF_FAST,true,TICKS_PER_SECOND*30,true); messagePlayer(p,MESSAGE_MISC,"Shrine of Speed: 30 seconds."); break;
            default:
                if (!stats[p]->weapon || stats[p]->weapon->beatitude >= 5) continue;
                ++stats[p]->weapon->beatitude;
                if (p > 0 && !players[p]->isLocalPlayer()) {
                    std::memcpy(net_packet->data,"BLE1",4); SDLNet_Write32(5,&net_packet->data[4]);
                    net_packet->address = net_clients[p-1]; net_packet->len = 8; sendPacketSafe(net_sock,-1,net_packet,p-1);
                }
                messagePlayer(p,MESSAGE_MISC,"Shrine of Enchantment: weapon blessed +1."); break;
        }
        shrine.skill[24] = ticks+TICKS_PER_SECOND*60;
        selectedEntity[p] = client_selected[p] = nullptr;
        playSoundEntity(&shrine,167,128);
        printlog("[DELVE] shrine slot=%d kind=%d",p,shrine.skill[20]); break;
    }
}
}
