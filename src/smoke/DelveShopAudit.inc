// Native SHOP/SHPI/SHPB transfer and purchase checks; excluded from production.
std::string shopItemKey(const Item& item, bool full) {
    std::string key=std::to_string(item.type)+":"+std::to_string(item.status)+":"+std::to_string(item.beatitude)+":";
    if(full)key+=std::to_string(item.appearance)+":"+std::to_string(item.count)+":"+std::to_string(item.x)+":"+std::to_string(item.y)+":"+std::to_string(item.identified);
    else key+=std::to_string(static_cast<int>(DelveRarity::stored(item)))+":"+std::to_string(DelveRarity::seed(item));
    return key;
}
std::string shopStockSnapshot(list_t& inventory) {
    std::vector<std::string> rows;
    for(auto node=inventory.first;node;node=node->next)rows.push_back(shopItemKey(*static_cast<Item*>(node->element),true));
    std::sort(rows.begin(),rows.end());std::string snapshot;
    for(auto& row:rows)snapshot+=row+"\n";
    return snapshot;
}
int shopItemCount(list_t& inventory,const std::string& key) {
    int count=0;for(auto node=inventory.first;node;node=node->next) {
        auto item=static_cast<Item*>(node->element);if(shopItemKey(*item,false)==key)count+=item->count;
    }
    return count;
}
int shopPieces(list_t& inventory) {
    int count=0;for(auto node=inventory.first;node;node=node->next)count+=static_cast<Item*>(node->element)->count;return count;
}
int purchasedPieces(list_t& inventory,const Item& reference) {
    // Native consumables merge into existing equivalent stacks. Their cosmetic
    // roll seed need not survive that merge; equipment affix seeds must match.
    int count=0;
    for(auto node=inventory.first;node;node=node->next) {
        auto item=static_cast<Item*>(node->element);
        if(!itemCompare(item,&reference,false,true))count+=item->count;
    }
    return count;
}
bool gateNetworkAudit() {
    static int floor=0,stage=0,before=0;
    static Uint32 leverUID=0,gateUID=0;
    if(floor!=currentlevel){floor=currentlevel;stage=0;leverUID=gateUID=0;}
    auto target=std::filesystem::path(sync)/("gate-target-"+std::to_string(floor));
    auto clicked=std::filesystem::path(sync)/("gate-clicked-"+std::to_string(floor));
    auto observed=std::filesystem::path(sync)/("gate-observed-"+std::to_string(floor));
    auto complete=std::filesystem::path(sync)/("gate-complete-"+std::to_string(floor));
    if(std::filesystem::exists(complete))return true;
    if(multiplayer==SERVER) {
        if(stage==0) {
            std::ifstream input("delve-generated-floor"+std::to_string(floor)+".json");
            std::string source((std::istreambuf_iterator<char>(input)),{});rapidjson::Document layout;layout.Parse(source.c_str());
            if(layout.HasParseError())return false;
            const rapidjson::Value* selected=nullptr;
            for(auto& door:layout["doors"].GetArray())if(!std::strcmp(door["kind"].GetString(),"gate")) {
                selected=&door;if(door["locked"].GetBool())break;
            }
            if(!selected){std::ofstream(complete)<<"no native gates";return true;}
            auto base=uidToEntity((*selected)["lever_uid"].GetUint());
            auto lever=base ? uidToEntity(base->parent):nullptr;if(!lever)return false;
            for(auto node=map.entities->first;node;node=node->next) {
                auto e=static_cast<Entity*>(node->element);
                if(e->behavior==&actGate && static_cast<int>(e->x)/16==(*selected)["x"].GetInt() &&
                    static_cast<int>(e->y)/16==(*selected)["y"].GetInt()){gateUID=e->getUID();before=e->gateStatus;break;}
            }
            if(!gateUID)return false;
            leverUID=lever->getUID();const int x=(*selected)["lever_x"].GetInt(),y=(*selected)["lever_y"].GetInt();
            players[1]->entity->teleporterMove(x,y,2);
            std::ofstream(target)<<leverUID<<' '<<gateUID<<' '<<x<<' '<<y<<' '<<before;
            stage=1;
        } else if(stage==1 && std::filesystem::exists(clicked) && std::filesystem::exists(observed)) {
            auto gate=uidToEntity(gateUID),lever=uidToEntity(leverUID);
            if(!gate || !lever || gate->gateStatus==before || gate->flags[PASSABLE]!=(before==0))return false;
            check(true,"gate_guest_lever_native_interaction_opens_original_gate");
            lever->toggleSwitch();stage=2;
        } else if(stage==2) {
            auto gate=uidToEntity(gateUID);
            if(!gate || gate->gateStatus!=before || gate->flags[PASSABLE]!=(before!=0))return false;
            check(true,"gate_native_control_round_trip");std::ofstream(complete)<<"PASS";return true;
        }
    } else if(clientnum==1) {
        if(stage==0 && std::filesystem::exists(target)) {
            int x=0,y=0;std::ifstream input(target);if(!(input>>leverUID>>gateUID>>x>>y>>before))return false;
            auto lever=uidToEntity(leverUID),gate=uidToEntity(gateUID);if(!lever || !gate)return false;
            auto player=players[clientnum]->entity;
            player->x=player->new_x=x*16+8;player->y=player->new_y=y*16+8;
            player->yaw=std::atan2(gate->y-player->y,gate->x-player->x);
            std::memcpy(net_packet->data,"CKIR",4);net_packet->data[4]=clientnum;SDLNet_Write32(leverUID,&net_packet->data[5]);
            net_packet->address=net_server;net_packet->len=9;sendPacketSafe(net_sock,-1,net_packet,0);
            std::ofstream(clicked)<<"sent";stage=1;
        } else if(stage==1) {
            auto gate=uidToEntity(gateUID);
            if(!gate || gate->gateStatus==before || gate->flags[PASSABLE]!=(before==0))return false;
            std::ofstream(std::filesystem::path(sync)/"gate-view")<<floor;
            if(!std::filesystem::exists(std::filesystem::path(sync)/("gate-captured-"+std::to_string(floor))))return false;
            check(true,"gate_guest_sees_native_gate_motion_and_collision");
            std::ofstream(observed)<<"PASS";stage=2;
        }
    }
    return false;
}
void shopNetworkAudit() {
    static int floor=0,ordinal=0,stage=0,beforePieces=0;
    static std::map<std::string,int> beforeStock;
    static Uint32 merchantUID=0;
    if(floor!=currentlevel){floor=currentlevel;ordinal=stage=0;printlog("[SMOKE][DELVE] client_floor slot=%d floor=%d",clientnum,floor);}
    if(floor<1 || floor>3 || !players[clientnum]->entity)return;
    const std::string tag=std::to_string(floor)+"-"+std::to_string(ordinal);
    const auto expectedPath=std::filesystem::path(sync)/("shop-expected-"+tag);
    const auto purchasePath=std::filesystem::path(sync)/("shop-purchased-"+tag);
    const auto merchantPath=std::filesystem::path(sync)/("shop-merchant-"+tag);
    const auto arrivedPath=std::filesystem::path(sync)/("shop-arrived-"+tag);
    if(multiplayer==SERVER) {
        if(!allLoaded() || !::Raid::smokeWorldReady())return;
        if(ordinal==3){for(int p=0;p<peers;++p)stats[p]->HP=0;return;}
        for(int p=0;p<peers;++p)if(stats[p]->HP>0)stats[p]->HP=stats[p]->MAXHP;
        for(auto node=map.entities->first;node;node=node->next) {
            auto mob=static_cast<Entity*>(node->element);
            if(mob->behavior==&actMonster && mob->getStats() && mob->getStats()->type!=SHOPKEEPER)mob->setEffect(EFF_ASLEEP,true,100000,true);
        }
        if(!gateNetworkAudit())return;
        if(stage==0) {
            if(ordinal==0) {
                check(::Raid::smokeKioskAudit(),"shops_three_spaced_kiosks_and_floor_rarity_stock");
                printlog("[SMOKE][DELVE] hostility enemy=%d friend=%d friendlyfire=%d",players[0]->entity->checkEnemy(players[1]->entity),players[0]->entity->checkFriend(players[1]->entity),(svFlags&SV_FLAG_FRIENDLYFIRE)!=0);
            }
            std::ifstream input("delve-generated-floor"+std::to_string(floor)+".json");
            std::string source((std::istreambuf_iterator<char>(input)),{});rapidjson::Document layout;layout.Parse(source.c_str());
            if(layout.HasParseError() || !layout.HasMember("kiosks") || layout["kiosks"].Size()!=3){check(false,"shops_layout");return;}
            auto& kiosk=layout["kiosks"][ordinal];merchantUID=kiosk["shopkeeper_uid"].GetUint();
            auto merchant=uidToEntity(merchantUID);if(!merchant || !merchant->getStats())return;
            players[1]->entity->teleporterMove(kiosk["approach_x"].GetInt(),kiosk["approach_y"].GetInt(),2);
            players[1]->entity->new_x=players[1]->entity->x;players[1]->entity->new_y=players[1]->entity->y;
            stats[1]->GOLD=1000000;
            beforePieces=shopPieces(merchant->getStats()->inventory);beforeStock.clear();
            for(auto node=merchant->getStats()->inventory.first;node;node=node->next) {
                auto item=static_cast<Item*>(node->element);beforeStock[shopItemKey(*item,false)]+=item->count;
            }
            {std::ofstream output(expectedPath);output<<shopStockSnapshot(merchant->getStats()->inventory);}
            {std::ofstream output(merchantPath);output<<merchantUID<<' '<<kiosk["approach_x"].GetInt()<<' '<<kiosk["approach_y"].GetInt();}
            stage=1;
        } else if(stage==1 && std::filesystem::exists(arrivedPath)) {
            std::ofstream(std::filesystem::path(sync)/"shop-scene-view")<<tag;
            if(!std::filesystem::exists(std::filesystem::path(sync)/("shop-scene-captured-"+tag)))return;
            startTradingServer(uidToEntity(merchantUID),1);stage=2;
        } else if(stage==2 && std::filesystem::exists(purchasePath)) {
            std::ifstream input(purchasePath);std::string key;std::getline(input,key);
            auto merchant=uidToEntity(merchantUID);if(!merchant || !merchant->getStats())return;
            // Native Barony keeps unequipped guest inventory on the guest. Check
            // its purchase there, and the exact authoritative stock/gold here.
            if(beforeStock[key]<1 || shopItemCount(merchant->getStats()->inventory,key)!=beforeStock[key]-1 ||
                shopPieces(merchant->getStats()->inventory)!=beforePieces-1 || stats[1]->GOLD>=1000000)return;
            check(true,"shops_host_removes_exact_rarity_item_and_charges_gold");
            printlog("[SMOKE][DELVE] shop_purchase_verified floor=%d ordinal=%d key=%s",floor,ordinal,key.c_str());
            ++ordinal;stage=0;
        }
    } else if(clientnum==1) {
        if(!gateNetworkAudit())return;
        if(std::filesystem::exists(merchantPath) && !std::filesystem::exists(arrivedPath)) {
            std::ifstream input(merchantPath);Uint32 uid=0;int x=0,y=0;input>>uid>>x>>y;
            // A remote merchant may not have replicated yet after this fixture's
            // teleport. Native shop UI closes immediately for a missing entity.
            if(auto merchant=uidToEntity(uid)) {
                players[clientnum]->entity->yaw=-PI/2;
                if(std::abs(players[clientnum]->entity->x-(x*16+8))<1 &&
                    std::abs(players[clientnum]->entity->y-(y*16+8))<1 &&
                    entityDist(merchant,players[clientnum]->entity)<=TOUCHRANGE+1)
                    std::ofstream(arrivedPath)<<"1";
            }
        }
        if(!std::filesystem::exists(expectedPath) || shopkeeper[clientnum]==0 || !shopInv[clientnum])return;
        if(stage==0) {
            std::ifstream input(expectedPath);std::string expected((std::istreambuf_iterator<char>(input)),{});
            if(shopStockSnapshot(*shopInv[clientnum])!=expected)return;
            check(true,"shops_guest_exact_stock_appearance_status_quantity_and_slots");
            std::ofstream(std::filesystem::path(sync)/"shop-view")<<tag;stage=1;
        }
        if(stage==1 && std::filesystem::exists(std::filesystem::path(sync)/("shop-captured-"+tag))) {
            Item* selected=nullptr;
            for(auto node=shopInv[clientnum]->first;node;node=node->next) {
                auto item=static_cast<Item*>(node->element);
                if(!selected || DelveRarity::stored(*item)>DelveRarity::stored(*selected))selected=item;
            }
            if(!selected)return;
            const std::string key=shopItemKey(*selected,false);const Item reference=*selected;
            const int before=purchasedPieces(stats[clientnum]->inventory,reference);
            stats[clientnum]->GOLD=1000000;bool consumed=false;
            const bool bought=buyItemFromShop(clientnum,selected,consumed);
            check(bought && purchasedPieces(stats[clientnum]->inventory,reference)==before+1,"shops_guest_native_purchase_preserves_rarity_and_rolls");
            closeShop(clientnum);
            std::ofstream(purchasePath)<<key<<"\n";
            ++ordinal;stage=0;
        }
    }
}
