void spellbookAudit(int player) {
    using namespace DelveSpellRolls;
    using DelveRarity::Tier;
    auto source = players[player]->entity;
    const auto oldSelection = players[player]->magic.selectedSpell();
    const int proficiency = stats[player]->getProficiency(PRO_SORCERY);
    const int intelligence = stats[player]->INT;
    stats[player]->setProficiency(PRO_SORCERY, 0);
    stats[player]->INT = -100;
    bool gateChecked = false;
    for (auto type : {SPELLBOOK_DOMINATE, SPELLBOOK_STONEBLOOD, SPELLBOOK_EXTRAHEALING}) {
        const int spellID = getSpellIDFromSpellbook(type);
        bool known = false;
        for (auto node = players[player]->magic.spellList.first; node; node = node->next)
            if (static_cast<spell_t*>(node->element)->ID == spellID) known = true;
        if (known) continue;
        auto gateBook = newItem(type, EXCELLENT, 0, 1, 123, true, nullptr);
        item_Spellbook(gateBook, player);
        if (gateBook) free(gateBook);
        for (auto node = players[player]->magic.spellList.first; node; node = node->next)
            if (static_cast<spell_t*>(node->element)->ID == spellID) {
                gateChecked = true;
                list_RemoveNode(node);
                break;
            }
        for (auto node = stats[player]->inventory.first; node; node = node->next) {
            auto item = static_cast<Item*>(node->element);
            if (item->type == SPELL_ITEM && item->appearance == spellID) { list_RemoveNode(node); break; }
        }
        break;
    }
    check(gateChecked, "spellbook_high_difficulty_read_without_skill_or_int_requirement");
    stats[player]->INT = intelligence;
    stats[player]->setProficiency(PRO_SORCERY, 100);
    spell_t* fireball = nullptr;
    for (auto node = players[player]->magic.spellList.first; node; node = node->next) {
        auto spell = static_cast<spell_t*>(node->element);
        if (spell->ID == SPELL_FIREBALL) fireball = spell;
    }
    const bool existed = fireball != nullptr;
    if (!fireball) {
        auto firstBook = newItem(SPELLBOOK_FIREBALL, EXCELLENT, 0, 1, 123, true, nullptr);
        item_Spellbook(firstBook, player);
        if (firstBook) free(firstBook);
        for (auto node = players[player]->magic.spellList.first; node; node = node->next)
            if (static_cast<spell_t*>(node->element)->ID == SPELL_FIREBALL) fireball = static_cast<spell_t*>(node->element);
    }
    if (!fireball) { check(false, "spellbook_fixture_memorized_spell"); return; }
    check(existed || fireball->delveImprint.bookType == SPELLBOOK_FIREBALL,
        "spellbook_first_read_creates_memorized_variant");
    const auto oldImprint = fireball->delveImprint;
    players[player]->magic.equipSpell(fireball);
    const int countBefore = list_Size(&players[player]->magic.spellList);
    auto book = newItem(SPELLBOOK_FIREBALL, EXCELLENT, 1, 1, 123, true, nullptr);
    DelveRarity::assign(*book, Tier::Legendary, 13);
    const auto expected = fromBook(*book);
    item_Spellbook(book, player);
    check(fireball->delveImprint.bookType == SPELLBOOK_FIREBALL && fireball->delveImprint.tier == 4
        && fireball->delveImprint.seed == expected.seed && fireball->delveImprint.blessed,
        "spellbook_read_legendary_inherits_exact_blessed_rolls");
    check(players[player]->magic.selectedSpell() == fireball && list_Size(&players[player]->magic.spellList) == countBefore,
        "spellbook_replacement_keeps_selection_and_single_spell");
    const auto copied = copySpell(fireball);
    if (multiplayer == CLIENT) {
        std::ofstream(sync + "/spellbook-cast-expected") << static_cast<int>(expected.seed);
        castSpell(source->getUID(), fireball, false, false);
    }
    const int nativeCost = getCostOfSpell(&spell_fireball, source);
    check(getCostOfSpell(fireball, source) == DelveAffixes::reducedCost(nativeCost,
        DelveSpellRules::value(expected, Kind::Efficiency)), "spellbook_memorized_mana_efficiency");
    int nativeDamage = getSpellDamageFromID(SPELL_FIREBALL, source, nullptr, source);
    {
        Scope context(source, fireball);
        check(getSpellDamageFromID(SPELL_FIREBALL, source, nullptr, source) > nativeDamage,
            "spellbook_native_damage_power");
        check(activeLegendary(source), "spellbook_inherited_legendary_siphon_eligible");
        check(DelveRarity::effectDuration(*stats[player], EFF_FAST, 1000) >= 1000 + 10 * DelveSpellRules::value(expected, Kind::Power),
            "spellbook_beneficial_duration_power");
    }
    spell_t healing = spell_healing;
    healing.delveImprint = {SPELLBOOK_HEALING, 4, 13, true};
    const int nativeHealing = getSpellDamageFromID(SPELL_HEALING, source, nullptr, source);
    {
        Scope context(source, &healing);
        check(getSpellDamageFromID(SPELL_HEALING, source, nullptr, source) > nativeHealing,
            "spellbook_native_healing_power");
    }
    SaveGameInfo::Player saved{};
    saved.spells.push_back(SPELL_FIREBALL);
    saved.stats.attributes.emplace_back("rmf_spell_rolls:" + std::to_string(SPELL_FIREBALL), encode(expected));
    bool serialization = true;
    for (auto format : {EFileFormat::Json_Compact, EFileFormat::Binary}) {
        SaveGameInfo::Player restored{};
        const std::string savePath = sync + "/spellbook-serialization-" + std::to_string(player)
            + (format == EFileFormat::Binary ? ".bin" : ".json");
        serialization &= FileHelper::writeObject(savePath.c_str(), format, saved)
            && FileHelper::readObject(savePath.c_str(), restored)
            && restored.spells == saved.spells && restored.stats.attributes == saved.stats.attributes;
    }
    const auto decoded = decode(encode(expected));
    check(serialization && decoded.bookType == expected.bookType && decoded.tier == expected.tier
        && decoded.seed == expected.seed && decoded.blessed == expected.blessed
        && decode("1 999 0 0").bookType == -1 && decode("1 2 3 1 junk").bookType == -1,
        "spellbook_save_metadata_roundtrip");
    check(!activeLegendary(source) && getSpellDamageFromID(SPELL_FIREBALL, source, nullptr, source) == nativeDamage,
        "spellbook_scope_does_not_buff_other_actions");
    book->beatitude = 0; DelveRarity::assign(*book, Tier::Blue, 71);
    const auto next = fromBook(*book);
    item_Spellbook(book, player);
    const auto rolls = DelveSpellRules::generate(fireball->delveImprint);
    check(fireball->delveImprint.tier == 2 && fireball->delveImprint.seed == next.seed && !fireball->delveImprint.blessed
        && rolls[1].kind == Kind::Count && rolls[2].kind == Kind::Count,
        "spellbook_duplicate_read_replaces_all_rolls_including_downgrade");
    check(copied->delveImprint.tier == 4 && copied->delveImprint.seed == expected.seed && copied->delveImprint.blessed,
        "spellbook_inflight_copy_keeps_old_rolls");
    check(DelveRarity::affixes(*book)[0].kind == DelveAffixes::Kind::Count,
        "spellbook_no_passive_equipment_double_count");
    auto oldShield = stats[player]->shield;
    stats[player]->shield = book;
    book->beatitude = 1; DelveRarity::assign(*book, Tier::Legendary, 13);
    {
        spell_t plain = spell_fireball;
        Scope context(source, &plain);
        check(!activeLegendary(source) && activePower() == 0,
            "spellbook_held_legendary_does_not_imprint_unrelated_spell");
    }
    check(getCostOfSpell(fireball, source, true) == DelveAffixes::reducedCost(nativeCost,
        DelveSpellRules::value(fromBook(*book), Kind::Efficiency)) && fireball->delveImprint.tier == 2,
        "spellbook_held_cast_uses_held_rolls_without_merging_memory");
    if (multiplayer == SERVER && player == 0) {
        auto target = players[1]->entity;
        const int savedHP = stats[0]->HP, savedMP = stats[0]->MP, targetHP = stats[1]->HP;
        stats[0]->HP = stats[0]->MAXHP - 10; stats[0]->MP = 0;
        const int beforeHP = stats[0]->HP;
        DelvePassives::reset();
        {
            spell_t plain = spell_fireball;
            Scope context(source, &plain);
            DelvePassives::Scope action(source, DelvePassives::Channel::Spell);
            target->modHP(-1);
        }
        check(stats[0]->HP == beforeHP && stats[0]->MP == 0,
            "spellbook_held_legendary_no_siphon_on_plain_spell");
        {
            Scope context(source, copied);
            DelvePassives::Scope action(source, DelvePassives::Channel::Spell);
            target->modHP(-1);
        }
        check(stats[0]->HP == beforeHP + 4 && stats[0]->MP == 2,
            "spellbook_learned_legendary_siphon_native_proc");
        stats[0]->HP = savedHP; stats[0]->MP = savedMP; stats[1]->HP = targetHP;
        DelvePassives::reset();
    }
    book->status = BROKEN;
    check(value(source, fireball, Kind::Efficiency, true) == 0,
        "spellbook_broken_held_book_inert");
    stats[player]->shield = oldShield;
    book->beatitude = 0;
    book->status = EXCELLENT; DelveRarity::assign(*book, Tier::Grey, 0);
    item_Spellbook(book, player);
    check(DelveSpellRules::generate(fireball->delveImprint)[0].kind == Kind::Count
        && getCostOfSpell(fireball, source) == nativeCost, "spellbook_plain_book_clears_previous_rolls");
    check(!book || summary(*book).empty(), "spellbook_plain_tooltip_has_no_phantom_rolls");
    if (book) free(book);
    spellDeconstructor(copied);
    fireball->delveImprint = oldImprint;
    players[player]->magic.equipSpell(oldSelection);
    if (!existed) {
        for (auto node = players[player]->magic.spellList.first; node; node = node->next)
            if (node->element == fireball) { list_RemoveNode(node); break; }
        for (auto node = stats[player]->inventory.first; node; node = node->next) {
            auto item = static_cast<Item*>(node->element);
            if (item->type == SPELL_ITEM && item->appearance == SPELL_FIREBALL) { list_RemoveNode(node); break; }
        }
    }
    stats[player]->setProficiency(PRO_SORCERY, proficiency);
}
