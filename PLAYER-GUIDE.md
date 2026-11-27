# Dungeon Delve 0.20.5 - Windows x64

The full Windows package includes Dungeon Delve, vending_machine framework,
the native runtime and mod content. Requires an owned compatible Steam Barony
5.0.2 installation, Windows x64 and .NET Framework 4.5+ (included with Windows 10/11).
Private 2-4 player games through Steam invites or Direct IP; protocol 35, framework API 1.

November 27 full release: `v0.20.5-full.1`. Merchant browsing shows full item
effects, rarity and exact rolls. Used hatches/portals disappear for every peer.
Spell preparation takes twice vanilla time before equipment/spellbook bonuses.
Every player must install this updated build, including users of older 0.20.5.

## Install with Setup

1. Install Barony through Steam, run it once, then keep Steam running.
2. Download Dungeon-Delve-0.20.5-Setup.exe and verify its SHA256 against the
   accompanying SHA256SUMS.txt.
3. Run Setup. Select the detected Barony folder or Browse to the folder containing
   barony.exe. Steam > Barony > Properties > Installed Files > Browse shows it.
4. Leave the desktop shortcut checked if wanted, then click Install.
5. Double-click Dungeon Delve.exe in the main Barony directory, beside barony.exe,
   or use the desktop shortcut. Steam's normal Play button still starts vanilla.

Setup verifies the owned game and DLLs. It adds the launcher to the game directory
and puts supporting files into Dungeon-Delve-0.20.5. Personal saves, configuration,
retail executable and assets are not replaced. No admin request, auto-updater or
background service. A modified/same-version supporting folder is preserved;
rename it before installing the full updated package.
An unrelated existing Dungeon Delve.exe is preserved and installation is refused.

## Extract the ZIP instead

1. Download Dungeon-Delve-0.20.5-windows-x64.zip and verify its checksum.
2. Right-click > Extract All. Set the destination to the Barony installation
   directory containing barony.exe. Extract all contents directly there.
3. Double-click Dungeon Delve.exe beside barony.exe. Keep the supporting folder.

```text
Barony/
  barony.exe                       <- retail base game
  Dungeon Delve.exe                <- mod launcher
  Dungeon-Delve-0.20.5/
    Launch.ps1
    release.json
    runtime/barony-rmf.exe
    mods/vending_machine_framework/
    mods/extraction-raid/
```

Do not add another outer folder when extracting, or copy mod files over retail
files. GitHub's automatic Source code archives are not playable packages.
The launcher has a custom portal icon and opens without a command window.
It finds the adjacent supporting folder automatically. A complete portable copy
also works outside the game directory through Steam discovery.

## Existing complete 0.20.5 installation

Close Dungeon Delve on every PC. Rename the existing Dungeon-Delve-0.20.5
supporting folder, then run the new Setup or extract the complete new ZIP into
the Barony directory. Copying only Dungeon Delve.exe does not update gameplay.
The new release.json identifies distribution revision full-windows-20261127-1.

If Dungeon Delve.exe is missing, copy it from the root of the full ZIP to sit
beside retail barony.exe, with the existing Dungeon-Delve-0.20.5 supporting folder
next to it. Double-click the root launcher. The EXE alone in Downloads cannot run
the mod. Older launchers inside the supporting folder can remain in place.

## Play with friends

Everyone uses the same release and joins before the match starts.

**Steam invites (added in 0.20.4):** with Steam running, the host clicks an open
seat's **INVITE** card to open Steam's invite dialog. Friends must already have
Dungeon Delve open (launched from Dungeon Delve.exe) when they accept: Steam
starts its normal Barony for invites accepted while the game is closed, and that
cannot join. Friends can also use **Join Game** on the host in their Steam friends
list. Invited players connect through Steam's relay, so no port forwarding is
needed. If the Steam overlay is unavailable, the invite code (`steam:<number>`) is
copied; friends paste it into the Direct IP box. Steam invites passed automated
multi-instance tests and a single real-account check, but have not yet been
tested between two real Steam accounts; please report problems.

**Direct IP:** host/join a fresh private Direct IP lobby with the host IP and the
displayed port; Internet connections may need host firewall/router setup.
Joining an already-running match is not supported.

## What the launcher does

The launcher checks the runtime, bundled content, compatible retail executable
and ten owned-game DLL hashes. Required DLLs are copied locally from your game;
they are not included in downloads. Matching version strings alone do not prove
compatibility. Every launch creates an isolated session under
%LOCALAPPDATA%\vending_machine_framework\0.20.5\sessions\.
Personal campaigns and configuration are not imported or edited. Back up normal
saves before trying unofficial software. Logs and launch.json stay in each session.

## Troubleshooting, updates and removal

- Missing files: the main Barony directory must contain the mod EXE and the
  complete Dungeon-Delve-0.20.5 supporting folder. Extract the full ZIP again.
- Game not found: choose the folder containing barony.exe in the launcher.
- Incompatible hashes: verify Barony files in Steam or obtain a matching release;
  do not edit expected hashes to bypass the check.
- Updating: close modded games and update every peer. Setup preserves older or
  modified supporting folders; rename those folders yourself before reinstalling.
- Uninstall: close the mod, remove only Dungeon Delve.exe and its
  Dungeon-Delve-0.20.5 supporting folder/shortcut. Keep barony.exe and retail assets.
  Keep any logs you need before optionally removing old isolated sessions.
- For support, provide versions, steps and sanitized session logs/launch.json.
  Do not post private IPs, usernames or personal saves. This is unsigned software;
  verify its publisher/checksum rather than disabling antivirus.

## Game and release limits

Three six-minute floors, 1.75x XP, three spaced merchant kiosks per floor with randomized floor-rarity stock, mutually neutral
shopkeepers/mobs, randomized rarity loot and character builds,
preserved within-match adventurer progression, monster returns, 45-second
repeat-rat cooldowns and ten-cheese Algernon evolution. Final-floor exits open
in the last minute; the first accepted claim wins. Full current rules are in
`mods/extraction-raid/README.md`; its historical Workshop installation section
does not apply to this combined GitHub release.

Windows x64 only, private Steam invites or Direct IP, limited WAN/human balance testing; not a
hardened public server. Steam invites only (no public matchmaking); no EOS. Native
FMOD gameplay effects/music are enabled; fmod.dll is copied from your owned game,
never redistributed. Supplied Delve lobby music/narration use SDL. DLC is optional; genuine per-player DLC
ownership remains required for DLC characters. No campaign imports or persistent
loot bank. Unofficial project, not affiliated with or endorsed by Turning Wheel.
License notices are in `licenses/` and each bundled mod. Development assistance:
OpenAI Codex through T3 Code.
