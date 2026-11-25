# Bring Your Gear - experimental 0.1.2

A separate quality-of-life mod for normal Barony runs. Start a new normal campaign with physical items and equipped gear copied from one of your saved characters. Play the normal dungeon, progress normally, and save/load that new campaign normally.

Requires Barony v5.0.2 and the matching vending_machine framework native runtime. This is independent of Extraction Raid. The current prototype supports single-player and private Direct IP multiplayer. Every multiplayer peer enables Bring Your Gear and uses the same runtime; each peer selects their own import or keeps their ordinary starter kit.

## Launch

From the project, use Universal Modder's Python:

```
python framework/tools/launch.py --gear-import "F:/Steam/steamapps/common/Barony/savegames/savegame0.baronysave"
```

Or run `framework/Play-Bring-Your-Gear.cmd --gear-import "<save.baronysave>"`. The launcher automatically selects this separate mod and copies the input into its isolated profile. `Play-Bring-Your-Gear.cmd` reuses an import already copied into that profile, or uses ordinary starter gear if none has been selected. Source campaign saves are never written or consumed.

Create a new normal character or fresh Direct IP lobby as usual. The gear copy replaces physical starter items; your selected class, race, identity, level, stats, class spells, and campaign progress stay those of the new game. Preserved item fields include type, condition, blessing/curse, stack count, appearance, identification, equipment assignment, and backpack coordinates. Learned campaign spell cards, gold, and campaign/world state are not imported.

Loading an existing campaign uses its saved inventory, including anything you gained or lost while playing. The import is applied when creating a new character, including an ongoing session joined through the optional Join In Progress companion. There is no extraction requirement, raid deployment cost, or special gear-loss rule. Normal game rules still apply. Use a separate `--profile` directory for each simultaneous process on this computer.

## Workshop package

This folder is the complete upload root. Source, runtime, launchers, tests, imported saves, and profiles stay outside it. Publish the vending_machine framework companion separately and list it under Required Items. A Workshop subscription alone cannot install native engine extensions. The current runtime has audio, Steamworks, and EOS disabled; public installation and online/Steam integration remain release work. Publication is handled by the owner.

Built from the user-provided gear handoff and Barony source by Turning Wheel LLC and contributors. Integration for Reyes with OpenAI Codex assistance. See LICENSE.txt.

## Join an ongoing campaign

Use `framework/Play-Join-In-Progress.cmd --gear-import "<save.baronysave>"` to enable both separate mods and bring your physical kit when joining a running campaign. The host starts with `Play-Join-In-Progress.cmd --bring-gear`. In this combination, guests can also join with Join In Progress alone and ordinary starters; otherwise ordinary Bring Your Gear lobbies still require matching mod selection. Every peer needs the updated matching framework runtime. Source campaign progress, gold, and learned spell cards are not imported. Reconnects are fresh characters and reapply the selected import.

Version 0.1.2 improves failure handling: malformed or incomplete gear imports are rejected, and snapshot-directory write failures report an error while the ongoing host session remains available. Broader regression results and reproduction scripts are recorded in the development project's `BUG_TEST_REPORT.md` and `tests/regression/`.
