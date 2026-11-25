"""Exercise installer discovery and safe installation without opening Barony."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[2]
GAME = Path(r'F:\Steam\steamapps\common\Barony')
VERSION = '0.17.2'
FOLDER = 'Dungeon-Delve-' + VERSION


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(folder):
    return {path.relative_to(folder).as_posix(): digest(path) for path in folder.rglob('*') if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', type=Path, default=ROOT / 'framework/dist' / ('installer-' + VERSION))
    args = parser.parse_args()
    release_root = args.release.resolve()
    executable = release_root / ('Dungeon-Delve-' + VERSION + '-Setup.exe')
    archive_path = release_root / ('Dungeon-Delve-' + VERSION + '-windows-x64.zip')
    evidence = ROOT / 'modding-evidence' / ('installer-' + uuid.uuid4().hex[:10])
    evidence.mkdir()
    cases = {}
    with zipfile.ZipFile(archive_path) as archive:
        release = json.loads(archive.read(FOLDER + '/release.json'))
        expected = {entry.filename[len(FOLDER) + 1:]: hashlib.sha256(archive.read(entry)).hexdigest()
                    for entry in archive.infolist() if entry.filename.startswith(FOLDER + '/')}
        root_launcher = archive.read('Dungeon Delve.exe') if 'Dungeon Delve.exe' in archive.namelist() else None
    checksums = dict(line.split()[::-1] for line in (release_root / 'SHA256SUMS.txt').read_text().splitlines())
    assert checksums[executable.name] == digest(executable)
    assert checksums[archive_path.name] == digest(archive_path)
    originals = {GAME / 'barony.exe': digest(GAME / 'barony.exe')}
    for dependency in release['game_dlls']:
        path = GAME / dependency['name']
        originals[path] = digest(path)

    def run(name, options, good=True, contains=None):
        report = evidence / (name + '.json')
        process = subprocess.run([str(executable), *map(str, options), '--report', str(report)],
                                 cwd=evidence, capture_output=True, timeout=90)
        result = json.loads(report.read_text(encoding='utf-8-sig'))
        passed = (process.returncode == 0) == good and result['status'] == ('PASS' if good else 'FAIL')
        if contains:
            passed = passed and contains in result.get('error', '')
        cases[name] = dict(status='PASS' if passed else 'FAIL', returncode=process.returncode, result=result)
        print(cases[name]['status'] + ' ' + name, flush=True)
        assert passed, cases[name]
        return result

    steam = evidence / '[Steam root]'
    library = evidence / '[Other library]'
    primary = steam / 'steamapps/common/Barony'
    renamed = library / 'steamapps/common/Barony Custom'
    for game in (primary, renamed):
        game.mkdir(parents=True)
        shutil.copy2(GAME / 'barony.exe', game / 'barony.exe')
    (steam / 'steamapps/libraryfolders.vdf').write_text(
        '"libraryfolders"\n{\n"0" { "path" ' + json.dumps(str(steam))
        + ' }\n"1" { "path" ' + json.dumps(str(library)) + ' }\n}\n')
    (library / 'steamapps/appmanifest_371970.acf').write_text('"AppState" { "installdir" "Barony Custom" }')
    detected = run('multiple-libraries-and-appmanifest', ['--detect', '--steam-root', steam])
    assert set(detected['games']) == {str(primary), str(renamed)}
    (steam / 'steamapps/libraryfolders.vdf').write_text('"LibraryFolders" { "1" ' + json.dumps(str(library)) + ' }')
    detected = run('legacy-library-file-and-deduplication', ['--detect', '--steam-root', steam, '--steam-root', steam])
    assert len(detected['games']) == 2
    detected = run('empty-library-manual-fallback', ['--detect', '--steam-root', evidence / 'missing'])
    assert detected['games'] == []
    detected = run('actual-steam-discovery', ['--detect'])
    assert str(GAME).casefold() in {path.casefold() for path in detected['games']}
    run('missing-game-folder', ['--install', '--game-dir', evidence], good=False, contains='containing barony.exe')
    bad_game = evidence / '[wrong binary]'
    bad_game.mkdir()
    (bad_game / 'barony.exe').write_bytes(b'not the game')
    run('unsupported-build-refused', ['--install', '--game-dir', bad_game], good=False, contains='not the supported')
    assert not (bad_game / FOLDER).exists()
    run('missing-dll-refused', ['--install', '--game-dir', primary], good=False, contains='Missing or incompatible')
    for dependency in release['game_dlls']:
        shutil.copy2(GAME / dependency['name'], primary / dependency['name'])
    (primary / 'fmod.dll').write_bytes(b'incompatible audio')
    run('incompatible-audio-refused', ['--install', '--game-dir', primary], good=False, contains='fmod.dll')
    assert not (primary / FOLDER).exists()
    shutil.copy2(GAME / 'fmod.dll', primary / 'fmod.dll')
    (primary / 'savegames').mkdir()
    (primary / 'savegames/keep.baronysave').write_bytes(b'untouched private character')
    (primary / 'config.cfg').write_bytes(b'untouched config')
    protected = inventory(primary)
    run('manual-check-no-writes', ['--check', '--game-dir', primary])
    assert inventory(primary) == protected
    if root_launcher is not None:
        occupied = primary / 'Dungeon Delve.exe'
        occupied.write_bytes(b'unrelated user executable')
        run('unrelated-root-exe-preserved', ['--install', '--game-dir', primary], good=False, contains='different Dungeon Delve.exe')
        assert occupied.read_bytes() == b'unrelated user executable' and not (primary / FOLDER).exists()
        occupied.unlink()
        occupied.mkdir()
        run('root-exe-directory-preserved', ['--install', '--game-dir', primary], good=False, contains='folder already occupies')
        assert occupied.is_dir() and not (primary / FOLDER).exists()
        occupied.rmdir()
    run('manual-install-spaces-brackets', ['--install', '--game-dir', primary])
    installed = primary / FOLDER
    assert inventory(installed) == expected
    if root_launcher is not None:
        assert (primary / 'Dungeon Delve.exe').read_bytes() == root_launcher
        previous = ROOT / 'framework/dist/github-exe-launcher-0.17.2-final/player/Dungeon-Delve-0.17.2/Dungeon Delve.exe'
        shutil.copy2(previous, primary / 'Dungeon Delve.exe')
        run('known-previous-root-launcher-updated', ['--install', '--game-dir', primary])
        assert (primary / 'Dungeon Delve.exe').read_bytes() == root_launcher
        assert not list(primary.glob('.dd-launcher-*'))
    for relative, checksum in protected.items():
        assert digest(primary / relative) == checksum
    assert not list(primary.glob('.dd-*'))
    mtimes = {path: path.stat().st_mtime_ns for path in installed.rglob('*') if path.is_file()}
    if root_launcher is not None:
        mtimes[primary / 'Dungeon Delve.exe'] = (primary / 'Dungeon Delve.exe').stat().st_mtime_ns
    run('identical-install-no-rewrite', ['--install', '--game-dir', primary])
    assert all(path.stat().st_mtime_ns == timestamp for path, timestamp in mtimes.items())
    added = installed / 'user-notes.txt'
    added.write_bytes(b'keep my notes')
    run('extra-user-file-preserved', ['--install', '--game-dir', primary], good=False, contains='incomplete or modified')
    assert added.read_bytes() == b'keep my notes'
    added.unlink()
    readme = installed / 'README.md'
    readme.write_bytes(b'user-edited readme')
    modified = inventory(installed)
    run('modified-install-preserved', ['--install', '--game-dir', primary], good=False, contains='incomplete or modified')
    assert inventory(installed) == modified
    readme.unlink()
    incomplete = inventory(installed)
    run('incomplete-install-preserved', ['--install', '--game-dir', primary], good=False, contains='incomplete or modified')
    assert inventory(installed) == incomplete
    installed.rename(primary / (FOLDER + '-backup'))
    (primary / FOLDER).write_bytes(b'preexisting user file')
    run('destination-file-preserved', ['--install', '--game-dir', primary], good=False, contains='file already occupies')
    assert (primary / FOLDER).read_bytes() == b'preexisting user file'
    (primary / FOLDER).unlink()
    junction_target = evidence / 'junction-target'
    junction_target.mkdir()
    (junction_target / 'keep.txt').write_bytes(b'untouched junction target')
    subprocess.run(['cmd.exe', '/c', 'mklink', '/J', str(primary / FOLDER), str(junction_target)],
                   cwd=evidence, check=True, capture_output=True)
    run('junction-destination-refused', ['--install', '--game-dir', primary], good=False, contains='link or junction')
    assert inventory(junction_target) == {'keep.txt': hashlib.sha256(b'untouched junction target').hexdigest()}
    for dependency in release['game_dlls']:
        shutil.copy2(GAME / dependency['name'], renamed / dependency['name'])
    damaged = evidence / 'damaged-setup.exe'
    executable_bytes = bytearray(executable.read_bytes())
    archive_bytes = archive_path.read_bytes()
    embedded = executable_bytes.find(archive_bytes)
    assert embedded >= 0
    executable_bytes[embedded + len(archive_bytes) // 2] ^= 1
    damaged.write_bytes(executable_bytes)
    damaged_report = evidence / 'damaged-payload.json'
    damaged_process = subprocess.run([str(damaged), '--install', '--game-dir', str(renamed), '--report', str(damaged_report)],
                                     cwd=evidence, capture_output=True, timeout=90)
    damaged_result = json.loads(damaged_report.read_text())
    assert damaged_process.returncode == 1 and 'payload checksum failed' in damaged_result['error']
    assert not (renamed / FOLDER).exists() and not list(renamed.glob('.dd-*'))
    assert not (renamed / 'Dungeon Delve.exe').exists()
    cases['damaged-payload-refused'] = dict(status='PASS', result=damaged_result)
    print('PASS damaged-payload-refused', flush=True)
    deep_game = evidence / ('deep-' + 'x' * 70)
    deep_game.mkdir()
    for source in originals:
        shutil.copy2(source, deep_game / source.name)
    deep_before = inventory(deep_game)
    run('long-path-failure-rolls-back', ['--install', '--game-dir', deep_game], good=False, contains='too long')
    assert inventory(deep_game) == deep_before and not list(deep_game.glob('.dd-*'))
    compiler = Path(os.environ.get('WINDIR', r'C:\Windows')) / 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    harness = evidence / 'InstallerUiSmoke.exe'
    subprocess.run([str(compiler), '/nologo', '/target:exe', '/out:' + str(harness),
                    '/reference:System.Windows.Forms.dll', '/reference:System.Web.Extensions.dll',
                    str(ROOT / 'tests/regression/InstallerUiSmoke.cs')], check=True)
    shortcuts = evidence / '[isolated desktop]'
    shortcuts.mkdir()
    gui_report = evidence / 'gui.json'
    gui = subprocess.run([str(harness), str(executable), str(renamed), str(shortcuts), str(gui_report)],
                         cwd=evidence, capture_output=True, timeout=90, creationflags=subprocess.CREATE_NO_WINDOW)
    gui_result = json.loads(gui_report.read_text())
    assert gui.returncode == 0 and gui_result['status'] == 'PASS', gui_result
    assert inventory(renamed / FOLDER) == expected
    shortcut = Path(gui_result['isolated_shortcut'])
    script = "$link = (New-Object -ComObject WScript.Shell).CreateShortcut($args[0]); @{target=$link.TargetPath; working=$link.WorkingDirectory; icon=$link.IconLocation} | ConvertTo-Json -Compress"
    probe = evidence / 'read-shortcut.ps1'
    probe.write_text(script, encoding='utf-8')
    checked_link = subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(probe), str(shortcut)],
                                  cwd=evidence, text=True, capture_output=True, timeout=30, check=True)
    link_data = json.loads(checked_link.stdout)
    launcher_name = 'Dungeon Delve.exe' if 'Dungeon Delve.exe' in expected else 'Play-Dungeon-Delve.cmd'
    launcher_directory = renamed if root_launcher is not None else renamed / FOLDER
    assert Path(link_data['target']) == launcher_directory / launcher_name
    if launcher_name.endswith('.exe'):
        assert link_data['icon'] == str(launcher_directory / launcher_name) + ',0'
    assert Path(link_data['working']) == launcher_directory
    cases['gui-scan-browse-manual-install-and-shortcut'] = dict(status='PASS', result=gui_result)
    print('PASS gui-scan-browse-manual-install-and-shortcut', flush=True)
    for path, checksum in originals.items():
        assert digest(path) == checksum
    result = dict(status='PASS', version=VERSION, installer_sha256=digest(executable),
                  archive_sha256=digest(archive_path), cases=cases, installed_file_count=len(expected),
                  retail_files_unchanged=True, live_install_modified=False,
                  game_launched=False, desktop_shortcut_created=False)
    (evidence / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('PASS: ' + str(len(cases)) + ' cases; evidence: ' + str(evidence / 'result.json'))


if __name__ == '__main__':
    main()
