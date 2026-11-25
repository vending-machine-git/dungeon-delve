# Dungeon Delve 0.17.2 - all-in-one Windows release

The complete Dungeon Delve Windows release, including vending_machine
framework, the native runtime, mod content, gameplay sound effects and music.
The installer and portable ZIP are full packages: no previous mod release is required.
This launcher revision places **Dungeon Delve.exe** beside retail **barony.exe**.
ZIP extraction and Setup both use that layout; supporting files stay in
**Dungeon-Delve-0.17.2**. Installer shortcuts target the root EXE and use its
original portal icon. Windowed startup/errors and the manual folder picker remain.
Native gameplay/runtime,
content version 0.17.2 and protocol 28 are unchanged.
You must own/install Barony. Required DLLs are verified and copied from your
own installation; no purchased DLLs/assets are redistributed.

## Recommended: installer

1. Own/install Barony through Steam and run it once. Keep Steam running.
2. Download **Dungeon-Delve-0.17.2-Setup.exe** and verify it against **SHA256SUMS.txt**.
3. Run Setup. Choose an automatically detected Barony installation, or use
   **Browse** / type the game folder containing barony.exe. Click **Install**.
4. Use the optional desktop shortcut, or run **Dungeon Delve.exe** beside **barony.exe** in the main game directory. Host/join a fresh private Direct IP lobby with
   2-4 players using the same release.

Setup includes everything offline, checks game compatibility, and adds the root
launcher plus its versioned supporting folder. Windows x64 with .NET Framework 4.5 or later is
required (included with Windows 10/11), along with write access to the game
directory. No admin request or automatic game launch. A modified/partial mod
folder is preserved; rename it yourself before reinstalling. An identical
existing install is accepted without rewriting it.

## Alternative: portable ZIP

Download **Dungeon-Delve-0.17.2-windows-x64.zip**, verify its checksum, extract
into your Barony folder and run **Dungeon Delve.exe** beside **barony.exe**.
The ZIP contains the same complete player files embedded in Setup. Do not
download GitHub's automatic Source code archives: they are not playable packages.

## Existing 0.17.2 installation: optional launcher only

Download **Dungeon-Delve.exe** and save it as **Dungeon Delve.exe** beside retail
**barony.exe**, with your existing **Dungeon-Delve-0.17.2** supporting folder next
to it. Double-click the root EXE. The small EXE alone in Downloads cannot play.
The full ZIP now extracts the launcher directly next to the base-game EXE;
Setup installs it there and creates shortcuts to that root launcher. Supporting
files stay in their own folder. The older public releases remain available.

No Workshop, separate framework download, Python, compiler or admin rights.
Retail barony.exe, assets, personal saves and config are not replaced. Portable
extraction elsewhere is also supported through Steam library discovery.

Windows x64; tested Steam Barony 5.0.2 binary hashes, protocol 28, API 1.
Native audio and volume controls are enabled. No Steam invites,
matchmaking, join-in-progress or runtime auto-updater. Experimental unsigned
software; do not disable antivirus. WAN/human balance testing remains limited.
Genuine per-player DLC gates remain intact; no game assets/DLLs are included.

Update every peer with the matching release, keeping the root launcher and its
supporting folder together. Uninstall by closing the game and removing only the
mod EXE, supporting folder and shortcut. Keep retail barony.exe and assets.
Read the included README for full support information.
Unofficial project, not affiliated with or endorsed by Turning Wheel.
