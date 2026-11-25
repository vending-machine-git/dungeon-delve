# Dungeon Delve

An unofficial Barony PvPvE battle-royale mod for private 2-4 player Direct IP
matches. Explore the Mines, Jungle and Crystal Caves, build around randomized
loot and abilities, return as monsters after death, and race for a final exit.

## Download and play

Start with the [latest release](https://github.com/vending-machine-git/dungeon-delve/releases/latest).
You need an owned Steam Windows installation of Barony, tested build 5.0.2,
and Windows x64 with .NET Framework 4.5+ (included with Windows 10/11).
Run normal Barony once after installing it, then keep Steam running.

### New players: use the installer

1. Download [Dungeon-Delve-0.17.2-Setup.exe](https://github.com/vending-machine-git/dungeon-delve/releases/download/v0.17.2-launcher.1/Dungeon-Delve-0.17.2-Setup.exe)
   and [SHA256SUMS.txt](https://github.com/vending-machine-git/dungeon-delve/releases/download/v0.17.2-launcher.1/SHA256SUMS.txt).
   Compare the download's SHA256 with its entry in that checksum file.
2. Run Setup. Choose your detected Barony installation, or click **Browse** and
   select the game folder containing `barony.exe`. You can find it in Steam:
   **Barony > Properties > Installed Files > Browse**.
3. Leave **Create a desktop shortcut** checked if you want one, then click **Install**.
4. Start **Dungeon Delve 0.17.2** from the desktop shortcut, or open the installed
   `Dungeon-Delve-0.17.2` folder and double-click **Dungeon Delve.exe**.

The installer includes the complete framework, mod content and native runtime.
No previous mod version, Workshop subscriptions, Python, compiler or terminal
commands are needed. Steam's normal Play button still starts vanilla Barony.

### Alternative: extract the full ZIP

1. Download [Dungeon-Delve-0.17.2-windows-x64.zip](https://github.com/vending-machine-git/dungeon-delve/releases/download/v0.17.2-launcher.1/Dungeon-Delve-0.17.2-windows-x64.zip)
   and verify it against the release's `SHA256SUMS.txt`.
2. Right-click the ZIP and choose **Extract All**. Extract into your Barony
   installation folder, or another writable folder if you prefer a portable copy.
3. Open the extracted **Dungeon-Delve-0.17.2** folder and double-click
   **Dungeon Delve.exe**. Extract all files before launching; keep the full folder together.

Do not use GitHub's automatic **Source code** ZIP; it is not the playable package.
The extracted folder should contain:

```text
Dungeon-Delve-0.17.2/
  Dungeon Delve.exe           <- double-click this
  Launch.ps1
  release.json
  runtime/barony-rmf.exe
  mods/vending_machine_framework/
  mods/extraction-raid/
```

### Already have the complete 0.17.2 package? Add just the EXE

1. Download the small [Dungeon-Delve.exe](https://github.com/vending-machine-git/dungeon-delve/releases/download/v0.17.2-launcher.1/Dungeon-Delve.exe)
   launcher from the release.
2. Put it inside your existing **Dungeon-Delve-0.17.2** folder, beside
   **Launch.ps1** and **release.json**. This is the mod folder, not the folder
   containing retail `barony.exe`.
3. Double-click **Dungeon-Delve.exe**. You can create a desktop shortcut to it.

This small download requires the existing complete package. It will not work
alone in Downloads. The old CMD launcher can stay. Use the full new installer
or ZIP if you also want the updated launch script and installer shortcut support.

### Start a match with friends

Everyone should use the same release and launch through the mod's EXE. Host or
join a fresh **Direct IP** lobby; all players must join before the match starts.
Use the host's IP and the port shown in the game. Steam invites/matchmaking and
joining a match already in progress are not supported. Internet connections
may need host firewall/router configuration.

### If the game does not start

The custom-icon launcher shows a startup window and closes it when the game opens.
If it cannot find Barony, click **Choose Barony folder...** and select the folder
containing `barony.exe`, then try again. If files are missing, extract the complete
ZIP again. If Setup finds an older same-version mod folder, close the mod and
rename that folder before reinstalling; Setup preserves it instead of overwriting it.

See [PLAYER-GUIDE.md](PLAYER-GUIDE.md) for compatibility checks, session logs,
updates, removal and troubleshooting. Personal campaign saves are not imported.

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

## Repository layout

This is a focused **distribution repository**, not the complete game source or
development workspace. `launcher/` contains player instructions, the custom
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
