# GitHub owner publication checklist - 0.20.5

Prepared locally, never published automatically. This replaces Workshop as the
selected player-distribution route. Native FMOD audio is restored; gameplay
rules/protocol are unchanged. Do not replace the published 0.17.1 assets in place.

1. Run `py -3.12 framework/tools/package_github_release.py` from the development
   workspace. It refuses existing output directories; preserve previous evidence
   and use `--output` with a new directory when rebuilding.
2. Run `py -3.12 tests/regression/check_github_release.py` and existing package
   checks. Record the actual result rather than claiming WAN/subscriber testing.
3. Create a GitHub repository under the intended owner/name. Upload **only** the
   contents of `framework/dist/github-0.20.5/repository/`, including `.gitignore`.
   The generated repository now includes the owner-approved clean source tree.
   Do not upload local evidence, saves, SDK caches, DLLs, the `player/` staging
   directory or binaries to Git history. Source belongs in main; include it in
   a release only if the owner separately requests that.
4. Create a release/tag `v0.20.5`. Use the prepared RELEASE-NOTES.md and attach
   `Dungeon-Delve-0.20.5-windows-x64.zip` and `SHA256SUMS.txt` from the output
   directory. GitHub's automatic source archives are NOT playable packages.
   For an installer, build with `framework/tools/build_installer.py` or pass
   `--installer` when packaging. Run `tests/regression/check_installer.py`
   against that output, then attach `Dungeon-Delve-0.20.5-Setup.exe` as well.
   Use the installer's combined SHA256SUMS.txt covering both EXE and ZIP.
   Keep the published ZIP unchanged when adding an installer for that same ZIP;
   obtain owner approval before uploading assets/updating release metadata.
5. Test downloads while logged out, verify the downloaded ZIP checksum, and
   extract it into the Barony folder on a clean second PC/account with no
   Workshop subscriptions/developer tools. Extract directly into the game root
   and launch Dungeon Delve.exe beside barony.exe.
6. Test private Direct IP with matching releases, real per-player DLC ownership,
   all three floors, loot/returns/exits, retail files/saves/config unchanged and
   uninstall by removing only the versioned folder. Do not claim WAN/human
   balance acceptance until tested. Then publish the release when satisfied.

The `repository/` folder is not initialized with Git and has no remote. No GitHub
repository, release, tag or commit is created by the packager. Choose the actual
account/name/visibility yourself; there are no invented URLs or credentials.

For optional actual startup testing, first use Universal Modder to snapshot
retail saves/config and record exact paths in MODLOG.md, then run
`py -3.12 tests/regression/check_github_release.py --launch`. It opens the
production runtime and closes only its own exact PID after startup verification.
Default regression checks only verify/stage files and do not launch a game.
