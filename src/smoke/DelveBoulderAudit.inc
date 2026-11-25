// The reported storm death -> floor return -> Jungle generation exit.
// Drive ordinary storm collision and death processing, without setting HP to zero.
void boulderWipe() {
    static int floor=0,stage=0;
    static Uint32 readyAt=0;
    for(auto node=map.entities->first;node;node=node->next) {
        auto entity=static_cast<Entity*>(node->element);
        if(entity->behavior==&actMonster && entity->getStats())entity->setEffect(EFF_ASLEEP,true,100000,true);
    }
    if(!::Raid::smokeWorldReady())return;
    if(floor!=currentlevel) {
        if(!allLoaded())return;
        floor=currentlevel;stage=0;readyAt=ticks;
        check(::Raid::smokeKioskAudit(),"storm_return_three_reachable_spaced_shops");
        for(int p=0;p<peers;++p)check(stats[p]->HP>0 && ::Raid::outcome(p)==O::Playing,"storm_return_alive_class_starter");
        printlog("[SMOKE][DELVE] storm_floor_ready floor=%d",floor);
        if(peers>1)printlog("[SMOKE][DELVE] hostility enemy=%d friend=%d friendlyfire=%d",players[0]->entity->checkEnemy(players[1]->entity),players[0]->entity->checkFriend(players[1]->entity),(svFlags&SV_FLAG_FRIENDLYFIRE)!=0);
    }
    if(stage || ticks-readyAt<3*TICKS_PER_SECOND)return;
    int x=0,y=0;double yaw=0;
    if(!layoutPoint("combat_lane",0,x,y,yaw)){check(false,"storm_death_lane");return;}
    ::Raid::setSmokeElapsed(360);::Raid::spawnStormWave(0);
    for(int p=0;p<peers;++p) {
        auto player=players[p]->entity;
        check(player && player->teleporterMove(x,y,2),"storm_death_positioned");
        Entity* storm=nullptr;
        for(auto node=map.entities->first;node;node=node->next) {
            auto entity=static_cast<Entity*>(node->element);
            if(::Raid::stormBoulder(*entity)){storm=entity;break;}
        }
        if(!storm || !player){check(false,"storm_death_boulder");return;}
        storm->x=player->x;storm->y=player->y;storm->vel_x=storm->vel_y=0;
        ::Raid::tickStormBoulder(*storm);
        check(stats[p]->HP<=0,"storm_collision_killed_player");
        check(std::string(stats[p]->obituary)=="crushed by the boulder storm","storm_death_obituary");
    }
    printlog("[SMOKE][DELVE] storm_wipe floor=%d players=%d",floor,peers);
    stage=1;
}
