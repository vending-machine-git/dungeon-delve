"""Build an offline Windows installer around an existing verified player ZIP."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import zipfile

from package_release import ROOT, VERSION, PRODUCTION_HASH, GAME_HASH, digest


def build(release_root, destination):
    release_root = release_root.resolve()
    destination = destination.resolve()
    archive_name = 'Dungeon-Delve-' + VERSION + '-windows-x64.zip'
    archive_path = release_root / archive_name
    checksums = dict(line.split()[::-1] for line in (release_root / 'SHA256SUMS.txt').read_text().splitlines() if line.strip())
    if checksums.get(archive_name) != digest(archive_path):
        raise ValueError('Player ZIP checksum does not match SHA256SUMS.txt')
    folder = 'Dungeon-Delve-' + VERSION
    files = {}
    root_launcher = None
    with zipfile.ZipFile(archive_path) as archive:
        if archive.testzip() is not None:
            raise ValueError('Player ZIP CRC check failed')
        for entry in archive.infolist():
            if entry.filename == 'Dungeon Delve.exe':
                if root_launcher is not None:
                    raise ValueError('Duplicated game-root launcher')
                root_launcher = hashlib.sha256(archive.read(entry)).hexdigest()
                continue
            if not entry.filename.startswith(folder + '/'):
                raise ValueError('Unexpected payload folder')
            relative = entry.filename[len(folder) + 1:]
            if (not relative or '\\' in relative or ':' in relative
                    or any(part in ('', '.', '..') or part.endswith(('.', ' ')) for part in relative.split('/'))
                    or relative.lower() in {name.lower() for name in files}):
                raise ValueError('Unsafe or duplicated payload path: ' + relative)
            if relative.lower().endswith(('.dll', '.baronysave', '.cpp', '.hpp')):
                raise ValueError('Forbidden player payload file: ' + relative)
            files[relative] = hashlib.sha256(archive.read(entry)).hexdigest()
        release = json.loads(archive.read(folder + '/release.json'))
    if release['version'] != VERSION or release['game_sha256'] != GAME_HASH or release['runtime_sha256'] != PRODUCTION_HASH:
        raise ValueError('Payload does not match the validated production release')
    if files.get('runtime/barony-rmf.exe') != PRODUCTION_HASH or 'Play-Dungeon-Delve.cmd' not in files:
        raise ValueError('Missing or incompatible runtime/launcher')
    if release.get('launcher_sha256') and files.get('Dungeon Delve.exe') != release['launcher_sha256']:
        raise ValueError('Missing or incompatible EXE launcher')
    if release.get('install_layout') == 'game-root-launcher' and root_launcher != release['launcher_sha256']:
        raise ValueError('Missing or incompatible game-root launcher')
    compiler = Path(os.environ.get('WINDIR', r'C:\Windows')) / 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    if not compiler.is_file():
        raise FileNotFoundError('Windows .NET Framework C# compiler is required to build the installer')
    destination.mkdir(parents=True, exist_ok=False)
    executable = destination / ('Dungeon-Delve-' + VERSION + '-Setup.exe')
    with tempfile.TemporaryDirectory(prefix='delve-installer-') as temporary:
        config = Path(temporary) / 'installer.json'
        configuration = dict(release=release, archive_sha256=digest(archive_path), files=files)
        if root_launcher is not None:
            configuration['root_launcher'] = root_launcher
        config.write_text(json.dumps(configuration), encoding='utf-8')
        subprocess.run([str(compiler), '/nologo', '/target:winexe', '/platform:x64', '/optimize+',
                        '/win32icon:' + str(ROOT / 'framework/launcher/DungeonDelve.ico'),
                        '/out:' + str(executable), '/reference:System.Windows.Forms.dll',
                        '/win32manifest:' + str(ROOT / 'framework/installer/app.manifest'),
                        '/reference:System.Drawing.dll', '/reference:System.Web.Extensions.dll',
                        '/reference:System.IO.Compression.dll', '/reference:System.Core.dll',
                        '/resource:' + str(archive_path) + ',payload.zip',
                        '/resource:' + str(config) + ',installer.json',
                        str(ROOT / 'framework/installer/Installer.cs')], check=True)
    shutil.copy2(archive_path, destination / archive_name)
    shutil.copy2(ROOT / 'framework/installer/README.md', destination / 'README.md')
    (destination / 'SHA256SUMS.txt').write_text(
        digest(executable) + '  ' + executable.name + '\n'
        + digest(archive_path) + '  ' + archive_name + '\n', encoding='ascii')
    report = dict(version=VERSION, installer=executable.name, installer_sha256=digest(executable),
                  archive_sha256=digest(archive_path), files=len(files), offline=True,
                  runtime_requirement='Windows x64 with .NET Framework 4.5 or later',
                  auto_elevation=False, game_files_redistributed=False)
    (destination / 'installer-build.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', type=Path, default=ROOT / 'framework/dist' / ('github-' + VERSION))
    parser.add_argument('--output', type=Path, default=ROOT / 'framework/dist' / ('installer-' + VERSION))
    args = parser.parse_args()
    print(json.dumps(build(args.release, args.output), indent=2))


if __name__ == '__main__':
    main()
