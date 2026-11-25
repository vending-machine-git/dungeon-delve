# Dungeon Delve

An unofficial Barony PvPvE battle-royale mod for private 2-4 player Direct IP
matches. Explore the Mines, Jungle and Crystal Caves, build around randomized
loot and abilities, return as monsters after death, and race for a final exit.

## Download and play

Open the [latest release](https://github.com/vending-machine-git/dungeon-delve/releases/latest).
You need an owned Steam Windows installation of Barony, tested build 5.0.2,
and Windows x64 with .NET Framework 4.5+ (included with Windows 10/11).
Run normal Barony once, then keep Steam running.

### Recommended: installer

1. Download [Dungeon-Delve-0.17.2-Setup.exe](https://github.com/vending-machine-git/dungeon-delve/releases/download/v0.17.2-launcher.2/Dungeon-Delve-0.17.2-Setup.exe)
   and [SHA256SUMS.txt](https://github.com/vending-machine-git/dungeon-delve/releases/download/v0.17.2-launcher.2/SHA256SUMS.txt).
   Compare the download's SHA256 with its entry in that file.
2. Run Setup and select the Barony game folder containing **barony.exe**.
   Automatic detection and **Browse** are available. Find the folder in Steam:
   **Barony > Properties > Installed Files > Browse**.
3. Leave **Create a desktop shortcut** checked if wanted, then click **Install**.
4. Open the Barony folder and double-click **Dungeon Delve.exe**, right next to
   **barony.exe**, or use the desktop shortcut.

The complete framework, mod content and runtime are included. No previous mod
version, Workshop subscriptions, Python, compiler or terminal commands are needed.
Steam's normal Play button still starts vanilla Barony.

### Alternative: extract the ZIP into the main game directory

1. Download [Dungeon-Delve-0.17.2-windows-x64.zip](https://github.com/vending-machine-git/dungeon-delve/releases/download/v0.17.2-launcher.2/Dungeon-Delve-0.17.2-windows-x64.zip)
   and check its SHA256 against the release's checksum file.
2. Right-click the ZIP, choose **Extract All**, and set the destination to the
   **Barony folder containing barony.exe**. Extract the ZIP's contents directly
   into that folder; do not add another outer Dungeon Delve folder.
3. Double-click **Dungeon Delve.exe** beside **barony.exe**.

The finished layout is:

```text
Barony/
  barony.exe                       <- original base game
  Dungeon Delve.exe                <- double-click this to play the mod
  Dungeon-Delve-0.17.2/             <- keep this supporting folder
    Launch.ps1
    release.json
    runtime/barony-rmf.exe
    mods/vending_machine_framework/
    mods/extraction-raid/
```

The launcher finds its supporting folder automatically. Retail files are not
replaced. GitHub's automatic **Source code** ZIP is not the playable package.

### Already have the complete 0.17.2 package? Add the root launcher

1. Download the small [Dungeon-Delve.exe](https://github.com/vending-machine-git/dungeon-delve/releases/download/v0.17.2-launcher.2/Dungeon-Delve.exe).
2. Save it as **Dungeon Delve.exe** in the **main Barony folder**, beside
   **barony.exe**. Leave the existing **Dungeon-Delve-0.17.2** supporting folder there.
3. Double-click the EXE in the main Barony folder. You can create a desktop shortcut to it.

The small EXE needs the complete supporting folder. It cannot run alone from
Downloads. Older launchers inside that supporting folder can remain in place.
For a fresh installation, use the complete installer or ZIP instead.

### Start a match with friends

Everyone uses the same release and launches the mod's EXE. Host or join a fresh
**Direct IP** lobby; all players must join before the match starts. Use the host's
IP and the port shown in the game. Steam invites/matchmaking and joining a match
in progress are not supported. Internet play may need host firewall/router setup.

### If the game does not start

If files are missing, check that **Dungeon Delve.exe** and the complete
**Dungeon-Delve-0.17.2** folder are both beside **barony.exe**. Extract the full ZIP
again if necessary. If the launcher cannot locate Barony, use **Choose Barony folder...**.
An older same-version supporting folder is preserved by Setup; rename that folder
before a full reinstall, or use the small root launcher with your existing package.

See [PLAYER-GUIDE.md](PLAYER-GUIDE.md) for checks, logs, updates and removal.
Personal campaign saves are not imported.

## Compatibility and limitations

- Current release: 0.17.2, protocol 28, vending_machine framework API 1.
- 0.17.2 restores native gameplay audio missing from 0.17.1; the required FMOD
  DLL is verified/copied from your own game rather than included in downloads.
- Windows x64, tested Steam Barony 5.0.2 executable/DLL hashes. Future game
  updates may require a matching release even if the version text is unchanged.
- Private Direct IP only; no Steam matchmaking/invites or join-in-progress.
  WAN/firewall/router setup and human balance testing remain limited.
- Retail executable, assets, saves and config are not replaced or imported.
  Sessions/logs stay under LocalAppData, separate from personal campaigns.
- Genuine DLC ownership gates remain intact. Never redistribute game assets/DLLs.
- Experimental, unsigned, unofficial software. Verify checksums and trust the
  publisher; do not disable antivirus. No auto-updater or background service.

## Source code

The C++ implementation is available in [src/](src/), including
[Dungeon Delve](src/raid.cpp) and the [framework](src/mod_framework.cpp).
The [launcher](framework/launcher/Launcher.cs) and
[installer](framework/installer/Installer.cs) C# sources are included too.
See [SOURCE.md](SOURCE.md) for build requirements, code locations, tests and licenses.
Source is committed to `main`; player binaries remain in the existing Releases.

## Repository layout

This repository contains source code and authored player content, without retail
assets, SDK caches, compiled build outputs, personal saves or local work logs.
`src/` contains the native code; `framework/` and `tests/` contain development
tools and test sources. `launcher/` contains player instructions, the custom
icon and portable launcher scripts,
`mods/` the two independent content roots, `licenses/` the notices and
`release-manifest.json` the tested release/hash inventory. Executables belong
in attached GitHub release downloads, not Git history. Launcher scripts alone
cannot run the game: download the complete binary release.

Each mod README retains its individual Workshop-package installation notes for
historical/standalone use; follow `PLAYER-GUIDE.md` for this bundled distribution.
Original RMF code uses BSD 2-Clause; see `licenses/LICENSE-RMF.txt`, engine and
third-party notices and the individual content licenses. Development assistance:
OpenAI Codex through T3 Code. Not affiliated with or endorsed by Turning Wheel.
