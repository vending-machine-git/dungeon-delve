# Dungeon Delve

An unofficial Barony PvPvE battle-royale mod for private 2-4 player Direct IP
matches. Explore the Mines, Jungle and Crystal Caves, build around randomized
loot and abilities, return as monsters after death, and race for a final exit.

## Download and play

Open this repository's **Releases** section and download
**Dungeon-Delve-0.17.1-windows-x64.zip** plus **SHA256SUMS.txt**.
Do not use GitHub's automatic "Source code" ZIP: it does not contain the runtime.

1. Own/install the Steam Windows edition of Barony and run it once.
2. Extract the release ZIP into Barony's installation folder. It creates a
   separate `Dungeon-Delve-0.17.1` folder without replacing retail files.
3. Double-click `Dungeon-Delve-0.17.1/Play-Dungeon-Delve.cmd`.
4. Host/join a fresh private Direct IP lobby with friends using the same release.

**The framework and mod content are already included. No Workshop subscriptions,
Python, compiler, administrator access or separate installer are needed.**
Extracting elsewhere also works through Steam-library discovery. See
`PLAYER-GUIDE.md` for verification, manual paths, updates, removal and support.

## Compatibility and limitations

- Current release: 0.17.1, protocol 28, vending_machine framework API 1.
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
development workspace. `launcher/` contains the portable launcher scripts,
`mods/` the two independent content roots, `licenses/` the notices and
`release-manifest.json` the tested release/hash inventory. Executables belong
in attached GitHub release downloads, not Git history. Launcher scripts alone
cannot run the game: download the complete binary release.

Each mod README retains its individual Workshop-package installation notes for
historical/standalone use; follow `PLAYER-GUIDE.md` for this bundled distribution.
Original RMF code uses BSD 2-Clause; see `licenses/LICENSE-RMF.txt`, engine and
third-party notices and the individual content licenses. Development assistance:
OpenAI Codex through T3 Code. Not affiliated with or endorsed by Turning Wheel.
