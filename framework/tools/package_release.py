"""Assemble Workshop content and a separate portable player runtime, never publish."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]
VERSION = '0.20.4'
GAME = Path(r'F:\Steam\steamapps\common\Barony')
PRODUCTION_HASH = 'c447b84053b2aa104e1d69d6dc9946eb8d612303f240d50c3dbebf0040dd5d81'
GAME_HASH = '8566da37bc39ea5a1ed08a8ad57608af4f019fb415869258fb3c1d310b4419e4'
DLLS = ('fmod.dll', 'glew32.dll', 'libfreetype-6.dll', 'libpng16-16.dll', 'physfs.dll',
        'SDL2.dll', 'SDL2_image.dll', 'SDL2_net.dll', 'SDL2_ttf.dll', 'zlib1.dll')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def archive(folder, destination):
    files = sorted(path for path in folder.rglob('*') if path.is_file())
    with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as output:
        for path in files:
            output.write(path, path.relative_to(folder).as_posix())
    with zipfile.ZipFile(destination) as output:
        assert output.testzip() is None
        assert output.namelist() == [path.relative_to(folder).as_posix() for path in files]
        for path in files:
            assert output.read(path.relative_to(folder).as_posix()) == path.read_bytes()


def main():
    runtime = ROOT / 'framework/runtime/barony-rmf.exe'
    if digest(runtime) != PRODUCTION_HASH or digest(GAME / 'barony.exe') != GAME_HASH:
        raise ValueError('Production runtime or tested retail build does not match release provenance')
    destination = ROOT / 'framework/dist' / ('workshop-' + VERSION)
    destination.mkdir(parents=True, exist_ok=True)
    bundle = destination / 'runtime-download'
    if bundle.exists():
        raise FileExistsError('Refusing to reuse a possibly stale bundle: ' + str(bundle))
    shutil.copytree(ROOT / 'framework/player', bundle)
    from build_launcher import build as build_launcher
    launcher = build_launcher(bundle)
    (bundle / 'runtime').mkdir()
    shutil.copy2(runtime, bundle / 'runtime/barony-rmf.exe')
    licenses = bundle / 'licenses'
    licenses.mkdir()
    for path in sorted(ROOT.glob('LICENSE*.txt')):
        shutil.copy2(path, licenses / path.name)
    shutil.copy2(ROOT / 'framework/LICENSE-RMF.txt', licenses / 'LICENSE-RMF.txt')
    release = dict(version=VERSION, protocol=35, api_version=1, platform='windows-x64',
                   launcher_sha256=launcher['sha256'],
                   game_version='5.0.2', game_sha256=GAME_HASH, runtime_sha256=PRODUCTION_HASH,
                   game_dlls=[dict(name=name, sha256=digest(GAME / name)) for name in DLLS])
    (bundle / 'release.json').write_text(json.dumps(release, indent=2) + '\n', encoding='utf-8')
    outputs = []
    for mod_id in ('vending_machine_framework', 'extraction-raid'):
        folder = ROOT / 'mods' / mod_id
        metadata = json.loads((folder / 'mod.json').read_text(encoding='utf-8'))
        assert metadata['version'] == VERSION and metadata['api_version'] == 1
        assert not any(path.suffix.lower() in ('.exe', '.dll', '.cpp', '.hpp', '.baronysave')
                       for path in folder.rglob('*') if path.is_file())
        output = destination / (mod_id + '-' + VERSION + '-workshop.zip')
        archive(folder, output)
        outputs.append(output)
    output = destination / ('vending_machine-framework-' + VERSION + '-windows.zip')
    archive(bundle, output)
    outputs.append(output)
    hashes = '\n'.join(digest(path) + '  ' + path.name for path in outputs) + '\n'
    (destination / 'SHA256SUMS.txt').write_text(hashes, encoding='ascii')
    for name in ('UPLOAD-CHECKLIST.md', 'framework-description.bbcode', 'delve-description.bbcode'):
        shutil.copy2(ROOT / 'framework/release' / name, destination / name)
    print(json.dumps(dict(destination=str(destination), artifacts=[str(path) for path in outputs],
                          hashes=hashes), indent=2))


if __name__ == '__main__':
    main()
