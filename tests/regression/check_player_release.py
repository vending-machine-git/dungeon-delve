"""Exercise the extracted Windows player release without a game launch."""
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
RELEASE = ROOT / 'framework/dist/workshop-0.17.1'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--launch', action='store_true', help='also open/close the production runtime; back up retail saves/config first')
    args = parser.parse_args()
    evidence = ROOT / 'modding-evidence' / ('player-release-' + uuid.uuid4().hex[:10])
    evidence.mkdir()
    bundle = evidence / '[clean player] runtime'
    with zipfile.ZipFile(RELEASE / 'vending_machine-framework-0.17.1-windows.zip') as archive:
        assert archive.testzip() is None
        assert not any(name.endswith(('.dll', '.baronysave', '.cpp')) or 'smoke' in name for name in archive.namelist())
        assert [name for name in archive.namelist() if name.endswith('.exe')] == ['runtime/barony-rmf.exe']
        archive.extractall(bundle)
    workshop = evidence / '[Steam library] Workshop' / 'content' / '371970'
    for item_id, mod_id in [('1001', 'vending_machine_framework'), ('1002', 'extraction-raid')]:
        with zipfile.ZipFile(RELEASE / (mod_id + '-0.17.1-workshop.zip')) as archive:
            archive.extractall(workshop / item_id)
    for item_id, text in [('0000', '{broken'), ('0001', '{}')]:
        folder = workshop / item_id
        folder.mkdir()
        (folder / 'mod.json').write_text(text)
    profiles = evidence / '[isolated] profiles'
    base = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(bundle / 'Launch.ps1'),
            '-GamePath', str(GAME), '-WorkshopRoot', str(workshop), '-ProfileRoot', str(profiles)]
    cases = {}

    def run(name, extra=(), good=True, command=None):
        result = subprocess.run([*(command or base), *extra], cwd=evidence, text=True, capture_output=True)
        passed = (result.returncode == 0) == good
        cases[name] = dict(status='PASS' if passed else 'FAIL', returncode=result.returncode,
                           stdout=result.stdout, stderr=result.stderr)
        print(cases[name]['status'] + ' ' + name, flush=True)
        if not passed:
            print(result.stdout + result.stderr, flush=True)
        return json.loads(result.stdout) if result.returncode == 0 else None

    plan = run('discovery-spaces-brackets', ['-CheckOnly'])
    assert plan and len(plan['content']) == 2 and not profiles.exists()
    automatic = base[:]
    game_index = automatic.index('-GamePath')
    del automatic[game_index:game_index + 2]
    run('steam-library-auto-discovery', ['-CheckOnly'], command=automatic)
    first = run('prepare-clean-session', ['-PrepareOnly'])
    second = run('prepare-new-session', ['-PrepareOnly'])
    assert first and second and first['profile'] != second['profile']
    release = json.loads((bundle / 'release.json').read_text())
    assert digest(Path(first['binary'])) == release['runtime_sha256']
    for dependency in release['game_dlls']:
        assert digest(Path(first['binary']).parent / dependency['name']) == dependency['sha256']
    for mod_id in ['1001', '1002']:
        for source in (workshop / mod_id).rglob('*'):
            relative = source.relative_to(workshop / mod_id)
            if source.is_file() and relative.parts[0] in ('data', 'maps', 'music', 'images'):
                assert (Path(first['profile']) / relative).read_bytes() == source.read_bytes()
    run('framework-only', ['-Mode', 'Framework', '-CheckOnly'])
    run('missing-content-rejected', ['-DelveContent', str(evidence / 'missing'), '-CheckOnly'], good=False)
    manifest_path = workshop / '1002/mod.json'
    original = manifest_path.read_text()
    metadata = json.loads(original)
    metadata['version'] = '0.1.0'
    manifest_path.write_text(json.dumps(metadata))
    run('wrong-version-rejected', ['-DelveContent', str(workshop / '1002'), '-CheckOnly'], good=False)
    manifest_path.write_text(original)
    run('game-profile-rejected', ['-ProfileRoot', str(GAME / 'do-not-create'), '-CheckOnly'], good=False)
    binary = bundle / 'runtime/barony-rmf.exe'
    with binary.open('ab') as output:
        output.write(b'corrupt')
    run('damaged-runtime-rejected', ['-CheckOnly'], good=False)
    shutil.copy2(ROOT / 'framework/runtime/barony-rmf.exe', binary)
    fake_game = evidence / 'incompatible game'
    fake_game.mkdir()
    (fake_game / 'barony.exe').write_bytes(b'not supported')
    run('wrong-retail-build-rejected', ['-GamePath', str(fake_game), '-CheckOnly'], good=False)
    shutil.copy2(GAME / 'barony.exe', fake_game / 'barony.exe')
    run('missing-dll-rejected', ['-GamePath', str(fake_game), '-CheckOnly'], good=False)
    if args.launch:
        launched = run('production-launch')
        assert launched and launched['pid'] > 0
        try:
            log = Path(launched['profile']) / 'log.txt'
            deadline = time.monotonic() + 35
            while time.monotonic() < deadline:
                if log.is_file() and 'giving keyboard to player 0' in log.read_text(errors='replace'):
                    break
                time.sleep(0.5)
            else:
                raise AssertionError('Production runtime did not finish native startup')
            (evidence / 'production-startup.json').write_text(json.dumps(launched, indent=2) + '\n')
        finally:
            binary_literal = launched['binary'].replace("'", "''")
            process_id = int(launched['pid'])
            cleanup = (f"$process = Get-Process -Id {process_id} -ErrorAction SilentlyContinue; "
                       f"if ($process -and $process.Path -eq '{binary_literal}') {{ "
                       "$null = $process.CloseMainWindow(); "
                       f"if (!$process.WaitForExit(5000)) {{ Stop-Process -Id {process_id} }} }}")
            subprocess.run(['powershell.exe', '-NoProfile', '-Command', cleanup], cwd=evidence, check=True)
    report = dict(status='PASS' if all(case['status'] == 'PASS' for case in cases.values()) else 'FAIL',
                  cases=cases, prepared_profiles=[first, second], game_launch_performed=args.launch)
    (evidence / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print('RESULT ' + report['status'] + ' ' + str(evidence / 'result.json'))
    return int(report['status'] != 'PASS')


if __name__ == '__main__':
    sys.exit(main())
