# Owner release checklist - 0.17.1

Historical Workshop route. The owner now selected the combined GitHub ZIP:
see `github/PUBLISH-CHECKLIST.md` instead. Retained for reference only.

Prepared locally, NOT published. Runtime/content 0.17.1, protocol 28, API 1.
The two Workshop packages are content-only. The native runtime must be hosted
separately before making the Workshop entries public. No real download URL or
Workshop item ID has been supplied; description placeholders are intentional.

## Files and upload roots

- Framework upload root: `mods/vending_machine_framework/` (5 files).
- Dungeon Delve upload root: `mods/extraction-raid/` (23 files).
- Exact-root content archives: `vending_machine_framework-0.17.1-workshop.zip`
  and `extraction-raid-0.17.1-workshop.zip`. If using an archive, extract first;
  select the folder containing `mod.json`, README, license and preview directly.
- External player download: `vending_machine-framework-0.17.1-windows.zip`.
  Includes only the production executable, portable launcher, player guide,
  licenses and release manifest; no source, smoke binary, game DLLs or game assets.
- `SHA256SUMS.txt` contains all three archive hashes. Host the runtime archive
  and checksums together on a release page you control. Do not host your entire
  development workspace or redistribute the game's DLLs/assets.

## Publish order (owner-operated)

1. Host the separate runtime ZIP and SHA256SUMS on your chosen release service.
   Verify the download anonymously/while logged out. Record the final URL.
2. Use retail Barony's built-in Workshop uploader (not the Steamworks-disabled
   custom runtime). Create/update **vending_machine framework**, selecting its
   exact content root above. Keep initial visibility private/unlisted while
   checking. Use its included preview and `framework-description.bbcode`.
3. Replace `[RUNTIME_RELEASE_URL]` in that description with the real release page.
   Do not publish the placeholder. Record the resulting framework Workshop ID/URL.
4. Create/update **Dungeon Delve (Battle Royale)** from its own exact root and
   preview. Use `delve-description.bbcode`; replace `[FRAMEWORK_WORKSHOP_URL]`
   with the actual framework page URL. Suggested categories: Gameplay, Dungeons,
   Audio where available. Keep the package ID `extraction-raid` unchanged.
5. On Dungeon Delve's Workshop page, add **vending_machine framework** under
   **Required Items**. `mod.json` alone does not configure Steam's dependency UI.
6. Inspect both item pages: correct preview, version, Windows-only limitation,
   private Direct IP requirement and prominent separate-runtime warning/link.
   Update existing items rather than creating duplicate IDs for each patch.
7. Follow the clean-subscriber acceptance below, then set your desired visibility.
   Nothing in these tools performs uploads, publishes or changes Steam visibility.

## Clean-subscriber acceptance gate

- Subscribe to both items using a clean Workshop download, not local mod copies.
- Extract the externally hosted runtime ZIP to a new folder; verify its hash.
- Run `Play-Dungeon-Delve.cmd` with no developer paths/Python installed. If Steam
  discovery fails, test documented explicit game/content options and report it.
- Confirm 0.17.1 content is discovered, launch.json points to a new isolated
  session, and the retail executable/hash, personal saves and config are unchanged.
- On a second PC/account, host/join a private Direct IP match; confirm matching
  builds and genuine per-player DLC gates. Steam invites are NOT supported.
- Play all three floors: six-minute clocks, one hatch/three portals, first portal
  at 1:00 remaining/two at timeout, final-floor storm at 1:00; one winner only.
- Check randomized rarity loot, 2.5x XP, preserved death progression, monster
  picker/rat cooldown, cheese evolution, spell learning and readable rarity slots.
- Check runtime update rejection, uninstall steps and log-report instructions.

Local native gameplay/UI evidence predates this packaging-only change:
`modding-evidence/return-ui-validation.json`. Packaging/portable-launcher evidence
is recorded separately. Local tests cannot establish that public links, Steam
Required Items or remote subscriptions work until the owner completes these steps.
Do not claim clean-subscriber/WAN acceptance before actually performing it.

## Maintainer commands

From the source workspace, with the documented Python/toolchain available:

```text
python framework/tools/package_release.py
python tests/regression/check_player_release.py
python tests/regression/check_packages.py
```

After taking Universal Modder save/config snapshots, optionally run
`python tests/regression/check_player_release.py --launch` to validate real
production startup from the extracted archive. It closes only its own fixture
process; it does not play a multiplayer match or substitute for subscriber tests.

The packager deliberately refuses to reuse an existing runtime-download staging
folder; retain old evidence and move it aside before rebuilding. It pins the
verified production/retail hashes and only packages explicit production files.
No native gameplay rebuild is needed for this packaging/documentation update.
