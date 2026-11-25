# Separate mod packages

Each immediate subfolder is a complete Workshop upload root with README, license, preview.png, mod.json, and game-relative content paths. Keep engine code, binaries, tools, tests, imported saves, profiles, and archives outside `mods/`.

| Folder | Purpose |
| --- | --- |
| `vending_machine_framework/` | Shared framework identity and API companion; native runtime installed separately |
| `join-in-progress/` | Independent normal multiplayer QoL: late arrivals through private Direct IP |
| `bring-your-gear/` | Independent normal-game QoL: copy saved physical gear into a new normal campaign |
| `extraction-raid/` | Private raid mode with raid-earned loot persistence; no campaign import |

All dependent packages require vending_machine framework API 1, not each other. The local launcher resolves mod.json dependencies. It enables Bring Your Gear with `--gear-import "<save.baronysave>"` or `--bring-gear`, and Extraction Raid with `--raid`. The features use separate profiles and launchers. Use `--join-in-progress` for the independent late-join mod; this prototype is selected separately from the gear-import and raid modes. Normal campaign saves work normally with Bring Your Gear.

Stage each individual mod folder under the game's `mods/` directory when ready to use Barony's uploader. Publish the framework companion separately, then configure its Workshop item under Required Items on dependent pages. These manifests do not automatically configure Steam dependencies or execute native code in vanilla Barony. The owner handles publication; public native installation, audio and Steam integration remain release work.
