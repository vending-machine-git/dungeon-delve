namespace {
struct AdventurerProgression {
    bool saved = false, awaitingKit = false;
    std::array<Sint32, 10> attributes{};
    std::array<Sint32, NUMPROFICIENCIES> proficiencies{};
    std::array<Sint32, NUMSTATS> bonuses{};
    std::array<Sint32, NUMSTATS * 2> timers{};
    static std::array<Sint32*, 10> fields(Stat& character) {
        return {{&character.MAXHP, &character.MAXMP, &character.STR, &character.DEX,
            &character.CON, &character.INT, &character.PER, &character.CHR,
            &character.EXP, &character.LVL}};
    }
    void capture(Stat& character) {
        const auto source = fields(character);
        for (size_t index = 0; index < attributes.size(); ++index) attributes[index] = *source[index];
        for (int skill = 0; skill < NUMPROFICIENCIES; ++skill) proficiencies[skill] = character.getProficiency(skill);
        std::copy_n(character.PLAYER_LVL_STAT_BONUS, bonuses.size(), bonuses.begin());
        std::copy_n(character.PLAYER_LVL_STAT_TIMER, timers.size(), timers.begin());
        saved = true;
    }
    void restore(Stat& character) const {
        if (!saved) return;
        const auto destination = fields(character);
        for (size_t index = 0; index < attributes.size(); ++index) *destination[index] = attributes[index];
        for (int skill = 0; skill < NUMPROFICIENCIES; ++skill) character.setProficiencyUnsafe(skill, proficiencies[skill]);
        std::copy(bonuses.begin(), bonuses.end(), character.PLAYER_LVL_STAT_BONUS);
        std::copy(timers.begin(), timers.end(), character.PLAYER_LVL_STAT_TIMER);
        character.HP = character.OLDHP = character.MAXHP;
        character.MP = character.MAXMP;
    }
};
std::array<AdventurerProgression, MAXPLAYERS> adventurerProgression;
struct ReturnForm {
    const char* name;
    Monster monster;
    Uint32 required;
    int health, strength, dexterity, constitution;
};
constexpr ReturnForm returnForms[] = {
    {"Rat", RAT, 0, 18, 0, 3, 0},
    {"Spider", SPIDER, 0, 24, 2, 3, 0},
    {"Skeleton", SKELETON, 0, 35, 4, 0, 1},
    {"Goblin", GOBLIN, 0, 30, 4, 2, 0},
    {"Gnome", GNOME, 0, 28, 3, 3, 0},
    {"Goatman", GOATMAN, 750, 50, 7, 2, 2},
    {"Insectoid", INSECTOID, 750, 45, 6, 4, 1},
    {"Gremlin", GREMLIN, 750, 40, 5, 5, 1},
    {"Imp", CREATURE_IMP, 750, 40, 6, 5, 1},
    {"Automaton", AUTOMATON, 2000, 75, 10, 1, 5},
    {"Incubus", INCUBUS, 2000, 65, 9, 4, 3},
    {"Succubus", SUCCUBUS, 2000, 60, 8, 5, 3},
    {"Myconid", MYCONID, 2000, 80, 8, 0, 5},
    {"Dryad", DRYAD, 2000, 70, 9, 3, 4},
    {"Vampire", VAMPIRE, 4500, 100, 13, 6, 5},
    {"Troll", TROLL, 4500, 140, 16, 0, 8},
    {"Salamander", SALAMANDER, 4500, 110, 14, 4, 6}
};
constexpr int returnFormCount = sizeof(returnForms) / sizeof(returnForms[0]);
Uint32 carriedReturnWorth(int player) {
    uint64_t worth = std::max(0, stats[player]->GOLD);
    for (auto node = stats[player]->inventory.first; node; node = node->next) {
        auto item = static_cast<Item*>(node->element);
        if (item->type >= 0 && item->type < NUMITEMS && itemCategory(item) != SPELL_CAT)
            worth += uint64_t(std::max(0, items[item->type].gold_value)) * std::max(0, int(item->count));
    }
    return static_cast<Uint32>(std::min<uint64_t>(worth, 100000000));
}
void measureReturnLoot(int player) {
    if (!valid(player) || !stats[player] || outcomes[player] != Outcome::Playing || stats[player]->HP <= 0) return;
    auto& life = returnLives[player];
    const auto worth = carriedReturnWorth(player);
    if (!life.measured) { life.baseline = worth; life.measured = true; }
    const auto score = worth > life.baseline ? worth - life.baseline : 0;
    if (score <= life.loot) return;
    life.loot = score;
    if (multiplayer == CLIENT) {
        std::memcpy(net_packet->data, "RLOO", 4);
        SDLNet_Write32(uniqueGameKey, &net_packet->data[4]); net_packet->data[8] = player;
        SDLNet_Write32(mapseed, &net_packet->data[9]); SDLNet_Write32(score, &net_packet->data[13]);
        net_packet->len = 17; net_packet->address = net_server;
        sendPacketSafe(net_sock, -1, net_packet, 0);
    }
}
void applyAlgernonBody(int player, Entity& entity) {
    auto character = stats[player];
    character->HP = character->MAXHP = character->OLDHP = 60;
    character->MP = character->MAXMP = 10;
    character->STR = -1; character->DEX = 20; character->CON = 2;
    character->INT = 20; character->PER = -2; character->CHR = 5;
    character->LVL = 10; character->EXP = 0;
    entity.sprite = 1068;
    returnLives[player].evolved = entity.getUID();
    if (multiplayer == SERVER) serverUpdateEntitySprite(&entity);
}
void configureReturnBody(int player, Entity& entity) {
    if (!adventurerProgression[player].saved) adventurerProgression[player].capture(*stats[player]);
    auto& life = returnLives[player];
    const auto& form = returnForms[std::clamp(life.choice, 0, returnFormCount - 1)];
    auto character = stats[player];
    character->freePlayerEquipment(); list_FreeAll(&character->inventory);
    for (int effect = 0; effect < NUMEFFECTS; ++effect) {
        character->clearEffect(effect); character->EFFECTS_TIMERS[effect] = 0;
    }
    character->HP = character->MAXHP = life.rat ? 5 : form.health; character->OLDHP = character->HP;
    character->MP = character->MAXMP = 0; character->GOLD = 0;
    character->STR = life.rat ? 0 : form.strength;
    character->DEX = life.rat ? 0 : form.dexterity;
    character->CON = life.rat ? 0 : form.constitution;
    character->INT = character->PER = character->CHR = 0;
    character->HUNGER = 1000;
    character->type = form.monster;
    entity.effectPolymorph = form.monster;
    entity.setEffect(EFF_POLYMORPH, true, -1, false);
    players[player]->bControlEnabled = true;
    if (players[player]->ghost.my) {
        list_RemoveNode(players[player]->ghost.my->mynode); players[player]->ghost.my = nullptr;
    }
    players[player]->ghost.reset();
    if (players[player]->isLocalPlayer()) {
        players[player]->paperDoll.clear();
        players[player]->inventoryUI.appraisal.appraisalProgressionItems.clear();
        players[player]->shootmode = true;
        players[player]->gui_mode = GUI_MODE_NONE;
    }
    life.applied = entity.getUID();
    if (life.algernon) applyAlgernonBody(player, entity);
}
bool spawnReturnBody(int player, int choice, Uint32 nonce) {
    if (!active() || multiplayer != SERVER || complete || intro || loading || fadeout || gamePaused || !valid(player)
        || client_disconnected[player] || !results[player].participant || outcomes[player] != Outcome::Slain
        || !players[player] || players[player]->entity || stats[player]->HP > 0 || choice < 0 || choice >= returnFormCount) return false;
    auto& life = returnLives[player];
    if (nonce != life.nonce || life.alive || life.cooldown || (life.used ? choice != 0 : life.loot < returnForms[choice].required)) return false;
    int spawnX = -1, spawnY = -1;
    const int count = map.width * map.height;
    const auto walkable = [](int tile) {
        const int floor = map.tiles[tile * MAPLAYERS];
        return floor && !swimmingtiles[floor] && !lavatiles[floor] && !map.tiles[tile * MAPLAYERS + OBSTACLELAYER];
    };
    std::vector<bool> reachable(count, false);
    std::vector<int> frontier;
    for (int slot = 0; slot < MAXPLAYERS; ++slot) {
        if (outcomes[slot] != Outcome::Playing || !players[slot] || !players[slot]->entity || stats[slot]->HP <= 0) continue;
        const int tileX = players[slot]->entity->x / 16, tileY = players[slot]->entity->y / 16;
        if (tileX < 0 || tileY < 0 || tileX >= map.width || tileY >= map.height) continue;
        const int tile = tileX * map.height + tileY;
        if (walkable(tile) && !reachable[tile]) { reachable[tile] = true; frontier.push_back(tile); }
    }
    for (size_t index = 0; index < frontier.size(); ++index) {
        const int tile = frontier[index], tileX = tile / map.height, tileY = tile % map.height;
        const std::pair<int, int> neighbors[] = {{tileX-1,tileY}, {tileX+1,tileY}, {tileX,tileY-1}, {tileX,tileY+1}};
        for (const auto& neighbor : neighbors) {
            if (neighbor.first < 0 || neighbor.second < 0 || neighbor.first >= map.width || neighbor.second >= map.height) continue;
            const int next = neighbor.first * map.height + neighbor.second;
            if (!reachable[next] && walkable(next)) { reachable[next] = true; frontier.push_back(next); }
        }
    }
    const int start = (mapseed + player * 997u + life.nonce * 127u) % std::max(1, count);
    for (int index = 0; index < count; ++index) {
        const int tile = (index + start) % count, tileX = tile / map.height, tileY = tile % map.height;
        if (tileX < 2 || tileY < 2 || tileX >= map.width - 2 || tileY >= map.height - 2) continue;
        const int floor = map.tiles[tile * MAPLAYERS];
        if (!floor || !reachable[tile]) continue;
        bool occupied = false;
        for (auto node = map.creatures->first; node; node = node->next) {
            auto other = static_cast<Entity*>(node->element);
            const double distance = std::hypot(other->x - (tileX * 16 + 8), other->y - (tileY * 16 + 8));
            if (distance < (other->behavior == &actPlayer ? 96 : 12)) { occupied = true; break; }
        }
        if (!occupied) { spawnX = tileX; spawnY = tileY; break; }
    }
    if (spawnX < 0) return false;
    life.rat = life.used; life.used = true; life.alive = true; life.choice = choice; ++life.nonce;
    life.cheese = 0; life.algernon = false; life.evolved = 0;
    auto entity = newEntity(113, 1, map.entities, nullptr);
    entity->x = entity->new_x = spawnX * 16 + 8;
    entity->y = entity->new_y = spawnY * 16 + 8;
    entity->z = -1; entity->sizex = entity->sizey = 4;
    entity->behavior = &actPlayer; entity->skill[2] = player;
    entity->flags[GENIUS] = entity->flags[UPDATENEEDED] = entity->flags[BLOCKSIGHT] = true;
    entity->addToCreatureList(map.creatures); players[player]->entity = entity;
    life.body = entity->getUID(); configureReturnBody(player, *entity);
    serverUpdateEffects(player); serverUpdateEntitySkill(entity, 50); serverUpdatePlayerStats();
    sendState();
    printlog("[DELVE] revenant_spawn slot=%d choice=%d rat=%d loot=%u hp=%d uid=%u", player, choice, life.rat, life.loot, stats[player]->HP, life.body);
    return true;
}
void tickReturnLives(Uint32 delta) {
    for (int player = 0; player < MAXPLAYERS; ++player) {
        if (!players[player] || !stats[player] || !results[player].participant) continue;
        auto& life = returnLives[player];
        if (outcomes[player] == Outcome::Playing && !adventurerProgression[player].awaitingKit
            && (players[player]->isLocalPlayer() || (multiplayer == SERVER && loadoutReceived[player])))
            adventurerProgression[player].capture(*stats[player]);
        if (players[player]->isLocalPlayer()) measureReturnLoot(player);
        if (players[player]->isLocalPlayer() && outcomes[player] == Outcome::Slain && !players[player]->entity
            && !gamePaused && keystatus[SDLK_r] && gameUIFrame[player] && !gameUIFrame[player]->findFrame("gameover")) {
            keystatus[SDLK_r] = 0; MainMenu::openGameoverWindow(player);
        }
        if (multiplayer == CLIENT) {
            if (life.alive && life.body != life.applied && players[player]->isLocalPlayer()
                && players[player]->entity && players[player]->entity->getUID() == life.body)
                configureReturnBody(player, *players[player]->entity);
            if (life.alive && life.algernon && life.evolved != life.body && players[player]->isLocalPlayer()
                && players[player]->entity && players[player]->entity->getUID() == life.body)
                applyAlgernonBody(player, *players[player]->entity);
            continue;
        }
        if (gamePaused) continue;
        if (life.cooldown) life.cooldown -= std::min(delta, life.cooldown);
        if (!life.alive) continue;
        if (client_disconnected[player] || !players[player]->entity) {
            life.alive = false; life.body = 0; ++life.nonce;
            life.cooldown = life.rat ? BattleRules::ratRespawnSeconds * TICKS_PER_SECOND : 0;
            life.cheese = 0; life.algernon = false;
            printlog("[DELVE] revenant_death slot=%d rat=%d cooldown=%u", player, life.rat, life.cooldown);
            sendState(); continue;
        }
        auto entity = players[player]->entity;
        if (life.body != entity->getUID()) { life.body = entity->getUID(); sendState(); }
        stats[player]->HUNGER = 1000;
        if (life.rat && !life.algernon) {
            stats[player]->MAXHP = 5; stats[player]->HP = std::min(5, stats[player]->HP);
            stats[player]->MP = stats[player]->MAXMP = stats[player]->STR = stats[player]->CON = 0;
            stats[player]->EXP = 0;
        }
        if (stats[player]->HP > 0) {
            entity->effectPolymorph = returnForms[life.choice].monster;
            entity->setEffect(EFF_POLYMORPH, true, -1, false);
        }
    }
}
void updateReturnWindow(Frame& window, int player) {
    if (auto parent = static_cast<Frame*>(window.getParent())) {
        auto bounds = window.getSize();
        bounds.x = (parent->getSize().w - 600) / 2;
        bounds.w = 600; bounds.h = 420;
        window.setSize(bounds); window.setActualSize({0, 0, 600, 420});
        if (auto dismiss = window.findButton("dismiss")) dismiss->setSize({255, 382, 90, 34});
    }
    auto panel = window.findFrame("delve return");
    if (!panel) {
        if (auto background = window.findImage("background")) background->disabled = true;
        for (const auto name : {"banner", "epitaph", "footer"})
            if (auto field = window.findField(name)) field->setDisabled(true);
        window.addImage({0, 0, 600, 420}, makeColorRGB(111, 91, 58), "images/system/white.png", "delve return border");
        window.addImage({1, 1, 598, 418}, makeColorRGB(23, 24, 27), "images/system/white.png", "delve return background");
        panel = window.addFrame("delve return");
        panel->setSize({0, 0, 600, 378}); panel->setActualSize({0, 0, 600, 378});
        panel->setBorder(0); panel->setHollow(true); panel->setOwner(player);
        panel->addImage({16, 62, 568, 1}, makeColorRGB(89, 76, 54), "images/system/white.png", "heading rule");
        panel->addImage({16, 70, 568, 30}, makeColorRGB(35, 35, 38), "images/system/white.png", "status background");
        const auto label = [panel](const char* name, SDL_Rect bounds, const char* font, Uint32 color, const char* text) {
            auto field = panel->addField(name, 256);
            field->setSize(bounds); field->setFont(font); field->setColor(color); field->setText(text);
            return field;
        };
        label("title", {16, 14, 424, 28}, "lang/en.ttf#22#1", DelveHUD::brass, "RETURN AS A MONSTER");
        label("intro", {16, 44, 568, 18}, "lang/en.ttf#12#1", DelveHUD::muted, "");
        label("loot", {450, 22, 134, 22}, "lang/en.ttf#14#1", DelveHUD::ivory, "")->setJustify(Field::RIGHT);
        label("status", {24, 76, 552, 20}, "lang/en.ttf#14#1", DelveHUD::ivory, "");
        label("cheese title", {400, 330, 184, 40}, "lang/en.ttf#10#1", DelveHUD::brass, "");
        label("cheese detail", {16, 260, 568, 54}, "lang/en.ttf#14#1", DelveHUD::muted, "");
        label("help", {16, 346, 568, 18}, "lang/en.ttf#12#1", DelveHUD::muted,
            "NPCs ignore monster returns. Adventurers remain hostile.");
        for (int choice = 0; choice < returnFormCount; ++choice) {
            auto button = panel->addButton(("form " + std::to_string(choice)).c_str());
            button->setBorder(1);
            button->setHighlightColor(makeColorRGB(80, 67, 45));
            button->setTextHighlightColor(DelveHUD::ivory);
            button->setUserData(reinterpret_cast<void*>(static_cast<intptr_t>(choice + 1)));
            button->setCallback([](Button& button) {
                const int choice = static_cast<int>(reinterpret_cast<intptr_t>(button.getUserData())) - 1;
                requestRevenant(button.getParent()->getOwner(), choice);
            });
        }
    }
    const auto& life = returnLives[player]; char text[256];
    snprintf(text, sizeof(text), "LOOT  %u", life.loot);
    panel->findField("loot")->setText(text);
    panel->findField("intro")->setText(life.used
        ? "Your monster life is spent. Fragile rats can keep returning."
        : "Choose one monster life. Your collected loot unlocks stronger forms.");
    if (life.cooldown) {
        const auto seconds = (life.cooldown + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
        snprintf(text, sizeof(text), "RAT RESPAWN  %02u:%02u", seconds / 60, seconds % 60);
    } else if (life.alive) snprintf(text, sizeof(text), "Your returned body is still alive.");
    else snprintf(text, sizeof(text), "%s", life.used ? "READY TO RETURN  /  5 HP, NORMAL DAMAGE" : "Select a form below  /  HP and loot requirement shown");
    panel->findField("status")->setText(text);
    auto cheeseTitle = panel->findField("cheese title");
    cheeseTitle->setSize(life.used ? SDL_Rect{16, 232, 568, 24} : SDL_Rect{400, 330, 184, 40});
    cheeseTitle->setFont(life.used ? "lang/en.ttf#16#1" : "lang/en.ttf#12#1");
    cheeseTitle->setJustify(life.used ? Field::LEFT : Field::CENTER);
    cheeseTitle->setText(life.used ? "A SMALL RAT. A BIG APPETITE." : "10 CHEESE\nBECOME ALGERNON");
    panel->findField("help")->setDisabled(!life.used);
    panel->findField("cheese detail")->setDisabled(!life.used);
    panel->findField("cheese detail")->setText("Eat 10 pieces of cheese in one rat life to become Algernon.\n60 HP and native Algernon stats. Progress resets on death.");
    for (int choice = 0; choice < returnFormCount; ++choice) {
        const auto& form = returnForms[choice];
        auto button = panel->findButton(("form " + std::to_string(choice)).c_str());
        const bool locked = life.loot < form.required;
        const bool unavailable = life.alive || life.cooldown || (life.used ? choice != 0 : locked);
        button->setInvisible(life.used && choice != 0);
        button->setDisabled(unavailable);
        button->setColor(unavailable ? makeColorRGB(28, 29, 32) : makeColorRGB(48, 43, 35));
        button->setBorderColor(unavailable ? makeColorRGB(54, 54, 57) : makeColorRGB(116, 93, 57));
        button->setTextColor(unavailable ? makeColorRGB(126, 126, 130) : DelveHUD::ivory);
        button->setSize(life.used ? SDL_Rect{16, 110, 568, 100}
            : SDL_Rect{16 + (choice % 3) * 192, 110 + (choice / 3) * 44, 184, 40});
        button->setFont(life.used ? "lang/en.ttf#18#1" : "lang/en.ttf#14#1");
        if (life.used) snprintf(text, sizeof(text), "RETURN AS A RAT\n5 HP / NORMAL DAMAGE");
        else if (locked) snprintf(text, sizeof(text), "%s\nLOCKED / %u LOOT", form.name, form.required);
        else if (form.required) snprintf(text, sizeof(text), "%s\n%d HP / %u LOOT", form.name, form.health, form.required);
        else snprintf(text, sizeof(text), "%s\n%d HP / FREE", form.name, form.health);
        button->setText(text);
    }
}
}
bool revenant(int player) {
    return active() && valid(player) && outcomes[player] == Outcome::Slain && returnLives[player].used && returnLives[player].alive;
}
bool revenantRat(int player) { return revenant(player) && returnLives[player].rat; }
bool ratReturn(int player) { return revenant(player) && returnLives[player].choice == 0; }
bool algernon(int player) { return ratReturn(player) && returnLives[player].algernon; }
void ateCheese(int player) {
    if (!active() || multiplayer != SERVER || !ratReturn(player) || complete || gamePaused
        || !players[player] || !players[player]->entity || stats[player]->HP <= 0) return;
    auto& life = returnLives[player];
    if (life.algernon || life.cheese >= BattleRules::algernonCheese) return;
    ++life.cheese;
    messagePlayer(player, MESSAGE_MISC, "Cheese eaten: %u / 10", life.cheese);
    if (life.cheese == BattleRules::algernonCheese) {
        life.algernon = true;
        applyAlgernonBody(player, *players[player]->entity);
        serverUpdatePlayerStats();
        messagePlayer(player, MESSAGE_MISC, "You have become Algernon!");
        printlog("[DELVE] algernon_evolution slot=%d uid=%u cheese=%u", player, life.body, life.cheese);
    }
    sendState();
}
bool neutralRevenantPair(const Entity& first, const Entity& second) {
    const auto returned = [](const Entity& entity) {
        return (entity.behavior == &actPlayer || entity.behavior == &actPlayerLimb) && revenant(entity.skill[2]);
    };
    return (first.behavior == &actMonster && returned(second)) || (second.behavior == &actMonster && returned(first));
}
bool requestRevenant(int player, int choice) {
    if (!active() || !valid(player) || choice < 0 || choice >= returnFormCount) return false;
    if (multiplayer == SERVER) return spawnReturnBody(player, choice, returnLives[player].nonce);
    if (!players[player] || !players[player]->isLocalPlayer() || complete || outcomes[player] != Outcome::Slain) return false;
    std::memcpy(net_packet->data, "RRET", 4); SDLNet_Write32(uniqueGameKey, &net_packet->data[4]);
    net_packet->data[8] = player; net_packet->data[9] = choice;
    SDLNet_Write32(mapseed, &net_packet->data[10]); SDLNet_Write32(returnLives[player].nonce, &net_packet->data[14]);
    net_packet->len = 18; net_packet->address = net_server;
    sendPacketSafe(net_sock, -1, net_packet, 0); return true;
}
void receiveRevenantRequest() {
    if (!active() || multiplayer != SERVER || net_packet->len != 18 || SDLNet_Read32(&net_packet->data[4]) != uniqueGameKey
        || SDLNet_Read32(&net_packet->data[10]) != mapseed) return;
    const int player = packetPlayer();
    if (player > 0) spawnReturnBody(player, net_packet->data[9], SDLNet_Read32(&net_packet->data[14]));
}
void receiveLootScore() {
    if (!active() || multiplayer != SERVER || net_packet->len != 17 || SDLNet_Read32(&net_packet->data[4]) != uniqueGameKey
        || SDLNet_Read32(&net_packet->data[9]) != mapseed) return;
    const int player = packetPlayer();
    if (player > 0 && outcomes[player] == Outcome::Playing && stats[player]->HP > 0)
        returnLives[player].loot = std::max(returnLives[player].loot, std::min(100000000u, SDLNet_Read32(&net_packet->data[13])));
}
#ifdef BARONY_SMOKE_TESTS
void smokeReturnLives() {
    static int phase = 0, formIndex = 0;
    static Uint32 phaseAt = 0;
    static bool guestScored = false, guestSelected = false, guestRat = false, guestChecked = false;
    static int guestCheese = 0;
    static bool observerChecked = false;
    const auto check = [](bool passed, const char* label) { printlog("[SMOKE][DELVE] check %s %s", passed ? "PASS" : "FAIL", label); };
    if (!active() || intro || loading || fadeout || complete) return;
    if (multiplayer == SERVER) {
        for (auto node = map.entities->first; node;) {
            auto next = node->next;
            auto entity = static_cast<Entity*>(node->element);
            if (entity->behavior == &actBoulder) list_RemoveNode(node);
            node = next;
        }
        for (int player = 2; player < MAXPLAYERS; ++player)
            if (phase < 12 && stats[player] && stats[player]->HP > 0 && outcomes[player] == Outcome::Playing) stats[player]->HP = stats[player]->MAXHP;
        for (auto node = map.creatures->first; node; node = node->next) {
            auto monster = static_cast<Entity*>(node->element);
            if (monster->behavior == &actMonster) monster->setEffect(EFF_ASLEEP, true, 100000, false);
        }
    }
    if (multiplayer == CLIENT) {
        if (clientnum != 1) {
            if (!observerChecked && algernon(0) && players[0]->entity
                && players[0]->entity->sprite >= 1068 && players[0]->entity->sprite <= 1074) {
                check(true, "revenant_algernon_observer_visual");
                std::ofstream(std::string(std::getenv("BARONY_SMOKE_RAID_SYNC")) + "/algernon-observer-" + std::to_string(clientnum)) << "ready";
                observerChecked = true;
            }
            return;
        }
        if (!guestScored && returnLives[1].measured && players[1]->entity && elapsed > 3*TICKS_PER_SECOND) {
            check(returnLives[1].loot == 0, "revenant_starter_not_loot");
            stats[1]->GOLD += 5000; measureReturnLoot(1); guestScored = true;
        }
        auto& life = returnLives[1];
        if (outcomes[1] == Outcome::Slain && !players[1]->entity && !life.alive) {
            auto dimmer = gameUIFrame[1] ? gameUIFrame[1]->findFrame("gameover") : nullptr;
            auto window = dimmer ? dimmer->findFrame("window") : nullptr;
            auto picker = window ? window->findFrame("delve return") : nullptr;
            if (picker && !life.used && !guestSelected) {
                const auto sync = std::string(std::getenv("BARONY_SMOKE_RAID_SYNC"));
                std::ofstream(sync + "/return-menu-first") << "ready";
                if (!std::filesystem::exists(sync + "/return-menu-first-captured")) return;
                bool layout = window->findField("banner")->isDisabled();
                const auto status = picker->findField("status")->getSize();
                for (int choice = 0; choice < returnFormCount; ++choice) {
                    const auto bounds = picker->findButton(("form " + std::to_string(choice)).c_str())->getSize();
                    layout &= bounds.x >= 16 && bounds.x + bounds.w <= 584 && bounds.y >= 110
                        && bounds.y + bounds.h <= 370 && !SDL_HasIntersection(&bounds, &status);
                }
                check(layout, "revenant_menu_readable_card_layout");
                check(picker->findButton("form 16") && !picker->findButton("form 16")->isDisabled(), "revenant_guest_tier_unlocked_ui");
                picker->findButton("form 16")->activate(); guestSelected = true;
            } else if (picker && life.used && !life.rat && !guestRat) {
                const auto sync = std::string(std::getenv("BARONY_SMOKE_RAID_SYNC"));
                std::ofstream(sync + "/return-menu-rat") << "ready";
                if (!std::filesystem::exists(sync + "/return-menu-rat-captured")) return;
                bool ratOnly = !picker->findButton("form 0")->isInvisible();
                for (int choice = 1; choice < returnFormCount; ++choice)
                    ratOnly &= picker->findButton(("form " + std::to_string(choice)).c_str())->isInvisible();
                check(ratOnly && !picker->findField("cheese detail")->isDisabled(), "revenant_menu_rat_only_view");
                check(picker->findButton("form 16")->isDisabled(), "revenant_guest_second_npc_locked_ui");
                picker->findButton("form 0")->activate(); guestRat = true;
            }
        }
        if (revenant(1) && players[1]->entity && life.applied == life.body && !guestChecked) {
            check(stats[1]->type == SALAMANDER && stats[1]->MAXHP == 110, "revenant_guest_body_synchronized");
            check(players[1]->bControlEnabled && players[1]->entity->effectPolymorph == SALAMANDER, "revenant_guest_control_enabled");
            std::ofstream(std::string(std::getenv("BARONY_SMOKE_RAID_SYNC")) + "/revenant-guest-body") << "ready";
            guestChecked = true;
        }
        if (revenantRat(1) && !algernon(1) && guestCheese < 10) {
            Item* cheese = nullptr;
            for (auto node = stats[1]->inventory.first; node; node = node->next) {
                auto item = static_cast<Item*>(node->element);
                if (item->type == FOOD_CHEESE) { cheese = item; break; }
            }
            if (cheese && guestCheese < 9) {
                useItem(cheese, 1); ++guestCheese;
            } else if (cheese && guestCheese == 9 && life.cheese == 9) {
                check(!algernon(1), "revenant_guest_nine_cheese_not_evolved");
                useItem(cheese, 1); ++guestCheese;
            }
        }
        if (algernon(1) && life.evolved == life.body && guestCheese == 10) {
            check(life.cheese == 10 && stats[1]->MAXHP == 60 && stats[1]->DEX == 20
                && stats[1]->INT == 20 && stats[1]->LVL == 10, "revenant_guest_algernon_stats_networked");
            check(players[1]->entity->sprite >= 1068 && players[1]->entity->sprite <= 1074,
                "revenant_guest_algernon_native_visual");
            std::ofstream(std::string(std::getenv("BARONY_SMOKE_RAID_SYNC")) + "/algernon-guest") << "ready";
            ++guestCheese;
        }
        return;
    }
    if (phase == 0) {
        for (int player = 0; player < MAXPLAYERS; ++player)
            if (!players[player] || !players[player]->entity || (player && !loadoutReceived[player])) return;
        if (elapsed < 5*TICKS_PER_SECOND || returnLives[1].loot < 4500) return;
        check(returnLives[0].loot == 0, "revenant_host_starter_not_loot");
        printlog("[SMOKE][DELVE] hostility enemy=%d friend=%d friendlyfire=%d", players[0]->entity->checkEnemy(players[1]->entity), players[0]->entity->checkFriend(players[1]->entity), (svFlags & SV_FLAG_FRIENDLYFIRE) != 0);
        check(returnLives[1].loot >= 4500, "revenant_guest_loot_report_authenticated");
        stats[0]->HP = stats[1]->HP = 0; phase = 1; phaseAt = ticks;
    } else if (phase == 1 && outcomes[0] == Outcome::Slain && !players[0]->entity && !returnLives[0].used) {
        if (ticks - phaseAt < 5*TICKS_PER_SECOND) return;
        check(!requestRevenant(0, 16), "revenant_locked_tier_rejected");
        check(!spawnReturnBody(0, 2, returnLives[0].nonce-1), "revenant_stale_nonce_rejected");
        check(!requestRevenant(0, -1) && !requestRevenant(0, returnFormCount), "revenant_invalid_choices_rejected");
        check(requestRevenant(0, 2), "revenant_host_first_npc");
        check(!requestRevenant(0, 2), "revenant_duplicate_request_rejected");
        phase = 2; phaseAt = ticks;
    } else if (phase == 2 && revenant(0) && revenant(1) && ticks-phaseAt > 2*TICKS_PER_SECOND
        && std::filesystem::exists(std::string(std::getenv("BARONY_SMOKE_RAID_SYNC")) + "/revenant-guest-body")) {
        check(stats[0]->type == SKELETON && stats[1]->type == SALAMANDER, "revenant_host_guest_form_types");
        check(outcomes[0] == Outcome::Slain && outcomes[1] == Outcome::Slain && !complete, "revenant_original_results_preserved");
        check(players[0]->entity->checkEnemy(players[2]->entity), "revenant_still_hostile_to_living_players");
        bool neutral = true; int monsters = 0;
        for (auto node = map.creatures->first; node; node = node->next) {
            auto monster = static_cast<Entity*>(node->element);
            if (monster->behavior != &actMonster || !monster->getStats()) continue;
            ++monsters;
            neutral &= !monster->checkEnemy(players[0]->entity) && monster->checkFriend(players[0]->entity);
            monster->monsterAcquireAttackTarget(*players[0]->entity, MONSTER_STATE_PATH, true);
            neutral &= monster->monsterTarget != players[0]->entity->getUID();
        }
        check(monsters > 0 && neutral, "revenant_all_npcs_ignore_even_retaliation");
        stats[0]->HP = stats[1]->HP = 0; phase = 3;
    } else if (phase == 3 && !players[0]->entity && !returnLives[0].alive) {
        check(!requestRevenant(0, 2), "revenant_npc_life_only_once");
        check(requestRevenant(0, 0), "revenant_first_rat_immediate"); phase = 4; phaseAt = ticks;
    } else if (phase == 4 && revenantRat(0) && revenantRat(1) && ticks-phaseAt > TICKS_PER_SECOND) {
        check(stats[0]->HP == 5 && stats[0]->MAXHP == 5 && stats[0]->MP == 0, "revenant_fragile_rat_stats");
        auto rat = players[0]->entity; auto target = players[2]->entity;
        rat->x = target->x - 16; rat->y = target->y; rat->yaw = 0;
        stats[2]->freePlayerEquipment(); stats[2]->CON = 0;
        stats[2]->HP = stats[2]->MAXHP = 500; const int before = stats[2]->HP;
        rat->attack(1, MAXCHARGE, target);
        check(before-stats[2]->HP > 1, "revenant_native_rat_attack_uncapped");
        const int beforePassive = stats[2]->HP;
        {
            DelvePassives::Scope attack(rat, DelvePassives::Channel::Melee);
            target->modHP(-17);
        }
        check(beforePassive-stats[2]->HP > 1, "revenant_passive_rat_damage_uncapped");
        auto loot = newItem(ARTIFACT_SWORD, EXCELLENT, 10, 1, 0, true, nullptr);
        check(!itemPickup(0, loot), "revenant_rat_cannot_equip_loot"); free(loot);
        check(!target->checkEnemy(players[2]->entity) && rat->checkEnemy(target), "revenant_rat_player_hostility");
        bool neutral = true;
        for (auto node = map.creatures->first; node; node = node->next) {
            auto monster = static_cast<Entity*>(node->element);
            if (monster->behavior == &actMonster && monster->getStats()) neutral &= !monster->checkEnemy(rat);
        }
        check(neutral, "revenant_rat_npc_neutrality");
        auto cheeseItem = newItem(FOOD_CHEESE, EXCELLENT, 0, 10, 0, true, nullptr);
        auto cheese = itemPickup(0, cheeseItem); free(cheeseItem);
        check(cheese && returnLives[0].cheese == 0, "revenant_rat_can_pick_up_cheese_without_counting");
        if (!cheese) return;
        rat->setEffect(EFF_VOMITING, true, TICKS_PER_SECOND, false);
        item_Food(cheese, 0);
        check(returnLives[0].cheese == 0 && cheese->count == 10, "revenant_rejected_food_not_counted");
        rat->setEffect(EFF_VOMITING, false, 0, false);
        for (int piece = 0; piece < 9; ++piece) item_Food(cheese, 0);
        check(returnLives[0].cheese == 9 && !algernon(0), "revenant_nine_cheese_not_evolved");
        item_Food(cheese, 0);
        check(algernon(0) && returnLives[0].cheese == 10 && stats[0]->MAXHP == 60
            && stats[0]->STR == -1 && stats[0]->DEX == 20 && stats[0]->CON == 2
            && stats[0]->INT == 20 && stats[0]->PER == -2 && stats[0]->CHR == 5
            && stats[0]->LVL == 10, "revenant_tenth_cheese_native_algernon_stats");
        const auto evolvedBody = returnLives[0].evolved;
        stats[0]->HP = 55; ateCheese(0);
        check(returnLives[0].evolved == evolvedBody && returnLives[0].cheese == 10 && stats[0]->HP == 55,
            "revenant_algernon_evolution_once_only");
        auto guestFood = newItem(FOOD_CHEESE, EXCELLENT, 0, 10, 0, true, nullptr);
        itemPickup(1, guestFood); free(guestFood);
        phase = 40; phaseAt = ticks;
    } else if (phase == 40 && ticks - phaseAt > TICKS_PER_SECOND) {
        const auto sync = std::string(std::getenv("BARONY_SMOKE_RAID_SYNC"));
        if (!std::filesystem::exists(sync + "/algernon-guest")
            || !std::filesystem::exists(sync + "/algernon-observer-2")
            || !std::filesystem::exists(sync + "/algernon-observer-3")) return;
        check(algernon(0) && algernon(1) && stats[0]->MAXHP == 60 && stats[1]->MAXHP == 60,
            "revenant_algernon_not_clamped_to_fragile_rat");
        check(returnLives[1].cheese == 10, "revenant_guest_cheese_host_authoritative");
        stats[0]->HP = 0; phase = 5;
    } else if (phase == 5 && !players[0]->entity && !returnLives[0].alive) {
        auto& life = returnLives[0];
        check(life.cooldown > 44*TICKS_PER_SECOND && life.cooldown <= 45*TICKS_PER_SECOND, "revenant_rat_cooldown_45_seconds");
        check(!life.algernon && life.cheese == 0, "revenant_cheese_resets_after_death");
        Frame menu("return menu audit"); menu.setOwner(0);
        updateReturnWindow(menu, 0);
        check(std::string(menu.findFrame("delve return")->findField("status")->getText()) == "RAT RESPAWN  00:45",
            "revenant_menu_cooldown_countdown");
        check(!requestRevenant(0, 0), "revenant_rat_early_respawn_rejected");
        const auto beforePause = life.cooldown;
        gamePaused = true; tickReturnLives(60*TICKS_PER_SECOND); gamePaused = false;
        check(life.cooldown == beforePause, "revenant_cooldown_paused");
        tickReturnLives(life.cooldown-1);
        check(life.cooldown == 1 && !requestRevenant(0, 0), "revenant_rat_cooldown_last_tick_blocked");
        tickReturnLives(1);
        check(requestRevenant(0, 0), "revenant_rat_respawn_after_exact_cooldown");
        phase = 6; phaseAt = ticks;
    } else if (phase == 6 && ticks-phaseAt > TICKS_PER_SECOND && players[0]->entity) {
        check(!algernon(0) && returnLives[0].cheese == 0 && stats[0]->MAXHP == 5,
            "revenant_new_rat_starts_without_cheese");
        stats[0]->HP = 0; phase = 7;
    } else if (phase == 7 && !players[0]->entity && !returnLives[0].alive) {
        auto& life = returnLives[0]; life.cooldown = 0; life.used = false;
        life.loot = returnForms[formIndex].required;
        if (life.loot) {
            --life.loot; check(!requestRevenant(0, formIndex), "revenant_below_exact_tier_threshold_rejected"); ++life.loot;
        }
        check(requestRevenant(0, formIndex), "revenant_catalogue_form_spawn");
        phase = 8; phaseAt = ticks;
    } else if (phase == 8 && ticks-phaseAt > TICKS_PER_SECOND && players[0]->entity) {
        const auto& form = returnForms[formIndex];
        check(stats[0]->type == form.monster && players[0]->entity->getMonsterTypeFromSprite() == form.monster, "revenant_catalogue_native_visual");
        check(stats[0]->MAXHP == form.health, "revenant_catalogue_tier_stats");
        printlog("[SMOKE][DELVE] revenant_form choice=%d name=%s", formIndex, form.name);
        ++formIndex;
        if (formIndex < returnFormCount) { stats[0]->HP = 0; phase = 7; }
        else phase = 9;
    } else if (phase == 9 && players[0]->entity) {
        check(!requestRevenant(2, 0), "revenant_living_player_cannot_respawn");
        for (int player = 2; player < MAXPLAYERS; ++player) stats[player]->HP = 0;
        phase = 10;
    } else if (phase == 10 && currentlevel == 2 && players[0]->entity && players[1]->entity && populated) {
        phase = 11; phaseAt = ticks;
    } else if (phase == 11 && ticks-phaseAt > 3*TICKS_PER_SECOND) {
        check(!revenant(0) && !revenantRat(1), "revenant_forms_clear_on_next_floor");
        check(outcomes[0] == Outcome::Playing && outcomes[1] == Outcome::Playing && stats[0]->HP > 0
            && stats[1]->HP > 0 && stats[0]->GOLD == 0 && stats[1]->GOLD == 0, "revenant_next_floor_fresh_adventurers");
        stats[0]->HP = stats[1]->HP = stats[2]->HP = stats[3]->HP = 0;
        phase = 12;
    } else if (phase == 12 && currentlevel == 3 && populated && players[0]->entity && players[1]->entity) {
        stats[0]->HP = stats[1]->HP = stats[2]->HP = stats[3]->HP = 0;
        phase = 13; printlog("[SMOKE][DELVE] check PASS revenants_host_complete");
    } else if (phase == 13) {
        for (int player = 0; player < MAXPLAYERS; ++player)
            if (stats[player]) stats[player]->HP = 0;
    }
}
#endif
