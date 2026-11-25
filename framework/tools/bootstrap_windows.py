"""Download pinned official SDL2 development archives and prepare a MinGW sysroot.
The compiler and common headers come from Ubuntu packages in WSL. Game DLLs are
read only to generate local import libraries; no purchased assets are packaged.
"""
from pathlib import Path
import concurrent.futures
import hashlib
import json
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / 'build' / 'rmf-deps'
PREFIX = '/root/.cache/reyes-mod-framework/windows-prefix'
DOWNLOADS = [
    ('SDL2', '2.30.12'),
    ('SDL2_image', '2.8.2'),
    ('SDL2_net', '2.2.0'),
    ('SDL2_ttf', '2.22.0'),
]

def wsl(*args, **kwargs):
    return subprocess.run(['wsl.exe', '-d', 'Ubuntu-24.04', '--', *args], check=True, **kwargs)

def linux(path):
    p = Path(path).resolve()
    return '/mnt/' + p.drive[0].lower() + p.as_posix()[2:]

def download(spec):
    name, version = spec
    filename = f'{name}-devel-{version}-mingw.tar.gz'
    url = f'https://github.com/libsdl-org/{name.replace('SDL2', 'SDL', 1)}/releases/download/release-{version}/{filename}'
    archive = CACHE / filename
    if not archive.exists():
        print('Downloading ' + filename, flush=True)
        request = urllib.request.Request(url, headers={'User-Agent': 'ReyesModFramework-build/0.1'})
        with urllib.request.urlopen(request, timeout=90) as response:
            content = response.read()
        archive.write_bytes(content)
    sha256 = hashlib.sha256(archive.read_bytes()).hexdigest()
    destination = CACHE / f'{name}-{version}'
    marker = destination / '.extracted'
    if not marker.exists():
        destination.mkdir(parents=True, exist_ok=True)
        with tarfile.open(archive) as tar:
            tar.extractall(destination, filter='data')
        marker.touch()
    candidates = list(destination.rglob('x86_64-w64-mingw32'))
    if len(candidates) != 1:
        raise RuntimeError(f'Unexpected archive layout: {filename}')
    return dict(name=name, version=version, url=url, sha256=sha256, directory=str(candidates[0]))

def main():
    CACHE.mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
        records = list(executor.map(download, DOWNLOADS))
    wsl('mkdir', '-p', PREFIX + '/include/GL', PREFIX + '/lib')
    for record in records:
        wsl('cp', '-a', linux(record['directory']) + '/.', PREFIX + '/')
    for name in ('rapidjson', 'glm'):
        wsl('cp', '-a', '/usr/include/' + name, PREFIX + '/include/')
    for name in ('physfs.h', 'png.h', 'pngconf.h', 'pnglibconf.h'):
        wsl('cp', '/usr/include/' + name, PREFIX + '/include/' + name)
    for name in ('glew.h', 'wglew.h', 'glxew.h'):
        wsl('cp', '/usr/include/GL/' + name, PREFIX + '/include/GL/' + name)
    for dll, lib in [('physfs.dll', 'physfs'), ('glew32.dll', 'glew32'),
                     ('libpng16-16.dll', 'png16'), ('zlib1.dll', 'z')]:
        game_dll = '/mnt/f/Steam/steamapps/common/Barony/' + dll
        definition = dll[:-4] + '.def'
        for command in [('gendef', game_dll), ('x86_64-w64-mingw32-dlltool', '-d', definition, '-l', 'lib' + lib + '.dll.a')]:
            subprocess.run(['wsl.exe', '-d', 'Ubuntu-24.04', '--cd', PREFIX + '/lib', '--exec', *command], check=True)
    (CACHE / 'provenance.json').write_text(json.dumps(records, indent=2) + '\n')
    print('Windows dependency sysroot prepared: ' + PREFIX, flush=True)

if __name__ == '__main__':
    main()
