"""Check the combined portable release, without opening a game unless requested."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import time
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[2]
GAME = Path(r'F:\Steam\steamapps\common\Barony')
VERSION = '0.20.3'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--release', type=Path, default=ROOT / 'framework/dist/github-0.20.3')
    parser.add_argument('--launch', action='store_true', help='open/close production runtime; back up saves/config first')
    args = parser.parse_args()
    release_root = args.release.resolve()
    evidence = ROOT / 'modding-evidence' / ('github-release-' + uuid.uuid4().hex[:10])
    evidence.mkdir()
    game = evidence / '[owned game] Barony'
    game.mkdir()
    archive_path = release_root / ('Dungeon-Delve-' + VERSION + '-windows-x64.zip')
    checksums = dict(line.split()[::-1] for line in (release_root / 'SHA256SUMS.txt').read_text().splitlines() if line.strip())
    assert digest(archive_path) == checksums[archive_path.name]
    with zipfile.ZipFile(archive_path) as archive:
        assert archive.testzip() is None
        names = archive.namelist()
        assert all(name == 'Dungeon Delve.exe' or name.startswith('Dungeon-Delve-' + VERSION + '/') for name in names)
        assert not any(name.endswith(('.dll', '.baronysave', '.cpp', '.hpp', '.cs', '.pyc')) for name in names)
        assert sorted(name for name in names if name.endswith('.exe')) == [
            'Dungeon Delve.exe',
            'Dungeon-Delve-' + VERSION + '/Dungeon Delve.exe',
            'Dungeon-Delve-' + VERSION + '/runtime/barony-rmf.exe']
        for name in names:
            assert archive.read(name) == (release_root / 'player' / name).read_bytes()
        archive.extractall(game)
    bundle = game / ('Dungeon-Delve-' + VERSION)
    release = json.loads((bundle / 'release.json').read_text())
    assert release['audio_backend'] == 'fmod'
    assert digest(bundle / 'Dungeon Delve.exe') == release['launcher_sha256']
    assert digest(game / 'Dungeon Delve.exe') == release['launcher_sha256']
    assert any(dependency['name'] == 'fmod.dll' for dependency in release['game_dlls'])
    notices = (bundle / 'licenses/THIRD-PARTY-NOTICES.txt').read_text(encoding='utf-8')
    for path in sorted(ROOT.glob('LICENSE*.txt')):
        try:
            text = path.read_text(encoding='utf-8')
        except UnicodeDecodeError:
            text = path.read_text(encoding='cp1252')
        assert path.name in notices and text in notices
    protected = {GAME / 'barony.exe': digest(GAME / 'barony.exe'),
                 ROOT / 'framework/runtime/barony-rmf.exe': release['runtime_sha256']}
    for dependency in release['game_dlls']:
        source = GAME / dependency['name']
        assert digest(source) == dependency['sha256']
        protected[source] = digest(source)
        shutil.copy2(source, game / source.name)
    shutil.copy2(GAME / 'barony.exe', game / 'barony.exe')
    repository = release_root / 'repository'
    assert not (repository / '.git').exists()
    assert not any(path.suffix.lower() in ('.exe', '.dll', '.o', '.obj', '.pyc', '.baronysave', '.zip')
                   for path in repository.rglob('*') if path.is_file())
    if (repository / 'SOURCE-MANIFEST.json').is_file():
        assert (repository / 'src/raid.cpp').is_file()
        assert (repository / 'framework/launcher/Launcher.cs').is_file()
        assert not (repository / 'src/Config.hpp').exists()
    for content in release['bundled_content']:
        for entry in content['files']:
            for folder in (bundle / 'mods', repository / 'mods', ROOT / 'mods'):
                assert digest(folder / content['id'] / entry['path']) == entry['sha256']
    profiles = evidence / '[isolated] sessions'
    base = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(bundle / 'Launch.ps1'),
            '-ProfileRoot', str(profiles)]
    cases = {}

    def run(name, extra=(), good=True, command=None, contains=None):
        result = subprocess.run([*(command or base), *extra], cwd=evidence, text=True,
                                capture_output=True, timeout=90)
        passed = (result.returncode == 0) == good
        if contains:
            passed = passed and contains in result.stdout + result.stderr
        cases[name] = dict(status='PASS' if passed else 'FAIL', returncode=result.returncode,
                           stdout=result.stdout, stderr=result.stderr)
        print(cases[name]['status'] + ' ' + name, flush=True)
        if not passed:
            raise AssertionError(name + '\n' + result.stdout + result.stderr)
        return json.loads(result.stdout) if result.returncode == 0 else None

    checked = run('adjacent-game-and-bundled-content', ['-CheckOnly'])
    assert checked['game'] == str(game) and len(checked['content']) == 2 and not profiles.exists()
    assert all(str(bundle / 'mods') in path for path in checked['content'])
    root_report = evidence / 'root-launcher.json'
    subprocess.run([str(game / 'Dungeon Delve.exe'), '--check', '--profile-root', str(profiles),
                    '--report', str(root_report)], cwd=evidence, check=True, timeout=90)
    root_checked = json.loads(root_report.read_text())
    assert root_checked['game'] == str(game) and not profiles.exists()
    cases['root-exe-beside-retail-and-correct-supporting-folder'] = dict(status='PASS', result=root_checked)
    run('explicit-game-override', ['-GamePath', str(GAME), '-CheckOnly'])
    first = run('prepare-clean-session', ['-PrepareOnly'])
    second = run('prepare-new-session', ['-PrepareOnly'])
    assert first['profile'] != second['profile'] and digest(Path(first['binary'])) == release['runtime_sha256']
    for dependency in release['game_dlls']:
        assert digest(Path(first['binary']).parent / dependency['name']) == dependency['sha256']
    for content in release['bundled_content']:
        for entry in content['files']:
            if Path(entry['path']).parts[0] in ('data', 'maps', 'images', 'music'):
                assert digest(Path(first['profile']) / entry['path']) == entry['sha256']
    run('framework-only', ['-Mode', 'Framework', '-CheckOnly'])
    run('game-profile-rejected', ['-ProfileRoot', str(game / 'do-not-create'), '-CheckOnly'],
        good=False, command=base[:-2], contains='ProfileRoot must be outside')
    run('content-profile-rejected', ['-ProfileRoot', str(bundle / 'mods/extraction-raid/session'), '-CheckOnly'],
        good=False, command=base[:-2], contains='ProfileRoot must be outside')
    run('missing-explicit-content-rejected', ['-DelveContent', str(evidence / 'absent'), '-CheckOnly'], good=False)

    return finish_checks(args, release_root, evidence, game, bundle, release, protected, base, run, cases)


def finish_checks(args, release_root, evidence, game, bundle, release, protected, base, run, cases):
    metadata_path = bundle / 'mods/extraction-raid/mod.json'
    original_metadata = metadata_path.read_bytes()
    metadata = json.loads(original_metadata)
    metadata['version'] = '0.0.0'
    metadata_path.write_text(json.dumps(metadata))
    run('wrong-content-version-rejected', ['-CheckOnly'], good=False, contains='Missing extraction-raid')
    metadata_path.write_bytes(original_metadata)
    metadata = json.loads(original_metadata)
    metadata['requires']['vending_machine_framework'] = 99
    metadata_path.write_text(json.dumps(metadata))
    run('wrong-framework-dependency-rejected', ['-CheckOnly'], good=False, contains='Invalid Dungeon Delve framework dependency')
    metadata_path.write_bytes(original_metadata)
    content_file = bundle / 'mods/extraction-raid/README.md'
    original_content = content_file.read_bytes()
    content_file.write_bytes(original_content + b'corrupt')
    run('damaged-bundled-content-rejected', ['-CheckOnly'], good=False, contains='Bundled content checksum mismatch')
    content_file.write_bytes(original_content)
    extra = bundle / 'mods/extraction-raid/unexpected.txt'
    extra.write_text('not part of the release')
    run('extra-bundled-file-rejected', ['-CheckOnly'], good=False, contains='Bundled content files differ')
    extra.unlink()
    workshop = evidence / '[legacy Workshop]'
    shutil.copytree(bundle / 'mods', workshop)
    metadata_path.unlink()
    run('missing-bundled-mod-no-workshop-fallback', ['-WorkshopRoot', str(workshop), '-CheckOnly'],
        good=False, contains='Missing extraction-raid')
    metadata_path.write_bytes(original_metadata)
    incomplete = evidence / '[incomplete] bundle'
    incomplete.mkdir()
    for name in ('Launch.ps1', 'release.json'):
        shutil.copy2(bundle / name, incomplete / name)
    shutil.copytree(bundle / 'runtime', incomplete / 'runtime')
    incomplete_command = base[:]
    incomplete_command[incomplete_command.index('-File') + 1] = str(incomplete / 'Launch.ps1')
    run('missing-all-bundled-mods-no-workshop-fallback',
        ['-GamePath', str(game), '-WorkshopRoot', str(workshop), '-CheckOnly'],
        good=False, command=incomplete_command)
    runtime = bundle / 'runtime/barony-rmf.exe'
    with runtime.open('ab') as output:
        output.write(b'corrupt')
    run('damaged-runtime-rejected', ['-CheckOnly'], good=False, contains='Runtime checksum mismatch')
    shutil.copy2(ROOT / 'framework/runtime/barony-rmf.exe', runtime)
    dependency = release['game_dlls'][0]
    game_dll = game / dependency['name']
    game_dll.write_bytes(b'incorrect dll')
    run('wrong-local-dll-rejected', ['-CheckOnly'], good=False, contains='Missing or incompatible game dependency')
    shutil.copy2(GAME / dependency['name'], game_dll)
    (game / 'barony.exe').write_bytes(b'incorrect game')
    run('wrong-retail-build-rejected', ['-CheckOnly'], good=False, contains='not the tested Steam Windows')
    shutil.copy2(GAME / 'barony.exe', game / 'barony.exe')
    portable = evidence / '[portable] bundle'
    shutil.copytree(bundle, portable)
    portable_command = base[:]
    portable_command[portable_command.index('-File') + 1] = str(portable / 'Launch.ps1')
    automatic = run('portable-steam-discovery', ['-CheckOnly'], command=portable_command)
    assert Path(automatic['game']).samefile(GAME)
    assert all(str(portable / 'mods') in path for path in automatic['content'])
    launcher_literal = str(bundle / 'Play-Dungeon-Delve.cmd').replace("'", "''")
    profiles_literal = str(evidence / '[cmd] sessions').replace("'", "''")
    run('double-click-cmd-entrypoint', command=['powershell.exe', '-NoProfile', '-Command',
        f"& '{launcher_literal}' -CheckOnly -ProfileRoot '{profiles_literal}'; exit $LASTEXITCODE"])
    reused = subprocess.run([sys.executable, str(ROOT / 'framework/tools/package_github_release.py'),
                             '--output', str(release_root)], cwd=ROOT, text=True, capture_output=True)
    assert reused.returncode != 0 and 'FileExistsError' in reused.stderr
    cases['stale-package-output-rejected'] = dict(status='PASS', stderr=reused.stderr)
    if args.launch:
        launched = run('production-launch', ['-GamePath', str(GAME)])
        try:
            log = Path(launched['profile']) / 'log.txt'
            deadline = time.monotonic() + 35
            while time.monotonic() < deadline:
                if log.is_file() and 'giving keyboard to player 0' in log.read_text(errors='replace'):
                    break
                time.sleep(0.5)
            else:
                raise AssertionError('Production runtime did not finish native startup')
            startup = log.read_text(errors='replace')
            assert '[FMOD]: initializing FMOD' in startup and '[FMOD]: Current audio device:' in startup
            assert 'DISABLING AUDIO' not in startup
            (evidence / 'production-startup.json').write_text(json.dumps(launched, indent=2) + '\n')
        finally:
            binary_literal = launched['binary'].replace("'", "''")
            process_id = int(launched['pid'])
            cleanup = (f"$process = Get-Process -Id {process_id} -ErrorAction SilentlyContinue; "
                       f"if ($process -and $process.Path -eq '{binary_literal}') {{ "
                       "$null = $process.CloseMainWindow(); "
                       f"if (!$process.WaitForExit(5000)) {{ Stop-Process -Id {process_id} }} }}")
            subprocess.run(['powershell.exe', '-NoProfile', '-Command', cleanup], cwd=evidence, check=True)
    for path, expected in protected.items():
        assert digest(path) == expected
    report = dict(status='PASS', cases=cases, archive=str(release_root),
                  archive_sha256=digest(release_root / ('Dungeon-Delve-' + VERSION + '-windows-x64.zip')),
                  game_launch_performed=args.launch, protected_files_unchanged=True)
    (evidence / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print('RESULT PASS ' + str(evidence / 'result.json'), flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
