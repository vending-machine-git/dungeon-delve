void affixAudit() {
    using namespace DelveRarity;
    using DelveAffixes::Kind;
    auto equipmentSlots = [](Stat& actorStats) {
        return std::array<Item**, 10>{{&actorStats.weapon, &actorStats.shield, &actorStats.helmet,
            &actorStats.breastplate, &actorStats.gloves, &actorStats.shoes, &actorStats.cloak,
            &actorStats.ring, &actorStats.mask, &actorStats.amulet}};
    };
    const auto hostSlots = equipmentSlots(*stats[0]), guestSlots = equipmentSlots(*stats[1]);
    std::array<Item*, 10> hostItems{}, guestItems{};
    for (size_t index = 0; index < hostSlots.size(); ++index) {
        hostItems[index] = *hostSlots[index]; *hostSlots[index] = nullptr;
        guestItems[index] = *guestSlots[index]; *guestSlots[index] = nullptr;
    }
    auto source = players[0]->entity, target = players[1]->entity;
    const std::array<int, 8> resources{{stats[0]->HP, stats[0]->MAXHP, stats[0]->MP, stats[0]->MAXMP,
        stats[1]->HP, stats[1]->MAXHP, stats[1]->MP, stats[1]->MAXMP}};
    const auto originalType = stats[1]->type;
    const double oldX = source->x, oldY = source->y, oldYaw = target->yaw;
    const bool defending = stats[0]->defending;
    auto sword = newItem(IRON_SWORD, EXCELLENT, 0, 1, 123, true, nullptr);
    auto shield = newItem(WOODEN_SHIELD, EXCELLENT, 0, 1, 123, true, nullptr);
    auto ring = newItem(RING_PROTECTION, EXCELLENT, 0, 1, 123, true, nullptr);
    auto findRoll = [&](Kind kind) {
        for (Uint32 entropy = 0; entropy <= seedMask; ++entropy) {
            assign(*ring, Tier::Blue, entropy);
            if (affixes(*ring)[0].kind == kind) return affixes(*ring)[0].value;
        }
        return 0;
    };
    auto prepare = [&]() {
        stats[0]->ring = nullptr; DelvePassives::refreshEquipment(0); DelvePassives::reset();
        stats[0]->weapon = sword; stats[0]->shield = shield; stats[0]->defending = false;
        stats[0]->HP = stats[0]->MP = 100; stats[0]->MAXHP = stats[0]->MAXMP = 500;
        stats[1]->HP = stats[1]->MAXHP = 500; stats[1]->type = HUMAN;
        source->x = target->x - 32; source->y = target->y; target->yaw = 0;
    };
    auto hitFor = [&](Entity* attacker, Entity* victim, DelvePassives::Channel channel, int damage) {
        const int before = victim->getStats()->HP;
        { DelvePassives::Scope action(attacker, channel); victim->modHP(-damage); }
        return before - victim->getStats()->HP;
    };
    for (int variant = 0; variant < 2 * static_cast<int>(Kind::Count); ++variant) {
        const int index = variant % static_cast<int>(Kind::Count);
        ring->beatitude = variant >= static_cast<int>(Kind::Count) ? 1 : 0;
        prepare();
        const auto kind = static_cast<Kind>(index);
        const int value = DelveAffixes::capped(kind, findRoll(kind));
        const int baseCharge = Stat::getMaxAttackCharge(stats[0]);
        const double baseRanged = rangedAttackGetSpeedModifier(stats[0]);
        const int baseCost = getCostOfSpell(&spell_fireball, source);
        const std::array<int, 6> baseStats{{source->getSTR(), source->getDEX(), source->getCON(), source->getINT(), source->getPER(), source->getCHR()}};
        const int baseStealth = stats[0]->getModifiedProficiency(PRO_STEALTH);
        const int baseLockpick = stats[0]->getModifiedProficiency(PRO_LOCKPICKING);
        stats[0]->ring = ring;
        bool passed = value > 0 && total(*stats[0], kind) == value;
        switch (kind) {
            case Kind::AttackSpeed:
                passed &= Stat::getMaxAttackCharge(stats[0]) < baseCharge && rangedAttackGetSpeedModifier(stats[0]) < baseRanged; break;
            case Kind::CastingSpeed: passed &= castTicks(*stats[0], 1000) == 100000 / (100 + value); break;
            case Kind::MoveSpeed: passed &= std::abs(DelvePassives::speedMultiplier(0) - (1.0 + value / 100.0)) < .0001; break;
            case Kind::ArmorPenetration: passed &= penetratedArmor(stats[0], 100) == 100 - value; break;
            case Kind::Backstab: passed &= hitFor(source, target, DelvePassives::Channel::Melee, 100) == 100 + value; break;
            case Kind::UndeadSlayer:
                stats[1]->type = SKELETON; passed &= hitFor(source, target, DelvePassives::Channel::Spell, 100) == 100 + value; break;
            case Kind::DemonSlayer:
                stats[1]->type = DEMON; passed &= hitFor(source, target, DelvePassives::Channel::Melee, 100) == 100 + value; break;
            case Kind::LifeOnKill: case Kind::ManaOnKill: {
                stats[1]->HP = 1;
                hitFor(source, target, DelvePassives::Channel::Melee, 2);
                passed &= (kind == Kind::LifeOnKill ? stats[0]->HP : stats[0]->MP) == 100 + value;
                DelvePassives::killed(*source, *target);
                passed &= (kind == Kind::LifeOnKill ? stats[0]->HP : stats[0]->MP) == 100 + value;
                stats[1]->HP = 500; break;
            }
            case Kind::MaxHealth: case Kind::MaxMana:
                DelvePassives::refreshEquipment(0); DelvePassives::refreshEquipment(0);
                passed &= (kind == Kind::MaxHealth ? stats[0]->MAXHP : stats[0]->MAXMP) == 500 + value;
                passed &= stats[0]->HP == 100 && stats[0]->MP == 100;
                stats[0]->ring = nullptr; DelvePassives::refreshEquipment(0);
                passed &= stats[0]->MAXHP == 500 && stats[0]->MAXMP == 500; break;
            case Kind::ProjectileResistance:
                passed &= hitFor(target, source, DelvePassives::Channel::Ranged, 100) == 100 - value; break;
            case Kind::MagicResistance:
                passed &= hitFor(target, source, DelvePassives::Channel::Spell, 100) == 100 - value; break;
            case Kind::BlockPower:
                stats[0]->defending = true;
                passed &= hitFor(target, source, DelvePassives::Channel::Melee, 100) == 100 - value; break;
            case Kind::DebuffResistance:
                source->setEffect(EFF_SLOW, false, 0, false);
                source->setEffect(EFF_SLOW, true, 1000, false);
                passed &= stats[0]->EFFECTS_TIMERS[EFF_SLOW] == 1000 - 10 * value;
                source->setEffect(EFF_SLOW, false, 0, false); break;
            case Kind::BuffDuration:
                source->setEffect(EFF_FAST, false, 0, false);
                source->setEffect(EFF_FAST, true, 1000, false);
                passed &= stats[0]->EFFECTS_TIMERS[EFF_FAST] == 1000 + 10 * value;
                source->setEffect(EFF_FAST, false, 0, false); break;
            case Kind::HealingPower: {
                DelvePassives::Scope action(source, DelvePassives::Channel::Drink, POTION_HEALING);
                source->modHP(100); passed &= stats[0]->HP == 200 + value; break;
            }
            case Kind::ManaEfficiency:
                passed &= getCostOfSpell(&spell_fireball, source) == DelveAffixes::reducedCost(baseCost, value); break;
            case Kind::HealthRegen: case Kind::ManaRegen:
                DelvePassives::tick(5 * TICKS_PER_SECOND);
                passed &= (kind == Kind::HealthRegen ? stats[0]->HP : stats[0]->MP) == 100 + value; break;
            case Kind::Strength: passed &= source->getSTR() == baseStats[0] + value; break;
            case Kind::Dexterity: passed &= source->getDEX() == baseStats[1] + value; break;
            case Kind::Constitution: passed &= source->getCON() == baseStats[2] + value; break;
            case Kind::Intelligence: passed &= source->getINT() == baseStats[3] + value; break;
            case Kind::Perception: passed &= source->getPER() == baseStats[4] + value; break;
            case Kind::Charisma: passed &= source->getCHR() == baseStats[5] + value; break;
            case Kind::Stealth: passed &= stats[0]->getModifiedProficiency(PRO_STEALTH) == std::min(100, baseStealth + value); break;
            case Kind::Lockpicking: passed &= stats[0]->getModifiedProficiency(PRO_LOCKPICKING) == std::min(100, baseLockpick + value); break;
            default: passed = false; break;
        }
        const std::string label = std::string(ring->beatitude > 0 ? "affix_blessed_native_effect_" : "affix_native_effect_") + std::to_string(index);
        check(passed, label.c_str());
        ring->status = BROKEN;
        passed = total(*stats[0], kind) == 0;
        ring->status = EXCELLENT; stats[0]->ring = nullptr;
        passed &= total(*stats[0], kind) == 0;
        check(passed, (std::string(ring->beatitude > 0 ? "affix_blessed_unequip_broken_inert_" : "affix_unequip_broken_inert_") + std::to_string(index)).c_str());
    }
    ring->beatitude = 0;
    prepare();
    bool distinct = true, distribution = true;
    auto other = newItem(RING_PROTECTION, EXCELLENT, 0, 1, 123, true, nullptr);
    assign(*ring, Tier::Blue, 1); assign(*other, Tier::Blue, 2);
    distinct &= itemCompare(ring, other, false) != 0;
    for (int rarity = 0; rarity < 5; ++rarity) {
        assign(*ring, static_cast<Tier>(rarity), 19);
        int count = 0;
        for (const auto& roll : affixes(*ring)) if (roll.kind != Kind::Count) ++count;
        distribution &= count == DelveAffixes::count(rarity);
    }
    check(distinct, "affix_different_seed_cannot_stack");
    check(distribution, "affix_native_zero_zero_one_two_three_rolls");
    bool blessings = true;
    for (int rarity = 0; rarity < 5; ++rarity) {
        assign(*ring, static_cast<Tier>(rarity), 19);
        const auto originalAppearance = ring->appearance;
        const auto originalRolls = affixes(*ring);
        for (int blessing : {-3, 0, 1, 5}) {
            ring->beatitude = blessing;
            const auto modified = affixes(*ring);
            const auto text = summary(*ring);
            for (size_t index = 0; index < modified.size(); ++index) {
                blessings &= modified[index].kind == originalRolls[index].kind
                    && modified[index].value == originalRolls[index].value * (blessing > 0 ? 2 : 1);
                if (modified[index].kind == Kind::Count) continue;
                const auto& definition = DelveAffixes::definitions[static_cast<int>(modified[index].kind)];
                const auto line = "+" + std::to_string(modified[index].value) + (definition.percent ? "% " : " ") + definition.name;
                blessings &= text.find(line) != std::string::npos;
            }
            blessings &= ring->appearance == originalAppearance;
        }
        ring->beatitude = 0;
    }
    check(blessings, "affix_blessing_tooltips_and_seed_stability");
    prepare();
    const int health = findRoll(Kind::MaxHealth);
    stats[0]->ring = ring;
    bool liveBlessing = health > 0;
    for (int blessing : {0, 1, 5, 0, -3}) {
        ring->beatitude = blessing;
        DelvePassives::refreshEquipment(0);
        DelvePassives::refreshEquipment(0);
        liveBlessing &= stats[0]->MAXHP == 500 + health * (blessing > 0 ? 2 : 1)
            && stats[0]->HP == 100;
    }
    ring->beatitude = 0;
    stats[0]->ring = nullptr;
    DelvePassives::refreshEquipment(0);
    liveBlessing &= stats[0]->MAXHP == 500 && stats[0]->HP == 100;
    check(liveBlessing, "affix_blessing_live_max_hp_no_free_healing");
    assign(*ring, Tier::Blue, 5); const auto expectedSummary = summary(*ring);
    std::unordered_set<Uint32> appearances{ring->appearance};
    for (int attempt = 0; attempt < 20; ++attempt) {
        Item::itemFindUniqueAppearance(ring, appearances); appearances.insert(ring->appearance);
        distinct &= summary(*ring) == expectedSummary && seed(*ring) == 5;
    }
    check(distinct, "affix_deduplication_never_rerolls");
    stats[0]->weapon = nullptr; stats[0]->shield = nullptr;
    DelvePassives::reset();
    for (size_t index = 0; index < hostSlots.size(); ++index) { *hostSlots[index] = hostItems[index]; *guestSlots[index] = guestItems[index]; }
    stats[0]->HP = resources[0]; stats[0]->MAXHP = resources[1]; stats[0]->MP = resources[2]; stats[0]->MAXMP = resources[3];
    stats[1]->HP = resources[4]; stats[1]->MAXHP = resources[5]; stats[1]->MP = resources[6]; stats[1]->MAXMP = resources[7];
    stats[1]->type = originalType; stats[0]->defending = defending;
    source->x = oldX; source->y = oldY; target->yaw = oldYaw;
    free(sword); free(shield); free(ring); free(other);
    serverUpdatePlayerStats();
}
