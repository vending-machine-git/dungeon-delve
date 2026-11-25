"""Launch vending_machine framework and selected content mods in an isolated profile."""
from pathlib import Path
import argparse
import json
import os
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
GAME = Path(r"F:\Steam\steamapps\common\Barony")
FRAMEWORK_ID = 'vending_machine_framework'
API_VERSION = 1

def resolve_mods(selected):
    """Read local package metadata and return dependencies before their dependents."""
    resolved = []
    loaded = {}
    visiting = set()
    def visit(mod_id):
        if mod_id in loaded:
            return loaded[mod_id]
        if not re.fullmatch(r'[a-z0-9][a-z0-9_-]*', mod_id):
            raise ValueError('Invalid mod folder: ' + mod_id)
        if mod_id in visiting:
            raise ValueError('Circular mod dependency: ' + mod_id)
        folder = ROOT / 'mods' / mod_id
        manifest_path = folder / 'mod.json'
        if not manifest_path.is_file():
            raise ValueError('Missing mod manifest: ' + str(manifest_path))
        manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
        if manifest.get('id') != mod_id or manifest.get('api_version') != API_VERSION:
            raise ValueError('Unsupported API or mismatched mod ID: ' + mod_id)
        requires = manifest.get('requires', {})
        if not isinstance(requires, dict):
            raise ValueError('Invalid dependency declaration: ' + mod_id)
        visiting.add(mod_id)
        for dependency, required_api in requires.items():
            metadata = visit(dependency)
            if metadata.get('api_version') != required_api:
                raise ValueError('Incompatible dependency API: ' + dependency)
        visiting.remove(mod_id)
        loaded[mod_id] = manifest
        resolved.append((folder, manifest))
        return manifest
    for mod_id in selected:
        visit(mod_id)
    return resolved

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--join-in-progress', action='store_true', help='allow private Direct IP joins after gameplay starts')
    parser.add_argument('--raid', '--delve', dest='raid', action='store_true', help='enable Dungeon Delve private PvPvE matches')
    parser.add_argument('--raid-import', type=Path, help=argparse.SUPPRESS)
    parser.add_argument('--bring-gear', action='store_true', help='enable Bring Your Gear for normal games')
    parser.add_argument('--gear-import', type=Path, help='campaign save whose physical gear to copy into new normal runs')
    parser.add_argument('--mod', action='append', default=[], help='load a folder under project/mods; repeat for multiple mods')
    parser.add_argument('--profile', type=Path)
    parser.add_argument('--connect-lobby', help='Steam invite code (steam:<lobby>) or lobby id to join once the game opens')
    parser.add_argument('--no-steam-invites', action='store_true', help='Direct IP only: do not start Steam lobbies or the relay')
    parser.add_argument('--dry-run', action='store_true', help='validate and show the launch plan without writing or opening the game')
    args = parser.parse_args()
    if args.raid_import:
        parser.error("Campaign gear import is now the separate Bring Your Gear mod. Use --gear-import for normal games.")
    selected = [FRAMEWORK_ID, *args.mod]
    if args.join_in_progress:
        selected.append('join-in-progress')
    if args.raid:
        selected.append('extraction-raid')
    if args.bring_gear or args.gear_import:
        selected.append('bring-your-gear')
    try:
        mods = resolve_mods(selected)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    raid = any(manifest.get('launcher_mode') == 'extraction-raid' for _, manifest in mods)
    bring_gear = any(manifest.get('launcher_mode') == 'bring-your-gear' for _, manifest in mods)
    join_in_progress = any(manifest.get('launcher_mode') == 'join-in-progress' for _, manifest in mods)
    if join_in_progress and raid:
        parser.error('Join In Progress supports normal campaigns; select it separately from Extraction Raid.')
    if raid and bring_gear:
        parser.error('Bring Your Gear is for normal games; select it separately from Extraction Raid.')
    if args.gear_import and (not args.gear_import.is_file() or args.gear_import.suffix.lower() != '.baronysave'):
        parser.error('--gear-import requires an existing .baronysave campaign file.')
    binary = ROOT / 'framework/runtime/barony-rmf.exe'
    if not binary.is_file():
        parser.error('Build the Windows runtime first; see framework/README.md')
    slug = ('dungeon-delve-' if raid else '') + '+'.join(manifest['id'] for _, manifest in mods if manifest['id'] != FRAMEWORK_ID) or 'framework-only'
    profile = args.profile or ROOT / 'framework/profiles' / slug
    profile = profile.resolve()
    cmd = [str(binary), '-windowed', '-size=1280x720', '-datadir=' + str(GAME)]
    connect_lobby = None
    if args.connect_lobby:
        text = args.connect_lobby.strip()
        text = text[6:].strip() if text.lower().startswith('steam:') else text
        if not text.isdigit() or int(text) == 0 or int(text) >= 2 ** 64:
            parser.error('Invalid Steam invite code: ' + args.connect_lobby)
        connect_lobby = str(int(text))
        cmd += ['+connect_lobby', connect_lobby]
    if args.no_steam_invites:
        cmd += ['-no-steam-invites']
    if join_in_progress:
        cmd += ['-join-in-progress']
    if raid:
        cmd += ['-raid-profile=' + str(profile / 'delve-profile.json')]
    if bring_gear:
        cmd += ['-bring-gear']
        if args.gear_import or (profile / 'gear-import.baronysave').is_file():
            cmd += ['-gear-import=' + str(profile / 'gear-import.baronysave')]
    menu_mods = resolve_mods(['extraction-raid']) if not (bring_gear or join_in_progress) else []
    if menu_mods and not raid:
        cmd += ['-delve-menu-profile=' + str(profile / 'delve-profile.json')]
    mod_ids = [manifest['id'] for _, manifest in mods]
    report = dict(framework='vending_machine framework', profile=str(profile), binary=str(binary),
                  selftest=False, mods=mod_ids, mode='extraction-raid' if raid else 'normal', gear_import=str(args.gear_import) if args.gear_import else None, join_in_progress=join_in_progress, connect_lobby=connect_lobby, steam_invites=not args.no_steam_invites)
    if args.dry_run:
        print(json.dumps(report, indent=2))
        return
    profile.mkdir(parents=True, exist_ok=True)
    selection = profile / 'rmf-mods.json'
    if selection.is_file() and json.loads(selection.read_text()) != mod_ids:
        parser.error('This profile uses a different mod set. Choose a fresh --profile for this selection.')
    if bring_gear and args.gear_import:
        destination = profile / 'gear-import.baronysave'
        if args.gear_import.resolve() != destination.resolve():
            shutil.copy2(args.gear_import, destination)
    for folder, _ in mods + menu_mods:
        for directory in ('data', 'maps', 'images', 'music'):
            content = folder / directory
            if content.is_dir():
                shutil.copytree(content, profile / directory, dirs_exist_ok=True)
    selection.write_text(json.dumps(mod_ids, indent=2) + '\n')
    env = os.environ.copy()
    env['PATH'] = str(GAME) + os.pathsep + env.get('PATH', '')
    # Steam keeps one client instance; -silent starts it or contacts that instance.
    # The runtime queries real entitlements and retries while the client starts.
    steam_executable = GAME.parents[2] / 'steam.exe'
    if os.name == 'nt' and steam_executable.is_file() and (GAME/'steam_api64.dll').is_file():
        subprocess.Popen([str(steam_executable), '-silent'], cwd=steam_executable.parent,
                         env=env, creationflags=subprocess.CREATE_NO_WINDOW)
    process = subprocess.Popen(cmd, cwd=profile, env=env)
    report['pid'] = process.pid
    (ROOT / 'framework/last-launch.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
