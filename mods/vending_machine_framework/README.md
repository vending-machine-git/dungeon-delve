# vending_machine framework 0.20.3

0.20.3 makes gates behave like vanilla: they start closed and their lever sits a
few tiles away against a wall. Gates that divide the floor have a lever on each
side, and either lever toggles them. Empty optional rooms keep their lock and get
a cache chest (or gear/treasure) from the existing floor loot budget. Only
structurally broken doorways are removed. Chest spacing, doorway clearance and
the three furnished kiosks remain.
All peers need protocol 35.

Native support for Dungeon Delve and compatible content mods. Framework API **1**;
Dungeon Delve protocol **35**. Tested **Steam Barony 5.0.2, Windows x64**.
Unofficial experimental software, not an official Turning Wheel release.

## Combined GitHub release (current)

The selected distribution is **Dungeon-Delve-0.20.3-windows-x64.zip** from the
project's GitHub Releases page. It includes this framework, its native runtime
and Dungeon Delve together. Extract into the owned Barony installation and run
**Dungeon-Delve-0.20.3/Play-Dungeon-Delve.cmd**, or extract elsewhere and use Steam
library discovery. No Workshop subscriptions or separate framework download are
required. Follow the combined ZIP's root README for installation/checksums,
compatibility, updates, logs and removal. Retail files/saves stay untouched.

## Historical Workshop route

The following separate-package notes are retained for reference. They are not
the installation steps for the combined GitHub release.

### Subscribing is only half the installation

This Workshop package supplies content/metadata, **not executable hooks**. The
vanilla loader cannot enable our native C++ changes. A separately downloaded,
matching **barony-rmf.exe** is required on every player's PC. Never overwrite
Steam's retail `barony.exe` or combine this with another native framework.

## Player installation

1. Install/run the owned Steam Windows edition of Barony once; keep Steam running.
2. Subscribe to this package and **Dungeon Delve 0.20.3**. Wait for downloads.
3. Download **vending_machine-framework-0.20.3-windows.zip** using this Workshop
   page's release link. Extract into a writable folder outside Barony.
4. Read its README, verify the posted SHA256 and run **Play-Dungeon-Delve.cmd**.
   No Python/compiler/admin access or global PowerShell policy change needed.
5. Host/join a fresh private Direct IP lobby with 2-4 matching players before
   starting. Everyone needs runtime/content **0.20.3**, API **1**, protocol **35**.

The launcher discovers Steam libraries and subscriptions by manifest, checks
runtime/game/dependency hashes and stages content into a fresh isolated session.
It copies ten verified DLLs, including FMOD, from your own game for local use; no game DLLs or
purchased assets are redistributed. Matching version strings alone do not prove
compatibility. Future Steam builds may need a new runtime; never bypass hash checks.
Manual game/content paths and troubleshooting are in the runtime download README.

## Features and limits

Dungeon Delve has three six-minute floors, 1.75x XP, three spaced merchant kiosks
per floor with randomized floor-rarity stock, floor-scaled randomized loot,
class builds, preserved within-match adventurer progression, monster/rat returns,
45-second repeat-rat cooldowns and cheese-to-Algernon evolution. 0.17.1 improves
the return picker and removes Delve's blue blessed-item background without
changing blessings or normal campaigns. See Dungeon Delve's README for rules.

**Play-Framework.cmd** launches framework-only normal play. This player download
focuses on Dungeon Delve; it does not enable separate Bring Your Gear or Join In
Progress mods. Do not mix raid/normal clients or profiles, native frameworks or
untested third-party content. Developer launcher commands remain in the source
workspace, not this upload root.

Steam matchmaking/invites and EOS networking are disabled: use private Direct IP.
Host firewall/router setup may be needed; WAN/human balance testing is limited.
0.20.3 restores native FMOD effects/music; Delve lobby music/narration use SDL.
FMOD is verified/copied from the owned installation, never shipped. Genuine DLC is checked per player;
a base-game host can play with DLC owners without sharing unlocks. No DLC bypass.

## Updates, logs and removal

Update both subscriptions and the separate runtime together on every peer.
Each launch uses a fresh session under
`%LOCALAPPDATA%\vending_machine_framework\0.20.3\sessions\`, containing
`launch.json`, logs and results. Retail saves/config are not copied or edited.
Back up normal saves before modded play. Report versions, reproduction steps and
sanitized session logs; never post private saves or IPs publicly.

Close modded games, delete the downloaded runtime and optionally isolated
sessions, then unsubscribe to uninstall. Retail stays untouched. No background
service/auto-updater is installed. The native executable is unsigned; trust the
publisher and verify checksums rather than disabling antivirus.

## Workshop package

This folder is the complete Workshop content root. Keep source, binaries, DLLs,
tools and player data outside it. Dependencies in `mod.json` are not a substitute
for each dependent mod's owner configuring this Workshop item under Required Items.
The publisher must host/link the separate runtime before public release.
Licenses/notices accompany that download. Nothing publishes/installs executables
automatically. Windows x64 only; experimental private play, not a hardened server.
