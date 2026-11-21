# Dungeon Delve 0.17.1 - Windows x64

Dungeon Delve and vending_machine framework are included together. No Workshop
subscriptions, separate framework download, Python, compiler or installer required.
Requires an owned Steam Windows installation of Barony, tested build 5.0.2.
Experimental private Direct IP play for 2-4 players; protocol 28, framework API 1.

## Install and play

1. Install Barony through Steam and run it normally once. Keep Steam running.
2. Download **Dungeon-Delve-0.17.1-windows-x64.zip** from this project's GitHub
   Releases page, not GitHub's automatic "Source code" downloads. Verify its
   SHA256 against the accompanying SHA256SUMS.txt from the trusted publisher.
3. Extract the ZIP into your Barony installation folder (Steam > Barony >
   Properties > Installed Files > Browse). The ZIP contains one new folder:

   ```text
   Barony/
     barony.exe                         <- original, never replace
     Dungeon-Delve-0.17.1/
       Play-Dungeon-Delve.cmd            <- double-click this
       Launch.ps1
       release.json
       runtime/barony-rmf.exe
       mods/vending_machine_framework/
       mods/extraction-raid/
   ```

4. Open **Dungeon-Delve-0.17.1** and double-click **Play-Dungeon-Delve.cmd**.
   A desktop shortcut to this file is optional. No administrator access or
   global PowerShell policy change is needed. Normal Steam Play starts vanilla.
5. Everyone uses the same release. Host/join a fresh private **Direct IP** lobby;
   everyone must join before starting. Use the port shown by the game; host
   firewall/router setup may be necessary. Steam matchmaking/invites and joining
   an already-running match are not supported.

You can also extract anywhere writable outside Barony; the launcher discovers
Steam libraries. If your game folder is protected, use that portable option
instead of running the mod as administrator. Keep the complete folder together.
Do not copy runtime files over retail files or mix releases/native frameworks.

## What the launcher does

It prefers the Barony installation directly above the extracted folder, then
Steam library discovery. It uses the two bundled mods, not Workshop downloads or
unrelated installed mods, and verifies their complete file lists and SHA256s.
It also verifies the native runtime, tested retail executable and nine DLLs
copied locally from your owned installation; game DLLs/assets are not shipped.
Matching version strings alone do not establish binary compatibility.

Each launch runs from a fresh isolated session under
`%LOCALAPPDATA%\vending_machine_framework\0.17.1\sessions\`.
Retail executable, assets, personal saves and configuration are not replaced,
imported or edited. Back up normal saves before trying unofficial software.
No background service, scheduled task or auto-updater is installed.

## Manual paths and checking installation

Open a terminal in the extracted Dungeon-Delve-0.17.1 folder:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Launch.ps1 -GamePath "D:\SteamLibrary\steamapps\common\Barony" -CheckOnly
```

Remove `-CheckOnly` to launch. `-PrepareOnly` stages an isolated session without
opening the game. Paths with spaces/brackets work. **Play-Framework.cmd** is an
optional framework-only normal-game launcher; it does not enable Bring Your
Gear or Join In Progress. Those separate mods are not included.

## Updates, uninstall and troubleshooting

- Close modded games before updating. Download/extract the complete new release
  into its own versioned folder; update your shortcut and have all peers update.
  Do not overlay versions or rely on automatic Workshop updates.
- If Barony updates and checksum checks fail, obtain a compatible mod release.
  Never edit expected hashes to bypass compatibility checks.
- To uninstall, close modded games and remove only the Dungeon-Delve versioned
  folder and its shortcut. Optionally remove old isolated sessions after keeping
  useful logs. Do not delete the Barony folder or personal saves.
- Missing/damaged bundled content: extract a fresh complete release ZIP rather
  than falling back to a different Workshop or local copy.
- Report release/game versions, reproduction steps and sanitized `launch.json`
  and session logs. Do not post private IPs, usernames or personal saves.
- This executable is unsigned third-party software. Trust the publisher and
  verify checksums; do not disable antivirus to run it.

## Game and release limits

Three six-minute floors, 2.5x XP, randomized rarity loot and character builds,
preserved within-match adventurer progression, monster returns, 45-second
repeat-rat cooldowns and ten-cheese Algernon evolution. Final-floor exits open
in the last minute; the first accepted claim wins. Full current rules are in
`mods/extraction-raid/README.md`; its historical Workshop installation section
does not apply to this combined GitHub release.

Windows x64 only, private Direct IP, limited WAN/human balance testing; not a
hardened public server. No Steam matchmaking/invites or EOS networking. FMOD is
absent; supplied Delve music uses SDL. DLC is optional; genuine per-player DLC
ownership remains required for DLC characters. No campaign imports or persistent
loot bank. Unofficial project, not affiliated with or endorsed by Turning Wheel.
License notices are in `licenses/` and each bundled mod. Development assistance:
OpenAI Codex through T3 Code.
