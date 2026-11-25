"""Prepare a combined portable ZIP and a clean GitHub distribution repository."""
from pathlib import Path
import argparse
import json
import shutil
import zipfile

from package_release import ROOT, VERSION, GAME, PRODUCTION_HASH, GAME_HASH, DLLS, digest, archive
from prepare_fmod import DLL_HASH as FMOD_HASH
from build_launcher import build as build_launcher


def validate_content(folder):
    metadata = json.loads((folder / 'mod.json').read_text(encoding='utf-8'))
    if metadata['version'] != VERSION or metadata['api_version'] != 1:
        raise ValueError('Incompatible content: ' + str(folder))
    for path in [folder, *folder.rglob('*')]:
        if path.is_symlink() or path.is_junction():
            raise ValueError('Content must not contain links or junctions: ' + str(path))
    files = sorted(path for path in folder.rglob('*') if path.is_file())
    for path in files:
        if path.is_symlink() or path.suffix.lower() in ('.exe', '.dll', '.cpp', '.hpp', '.baronysave'):
            raise ValueError('Development/runtime files must not be packaged as content: ' + str(path))
    return dict(id=metadata['id'], files=[dict(path=path.relative_to(folder).as_posix(),
                                             sha256=digest(path)) for path in files])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, help='new output directory; existing directories are never reused')
    parser.add_argument('--installer', action='store_true', help='also compile the offline Windows installer (Windows build host required)')
    args = parser.parse_args()
    runtime = ROOT / 'framework/runtime/barony-rmf.exe'
    if digest(runtime) != PRODUCTION_HASH or digest(GAME / 'barony.exe') != GAME_HASH:
        raise ValueError('Production runtime or retail build does not match release provenance')
    if digest(GAME / 'fmod.dll') != FMOD_HASH:
        raise ValueError('FMOD DLL differs from the hash-pinned native import adapter')
    content_ids = ('vending_machine_framework', 'extraction-raid')
    contents = [validate_content(ROOT / 'mods' / mod_id) for mod_id in content_ids]
    destination = (args.output or ROOT / 'framework/dist' / ('github-' + VERSION)).resolve()
    destination.mkdir(parents=True, exist_ok=False)
    folder_name = 'Dungeon-Delve-' + VERSION
    bundle = destination / 'player' / folder_name
    shutil.copytree(ROOT / 'framework/player', bundle)
    launcher = build_launcher(bundle)
    shutil.copy2(bundle / 'Dungeon Delve.exe', bundle.parent / 'Dungeon Delve.exe')
    docs = ROOT / 'framework/release/github'
    shutil.copy2(docs / 'PLAYER-README.md', bundle / 'README.md')
    (bundle / 'runtime').mkdir()
    shutil.copy2(runtime, bundle / 'runtime/barony-rmf.exe')
    licenses = bundle / 'licenses'
    licenses.mkdir()
    notices = []
    for path in sorted(ROOT.glob('LICENSE*.txt')):
        try:
            text = path.read_text(encoding='utf-8')
        except UnicodeDecodeError:
            text = path.read_text(encoding='cp1252')
        notices.append('===== ' + path.name + ' =====\n\n' + text)
    (licenses / 'THIRD-PARTY-NOTICES.txt').write_text('\n\n'.join(notices) + '\n', encoding='utf-8')
    shutil.copy2(ROOT / 'framework/LICENSE-RMF.txt', licenses / 'LICENSE-RMF.txt')
    for mod_id in content_ids:
        shutil.copytree(ROOT / 'mods' / mod_id, bundle / 'mods' / mod_id)
    release = dict(version=VERSION, protocol=28, api_version=1, platform='windows-x64',
                   distribution='github-combined-portable', game_version='5.0.2',
                   audio_backend='fmod', audio_version='2.02.14',
                   distribution_revision='exe-launcher-2', install_layout='game-root-launcher',
                   launcher_sha256=launcher['sha256'],
                   game_sha256=GAME_HASH, runtime_sha256=PRODUCTION_HASH,
                   game_dlls=[dict(name=name, sha256=digest(GAME / name)) for name in DLLS],
                   bundled_content=contents)
    (bundle / 'release.json').write_text(json.dumps(release, indent=2) + '\n', encoding='utf-8')
    output = destination / ('Dungeon-Delve-' + VERSION + '-windows-x64.zip')
    archive(bundle.parent, output)
    with zipfile.ZipFile(output) as packaged:
        if not all(name == 'Dungeon Delve.exe' or name.startswith(folder_name + '/') for name in packaged.namelist()):
            raise ValueError('ZIP must contain the game-root launcher and its versioned supporting folder')
    (destination / 'SHA256SUMS.txt').write_text(digest(output) + '  ' + output.name + '\n', encoding='ascii')
    repository = destination / 'repository'
    repository.mkdir()
    shutil.copy2(docs / 'REPOSITORY-README.md', repository / 'README.md')
    shutil.copy2(docs / 'PLAYER-README.md', repository / 'PLAYER-GUIDE.md')
    shutil.copytree(licenses, repository / 'licenses')
    shutil.copy2(ROOT / 'framework/LICENSE-RMF.txt', repository / 'LICENSE')
    shutil.copytree(bundle / 'mods', repository / 'mods')
    (repository / 'launcher').mkdir()
    for name in ('Launch.ps1', 'Play-Dungeon-Delve.cmd', 'Play-Framework.cmd'):
        shutil.copy2(bundle / name, repository / 'launcher' / name)
    # Player archives stay binary-only; the repository also includes the owner-approved source.
    for name in ('DungeonDelve.ico', 'DungeonDelve.png'):
        shutil.copy2(ROOT / 'framework/launcher' / name, repository / 'launcher' / name)
    shutil.copy2(docs / 'LAUNCHER-README.md', repository / 'launcher/README.md')
    shutil.copy2(bundle / 'release.json', repository / 'release-manifest.json')
    (repository / '.gitignore').write_text('*.zip\n*.exe\n*.dll\n__pycache__/\n*.baronysave\n', encoding='ascii')
    from prepare_source_repository import export_sources
    export_sources(repository)
    for name in ('PUBLISH-CHECKLIST.md', 'RELEASE-NOTES.md'):
        shutil.copy2(docs / name, destination / name)
    report = dict(status='PREPARED_NOT_PUBLISHED', version=VERSION, archive=str(output),
                  sha256=digest(output), repository=str(repository),
                  runtime_sha256=PRODUCTION_HASH, game_sha256=GAME_HASH,
                  bundled_content_files=sum(len(content['files']) for content in contents))
    (destination / 'package.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if args.installer:
        from build_installer import build
        report['installer'] = build(destination, destination / 'installer')
        (destination / 'package.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
