# Source code and builds

The repository includes the current native C++ implementation, the C# launcher
and installer, build scripts, regression/smoke-test sources and authored mod content.
Source is committed to `main`; this source publication does not create a GitHub
release or change the existing player downloads. Players should still install
the tested binary package from Releases.

## Where to look

- `src/mod_framework.*`: vending_machine framework and custom-item integration.
- `src/raid.*`, `src/raid_world.cpp`, `src/delve_*`: Dungeon Delve mechanics,
  authored builds, loot, effects, interface and networking.
- `src/bring_gear.*`, `src/gear_loadout.*`, `src/join_progress.*`: independent
  normal-campaign features and shared loadout utilities.
- Remaining `src/`: the Barony engine and native integration changes, including
  the bundled ImGui source. `src/smoke/` is excluded from normal production builds.
- `framework/launcher/Launcher.cs`: custom-icon, windowed EXE launcher.
- `framework/installer/Installer.cs`: offline installer and game-root placement.
- `framework/player/`: the verified PowerShell/CMD launch flow.
- `framework/tools/`: builds, dependency preparation, packaging and authoring tools.
- `tests/`: native rule tests and isolated launcher/installer/game automation code.
- `mods/`: separate authored mod packages, with their own README/license/mod.json.

`SOURCE-MANIFEST.json` records hashes of exported source/content files. The engine
comes from the supplied Barony 5.0.2 open-source workspace; its exact upstream
commit was not recorded. Equal version strings do not establish equivalence to
the retail executable. Generated Config.hpp, SDK/dependency caches, binaries,
personal profiles/saves, recordings, historical backups and local work logs are
excluded. Historical snapshot-based patch export is unnecessary for this full
source tree and is not included.

## Licenses and dependencies

Original framework/Dungeon Delve code uses BSD 2-Clause (`LICENSE` and
`framework/LICENSE-RMF.txt`). The inherited Barony source is copyright Turning
Wheel LLC and contributors, under the retained BSD license in `LICENSE-Barony.txt`.
That file also retains bundled-library notices; additional original notices are
in `licenses/upstream/` and individual source headers. Bundled ImGui and the UTF-8
decoder retain their MIT notices. CMake discovery modules retain their own notices,
including the LGPL/static-link exception stated by the FMOD discovery module;
the LGPL 2.1 text is retained in `licenses/LGPL-2.1.txt`.

An owned compatible Barony installation supplies runtime assets and the verified
DLLs. No retail assets, DLLs, Steam/EOS credentials, SDK headers or import libraries
are committed here. FMOD/Steam/EOS and other external dependencies retain their own
licenses. Obtain any needed SDKs under their applicable terms.

## Build the Windows native runtime

The supported local recipe uses **Windows, Python 3.12+, WSL Ubuntu-24.04, CMake,
Ninja and the MinGW-w64 x86_64 POSIX toolchain**. The scripts were developed with
WSL's root user/cache at `/root/.cache/reyes-mod-framework` and an owned game at
`F:\Steam\steamapps\common\Barony`. Adjust the game locations in `build.py`,
`bootstrap_windows.py`, `prepare_fmod.py`, `package_release.py` and `launch.py`
for your machine. Do not assume these owner-specific defaults match your setup.

Install the compiler/tools and development headers in that WSL distribution:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix mingw-w64-tools libsdl2-dev libsdl2-image-dev libsdl2-net-dev libsdl2-ttf-dev libphysfs-dev libpng-dev libglew-dev rapidjson-dev libglm-dev zlib1g-dev
```

From the repository root in Windows PowerShell:

```powershell
py -3.12 framework/tools/bootstrap_windows.py
py -3.12 framework/tools/build.py --configure-only
py -3.12 framework/tools/build.py --jobs 4
```

The bootstrap downloads pinned official SDL development archives and derives
local import libraries from your owned game DLLs. FMOD preparation uses the
hash-pinned owned FMOD 2.02.14 DLL and a separately downloaded header cache; no
SDK files are exported to this repository. Normal builds disable Steamworks,
EOS and the editor. The Windows output is `framework/runtime/barony-rmf.exe`.
It does not overwrite retail `barony.exe`. `--audio none` is an explicit silent
development build; it is not the audio-enabled player release.

Use `--smoke-tests` to build the separate `barony-raid-smoke.exe` automation client.
Never share live profiles between production/smoke clients. Historical test runners
can expect local fixtures/tools not included in the source tree; never substitute
your personal saves. Build tools and package scripts do not publish anything.
Automation that calls Universal Modder accepts its CLI path through
`UNIVERSAL_MODDER`; otherwise it checks the documented Tools layout beneath
the current user's home directory. No contributor's absolute home path is required.

## Build the launcher and installer

The launcher needs the Windows .NET Framework C# compiler (included on the tested
Windows machine), not the native C++ compiler:

```powershell
py -3.12 framework/tools/build_launcher.py --output build/launcher
```

Place the resulting `Dungeon Delve.exe` beside retail `barony.exe` and the complete
`Dungeon-Delve-0.17.2` supporting folder. The EXE alone cannot run the game.
The original icon/manifest and source are in `framework/launcher/`. Regenerating
the icon with `draw_launcher_icon.py` additionally requires Pillow.

To wrap a complete verified player ZIP in Setup:

```powershell
py -3.12 framework/tools/build_installer.py --release path/to/verified-release --output build/setup
```

The packagers intentionally check pinned production/game/DLL hashes. Freshly
compiled executables can have different bytes; they are not automatically trusted
as the previously tested release. Review/revalidate a new build before creating
its own release inventory. Do not edit expected hashes merely to bypass a failed
compatibility check. Builds are not claimed to be byte-reproducible.

## Small native rule tests

These standalone tests do not open the game or use personal profiles. In WSL/Linux
with a C++17 compiler, run from the repository root:

```sh
mkdir -p build/rules
for name in battle_rules affix_rules skill_effect_rules spell_rolls; do
  g++ -std=c++17 -O2 "tests/regression/${name}_test.cpp" -o "build/rules/${name}"
  "build/rules/${name}"
done
```

Windows automation writes to the ignored `modding-evidence/` directory; create it
before the first run. On the documented Windows/WSL setup:

```powershell
New-Item -ItemType Directory -Force modding-evidence | Out-Null
py -3.12 tests/regression/check_dlc_entitlements.py
```

This also compiles isolated API fakes against the real ownership detector/validator.
It does not install fake DLLs or grant DLC in a game. Save/config snapshots are
required before any actual modded game launch; use disposable profiles only.
