# Dungeon Delve Windows installer

`Dungeon-Delve-0.20.3-Setup.exe` is a complete offline installer containing the
same tested player ZIP as the portable release. Players need Windows x64 with
.NET Framework 4.5 or later (included with Windows 10/11), an owned compatible
Steam Barony installation, and write access to its directory. No Python,
compiler, download during installation, or separate framework setup is needed.

## Player flow

1. Run Setup. It checks Steam's registry entries, standard Steam locations,
   libraryfolders.vdf and Barony's app manifest (371970) for installations.
2. Choose a detected folder or use Browse/paste a manual path containing
   `barony.exe`. Multiple Steam libraries and renamed app-manifest directories
   are supported. Setup does not scan entire disks.
3. Leave the optional desktop shortcut checked and click Install.
4. Start the desktop shortcut or `Dungeon Delve.exe` beside retail `barony.exe`.
   Keep Steam running. Steam's normal Play continues to launch vanilla Barony.
   The EXE and shortcut use an original portal icon. Startup opens without a
   command window; failures offer Retry and a game-folder picker.

The installer verifies the original executable and all ten required local
DLLs, including FMOD. Incompatible builds are rejected before extracting.
It verifies its embedded ZIP and each installed file, stages under a unique
temporary directory inside the selected game, then moves the completed folder
into place, then installs the verified launcher beside barony.exe. Retail files, saves, config and old mod versions are not overwritten.
An identical existing install is accepted without rewriting it; a partial or
modified install is refused. Rename that versioned folder yourself to reinstall.
An existing desktop shortcut with the same name is left untouched.

No admin request, background service, auto-updater, registry registration or
game launch occurs. A protected installation can instead use the portable ZIP
in a writable folder. Uninstall by closing the mod and removing only its
Dungeon Delve.exe, its versioned supporting folder and shortcut. Session logs remain under LocalAppData.
Unsigned software may trigger Windows reputation warnings; verify the publisher
and SHA256 rather than disabling antivirus.

## Build and test

From the development workspace on Windows:

```powershell
py -3.12 framework/tools/build_installer.py
py -3.12 tests/regression/check_installer.py
```

The builder consumes a verified 0.20.3 release ZIP (pass `--release` for a newly
prepared launcher revision). It also accepts older CMD-only archives without
changing their payload; shortcuts then use their legacy entrypoint. Output defaults
to `framework/dist/installer-0.20.3/` and refuses an existing output directory.
Use `--output` with a fresh directory when rebuilding. It invokes Windows'
installed .NET Framework C# compiler, embeds the player ZIP and file inventory,
and writes Setup, a copy of the ZIP, SHA256SUMS.txt (both downloads), README and build
provenance. No purchased DLLs or SDK headers are embedded. Compilations are
unsigned and not byte-reproducible; recheck SHA256 after every build.

`package_github_release.py --installer --output <new-directory>` optionally
builds both formats in one pass, placing installer assets under `installer/`.
Publish the installer, ZIP and the installer's combined SHA256SUMS.txt together
only after validation and owner approval. No build/test command publishes.

Automation (does not open a game or create desktop shortcuts):

```powershell
Dungeon-Delve-0.20.3-Setup.exe --detect --report detected.json
Dungeon-Delve-0.20.3-Setup.exe --check --game-dir "F:\Steam\steamapps\common\Barony" --report checked.json
Dungeon-Delve-0.20.3-Setup.exe --install --game-dir "F:\Steam\steamapps\common\Barony" --report installed.json
```

`--steam-root <path>` may be repeated with `--detect` to scan only specified
Steam roots for isolated tests. Exit code 0 indicates success, 1 failure.
Regression tests install only into copied game fixtures, never the live game.
