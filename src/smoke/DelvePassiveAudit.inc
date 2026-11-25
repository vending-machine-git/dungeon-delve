void smokeAudit(Entity& host, Entity& guest, const std::function<void(const std::string&, bool)>& check) {
    auto savedStates = states;
    auto savedAction = current;
    unsigned int savedSequence = sequence;
    auto savedWeapon = stats[0]->weapon;
    auto savedShield = stats[0]->shield;
    bool savedDefending = stats[0]->defending;
    auto clearEffects = [](Entity& entity) {
        for (int effect = 0; effect < NUMEFFECTS; ++effect) entity.setEffect(effect, false, 0, false);
        entity.flags[BURNING] = false;
    };
    auto prepare = [&](int characterClass, int perkIndex, int second = -1) {
        states = {}; current = {}; processing = false;
        DelveBuilds::smokeEquip(0, characterClass, perkIndex, second);
        DelveBuilds::smokeEquip(1, CLASS_WARRIOR, -1);
        stats[0]->HP = 250; stats[0]->MAXHP = 500; stats[0]->MP = 250; stats[0]->MAXMP = 500;
        stats[1]->HP = 900; stats[1]->MAXHP = 1000;
        stats[0]->weapon = savedWeapon; stats[0]->shield = savedShield; stats[0]->defending = false;
        clearEffects(host); clearEffects(guest); Raid::smokeResetControl();
        states[0].floor = states[1].floor = currentlevel;
        states[0].guardX = host.x; states[0].guardY = host.y;
    };
    auto strike = [&](Channel channel = Channel::Melee, int detail = -1, int paid = 0, unsigned int token = 0) {
        Scope action(&host, channel, detail, token, paid);
        int previousHP = stats[1]->HP;
        guest.modHP(-8);
        return previousHP - stats[1]->HP;
    };
    auto useSpell = [&](int spell, int paid = 8) {
        host.modMP(-paid);
        Scope action(&host, DelveBuilds::isAbilitySpell(spell) ? Channel::Ability : Channel::Spell, spell, 0, paid);
        action.commit();
    };
    auto drink = [&](int type) { Scope action(&host, Channel::Drink, type); action.commit(); };
    auto incoming = [&](int damage = 8) { Scope action(&guest, Channel::Melee); host.modHP(-damage); };
    Entity* fixture = newEntity(-1, 1, map.entities, nullptr);
    fixture->behavior = &actChest; fixture->x = host.x; fixture->y = host.y;
    auditing = true;
    for (int characterClass = 0; characterClass < 26; ++characterClass) {
        for (int perkIndex = 0; perkIndex < 6; ++perkIndex) {
            prepare(characterClass, perkIndex);
            const auto& definition = rule(characterClass, perkIndex);
            const auto effectCount = DelveEffects::smokeCount(0, perkIndex);
            auto& state = states[0]; auto& slot = state.slots[0];
            bool triggered = false;
            int ability = 600 + characterClass * 4 + std::max(0, definition.filter - 6);
            switch (definition.kind) {
                case Kind::WoundedBlock:
                    blocked(host, guest, 4); check("passive_wounded_block_half_hp_rejected", !slot.charges);
                    stats[0]->HP = 200; blocked(host, guest, 4);
                    triggered = slot.charges == 1 && slot.target == guest.getUID();
                    ++slot.target; strike(); check("passive_wounded_counter_wrong_target", slot.charges == 1);
                    --slot.target; strike(); triggered &= !slot.charges; break;
                case Kind::DuelCounter:
                    blocked(host, guest, 4); triggered = slot.charges == 1 && slot.window == definition.duration;
                    ++slot.target; strike(); check("passive_duel_counter_wrong_target", slot.charges == 1);
                    --slot.target; strike(); triggered &= !slot.charges; break;
                case Kind::UnarmedCounter:
                    stats[0]->weapon = nullptr; blocked(host, guest, 4); triggered = slot.charges == 1;
                    { Item armed{}; armed.type = IRON_SWORD; stats[0]->weapon = &armed; strike(); }
                    check("passive_unarmed_counter_armed_hit_rejected", slot.charges == 1);
                    stats[0]->weapon = nullptr; strike(); triggered &= !slot.charges;
                    stats[0]->weapon = savedWeapon; break;
                case Kind::Crossfire:
                    strike(Channel::Ranged); check("passive_crossfire_requires_spell", !slot.cooldown);
                    strike(Channel::Spell, SPELL_FORCEBOLT, 8); ++slot.target; strike(Channel::Ranged);
                    check("passive_crossfire_requires_same_target", !slot.cooldown);
                    --slot.target; strike(Channel::Ranged); triggered = state.barrier == definition.value; break;
                case Kind::Variety:
                    strike(); strike(); check("passive_variety_repeats_do_not_advance", slot.stacks == 1);
                    strike(Channel::Thrown); strike(Channel::Spell, SPELL_FORCEBOLT, 8);
                    triggered = state.hasteTime == definition.duration && slot.cooldown > 0; break;
                case Kind::ControlledKill:
                    guest.setEffect(EFF_FEAR, true, 100, false); stats[1]->HP = 1; strike();
                    triggered = state.barrier == definition.value; break;
                case Kind::ThrownRear: {
                    double yaw = guest.yaw;
                    guest.yaw = std::atan2(host.y - guest.y, host.x - guest.x) + PI;
                    strike(); check("passive_shadow_route_melee_rejected", !slot.cooldown);
                    strike(Channel::Thrown); guest.yaw = yaw; triggered = state.hasteTime == definition.duration; break;
                }
                case Kind::SummonEcho: {
                    auto summon = summonMonster(SKELETON, host.x, host.y);
                    if (summon) {
                        summon->monsterAllyIndex = 0; summon->getStats()->leader_uid = host.getUID();
                        { Scope action(summon, Channel::Melee); guest.modHP(-8); }
                        check("passive_summon_echo_requires_cast", !slot.cooldown);
                        useSpell(SPELL_FORCEBOLT); strike();
                        check("passive_summon_echo_owner_hit_keeps_relay", slot.stacks == 1);
                        { Scope action(summon, Channel::Melee); guest.modHP(-8); }
                        triggered = state.barrier == definition.value && !slot.stacks;
                        list_RemoveNode(summon->mynode);
                    }
                    break;
                }
                case Kind::BloodHunt:
                    strike(); check("passive_blood_hunt_half_hp_rejected", !slot.cooldown);
                    stats[0]->HP = 249; strike(); triggered = stats[0]->HP == 252 && slot.cooldown > 0; break;
                case Kind::ControlledStrike:
                    check("passive_illusion_requires_control", strike() == 8 && !slot.cooldown);
                    guest.setEffect(EFF_CONFUSED, true, 100, false); triggered = strike() > 8 && slot.cooldown > 0; break;
                case Kind::Execution:
                    check("passive_execution_requires_wounded_target", strike() == 8 && !slot.cooldown);
                    stats[1]->HP = 200; triggered = strike() > 8 && slot.cooldown > 0; break;
                case Kind::PatientStrike:
                    check("passive_patient_strike_requires_quiet", strike() == 8 && !slot.cooldown);
                    tick(10 * TICKS_PER_SECOND); triggered = strike() > 8 && slot.cooldown > 0; break;
                case Kind::RangedKill:
                    stats[1]->HP = 1; strike(Channel::Ranged); triggered = stats[0]->HP == 254 && slot.cooldown > 0; break;
                case Kind::Refrain:
                    useSpell(600 + characterClass * 4); useSpell(601 + characterClass * 4);
                    check("passive_refrain_requires_melee_bridge", !slot.cooldown);
                    strike(); useSpell(600 + characterClass * 4); triggered = state.barrier == definition.value; break;
                case Kind::HealingOath:
                    { Scope action(&host, Channel::Spell, SPELL_HEALING, 0, 8); host.modHP(10); }
                    triggered = slot.charges == 1; strike(); triggered &= stats[0]->HP == 263 && !slot.charges; break;
                case Kind::BlockCharge: case Kind::BlockHaste:
                    blocked(host, guest, 0); check("passive_empty_block_" + std::to_string(characterClass) + "_" + std::to_string(perkIndex), !slot.charges && !slot.cooldown);
                    blocked(host, guest, 4); triggered = slot.cooldown > 0 || slot.charges > 0; break;
                case Kind::ComboSwitch:
                    strike(); strike(); triggered = slot.stacks == 2 && slot.target == guest.getUID(); break;
                case Kind::WeaponWeave: {
                    Item first{}; first.type = IRON_SWORD; stats[0]->weapon = &first; strike();
                    Item second{}; second.type = IRON_MACE; stats[0]->weapon = &second; strike();
                    stats[0]->weapon = savedWeapon; triggered = slot.cooldown > 0; break;
                }
                case Kind::Guard:
                    stats[0]->defending = true; tick(TICKS_PER_SECOND); triggered = slot.charges == 1;
                    incoming(8); triggered &= stats[0]->HP == 248 && !slot.charges; break;
                case Kind::AbilityMark:
                    strike(Channel::Ability, ability, 8); triggered = slot.charges == 1 && slot.target == guest.getUID();
                    strike(characterClass == CLASS_MACHINIST ? Channel::Ranged : Channel::Melee);
                    triggered &= !slot.charges; break;
                case Kind::Ambush:
                    host.setEffect(EFF_INVISIBLE, true, 100, false); tick(1);
                    host.setEffect(EFF_INVISIBLE, false, 0, false); tick(1); triggered = slot.charges == 1;
                    strike(); triggered &= !slot.charges; break;
                case Kind::RearHaste: case Kind::Rear: {
                    double yaw = guest.yaw;
                    guest.yaw = std::atan2(host.y - guest.y, host.x - guest.x) + PI;
                    strike(); guest.yaw = yaw; triggered = slot.cooldown > 0; break;
                }
                case Kind::TrapSense: {
                    auto savedSelected = selectedEntity[0]; auto savedRange = inrange[0];
                    fixture->behavior = &actSpearTrap;
                    fixture->actTrapSabotaged = 0; selectedEntity[0] = fixture; inrange[0] = true;
                    triggered = disarmTrap(*fixture) && fixture->actTrapSabotaged == 1;
                    selectedEntity[0] = savedSelected; inrange[0] = savedRange; fixture->behavior = &actChest; break;
                }
                case Kind::Unlock:
                    fixture->skill[20] = 0; fixture->chestLocked = 1; triggered = unlockChest(*fixture, 0) && !fixture->chestLocked;
                    fixture->skill[20] = 4; fixture->chestLocked = 1;
                    triggered &= !unlockChest(*fixture, 0) && fixture->chestLocked;
                    fixture->skill[20] = 0; break;
                case Kind::ThrowMark:
                    strike(Channel::Thrown); triggered = slot.charges == 1; strike(); triggered &= slot.charges == 0; break;
                case Kind::CacheEscape:
                    openedCache(*fixture, 0); triggered = slot.charges == 1; incoming(); triggered &= !slot.charges && state.hasteTime > 0; break;
                case Kind::Shatter:
                    strike(Channel::Spell, SPELL_COLD, 8); triggered = slot.charges == 1;
                    triggered &= strike() > 8 && !slot.charges; break;
                case Kind::Cadence:
                    strike(Channel::Spell, SPELL_FIREBALL, 8); strike(Channel::Spell, SPELL_FORCEBOLT, 8);
                    triggered = stats[0]->MP == 252 && slot.cooldown > 0; break;
                case Kind::Cinder:
                    strike(Channel::Spell, SPELL_FIREBALL, 8); triggered = slot.charges == 1;
                    strike(Channel::Spell, SPELL_FORCEBOLT, 8); triggered &= !slot.charges && slot.cooldown > 0; break;
                case Kind::Spellblade:
                    useSpell(SPELL_FORCEBOLT); triggered = slot.charges == 1; strike(); triggered &= state.discount == 2 && !slot.charges; break;
                case Kind::ReactiveFrost:
                    incoming(); triggered = stats[1]->getEffectActive(EFF_SLOW) && slot.cooldown > 0; break;
                case Kind::SpendReserve:
                    useSpell(SPELL_FORCEBOLT, 10); triggered = slot.charges == 1;
                    useSpell(ability, 0); triggered &= !slot.charges && state.barrier == definition.value; break;
                case Kind::Overflow: {
                    stats[0]->HP = 495;
                    Scope action(&host, Channel::Spell, SPELL_HEALING, 0, 8); host.modHP(20);
                    triggered = stats[0]->HP == 500 && state.barrier == definition.value; break;
                }
                case Kind::HealHaste: {
                    Scope action(&host, Channel::Spell, SPELL_HEALING, 0, 8); host.modHP(10);
                    triggered = slot.cooldown > 0; break;
                }
                case Kind::CleanseBarrier: {
                    host.setEffect(EFF_POISONED, true, 100, false);
                    { Scope action(&host, Channel::Spell, SPELL_CUREAILMENT, 0, 8); action.commit(); host.setEffect(EFF_POISONED, false, 0, false); }
                    triggered = slot.cooldown > 0; break;
                }
                case Kind::RecoveryStrike:
                    { Scope action(&host, Channel::Spell, SPELL_HEALING, 0, 8); host.modHP(10); }
                    triggered = slot.charges == 1; strike(); triggered &= stats[0]->MP == 252 && !slot.charges; break;
                case Kind::Dressing:
                    host.setEffect(EFF_BLEEDING, true, 100, false); drink(POTION_HEALING);
                    triggered = !stats[0]->getEffectActive(EFF_BLEEDING) && state.barrier == definition.value; break;
                case Kind::Stabilize:
                    stats[0]->HP = 155; incoming(10); triggered = state.healing == 6 && slot.cooldown > 0; break;
                case Kind::DrinkCharge:
                    drink(POTION_HEALING); triggered = slot.charges == 1; strike(); triggered &= !slot.charges; break;
                case Kind::PotionBarrier:
                    strike(Channel::Potion); triggered = state.barrier == definition.value; break;
                case Kind::CleanDrink:
                    host.setEffect(EFF_POISONED, true, 100, false); drink(POTION_HEALING);
                    triggered = !stats[0]->getEffectActive(EFF_POISONED); break;
                case Kind::DrinkWeave:
                    drink(POTION_HEALING); drink(POTION_RESTOREMAGIC); triggered = state.hasteTime > 0; break;
                case Kind::Recipe:
                    useSpell(ability); triggered = slot.charges == 1; strike(Channel::Potion); triggered &= !slot.charges; break;
                case Kind::Distill:
                    openedCache(*fixture, 0); triggered = slot.charges == 1; drink(POTION_RESTOREMAGIC);
                    triggered &= !slot.charges && stats[0]->MP == 252;
                    openedCache(*fixture, 0); triggered &= !slot.charges; break;
                case Kind::Kill:
                    stats[1]->HP = 1; strike(); triggered = slot.cooldown > 0; break;
                case Kind::Cache:
                    openedCache(*fixture, 0); triggered = slot.cooldown > 0; break;
                case Kind::Ability:
                    useSpell(ability); triggered = slot.cooldown > 0; break;
                case Kind::ControlEnd:
                    host.setEffect(EFF_FEAR, true, 100, false); tick(1); host.setEffect(EFF_FEAR, false, 0, false); tick(1);
                    triggered = slot.cooldown > 0; break;
                case Kind::MeleeCombo:
                    if (definition.filter == 1) stats[0]->weapon = nullptr;
                    strike(); strike(); strike(); triggered = slot.cooldown > 0; stats[0]->weapon = savedWeapon; break;
                case Kind::AbilityWeave:
                    useSpell(600 + characterClass * 4); useSpell(601 + characterClass * 4); triggered = slot.cooldown > 0; break;
                case Kind::Explore:
                    tick(1); triggered = slot.cooldown > 0; break;
                case Kind::Quiet:
                    tick(10 * TICKS_PER_SECOND); triggered = slot.cooldown > 0; break;
                case Kind::Breacher:
                    fixture->behavior = &actDoor; fixture->doorForceLockedUnlocked = 0; fixture->doorHealth = 20;
                    hitDoor(host, *fixture, Stat::getMaxAttackCharge(stats[0]));
                    triggered = fixture->doorHealth == 0 && slot.cooldown > 0; fixture->behavior = &actChest; break;
                case Kind::Ranged:
                    strike(Channel::Ranged); triggered = slot.cooldown > 0; break;
                case Kind::SpellMark:
                    useSpell(SPELL_FORCEBOLT); triggered = slot.charges == 1; strike(Channel::Ranged); triggered &= !slot.charges; break;
                case Kind::SummonBond: {
                    auto summon = summonMonster(SKELETON, host.x, host.y);
                    if (summon) {
                        summon->monsterAllyIndex = 0; summon->getStats()->leader_uid = host.getUID();
                        { Scope action(summon, Channel::Melee); guest.modHP(-8); }
                        strike(); triggered = slot.cooldown > 0; list_RemoveNode(summon->mynode);
                    }
                    break;
                }
                case Kind::SummonCast:
                    useSpell(SPELL_SUMMON); triggered = slot.cooldown > 0; break;
                default: break;
            }
            feedback(0);
            check("passive_effect_" + std::to_string(characterClass) + "_" + std::to_string(perkIndex), DelveEffects::smokeCount(0, perkIndex) > effectCount);
            check("passive_trigger_" + std::to_string(characterClass) + "_" + std::to_string(perkIndex), triggered);
            check("passive_bounds_" + std::to_string(characterClass) + "_" + std::to_string(perkIndex), state.barrier <= 10 && state.discount <= 2 && state.haste <= 10 && slot.charges <= 1);
            check("passive_cooldown_hud_" + std::to_string(characterClass) + "_" + std::to_string(perkIndex), cooldownRemaining(0, 0) == slot.cooldown);
        }
    }
    prepare(CLASS_WIZARD, 0);
    strike(Channel::Spell, SPELL_COLD, 8);
    tick(4 * TICKS_PER_SECOND);
    check("passive_mark_lasts_beyond_old_duration", states[0].slots[0].charges == 1);
    tick(TICKS_PER_SECOND);
    check("passive_mark_expires_at_extended_duration", states[0].slots[0].charges == 0);
    prepare(CLASS_WARRIOR, 1);
    blocked(host, guest, 4);
    tick(2 * TICKS_PER_SECOND);
    check("passive_haste_lasts_beyond_old_duration", states[0].hasteTime == TICKS_PER_SECOND / 2);
    tick(TICKS_PER_SECOND / 2);
    check("passive_haste_expires_at_extended_duration", states[0].hasteTime == 0);
    prepare(CLASS_HEALER, 5);
    stats[0]->HP = 155; incoming(10);
    int recoveryHP = stats[0]->HP;
    for (int elapsed = 0; elapsed < rule(CLASS_HEALER, 5).duration - 1; ++elapsed) tick(1);
    check("passive_stabilize_extended_timeline", stats[0]->HP == recoveryHP + 5 && states[0].healing == 1);
    tick(1);
    check("passive_stabilize_preserves_healing_total", stats[0]->HP == recoveryHP + 6 && !states[0].healing);
    prepare(CLASS_WARRIOR, 0, 1);
    blocked(host, guest, 5);
    check("passive_two_perk_block_build", states[0].slots[0].charges == 1 && speedMultiplier(0) == 1.1);
    int firstDamage = strike(); int secondDamage = strike();
    check("passive_one_hit_charge_only", firstDamage > secondDamage && secondDamage == 8);
    prepare(CLASS_WIZARD, 1);
    strike(Channel::Spell, SPELL_FIREBALL, 8, 123456);
    strike(Channel::Spell, SPELL_FORCEBOLT, 8, 654321);
    int afterRefund = stats[0]->MP;
    states[0].slots[0].cooldown = 0;
    strike(Channel::Spell, SPELL_FIREBALL, 8, 654321);
    check("passive_refund_once_per_cast", stats[0]->MP == afterRefund);
    prepare(CLASS_HEALER, 0, 1);
    { Scope action(&host, Channel::Spell, SPELL_HEALING, 0, 8); host.modHP(300); }
    check("passive_healing_combo_no_recursion", states[0].barrier == 10 && stats[0]->HP == 500 && states[0].hasteTime > 0);
    int maxHP = stats[0]->MAXHP;
    { Scope action(nullptr, Channel::None); host.modHP(-14); }
    check("passive_finite_barrier_not_maxhp", states[0].barrier == 0 && stats[0]->HP == 496 && stats[0]->MAXHP == maxHP);
    prepare(CLASS_WARRIOR, 0);
    gamePaused = true; blocked(host, guest, 5); tick(50);
    check("passive_pause_no_proc", states[0].slots[0].charges == 0); gamePaused = false;
    blocked(host, guest, 5); auto remaining = states[0].slots[0].cooldown;
    gamePaused = true; tick(50); check("passive_pause_freezes_cooldown", states[0].slots[0].cooldown == remaining); gamePaused = false;
    int savedFloor = currentlevel; currentlevel += 1; tick(1);
    check("passive_floor_preserves_charge_and_recovery", states[0].slots[0].charges == 1 && states[0].slots[0].cooldown == remaining - 1 && states[0].slots[0].target == 0);
    currentlevel = savedFloor;
    prepare(CLASS_WIZARD, 0);
    strike(Channel::Spell, SPELL_COLD, 8); currentlevel += 1; tick(1);
    check("passive_floor_drops_target_marks", !states[0].slots[0].charges && !states[0].slots[0].target);
    currentlevel = savedFloor;
    prepare(CLASS_BREWER, 0);
    drink(POTION_SICKNESS); check("passive_harmful_drink_not_beneficial", states[0].slots[0].charges == 0);
    prepare(CLASS_HEALER, 4);
    Item* potion = newItem(POTION_HEALING, SERVICABLE, 0, 1, 0, true, &stats[0]->inventory);
    host.setEffect(EFF_BLEEDING, true, 100, false);
    useItem(potion, 0, &host);
    check("passive_native_potion_use", !stats[0]->getEffectActive(EFF_BLEEDING) && states[0].barrier == 4);
    prepare(CLASS_WIZARD, 3);
    auto projectile = castSpell(host.getUID(), getSpellFromID(SPELL_FIREBALL), false, false, false);
    check("passive_native_paid_cast", projectile && states[0].slots[0].charges == 1);
    if (projectile) {
        auto spell = static_cast<spell_t*>(projectile->children.first->element);
        check("passive_projectile_keeps_cast_identity", spell->delveCastToken != 0 && spell->delveManaPaid > 0);
        list_RemoveNode(projectile->mynode);
    }
    prepare(CLASS_WIZARD, 0);
    auto cold = castSpell(host.getUID(), getSpellFromID(SPELL_COLD), false, false, false);
    unsigned int coldUID = cold ? cold->getUID() : 0;
    if (cold) {
        cold->x = guest.x - 8; cold->y = guest.y; cold->yaw = 0;
        cold->vel_x = 4; cold->vel_y = 0;
        for (int step = 0; step < 8; ++step) {
            auto moving = uidToEntity(coldUID);
            if (!moving) break;
            actMagicMissile(moving);
        }
    }
    check("passive_native_projectile_shatter", states[0].slots[0].charges == 1 && states[0].slots[0].target == guest.getUID());
    if (auto remaining = uidToEntity(coldUID)) list_RemoveNode(remaining->mynode);
    prepare(CLASS_WARRIOR, -1);
    DelveBuilds::smokeEquip(1, CLASS_WARRIOR, 0, 1);
    auto oldGuestShield = stats[1]->shield;
    bool oldGuestGuard = stats[1]->defending;
    auto weapon = newItem(IRON_SWORD, EXCELLENT, 0, 1, 0, true, &stats[0]->inventory);
    auto shield = newItem(IRON_SHIELD, EXCELLENT, 0, 1, 0, true, &stats[1]->inventory);
    stats[0]->weapon = weapon; stats[0]->STR = 15;
    stats[1]->shield = shield; stats[1]->CON = 0; stats[1]->defending = true;
    stats[1]->setProficiencyUnsafe(PRO_SHIELD, 99);
    host.attack(1, MAXCHARGE, &guest);
    check("passive_native_block_riposte", states[1].slots[0].charges == 1 && states[1].hasteTime > 0);
    stats[0]->weapon = savedWeapon; stats[1]->shield = oldGuestShield; stats[1]->defending = oldGuestGuard;
    if (weapon->node) list_RemoveNode(weapon->node);
    if (shield->node) list_RemoveNode(shield->node);
    prepare(CLASS_WIZARD, 3);
    useSpell(SPELL_HEALING);
    check("passive_utility_spell_does_not_prime_weapon", !states[0].slots[0].charges);
    prepare(CLASS_ARCANIST, 1);
    strike(Channel::Ranged);
    int beforePaidCast = stats[0]->MP;
    useSpell(600 + CLASS_ARCANIST * 4, 6);
    check("passive_discount_applies_to_ability_spells", stats[0]->MP == beforePaidCast - 4 && !states[0].discount);
    auto savedAddress = net_packet->address;
    int savedLength = net_packet->len, savedMultiplayer = multiplayer, savedClient = clientnum;
    std::vector<unsigned char> savedBytes(net_packet->data, net_packet->data + savedLength);
    multiplayer = CLIENT; clientnum = 1; states[1] = {};
    std::memset(net_packet->data, 0, 40); net_packet->len = 40; net_packet->address = net_server;
    std::memcpy(net_packet->data, "DPST", 4); SDLNet_Write32(uniqueGameKey, net_packet->data + 4);
    net_packet->data[8] = 1; SDLNet_Write32(2, net_packet->data + 9); SDLNet_Write16(6, net_packet->data + 13);
    net_packet->address.port ^= 1; receiveState();
    check("passive_state_spoof_rejected", states[1].barrier == 0);
    net_packet->address = net_server; net_packet->len = 39; receiveState();
    check("passive_state_length_rejected", states[1].barrier == 0);
    net_packet->len = 40; SDLNet_Write32(uniqueGameKey + 1, net_packet->data + 4); receiveState();
    check("passive_state_session_rejected", states[1].barrier == 0);
    SDLNet_Write32(uniqueGameKey, net_packet->data + 4); receiveState();
    check("passive_state_valid_accepted", states[1].barrier == 6);
    SDLNet_Write16(10, net_packet->data + 13); receiveState();
    check("passive_state_replay_rejected", states[1].barrier == 6);
    multiplayer = SERVER; receiveState();
    check("passive_server_rejects_client_state", states[1].barrier == 6);
    multiplayer = savedMultiplayer; clientnum = savedClient;
    std::copy(savedBytes.begin(), savedBytes.end(), net_packet->data); net_packet->len = savedLength; net_packet->address = savedAddress;
    reset(); check("passive_new_match_clears_state", !states[0].barrier && !states[0].slots[0].charges && !states[0].slots[0].cooldown);
    stats[0]->weapon = savedWeapon; stats[0]->shield = savedShield; stats[0]->defending = savedDefending;
    list_RemoveNode(fixture->mynode);
    states = savedStates; current = savedAction; sequence = savedSequence; processing = false; auditing = false;
}
