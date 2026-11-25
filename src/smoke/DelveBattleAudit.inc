void seedBattleProgression(int player) {
    auto character = stats[player];
    character->LVL = 7 + player; character->EXP = 37 + player;
    character->HP = character->MAXHP = 120 + player;
    character->MP = character->MAXMP = 80 + player;
    character->STR = 11 + player; character->DEX = 12 + player; character->CON = 13 + player;
    character->INT = 14 + player; character->PER = 15 + player; character->CHR = 16 + player;
    for (int skill = 0; skill < NUMPROFICIENCIES; ++skill) character->setProficiencyUnsafe(skill, 40 + skill + player);
}
bool battleProgressionRetained(int player) {
    auto character = stats[player];
    bool retained = character->LVL == 7 + player && character->EXP == 37 + player
        && character->MAXHP == 120 + player && character->MAXMP == 80 + player
        && character->STR == 11 + player && character->DEX == 12 + player && character->CON == 13 + player
        && character->INT == 14 + player && character->PER == 15 + player && character->CHR == 16 + player;
    for (int skill = 0; skill < NUMPROFICIENCIES; ++skill)
        retained &= character->getProficiency(skill) == 40 + skill + player;
    if (!retained) {
        printlog("[DELVE-PROGRESSION-TEST] slot=%d floor=%d lvl=%d xp=%d hp=%d mp=%d attrs=%d,%d,%d,%d,%d,%d",
            player, currentlevel, character->LVL, character->EXP, character->MAXHP, character->MAXMP,
            character->STR, character->DEX, character->CON, character->INT, character->PER, character->CHR);
        for (int skill = 0; skill < NUMPROFICIENCIES; ++skill)
            if (character->getProficiency(skill) != 40 + skill + player)
                printlog("[DELVE-PROGRESSION-TEST] skill=%d actual=%d expected=%d", skill, character->getProficiency(skill), 40 + skill + player);
    }
    return retained;
}
Entity* battleExit(int ordinal) {
    for (auto node = map.entities->first; node; node = node->next) {
        auto entity = static_cast<Entity*>(node->element);
        if ((entity->behavior == &actLadder || entity->behavior == &actPortal) && entity->skill[20] == ordinal) return entity;
    }
    return nullptr;
}
void battleHost() {
    for (auto node = map.entities->first; node; node = node->next) {
        auto entity = static_cast<Entity*>(node->element);
        if (entity->behavior == &actMonster && entity->getStats()) entity->setEffect(EFF_ASLEEP, true, 100000, true);
    }
    for (int player = 0; player < peers; ++player) {
        if (stats[player]->HP > 0) stats[player]->HP = stats[player]->MAXHP;
        if (pendingTargets[player] && ticks >= moveAt[player]) {
            if (auto target = uidToEntity(pendingTargets[player])) move(player, *target);
            moveAt[player] = ticks + TICKS_PER_SECOND / 2;
        }
    }
    if (!::Raid::smokeWorldReady()) return;
    if (observedFloor != currentlevel) {
        if (!allLoaded()) return;
        observedFloor = currentlevel; phase = 0; began = ticks; tourView = 0;
        pendingTargets.fill(0);
        check(::Raid::floorSecondsRemaining() > ::Raid::BattleRules::floorSeconds - 10
            && ::Raid::floorSecondsRemaining() <= ::Raid::BattleRules::floorSeconds, "battle_floor_clock_reset");
        check(map.width == 90 && map.height == 90, "battle_equal_full_size_floors");
        if (currentlevel == 2) {
            int spellTraps = 0;
            for (auto node = map.entities->first; node; node = node->next) {
                auto entity = static_cast<Entity*>(node->element);
                if (entity->behavior == &actMagicTrap || entity->behavior == &actMagicTrapCeiling) ++spellTraps;
            }
            check(spellTraps == 0, "battle_jungle_has_no_spell_traps");
            check(stats[0]->GOLD == 4321, "battle_descent_preserves_gold");
            bool retained = false;
            for (auto node = stats[0]->inventory.first; node; node = node->next) {
                auto item = static_cast<Item*>(node->element);
                retained |= item->type == RING_REGENERATION && item->appearance == 789;
            }
            check(retained, "battle_descent_preserves_backpack");
            check(stats[1]->HP > 0 && stats[1]->GOLD == 0, "battle_dead_guest_returns_starter");
            check(battleProgressionRetained(1), "battle_dead_guest_retains_progression");
            check(!::Raid::revenant(1) && !stats[1]->getEffectActive(EFF_POLYMORPH), "battle_guest_monster_returns_normal_next_floor");
        }
        if (currentlevel == 3) {
            for (int player = 0; player < peers; ++player) {
                check(stats[player]->HP > 0 && stats[player]->GOLD == 0, "battle_wipe_returns_starters");
                check(battleProgressionRetained(player), "battle_wipe_retains_progression");
            }
            check(!::Raid::revenant(0) && !stats[0]->getEffectActive(EFF_POLYMORPH), "battle_host_monster_returns_normal_next_floor");
        }
    }
    if (currentlevel > 1 && phase == 0)
        for (int player = 1; player < peers; ++player)
            if (!std::filesystem::exists(sync + "/battle-progression-floor-" + std::to_string(currentlevel) + "-" + std::to_string(player))) return;
    if (currentlevel == 2 && SmokeHooksCommon::parseEnvString("BARONY_SMOKE_DELVE_TOUR", "") == "1"
        && !std::filesystem::exists(sync + "/battle-jungle-captured")) {
        if (tourView == 0) {
            int x = 0, y = 0; double yaw = 0;
            check(layoutPoint("views", 1, x, y, yaw), "battle_jungle_view_point");
            check(players[0]->entity->teleporterMove(x, y, 2), "battle_jungle_view_reachable");
            players[0]->entity->yaw = yaw;
            players[0]->entity->pitch = 0.1;
            tourView = 1; tourAt = ticks;
        }
        if (tourView == 1 && ticks - tourAt > TICKS_PER_SECOND) {
            std::ofstream(sync + "/battle-jungle-view-ready") << 1;
            tourView = 2;
        }
        return;
    }
    if (currentlevel == 1) {
        if (phase == 0) {
            static bool progressionSeeded = false;
            if (!progressionSeeded) {
                for (int player = 0; player < peers; ++player) seedBattleProgression(player);
                progressionSeeded = true;
            }
            for (int player = 1; player < peers; ++player)
                if (!std::filesystem::exists(sync + "/battle-progression-ready-" + std::to_string(player))) return;
            check((svFlags & SV_FLAG_FRIENDLYFIRE) && players[0]->entity->checkEnemy(players[2]->entity)
                && !players[0]->entity->checkFriend(players[2]->entity), "battle_ffa_enforced");
            check(battleExit(1) && battleExit(2) && battleExit(3) && battleExit(4) && !battleExit(5)
                && battleExit(1)->behavior == &actLadder && battleExit(2)->behavior == &actPortal
                && battleExit(3)->behavior == &actPortal && battleExit(4)->behavior == &actPortal,
                "battle_one_hatch_three_portal_sites");
            for (int ordinal = 1; ordinal <= 4; ++ordinal) if (auto exit = battleExit(ordinal)) ::Raid::useExit(*exit);
            check(!battleExit(1)->flags[INVISIBLE] && battleExit(2)->flags[INVISIBLE]
                && battleExit(3)->flags[INVISIBLE] && battleExit(4)->flags[INVISIBLE], "battle_one_early_hatch_only");
            check(::Raid::experienceGain(2) == 5 && ::Raid::experienceGain(17) == 43
                && ::Raid::experienceGain(30) == 75, "battle_xp_two_point_five");
            newItem(RING_REGENERATION, EXCELLENT, 0, 1, 789, true, &stats[0]->inventory);
            stats[0]->GOLD = 4321; stats[1]->GOLD = 999;
            stats[1]->HP = 0; phase = 1; began = ticks;
        } else if (phase == 1 && ::Raid::outcome(1) == O::Slain && ticks - began > TICKS_PER_SECOND) {
            if (!::Raid::revenant(1) || !std::filesystem::exists(sync + "/battle-monster-guest")) return;
            int drops = 0, bags = 0;
            for (auto node = map.entities->first; node; node = node->next) {
                auto entity = static_cast<Entity*>(node->element);
                if (entity->behavior == &actItem && entity->skill[10] == TOOL_PLAYER_LOOT_BAG) ++bags;
                if (entity->behavior == &actItem && entity->flags[USERFLAG1]) ++drops;
            }
            check(drops > 0 && bags == 0, "battle_death_loot_not_bagged");
            check(::Raid::revenant(1) && ::Raid::outcome(1) == O::Slain, "battle_guest_monster_stays_slain_this_floor");
            const int monsterX=static_cast<int>(players[1]->entity->x)/16;
            const int monsterY=static_cast<int>(players[1]->entity->y)/16;
            auto monsterExit = battleExit(1); move(1, *monsterExit); client_selected[1] = monsterExit; ::Raid::useExit(*monsterExit);
            check(::Raid::outcome(1) == O::Slain, "battle_monster_cannot_descend_with_loot");
            // Return the fixture actor so it cannot body-block a one-tile hatch approach.
            check(players[1]->entity->teleporterMove(monsterX,monsterY,2),"battle_monster_releases_exit_approach");
            auto exit = battleExit(1); move(0, *exit); selectedEntity[0] = exit; ::Raid::useExit(*exit);
            check(::Raid::outcome(0) == O::Descended, "battle_early_descent_native_use");
            move(2, *exit); client_selected[2] = exit; ::Raid::useExit(*exit);
            check(::Raid::outcome(2) == O::Playing, "battle_descent_single_use");
            ::Raid::setSmokeElapsed(299); ::Raid::useExit(*battleExit(2));
            check(battleExit(2)->flags[INVISIBLE], "battle_portal_hidden_before_last_minute");
            ::Raid::setSmokeElapsed(300); ::Raid::useExit(*battleExit(2));
            check(!battleExit(2)->flags[INVISIBLE] && battleExit(3)->flags[INVISIBLE]
                && battleExit(4)->flags[INVISIBLE], "battle_one_portal_at_one_minute_left");
            publish(2, battleExit(2)); phase = 2;
        } else if (phase == 2 && ::Raid::outcome(2) == O::Descended) {
            ::Raid::setSmokeElapsed(359);
            ::Raid::useExit(*battleExit(3)); ::Raid::useExit(*battleExit(4));
            check(battleExit(3)->flags[INVISIBLE] && battleExit(4)->flags[INVISIBLE], "battle_overtime_portals_hidden_before_zero");
            ::Raid::setSmokeElapsed(360);
            ::Raid::useExit(*battleExit(3)); ::Raid::useExit(*battleExit(4));
            check(!battleExit(3)->flags[INVISIBLE] && !battleExit(4)->flags[INVISIBLE], "battle_two_more_portals_at_zero");
            publish(3, battleExit(3)); phase = 3;
        }
    } else if (currentlevel == 2) {
        if (phase == 0 && ticks - began > 2 * TICKS_PER_SECOND) {
            ::Raid::spawnStormWave(0);
            Entity* storm = nullptr;
            for (auto node = map.entities->first; node; node = node->next) {
                auto entity = static_cast<Entity*>(node->element);
                if (::Raid::stormBoulder(*entity)) { storm = entity; break; }
            }
            check(storm != nullptr, "battle_storm_spawns_perimeter_boulder");
            int x = 0, y = 0; double yaw = 0;
            check(layoutPoint("combat_lane", 0, x, y, yaw), "battle_storm_lane_available");
            const int offset = OBSTACLELAYER + y * MAPLAYERS + x * MAPLAYERS * map.height;
            map.tiles[offset] = 1;
            auto prop = newEntity(1, 1, map.entities, nullptr);
            prop->behavior = &actFurniture; prop->x = x * 16 + 8; prop->y = y * 16 + 8;
            prop->sizex = prop->sizey = 4; const auto propUID = prop->getUID();
            storm->x = prop->x; storm->y = prop->y; storm->vel_x = 2.5; storm->vel_y = 0;
            ::Raid::tickStormBoulder(*storm);
            check(!map.tiles[offset] && !uidToEntity(propUID), "battle_storm_breaks_wall_and_prop");
            auto creature = summonMonster(RAT, storm->x, storm->y);
            check(creature != nullptr, "battle_storm_target_spawned");
            if (creature) {
                creature->getStats()->HP = 10;
                ::Raid::tickStormBoulder(*storm);
                check(creature->getStats()->HP <= 0, "battle_storm_crushes_creature");
            }
            list_RemoveNode(storm->mynode);
            std::ofstream(sync + "/battle-wall") << x << ' ' << y;
            phase = 1; began = ticks;
        } else if (phase == 1 && ticks - began > 2 * TICKS_PER_SECOND) {
            bool guests = true;
            for (int player = 1; player < peers; ++player)
                guests &= std::filesystem::exists(sync + "/battle-wall-guest-" + std::to_string(player));
            if (!guests) return;
            ::Raid::setSmokeElapsed(360); phase = 2; began = ticks;
        } else if (phase == 2 && ticks - began > TICKS_PER_SECOND) {
            check(::Raid::floorSecondsRemaining() == 0 && !::Raid::matchComplete(), "battle_overtime_not_flat_damage_end");
            // The storm assertions above are complete. Keep their boulders from
            // crushing the host's fragile return before the separate lifecycle check.
            for(auto node=map.entities->first;node;) {
                auto next=node->next;
                if(::Raid::stormBoulder(*static_cast<Entity*>(node->element)))list_RemoveNode(node);
                node=next;
            }
            stats[0]->HP = 0;
            phase = 3;
        } else if (phase == 3 && ::Raid::outcome(0) == O::Slain && !players[0]->entity) {
            check(::Raid::requestRevenant(0, 0), "battle_host_same_floor_monster_return");
            phase = 4; began = ticks;
        } else if (phase == 4 && ::Raid::revenant(0) && ticks - began > TICKS_PER_SECOND) {
            check(::Raid::outcome(0) == O::Slain && stats[0]->HP > 0, "battle_live_host_monster_does_not_block_floor");
            for (int player = 1; player < peers; ++player) stats[player]->HP = 0;
            phase = 5;
        }
    } else if (currentlevel == 3 && phase == 0) {
        int count = 0;
        for (auto node = map.entities->first; node; node = node->next) {
            auto entity = static_cast<Entity*>(node->element);
            count += entity->behavior == &actPortal || entity->behavior == &actLadder;
        }
        check(count == 4 && battleExit(1)->behavior == &actLadder && battleExit(2)->behavior == &actPortal
            && battleExit(3)->behavior == &actPortal && battleExit(4)->behavior == &actPortal,
            "battle_one_hatch_three_final_portals");
        ::Raid::setSmokeElapsed(299);
        for (int ordinal = 1; ordinal <= 4; ++ordinal) ::Raid::useExit(*battleExit(ordinal));
        check(battleExit(1)->flags[INVISIBLE] && battleExit(2)->flags[INVISIBLE], "battle_final_portal_hidden_before_last_minute");
        move(0, *battleExit(1)); selectedEntity[0] = battleExit(1); ::Raid::useExit(*battleExit(1));
        check(::Raid::outcome(0) == O::Playing, "battle_final_hatch_cannot_win_early");
        selectedEntity[0] = nullptr;
        check(!battleExit(2)->light, "battle_hidden_portal_has_no_light");
        phase = 10;
    } else if (currentlevel == 3 && (phase == 10 || phase == 11)) {
        int boulders = 0;
        for (auto node = map.entities->first; node; node = node->next)
            boulders += ::Raid::stormBoulder(*static_cast<Entity*>(node->element));
        if (phase == 10) {
            check(boulders == 0, "battle_final_no_storm_before_last_minute");
            ::Raid::setSmokeElapsed(300);
            phase = 11;
        } else {
            check(boulders > 0, "battle_final_storm_starts_last_minute");
            for (auto node = map.entities->first; node;) {
                auto next = node->next;
                if (::Raid::stormBoulder(*static_cast<Entity*>(node->element))) list_RemoveNode(node);
                node = next;
            }
            phase = 12;
        }
    } else if (currentlevel == 3 && phase == 12) {
        ::Raid::setSmokeElapsed(300);
        for (int ordinal = 1; ordinal <= 4; ++ordinal) ::Raid::useExit(*battleExit(ordinal));
        check(!battleExit(1)->flags[INVISIBLE] && !battleExit(2)->flags[INVISIBLE]
            && battleExit(3)->flags[INVISIBLE] && battleExit(4)->flags[INVISIBLE], "battle_final_portal_opens_last_minute");
        check(battleExit(2)->light != nullptr, "battle_open_portal_restores_light");
        ::Raid::setSmokeElapsed(359); ::Raid::useExit(*battleExit(3)); ::Raid::useExit(*battleExit(4));
        check(battleExit(3)->flags[INVISIBLE] && battleExit(4)->flags[INVISIBLE], "battle_final_overtime_portals_hidden_before_zero");
        ::Raid::setSmokeElapsed(360); ::Raid::useExit(*battleExit(3)); ::Raid::useExit(*battleExit(4));
        check(!battleExit(3)->flags[INVISIBLE] && !battleExit(4)->flags[INVISIBLE], "battle_final_two_more_portals_at_zero");
        if (scenario == "battle-wipe") {
            for (int player = 0; player < peers; ++player) stats[player]->HP = 0;
        } else if (scenario == "battle-host-win") {
            auto exit = battleExit(1); move(0, *exit); selectedEntity[0] = exit; ::Raid::useExit(*exit);
        } else {
            auto exit = battleExit(4); move(1, *exit); client_selected[1] = exit; ::Raid::useExit(*exit);
            check(::Raid::outcome(1) == O::PendingExtraction, "battle_guest_first_claim_reserved");
            auto rivalExit = battleExit(2); move(0, *rivalExit); selectedEntity[0] = rivalExit; ::Raid::useExit(*rivalExit);
            check(::Raid::outcome(0) == O::Playing && ::Raid::outcome(1) == O::PendingExtraction,
                "battle_other_exit_cannot_steal_pending_victory");
            selectedEntity[0] = nullptr;
        }
        phase = 1;
    }
}
void battleGuest() {
    static int floorSeen = 0;
    static int progressionFloor = 0;
    static Uint32 progressionLoadedAt = 0;
    static bool progressionSeeded = false;
    if (currentlevel == 1 && !progressionSeeded && players[clientnum]->entity && ::Raid::outcome(clientnum) == O::Playing) {
        seedBattleProgression(clientnum);
        std::ofstream(sync + "/battle-progression-ready-" + std::to_string(clientnum)) << "ready";
        progressionSeeded = true;
    }
    static bool monsterRequested = false, monsterConfirmed = false;
    if (currentlevel == 1 && clientnum == 1) {
        if (!monsterRequested && ::Raid::outcome(1) == O::Slain && !players[1]->entity) {
            monsterRequested = ::Raid::requestRevenant(1, 1);
            check(monsterRequested, "battle_guest_same_floor_monster_request");
        }
        if (!monsterConfirmed && ::Raid::revenant(1) && players[1]->entity && stats[1]->HP > 0) {
            check(::Raid::outcome(1) == O::Slain, "battle_guest_monster_stays_on_current_floor");
            std::ofstream(sync + "/battle-monster-guest") << "ready";
            monsterConfirmed = true;
        }
    }
    if (floorSeen != currentlevel && players[clientnum]->entity && ::Raid::outcome(clientnum) == O::Playing) {
        floorSeen = currentlevel;
        progressionLoadedAt = ticks;
        if ((currentlevel == 2 && clientnum == 1) || currentlevel == 3) {
            check(stats[clientnum]->GOLD == 0 && stats[clientnum]->HP > 0, "battle_guest_starter_inventory_return");
            bool token = false;
            for (auto node = stats[clientnum]->inventory.first; node; node = node->next)
                token |= static_cast<Item*>(node->element)->appearance == 789;
            check(!token, "battle_guest_no_previous_loot");
        }
    }
    if (currentlevel > 1 && floorSeen == currentlevel && progressionFloor != currentlevel
        && ticks - progressionLoadedAt > TICKS_PER_SECOND) {
        check(battleProgressionRetained(clientnum), "battle_guest_retains_progression");
        std::ofstream(sync + "/battle-progression-floor-" + std::to_string(currentlevel) + "-" + std::to_string(clientnum)) << "ready";
        progressionFloor = currentlevel;
    }
    if (currentlevel == 2 && std::filesystem::exists(sync + "/battle-wall")) {
        const auto path = sync + "/battle-wall-guest-" + std::to_string(clientnum);
        int x = 0, y = 0; std::ifstream input(sync + "/battle-wall");
        if (!std::filesystem::exists(path) && input >> x >> y
            && !map.tiles[OBSTACLELAYER + y * MAPLAYERS + x * MAPLAYERS * map.height]) {
            check(true, "battle_guest_storm_wall_replicated");
            std::ofstream(path) << "ready";
        }
    }
}
