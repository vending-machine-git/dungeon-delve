# Dungeon Delve EXE launcher

Double-click `Dungeon Delve.exe` beside retail `barony.exe`. Its versioned supporting folder stays beside it. It opens a
small startup window, discovers the owned game, checks the release and starts
an isolated session. Startup failures remain visible with Retry and Choose
Barony folder buttons. There is no command window or administrator prompt.
Keep the EXE beside the complete Dungeon-Delve-0.17.2 supporting folder. Legacy launchers inside that folder remain supported.

Windows x64 with .NET Framework 4.5+ is required (included with Windows 10/11).
The launcher uses the existing PowerShell launch script in a hidden child
process, with a process-local execution-policy setting. It does not change the
machine's policy. No game/DLL/assets are embedded in this small launcher.
The full portable ZIP/Setup includes the native runtime and content separately.
The older CMD entrypoints remain available for support and framework-only use.

The original portal/stairs artwork is drawn by `framework/tools/draw_launcher_icon.py`
in the development workspace. PNG preview and ICO sizes 16/24/32/48/64/128/256
are provided; the ICO is embedded in the launcher and Setup. Artwork and launcher
use the project's BSD 2-Clause license. No retail artwork was copied.

From the development workspace:

```powershell
py -3.12 framework/tools/build_launcher.py --output <new-folder>
py -3.12 framework/tools/package_github_release.py --installer --output <new-release-folder>
```

The public source repository includes the launcher source, icon and manifest;
see the root SOURCE.md for native and C# build instructions.
The launcher can also be compiled from this directory using:

```powershell
& "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:winexe /platform:x64 /optimize+ '/out:Dungeon Delve.exe' /win32icon:DungeonDelve.ico /win32manifest:app.manifest /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll /reference:System.Core.dll Launcher.cs
```

Move the compiled EXE next to barony.exe and the complete Dungeon-Delve-0.17.2
supporting folder. Legacy placement beside Launch.ps1/release.json also works.
This compiles only the launcher, not the native game runtime.

Automation (no dialogs when arguments are supplied):

```powershell
& '.\Dungeon Delve.exe' --check --report checked.json
& '.\Dungeon Delve.exe' --prepare --game-dir 'F:\Steam\steamapps\common\Barony' --profile-root 'C:\Delve tests' --report prepared.json
```

Exit 0 means success; exit 1 means failure. `--check` creates no session;
`--prepare` stages one without opening a game. Omit both to launch. `--framework`
selects framework-only mode. Error reports contain status/error; successful
reports retain the launch script's version, content, profile and PID fields.
