# vending_machine framework

Native implementation: `../src/mod_framework.*`; framework-owned build, launcher
and installer sources live here. Dungeon Delve, Bring Your Gear and Join In
Progress retain independent content roots under `../mods/`. Internal `rmf` APIs,
runtime filename and package IDs are retained for compatibility.

Read [SOURCE.md](../SOURCE.md) for source locations, dependency/licensing notes,
Windows/WSL native builds, launcher/installer builds and standalone rule tests.
Read the root README/PLAYER-GUIDE for installation of tested player downloads.
The root launcher goes beside retail barony.exe and finds the adjacent versioned
supporting folder. Personal campaigns and purchased runtime data are not included.

`tools/prepare_source_repository.py` creates a source staging tree only. It never
uploads, commits or creates a GitHub release. Historical local backups, build
outputs, SDK caches and test profiles are deliberately excluded.
