void rarityAudit(int player) {
    if (multiplayer == SERVER) check(::Raid::smokeEnemyLootRules(player), "enemy_random_loot_floor_rarity_books_and_once_only");
    spellbookAudit(player);
    using namespace DelveRarity;
    bool blessedBackgrounds = true;
    bool metadata = true, visuals = true, powers = true, stacks = true, unique = true, tooltipRims = true, unobscured = true;
    const ItemType cases[] = {IRON_SWORD,ARTIFACT_BOW,STEEL_HELM,CRYSTAL_BREASTPIECE,RING_STRENGTH,
        AMULET_MAGICREFLECTION,MAGICSTAFF_LIGHTNING,SPELLBOOK_FIREBALL,IRON_DAGGER,GEM_DIAMOND,GEM_JEWEL,
        POTION_HEALING,POTION_RESTOREMAGIC,POTION_ACID,POTION_SPEED,TOOL_BEARTRAP};
    Frame frame("rarity audit"); frame.setOwner(player); frame.setSize(SDL_Rect{0,0,48,48});
    createPlayerInventorySlotFrameElements(&frame);
    Frame compactFrame("compact rarity audit"); compactFrame.setOwner(player); compactFrame.setSize(SDL_Rect{0,0,40,40});
    createPlayerInventorySlotFrameElements(&compactFrame);
    for (auto type : cases) {
        auto item = newItem(type,items[type].category == GEM ? DECREPIT : SERVICABLE,-2,1,123,true,nullptr);
        const auto originalModel = itemModel(item);
        const int baseAttack = item->weaponGetAttack(), baseArmor = item->armorGetAC();
        const auto baseValue = item->getGoldValue();
        const auto baseHeal = item->potionGetEffectHealth(nullptr,nullptr), baseDamage = item->potionGetEffectDamage(nullptr,nullptr);
        const auto baseMinimum = item->potionGetEffectDurationMinimum(nullptr,nullptr), baseMaximum = item->potionGetEffectDurationMaximum(nullptr,nullptr);
        const auto cursedMinimum = item->potionGetCursedEffectDurationMinimum(nullptr,nullptr), cursedMaximum = item->potionGetCursedEffectDurationMaximum(nullptr,nullptr);
        for (int index = 0; index < 5; ++index) {
            const auto rarity = static_cast<Tier>(index);
            assign(*item,rarity,1234);
            auto clone = newItem(type,item->status,item->beatitude,item->count,item->appearance,true,nullptr);
            metadata &= tier(*clone) == rarity && itemCompare(item,clone,false) == 0;
            metadata &= item->status == (items[type].category == GEM || equipment(*item) ? static_cast<Status>(std::min(index+1,4)) : SERVICABLE) && item->beatitude == -2;
            visuals &= itemModel(item) == originalModel;
            auto native = newItem(type,item->status,item->beatitude,1,123,true,nullptr);
            powers &= item->weaponGetAttack() == (equipment(*item) ? native->weaponGetAttack() : baseAttack) + attackBonus(*item);
            powers &= item->armorGetAC() == (equipment(*item) ? native->armorGetAC() : baseArmor) + armorBonus(*item);
            free(native);
            powers &= index == 0 ? item->getGoldValue() == baseValue : item->getGoldValue() >= baseValue;
            powers &= item->potionGetEffectHealth(nullptr,nullptr) == scalePotency(*item,baseHeal);
            powers &= item->potionGetEffectDamage(nullptr,nullptr) == scalePotency(*item,baseDamage);
            powers &= item->potionGetEffectDurationMinimum(nullptr,nullptr) == (baseMinimum > 1 ? scalePotency(*item,baseMinimum) : baseMinimum);
            powers &= item->potionGetEffectDurationMaximum(nullptr,nullptr) == (baseMaximum > 1 ? scalePotency(*item,baseMaximum) : baseMaximum);
            powers &= item->potionGetCursedEffectDurationMinimum(nullptr,nullptr) == cursedMinimum && item->potionGetCursedEffectDurationMaximum(nullptr,nullptr) == cursedMaximum;
            auto surface = players[player]->worldUI.worldTooltipItem.blitItemWorldTooltip(item);
            const auto bonuses = summary(*item);
            const auto& worldSummary = players[player]->worldUI.worldTooltipItem.statSummary;
            visuals &= surface && (bonuses.empty() || worldSummary.find(bonuses) != std::string::npos);
            visuals &= worldSummary.find("no bonus rolls") == std::string::npos
                && worldSummary.find("standard loot") == std::string::npos
                && worldSummary.find("equipment roll") == std::string::npos;
            if (surface && surface->format->BytesPerPixel == 4 && SDL_LockSurface(surface) == 0) {
                const auto position = players[player]->worldUI.worldTooltipItem.itemFrame->getSize();
                Uint32 pixel = 0;
                memcpy(&pixel, static_cast<Uint8*>(surface->pixels) + position.y * surface->pitch + (position.x + position.w / 2) * 4, 4);
                Uint8 red, green, blue, alpha, expectedRed, expectedGreen, expectedBlue, expectedAlpha;
                SDL_GetRGBA(pixel, surface->format, &red, &green, &blue, &alpha);
                getColor(color(rarity), &expectedRed, &expectedGreen, &expectedBlue, &expectedAlpha);
                tooltipRims &= red == expectedRed && green == expectedGreen && blue == expectedBlue && alpha == expectedAlpha;
                SDL_UnlockSurface(surface);
            } else tooltipRims = false;
            updateSlotFrameFromItem(&frame,item);
            auto rim = frame.findFrame("rarity rim frame");
            auto well = frame.findImage("rarity well img");
            visuals &= rim && !rim->isDisabled() && !frame.findFrame("rarity banner frame") && !frame.findFrame("rarity haze frame");
            visuals &= well && !well->disabled && well->color == color(rarity)
                && rim && rim->getImages().size() == 4 && rim->findImage("top")->color == color(rarity);
            for (auto slot : {&compactFrame, &frame}) {
                item->beatitude = 2;
                updateSlotFrameFromItem(slot,item);
                blessedBackgrounds &= slot->findFrame("beatitude status frame")->isDisabled();
                blessedBackgrounds &= !slot->findFrame("rarity rim frame")->isDisabled()
                    && slot->findFrame("rarity rim frame")->findImage("top")->color == color(rarity);
                item->beatitude = -2;
                updateSlotFrameFromItem(slot,item);
                blessedBackgrounds &= !slot->findFrame("beatitude status frame")->isDisabled();
                const auto artwork = slot->findFrame("item sprite frame")->getSize();
                for (const auto edge : slot->findFrame("rarity rim frame")->getImages()) unobscured &= !SDL_HasIntersection(&artwork, &edge->pos);
                unobscured &= slot->getFrames()[2] == slot->findFrame("item sprite frame")
                    && artwork.w == players[player]->inventoryUI.getItemSpriteSize()
                    && slot->findFrame("item sprite frame")->getImages()[0]->color == 0xffffffff;
            }
            if (type == IRON_SWORD && surface) IMG_SavePNG(surface, ("rarity-hover-"+std::to_string(index)+".png").c_str());
            if (index > 0) {
                const auto beforeRolls = summary(*clone);
                std::unordered_set<Uint32> used{item->appearance};
                Item::itemFindUniqueAppearance(clone,used);
                unique &= tier(*clone) == rarity && itemModel(clone) == originalModel && clone->appearance != item->appearance;
                unique &= summary(*clone) == beforeRolls && seed(*clone) == seed(*item);
                assign(*clone,Tier::Grey);
                stacks &= itemCompare(item,clone,false) != 0;
            }
            free(clone);
        }
        free(item);
    }
    bool distributions = true;
    for (int floor = 1; floor <= 3; ++floor) {
        int counts[5]{};
        int flawless=0;
        for (unsigned int value = 0; value < 10000; ++value) {
            ++counts[static_cast<int>(roll(floor,value))];
            Item gem{}; gem.type=GEM_DIAMOND; gem.status=EXCELLENT;
            rollLoot(&gem,floor,value,value);
            distributions &= tier(gem)==roll(floor,value);
            if (gem.status==EXCELLENT) ++flawless;
        }
        distributions &= flawless==weights[floor-1][3]+weights[floor-1][4];
        for (int index = 0; index < 5; ++index) distributions &= counts[index] == weights[floor-1][index];
        printlog("[SMOKE][DELVE] rarity_odds floor=%d grey=%d white=%d blue=%d purple=%d legendary=%d",floor,counts[0],counts[1],counts[2],counts[3],counts[4]);
    }
    Item functional{}; functional.type=MAGICSTAFF_SCEPTER; functional.appearance=123;
    assign(functional,Tier::Legendary);
    check(functional.appearance==123 && tier(functional)==Tier::Grey,"rarity_functional_appearance_excluded");
    functional.type=POTION_EMPTY;
    assign(functional,Tier::Legendary);
    check(functional.appearance==123 && tier(functional)==Tier::Grey,"rarity_empty_bottles_excluded");
    bool gemQuality = true;
    for (auto type : {GEM_DIAMOND,GEM_JEWEL,GEM_RUBY,GEM_EMERALD}) {
        for (int quality = BROKEN; quality <= EXCELLENT; ++quality) {
            Item gem{}; gem.type=type; gem.status=static_cast<Status>(quality);
            gemQuality &= tier(gem)==static_cast<Tier>(std::max(0,quality-1));
            gem.appearance=signature | (static_cast<Uint32>(Tier::Blue)<<tierShift);
            gemQuality &= tier(gem)==static_cast<Tier>(std::max(0,quality-1));
        }
    }
    check(gemQuality,"rarity_gem_quality_flawless_is_rare");
    bool equipmentQuality = true, consumableQuality = true;
    for (int type = 0; type < RMF_FIRST_ITEM; ++type) {
        auto item = newItem(static_cast<ItemType>(type),SERVICABLE,-2,1,123,true,nullptr);
        if (equipment(*item)) {
            for (int index = 0; index < 5; ++index) {
                assign(*item,static_cast<Tier>(index),73);
                equipmentQuality &= item->status == static_cast<Status>(std::min(index+1,4))
                    && tier(*item) == static_cast<Tier>(index) && item->beatitude == -2;
                const auto rolls = affixes(*item);
                item->status = WORN;
                const auto wornRolls = affixes(*item);
                equipmentQuality &= tier(*item) == static_cast<Tier>(index);
                for (size_t roll = 0; roll < rolls.size(); ++roll)
                    equipmentQuality &= rolls[roll].kind == wornRolls[roll].kind && rolls[roll].value == wornRolls[roll].value;
            }
        } else if (itemCategory(item) != GEM && eligible(*item)) {
            for (int index = 0; index < 5; ++index) {
                assign(*item,static_cast<Tier>(index),73);
                consumableQuality &= item->status == SERVICABLE && item->beatitude == -2;
            }
        }
        free(item);
    }
    check(equipmentQuality,"rarity_equipment_roll_quality_and_stable_wear_affixes");
    check(consumableQuality,"rarity_consumable_condition_unchanged");
    bool concise = true, gemPrices = true;
    for (auto type : cases) {
        auto item = newItem(type,SERVICABLE,0,1,123,true,nullptr);
        for (int index = 0; index < 5; ++index) {
            assign(*item,static_cast<Tier>(index),1234);
            const auto text = summary(*item);
            concise &= text.empty() || (text.front() != '\n' && text.back() != '\n');
            concise &= text.find("equipment roll") == std::string::npos && text.find("gold each") == std::string::npos;
            if (index == 0 || (equipment(*item) && index == 1)) concise &= text.empty();
        }
        free(item);
    }
    for (int type = 0; type < NUMITEMS; ++type) {
        if (items[type].category != GEM) continue;
        auto gem = newItem(static_cast<ItemType>(type),DECREPIT,0,1,123,true,nullptr);
        int previous = 0;
        for (int index = 0; index < 5; ++index) {
            assign(*gem,static_cast<Tier>(index),1234);
            const int value = gem->getGoldValue();
            gemPrices &= value >= previous && gem->sellValue(player) <= value && gem->buyValue(player) >= value;
            if (index < 3) {
                const int caps[] = {15,40,100};
                const int percentages[] = {5,10,25};
                const int nativeValue = type == GEM_JEWEL ? (250 << index) : items[type].gold_value;
                gemPrices &= value == (nativeValue <= 0 ? 0 : std::clamp(nativeValue * percentages[index] / 100,1,caps[index]));
                gem->identified = false;
                gem->beatitude = 5;
                gemPrices &= gem->sellValue(player) <= value;
                gem->identified = true;
                gem->beatitude = 0;
            } else {
                const int nativeValue = type == GEM_JEWEL ? 2000 : items[type].gold_value;
                gemPrices &= value == nativeValue * (index == 3 ? 250 : 600) / 100;
            }
            previous = value;
        }
        free(gem);
    }
    check(concise,"rarity_tooltips_only_actual_bonuses");
    check(gemPrices,"rarity_low_gem_shop_and_resale_caps");
    check(metadata,"rarity_native_copy_metadata"); check(visuals,"rarity_models_hover_inventory_colors");
    check(tooltipRims,"rarity_tooltip_rim_pixels_all_tiers");
    check(unobscured,"rarity_rims_never_overlap_native_artwork");
    check(blessedBackgrounds,"rarity_blessed_background_removed");
    check(powers,"rarity_native_attack_armor_value"); check(stacks,"rarity_cross_tier_stacking_rejected");
    check(unique,"rarity_unique_appearance_retains_tier"); check(distributions,"rarity_exact_floor_distributions");
    auto bankItem=newItem(IRON_SWORD,EXCELLENT,2,1,123,true,nullptr);
    assign(*bankItem,Tier::Legendary);
    auto bank=GearLoadout::capture(player);
    bank.inventory.emplace_back(bankItem->type,bankItem->status,bankItem->appearance,bankItem->beatitude,1,true,0,0);
    const auto filename="rarity-bank-"+std::to_string(player)+".json";
    GearLoadout::Loadout restored; std::string error;
    bool banked=GearLoadout::writeProfile(filename,bank,error);
    banked &= GearLoadout::readStartingLoadout(filename,"",restored,error)==GearLoadout::LoadResult::Ready;
    banked &= !restored.inventory.empty() && restored.inventory.back().appearance==bankItem->appearance;
    check(banked,"rarity_native_bank_write_read_roundtrip"); free(bankItem);
}
void rarityConsumableAudit() {
    using namespace DelveRarity;
    auto source=players[0]->entity, target=players[1]->entity;
    const int oldHP=stats[0]->HP;
    auto potion=newItem(POTION_HEALING,EXCELLENT,0,2,123,true,nullptr);
    assign(*potion,Tier::Legendary);
    source->setHP(1);
    const int heal=potion->potionGetEffectHealth(source,stats[0])+2*std::max(0,statGetCON(stats[0],source));
    item_PotionHealing(potion,source,source);
    check(stats[0]->HP==std::min(stats[0]->MAXHP,1+heal) && potion && potion->count==1
        && tier(*potion)==Tier::Legendary,"rarity_potion_native_drink_healing_consumption");
    free(potion); source->setHP(oldHP);
    auto trapItem=newItem(TOOL_BEARTRAP,EXCELLENT,0,2,123,true,nullptr);
    assign(*trapItem,Tier::Purple);
    const auto appearance=trapItem->appearance;
    auto trap=item_ToolBeartrap(trapItem,source);
    check(trap && trap->skill[14]==static_cast<Sint32>(appearance) && trapItem && trapItem->count==1,"rarity_trap_native_deploy_metadata");
    free(trapItem);
    if (!trap) return;
    const int targetHP=stats[1]->HP, targetMaxHP=stats[1]->MAXHP;
    const int bleed=stats[1]->EFFECTS_TIMERS[EFF_BLEEDING];
    const bool bleeding=stats[1]->getEffectActive(EFF_BLEEDING);
    stats[1]->MAXHP=1000; target->setHP(1000);
    trap->x=target->x; trap->y=target->y;
    actBeartrap(trap);
    check(stats[1]->HP==1000-22*175/100 && stats[1]->EFFECTS_TIMERS[EFF_BLEEDING]>=300*175/100,
        "rarity_trap_native_trigger_damage_bleeding");
    const auto oldSelected=selectedEntity[0]; const bool oldRange=inrange[0];
    selectedEntity[0]=trap; inrange[0]=true;
    const auto uid=trap->getUID();
    actBeartrap(trap);
    selectedEntity[0]=oldSelected; inrange[0]=oldRange;
    bool recovered=false;
    for (auto node=map.entities->first;node;node=node->next) {
        auto entity=static_cast<Entity*>(node->element);
        if(entity->behavior==&actItem && entity->skill[10]==TOOL_BEARTRAP && static_cast<Uint32>(entity->skill[14])==appearance) {
            auto item=newItemFromEntity(entity);
            recovered=item && tier(*item)==Tier::Purple;
            free(item); list_RemoveNode(entity->mynode); break;
        }
    }
    check(!uidToEntity(uid) && recovered,"rarity_trap_native_recovery_retains_tier");
    stats[1]->MAXHP=targetMaxHP; target->setHP(targetHP);
    target->setEffect(EFF_BLEEDING,bleeding,bleed,false);
    target->setEffect(EFF_PARALYZED,false,0,false);
    DelvePassives::reset();
}
void rarityCombatAudit() {
    using namespace DelveRarity;
    auto source = players[0]->entity, target = players[1]->entity;
    auto weapon = stats[0]->weapon, shield = stats[0]->shield, armor = stats[1]->breastplate;
    auto sword = newItem(IRON_SWORD,EXCELLENT,0,1,123,true,nullptr);
    auto book = newItem(SPELLBOOK_FIREBALL,EXCELLENT,0,1,123,true,nullptr);
    auto plate = newItem(STEEL_BREASTPIECE,EXCELLENT,0,1,123,true,nullptr);
    assign(*sword,Tier::Legendary); assign(*book,Tier::Purple); assign(*plate,Tier::Legendary);
    stats[0]->weapon=sword; stats[0]->shield=nullptr;
    stats[0]->HP=stats[0]->MAXHP-20; stats[0]->MP=std::max(0,stats[0]->MAXMP-10);
    stats[1]->HP=stats[1]->MAXHP; DelvePassives::reset();
    const int hp=stats[0]->HP, mp=stats[0]->MP;
    { DelvePassives::Scope action(source,DelvePassives::Channel::Melee); target->modHP(-1); }
    check(stats[0]->HP==hp+4 && stats[0]->MP==mp+2,"rarity_legendary_siphon_native_health_mana");
    { DelvePassives::Scope action(source,DelvePassives::Channel::Melee); target->modHP(-1); }
    check(stats[0]->HP==hp+4 && stats[0]->MP==mp+2,"rarity_siphon_cooldown_blocks_repeat");
    DelvePassives::tick(TICKS_PER_SECOND);
    { DelvePassives::Scope action(source,DelvePassives::Channel::Melee); target->modHP(-1); }
    check(stats[0]->HP==hp+8,"rarity_siphon_exact_one_second_recharge");
    stats[0]->weapon=nullptr; stats[0]->shield=book;
    int before=stats[1]->HP;
    { DelvePassives::Scope action(source,DelvePassives::Channel::Spell); target->modHP(-1); }
    check(before-stats[1]->HP==1,"rarity_spell_focus_has_no_flat_rarity_damage");
    stats[1]->breastplate=plate; before=stats[1]->HP;
    { DelvePassives::Scope action(source,DelvePassives::Channel::Melee); target->modHP(-20); }
    check(before-stats[1]->HP==17,"rarity_legendary_aegis_native_damage");
    stats[0]->weapon=weapon; stats[0]->shield=shield; stats[1]->breastplate=armor;
    source->setHP(stats[0]->MAXHP); target->setHP(stats[1]->MAXHP);
    free(sword); free(book); free(plate); DelvePassives::reset();
}
void rarityHost() {
    if (!allLoaded() || !::Raid::smokeWorldReady()) return;
    for (auto node=map.creatures->first; node; node=node->next) {
        auto entity=static_cast<Entity*>(node->element);
        if(entity->behavior==&actMonster)entity->setEffect(EFF_ASLEEP,true,100000,true);
    }
    if (!phase) {
        rarityAudit(0); rarityCombatAudit(); rarityConsumableAudit(); affixAudit();
        stats[1]->setProficiency(PRO_SORCERY, 100);
        stats[1]->MP = stats[1]->MAXMP = 100;
        check(players[0]->entity->checkEnemy(players[1]->entity),"rarity_ffa_enforced");
        for (int index=0; index<5; ++index) {
            for (auto type : {GEM_JEWEL,POTION_HEALING,TOOL_BEARTRAP}) {
                auto item=newItem(type,EXCELLENT,0,1,123,true,nullptr);
                DelveRarity::assign(*item,static_cast<DelveRarity::Tier>(index));
                itemPickup(1,item); free(item);
            }
            auto item=newItem(IRON_SWORD,EXCELLENT,2,1,123,true,nullptr);
            DelveRarity::assign(*item,static_cast<DelveRarity::Tier>(index));
            itemPickup(1,item); free(item);
        }
        auto item=newItem(IRON_SWORD,EXCELLENT,1,1,123,true,nullptr);
        DelveRarity::assign(*item,DelveRarity::Tier::Legendary);
        std::ofstream(sync+"/affix-ground-summary") << DelveRarity::summary(*item);
        auto drop=dropItemMonster(item,players[0]->entity,nullptr);
        if (!drop) { check(false,"rarity_world_fixture_created"); return; }
        bool placed=false;
        for(int horizontal=8;horizontal<50 && !placed;++horizontal)for(int vertical=8;vertical<50 && !placed;++vertical) {
            const int offset=MAPLAYERS*(vertical+horizontal*map.height), floor=map.tiles[offset];
            if(!floor || map.tiles[offset+OBSTACLELAYER] || swimmingtiles[floor] || lavatiles[floor])continue;
            if(checkObstacle(horizontal*16+8,vertical*16+8,players[1]->entity,drop))continue;
            if(checkObstacle((horizontal-1)*16+8,vertical*16+8,players[1]->entity,drop))continue;
            if(players[1]->entity->teleporterMove(horizontal-2,vertical,2)) {
                players[1]->entity->yaw = 0;
                players[1]->entity->pitch = 0.15;
                drop->x=horizontal*16+8; drop->y=vertical*16+8; placed=true;
            }
        }
        check(placed,"rarity_fixture_native_coordinate_range");
        drop->vel_x=drop->vel_y=drop->vel_z=0; lootUID=drop->getUID();
        drop->flags[INVISIBLE] = false;
        sendEntityUDP(drop,1,true);
        std::ofstream fixtures(sync+"/rarity-consumable-ground");
        int offset=0;
        for (auto type : {GEM_JEWEL,POTION_ACID,TOOL_BEARTRAP,GEM_DIAMOND}) {
            auto item=newItem(type,EXCELLENT,0,1,123,true,nullptr);
            DelveRarity::assign(*item,type==GEM_DIAMOND ? DelveRarity::Tier::Grey : DelveRarity::Tier::Purple);
            if (auto fixture=dropItemMonster(item,players[0]->entity,nullptr)) {
                fixture->x=drop->x; fixture->y=drop->y+8+offset;
                fixture->vel_x=fixture->vel_y=fixture->vel_z=0;
                fixture->flags[INVISIBLE]=false;
                sendEntityUDP(fixture,1,true);
                fixtures << fixture->getUID() << ' ';
            }
            offset+=8;
        }
        std::ofstream(sync+"/rarity-ground") << lootUID;
        phase=1; began=ticks;
    } else if (phase==1) {
        if (!std::filesystem::exists(sync + "/spellbook-network-confirmed")) {
            int expectedSeed = -1;
            std::ifstream(sync + "/spellbook-cast-expected") >> expectedSeed;
            if (expectedSeed >= 0) for (auto node = map.entities->first; node; node = node->next) {
                auto missile = static_cast<Entity*>(node->element);
                if (missile->behavior != &actMagicMissile || missile->parent != players[1]->entity->getUID()
                    || !missile->children.first) continue;
                auto spell = static_cast<spell_t*>(missile->children.first->element);
                if (spell->ID != SPELL_FIREBALL) continue;
                check(spell->delveImprint.bookType == SPELLBOOK_FIREBALL && spell->delveImprint.tier == 4
                    && spell->delveImprint.seed == expectedSeed && spell->delveImprint.blessed,
                    "spellbook_guest_cast_exact_snapshot_on_host");
                std::ofstream(sync + "/spellbook-network-confirmed") << 1;
                break;
            }
        }
        if (auto drop=uidToEntity(lootUID)) sendEntityUDP(drop,1,true);
        if (std::filesystem::exists(sync+"/rarity-guest-ground") && std::filesystem::exists(sync+"/spellbook-network-confirmed")) {
            if (auto drop=uidToEntity(lootUID)) publish(1,drop);
            phase=2;
        }
    } else if (phase==2 && std::filesystem::exists(sync+"/rarity-guest-equipped")) {
        if (stats[1]->weapon && DelveRarity::tier(*stats[1]->weapon)==DelveRarity::Tier::Legendary) {
            check(!uidToEntity(lootUID),"rarity_native_ground_pickup_complete");
            check(true,"rarity_guest_equipment_server_synchronized");
            std::ofstream(sync+"/rarity-drop") << 1; phase=3; began=ticks;
        }
    } else if (phase==3 && ticks-began>2*TICKS_PER_SECOND) {
        bool dropped=false;
        for (auto node=map.entities->first;node;node=node->next) {
            auto entity=static_cast<Entity*>(node->element);
            if(entity->behavior!=&actItem || entity->skill[10]!=IRON_SWORD)continue;
            Item item{}; item.type=IRON_SWORD; item.appearance=static_cast<Uint32>(entity->skill[14]);
            dropped |= DelveRarity::tier(item)==DelveRarity::Tier::Legendary;
        }
        check(dropped,"rarity_guest_drop_roundtrip");
        check(true,"rarity_host_complete");
        stats[0]->HP=stats[1]->HP=0; phase=4; began=ticks;
    } else if (phase==4 && ticks-began>TICKS_PER_SECOND) {
        stats[0]->HP=stats[1]->HP=0; began=ticks;
    }
}
bool rarityGuest() {
    if (!players[clientnum]->entity) return false;
    static int stage=0;
    Uint32 uid=0; std::ifstream(sync+"/rarity-ground") >> uid;
    if (!uid) return false;
    if (stage==0) {
        auto ground=uidToEntity(uid);
        if (!ground || !ground->itemReceivedDetailsFromServer) return false;
        Entity* smoke = nullptr;
        for (auto node = map.entities->first; node; node = node->next) {
            auto entity = static_cast<Entity*>(node->element);
            if (entity->behavior == &actDelveLootSmoke && entity->parent == uid && entity->ticks > 10) smoke = entity;
        }
        if (!smoke) return false;
        std::ifstream fixtures(sync+"/rarity-consumable-ground");
        bool worldRarity=true;
        for (int index=0;index<4;++index) {
            Uint32 fixtureUID=0; fixtures >> fixtureUID;
            auto fixture=uidToEntity(fixtureUID);
            if (!fixture || !fixture->itemReceivedDetailsFromServer) return false;
            auto item=newItemFromEntity(fixture,true);
            if (!item) return false;
            bool hasSmoke=false;
            for (auto node=map.entities->first;node;node=node->next) {
                auto puff=static_cast<Entity*>(node->element);
                if(puff->behavior==&actDelveLootSmoke && puff->parent==fixtureUID && puff->ticks>5)hasSmoke=true;
            }
            const auto expected=index==3 ? DelveRarity::Tier::Grey : DelveRarity::Tier::Purple;
            worldRarity &= DelveRarity::tier(*item)==expected && (index==3 ? !hasSmoke : hasSmoke);
            free(item);
        }
        if (!worldRarity) return false;
        std::ofstream(sync+"/rarity-smoke-ready") << 1;
        if (!std::filesystem::exists(sync+"/rarity-smoke-captured")) return false;
        auto item=newItemFromEntity(ground,true);
        if(!item)return false;
        check(DelveRarity::tier(*item)==DelveRarity::Tier::Legendary,"rarity_guest_itmu_full_appearance");
        std::ifstream expectedRolls(sync+"/affix-ground-summary");
        std::string expectedSummary((std::istreambuf_iterator<char>(expectedRolls)),std::istreambuf_iterator<char>());
        check(!expectedSummary.empty() && DelveRarity::summary(*item)==expectedSummary,"affix_guest_exact_host_rolls_networked");
        check(worldRarity,"rarity_guest_gem_potion_trap_world_smoke_and_grey_exclusion");
        check(smoke->z < ground->z - 1 && smoke->scalex < .13
            && smoke->fskill[1] > 0 && smoke->skill[0] == 4
            && smoke->flags[NOUPDATE] && smoke->flags[PASSABLE]
            && !ground->light,"rarity_guest_rising_smoke_pillar"); free(item);
        rarityAudit(clientnum);
        bool tiers[5]{};
        for(auto node=stats[clientnum]->inventory.first;node;node=node->next) {
            auto item=static_cast<Item*>(node->element);
            if(item->type==IRON_SWORD)tiers[static_cast<int>(DelveRarity::tier(*item))]=true;
        }
        check(std::all_of(tiers,tiers+5,[](bool value){return value;}),"rarity_guest_inventory_all_tiers_networked");
        bool consumables=true;
        for (auto type : {GEM_JEWEL,POTION_HEALING,TOOL_BEARTRAP}) {
            bool received[5]{};
            for (auto node=stats[clientnum]->inventory.first;node;node=node->next) {
                auto item=static_cast<Item*>(node->element);
                if(item->type==type)received[static_cast<int>(DelveRarity::tier(*item))]=true;
            }
            consumables &= std::all_of(received,received+5,[](bool value){return value;});
        }
        check(consumables,"rarity_guest_gems_potions_traps_all_tiers_networked");
        std::ofstream(sync+"/rarity-guest-ground") << 1; stage=1;
    }
    int legendary=0; Item* weapon=nullptr;
    for(auto node=stats[clientnum]->inventory.first;node;node=node->next) {
        auto item=static_cast<Item*>(node->element);
        if(item->type==IRON_SWORD && DelveRarity::tier(*item)==DelveRarity::Tier::Legendary) { legendary+=item->count; weapon=item; }
    }
    if(stage==1 && legendary>=2 && !uidToEntity(uid)) {
        bool cleaned = true;
        for (auto node = map.entities->first; node; node = node->next) {
            auto entity = static_cast<Entity*>(node->element);
            if (entity->behavior == &actDelveLootSmoke && entity->parent == uid) cleaned = false;
        }
        if (!cleaned) return false;
        check(cleaned,"rarity_smoke_cleans_up_after_pickup");
        check(true,"rarity_guest_pickup_preserves_tier"); useItem(weapon,clientnum);
        std::ofstream(sync+"/rarity-guest-equipped") << 1; stage=2;
    } else if(stage==2 && std::filesystem::exists(sync+"/rarity-drop") && weapon) {
        dropItem(weapon,clientnum,false,true); stage=3;
        check(true,"rarity_guest_complete"); return false;
    }
    return stage==1;
}
