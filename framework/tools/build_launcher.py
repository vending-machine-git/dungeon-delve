"""Build the windowed player launcher with its original, embedded multi-size icon."""
from pathlib import Path
import argparse
import json
import os
import subprocess

from package_release import ROOT, VERSION, digest


def build(destination):
    destination = Path(destination).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    executable = destination / 'Dungeon Delve.exe'
    if executable.exists():
        raise FileExistsError('Refusing to replace an existing launcher: ' + str(executable))
    compiler = Path(os.environ.get('WINDIR', r'C:\Windows')) / 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    source = ROOT / 'framework/launcher'
    if 'PayloadFolder = "Dungeon-Delve-' + VERSION + '"' not in (source / 'Launcher.cs').read_text():
        raise ValueError('Update the launcher payload-folder version before building this release')
    subprocess.run([str(compiler), '/nologo', '/target:winexe', '/platform:x64', '/optimize+',
                    '/out:' + str(executable), '/win32icon:' + str(source / 'DungeonDelve.ico'),
                    '/win32manifest:' + str(source / 'app.manifest'),
                    '/reference:System.Windows.Forms.dll', '/reference:System.Drawing.dll',
                    '/reference:System.Web.Extensions.dll', '/reference:System.Core.dll',
                    str(source / 'Launcher.cs')], check=True)
    return dict(version=VERSION, executable=str(executable), sha256=digest(executable),
                subsystem='Windows GUI', icon_sizes=[16, 24, 32, 48, 64, 128, 256],
                requires_administrator=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'framework/dist' / ('launcher-' + VERSION))
    print(json.dumps(build(parser.parse_args().output), indent=2))
