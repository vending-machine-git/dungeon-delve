# vending_machine framework 0.17.1 - Windows player runtime

Dungeon Delve 0.17.1 / protocol 28 / framework API 1. Experimental private
Direct IP play, 2-4 players. This is not an official Turning Wheel release.

## Install

1. Install the Steam Windows edition of Barony and run it normally once.
   This release supports the tested 64-bit Steam 5.0.2 executable only.
2. Subscribe to **vending_machine framework** and **Dungeon Delve** in Barony's
   Steam Workshop. Wait for Steam to finish both downloads.
3. Download **vending_machine-framework-0.17.1-windows.zip** from the release
   download linked by the framework Workshop description. Extract it to a
   writable folder outside Barony's installation directory.
4. Read this file and the licenses before running the unsigned native executable.
   Double-click `Play-Dungeon-Delve.cmd`. No Python, compiler, administrator
   access, replacement retail executable or global PowerShell policy change
   is needed. The wrapper bypasses script policy for its own process only.
5. Create a fresh private Direct IP lobby. Everyone must use this same runtime
   and both 0.17.1 content packages. Join before starting the match. Steam invites,
   Steam matchmaking and join-in-progress are not supported by this runtime.
   Use the Direct IP menu's displayed port; host firewall/router setup may be
   needed outside a LAN. WAN playtesting remains limited.

Subscribing alone does not install native code. Launching retail Barony from
Steam does not enable Dungeon Delve. Keep Steam running for subscribed downloads
and access to your installed game. Purchased assets and game DLLs are NOT included.
The launcher locates Steam libraries and copies nine verified DLLs from your own
game into an isolated session. It never replaces files in the retail install.

## If automatic discovery fails

Open a terminal in the extracted runtime folder, then run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Launch.ps1 -GamePath "D:\SteamLibrary\steamapps\common\Barony" -CheckOnly
```

For custom Workshop libraries, add
`-WorkshopRoot "D:\SteamLibrary\steamapps\workshop\content\371970"`, or provide
`-FrameworkContent "D:\mods\vending_machine_framework" -DelveContent "D:\mods\extraction-raid"`.
Each explicit folder must contain `mod.json`; no nested content folder.
Remove `-CheckOnly` to play. `-PrepareOnly` stages a session without opening a game.
Paths with spaces/brackets work. `-Mode Framework` launches without Dungeon Delve.
The framework-only launcher does not enable Bring Your Gear or Join In Progress.

## Saves, updates and removal

Each launch creates a fresh isolated session under
`%LOCALAPPDATA%\vending_machine_framework\0.17.1\sessions\`.
Do not import existing characters or campaign saves. Match progress is not a
cross-session campaign. Logs, results and `launch.json` remain inside that session;
the launcher prints its path. Retail saves/configuration are not copied or edited.
Back up normal Barony saves/config before trying any modded runtime.

To update, close modded games, update both subscriptions and extract the matching
runtime into a new folder. All peers must update. The launcher deliberately
rejects mismatched content, executable or DLL hashes, even if version strings
look identical. A future Steam update may need a new runtime release; do not
work around the check by replacing hashes. No auto-updater is installed.

To uninstall, close modded games, delete the extracted runtime and, optionally,
the isolated `%LOCALAPPDATA%\vending_machine_framework` folder, then unsubscribe.
Retail Barony remains untouched. Sessions accumulate; remove old sessions manually
only after closing their games and retaining any logs you want.

## Troubleshooting and limitations

- Missing content: finish Steam downloads, confirm both packages are 0.17.1,
  then use explicit content paths if necessary. Do not mix third-party mods.
- Unsupported game/DLL checksum: verify Barony files in Steam. If verification
  yields a newer build, wait for a framework update rather than editing hashes.
- Game will not open: include console output, `launch.json`, runtime/content
  versions, Steam game build and the session's game logs in a bug report.
  Remove private IPs/usernames before sharing logs. Never upload saves publicly.
- Defender/SmartScreen: this is unsigned third-party native code. Verify the
  release SHA256 and trust its publisher before running; do not disable antivirus.
- Windows x64 only for this download. FMOD/Steamworks/EOS are disabled. Dungeon
  Delve's supplied music uses SDL. Human balance and WAN testing remain limited.
- This is experimental private PvPvE, not a hardened public server or persistent
  economy. Do not expect retail matchmaking or use it for ranked play.

Licenses/notices are in `licenses/`. `release.json` records runtime, game and
local dependency hashes. No game DLLs or purchased assets are redistributed.
