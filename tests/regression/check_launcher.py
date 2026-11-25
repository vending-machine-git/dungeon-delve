"""Exercise the actual GUI EXE, path quoting, failure handling and optional game startup."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
GAME = Path(r'F:\Steam\steamapps\common\Barony')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--launch', action='store_true', help='launch/close the production game; back up saves/config first')
    args = parser.parse_args()
    source = args.bundle.resolve()
    evidence = ROOT / 'modding-evidence' / ('exe-launcher-' + uuid.uuid4().hex[:10])
    evidence.mkdir()
    # Exercise paths passed as arguments, not shell source, including non-ASCII and shell punctuation.
    bundle = evidence / "[portable] Delve & O'Brien Î©"
    shutil.copytree(source, bundle)
    executable = bundle / 'Dungeon Delve.exe'
    profiles = evidence / "[sessions] & O'Brien Î©"
    release = json.loads((bundle / 'release.json').read_text())
    assert digest(executable) == release['launcher_sha256']
    original_game_hash = digest(GAME / 'barony.exe')
    data = executable.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe + 4] == b'PE\0\0'
    assert struct.unpack_from('<H', data, pe + 24 + 68)[0] == 2  # GUI subsystem, no console.
    icon = (ROOT / 'framework/launcher/DungeonDelve.ico').read_bytes()
    assert struct.unpack_from('<HHH', icon) == (0, 1, 7)
    cases = {'windowed-exe-and-seven-icon-sizes': {'status': 'PASS'}}

    def run(name, extra=(), good=True, contains=None, binary=executable):
        report = evidence / (name + '.json')
        process = subprocess.run([str(binary), '--report', str(report), *map(str, extra)],
                                 cwd=Path(os.environ.get('WINDIR', r'C:\Windows')) / 'System32',
                                 capture_output=True, timeout=90)
        result = json.loads(report.read_text(encoding='utf-8-sig'))
        assert (process.returncode == 0) == good, result
        if contains:
            assert contains in result.get('error', ''), result
        cases[name] = dict(status='PASS', returncode=process.returncode, result=result)
        print('PASS ' + name, flush=True)
        return result

    checked = run('portable-discovery-unicode-paths', ['--check', '--profile-root', profiles])
    assert Path(checked['game']).samefile(GAME) and checked['profile_root'] == str(profiles)
    assert not profiles.exists()
    first = run('prepare-and-argument-quoting', ['--prepare', '--game-dir', str(GAME) + '\\', '--profile-root', profiles])
    second = run('fresh-session-every-launch', ['--prepare', '--profile-root', profiles])
    assert first['profile'] != second['profile']
    assert digest(Path(first['binary'])) == release['runtime_sha256']
    run('framework-mode', ['--check', '--framework'])
    run('protected-profile-rejected', ['--check', '--profile-root', GAME / 'do-not-create'], good=False, contains='ProfileRoot must be outside')
    run('missing-argument-rejected', ['--game-dir'], good=False, contains='Missing value')
    run('unknown-option-rejected', ['--unknown'], good=False, contains='Unknown option')
    isolated = evidence / '[incomplete]'
    isolated.mkdir()
    shutil.copy2(executable, isolated / executable.name)
    run('missing-release-shows-useful-error', ['--check'], good=False, contains='complete Dungeon Delve ZIP', binary=isolated / executable.name)
    runtime = bundle / 'runtime/barony-rmf.exe'
    with runtime.open('ab') as stream:
        stream.write(b'corrupt')
    run('checksum-error-propagated', ['--check'], good=False, contains='Runtime checksum mismatch')
    shutil.copy2(source / 'runtime/barony-rmf.exe', runtime)
    root_layout = evidence / '[root] portable'
    root_layout.mkdir()
    root_executable = root_layout / 'Dungeon Delve.exe'
    shutil.copy2(source.parent / 'Dungeon Delve.exe', root_executable)
    shutil.copytree(source, root_layout / 'Dungeon-Delve-0.20.2')
    root_checked = run('root-exe-resolves-sibling-support-folder', ['--check'], binary=root_executable)
    assert all(str(root_layout / 'Dungeon-Delve-0.20.2/mods') in path for path in root_checked['content'])
    compiler = Path(os.environ.get('WINDIR', r'C:\Windows')) / 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    harness = evidence / 'LauncherUiSmoke.exe'
    subprocess.run([str(compiler), '/nologo', '/target:winexe', '/out:' + str(harness),
                    '/reference:System.Windows.Forms.dll', '/reference:System.Drawing.dll',
                    str(ROOT / 'tests/regression/LauncherUiSmoke.cs')], check=True)
    ui_report = evidence / 'gui.txt'
    subprocess.run([str(harness), str(executable), str(GAME), str(ui_report), str(evidence)],
                   cwd=evidence, check=True, timeout=90)
    assert ui_report.read_text() == 'PASS'
    cases['gui-startup-error-browse-cancel-retry-and-icon'] = dict(status='PASS')
    print('PASS gui-startup-error-browse-cancel-retry-and-icon', flush=True)
    if args.launch:
        env = os.environ.copy()
        env['LOCALAPPDATA'] = str(evidence / '[double click] localappdata')
        process = subprocess.Popen([str(root_executable)], cwd=Path(env.get('WINDIR', r'C:\Windows')) / 'System32', env=env)
        launched = None
        try:
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline:
                reports = list(Path(env['LOCALAPPDATA']).rglob('launch.json'))
                if reports:
                    launched = json.loads(reports[0].read_text(encoding='utf-8-sig'))
                    break
                time.sleep(0.25)
            assert launched, 'Default EXE did not create an isolated launch report'
            process.wait(timeout=10)
            assert process.returncode == 0
            log = Path(launched['profile']) / 'log.txt'
            deadline = time.monotonic() + 35
            while time.monotonic() < deadline:
                text = log.read_text(errors='replace') if log.exists() else ''
                if 'giving keyboard to player 0' in text:
                    break
                time.sleep(0.5)
            else:
                raise AssertionError('Native startup did not complete')
            assert '[FMOD]: Current audio device:' in text and 'DISABLING AUDIO' not in text
            cases['double-click-exe-native-startup-and-audio'] = dict(status='PASS', result=launched)
            print('PASS double-click-exe-native-startup-and-audio', flush=True)
        finally:
            if launched:
                binary_literal = launched['binary'].replace("'", "''")
                cleanup = (f"$ownedProcess = Get-Process -Id {int(launched['pid'])} -ErrorAction SilentlyContinue; "
                           f"if ($ownedProcess -and $ownedProcess.Path -eq '{binary_literal}') {{ "
                           "$null = $ownedProcess.CloseMainWindow(); "
                           "if (!$ownedProcess.WaitForExit(5000)) { Stop-Process -Id $ownedProcess.Id } }")
                subprocess.run(['powershell.exe', '-NoProfile', '-Command', cleanup], cwd=evidence, check=True)
            if process.poll() is None:
                process.kill()
                process.wait()
    assert digest(GAME / 'barony.exe') == original_game_hash
    result = dict(status='PASS', cases=cases, launcher_sha256=digest(executable),
                  game_launched=args.launch, retail_executable_unchanged=True)
    (evidence / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print('RESULT PASS ' + str(evidence / 'result.json'), flush=True)


if __name__ == '__main__':
    main()
