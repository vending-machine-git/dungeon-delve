# Join In Progress - experimental 0.1.2

Join a friend's already-running normal Barony multiplayer campaign through Direct IP. The host's run keeps its current character, floor, map changes, enemies, and items. New arrivals use their selected class's ordinary starter character and spawn in a free space near a living party member. This also works while the listen host's character is dead and another party member is alive.

Requires Barony v5.0.2 and the matching separately installed vending_machine framework runtime on every peer. Enable this mod before the host starts the session. The current engine has four total player slots. A full or locked session rejects additional arrivals. Joins during another synchronization or floor change wait for a safe point. A player joins as a new character; this prototype does not restore a disconnected character's previous backpack or progress.

## Local development launch

Run `framework/Play-Join-In-Progress.cmd`, or:

```
python framework/tools/launch.py --join-in-progress
```

The host starts a normal Direct IP multiplayer campaign. A guest creates their chosen character and joins the host's address through Direct IP, even after gameplay has started. With Steam running, the host can instead type `/invite` in chat to invite Steam friends who already have the same runtime open; they join through Steam's relay without port forwarding. Use a separate profile for every simultaneous process on one computer. Each peer must enable Join In Progress and use the same runtime. Bring Your Gear is optional per guest when the host enables both mods.

The host briefly pauses simulation to capture and transfer live terrain and entities. Transfer chunks are checked against the expected peer and transfer ID, bounded in size, checksummed, acknowledged, and retried when lost. The joining client applies the snapshot and confirms readiness before play resumes. Failed or abandoned synchronization times out and lets the existing host continue. The host does not regenerate its dungeon or restart the run.

This is an independent QoL mod for normal campaigns. Bring Your Gear remains a separate, optional companion mod. Extraction Raid cannot be combined with this prototype. Existing campaign saves stay in the isolated development profile. Loading the host's campaign in a lobby is still the ordinary Barony workflow; a late arrival does not need a copy of that campaign save.

## Optional Bring Your Gear support

The host enables both mods before starting the campaign:

```
framework\Play-Join-In-Progress.cmd --bring-gear
```

A late guest who wants to bring gear selects their own campaign save:

```
framework\Play-Join-In-Progress.cmd --gear-import "C:\path\to\savegame.baronysave"
```

Guests without Bring Your Gear can use `Play-Join-In-Progress.cmd` by itself and receive ordinary class starters. Guests may also enable both without selecting an import to keep starters. The host must enable Bring Your Gear to admit importing guests; having only Join In Progress on the host does not opt it into imports. All peers use the updated matching native runtime.

Only physical backpack items and equipment are copied. The guest keeps their chosen new character identity, class, race, level, stats, and class spells; the host's campaign state stays intact. Late-join synchronization sends imported combat equipment to the host before play resumes. Source saves are copied into isolated profiles and never changed. Rejoining still creates a fresh character and reapplies the selected kit; disconnected-character restoration and persistent public-server inventories remain future work. Both mods stay separate Workshop packages; Bring Your Gear is an optional companion, not a required dependency of Join In Progress.

## Current limits

Private Direct IP or framework Steam invites (friends-only lobby, Steam relay); EOS remains disabled. An invite accepted while the game is closed starts Steam's retail Barony, so friends open the framework runtime first. There is no dedicated server, host migration, extra player capacity, or secure public-server inventory service. Wider floor/boss/custom-map coverage, extended combat and networking tests, and the reconnect experience remain release validation work. See JOIN_IN_PROGRESS_PLAN.md in the development project and tests/smoke/README.md for current evidence and reproduction.

## Workshop package

This individual folder is the complete Workshop upload root. It contains metadata, documentation, an original preview, and license, not native code or game assets. A Workshop subscription alone cannot install engine changes. Publish vending_machine framework separately and add its Workshop item under Required Items. Source, runtime, snapshots, campaign data, and tests stay outside this directory. The owner handles publication.

Barony by Turning Wheel LLC and contributors. Developed for Reyes with OpenAI Codex assistance. See LICENSE.txt.

Version 0.1.2 improves failure handling: malformed or incomplete gear imports are rejected, and snapshot-directory write failures report an error while the ongoing host session remains available. Broader regression results and reproduction scripts are recorded in the development project's `BUG_TEST_REPORT.md` and `tests/regression/`.
