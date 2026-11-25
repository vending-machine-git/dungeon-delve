"""Export inspectable/buildable source to a repository, excluding local data and binaries.

This never creates a release, uploads files, or copies purchased game assets.
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
SOURCE_GROUPS = {
    'src': {'.cpp', '.hpp', '.h', '.inc', '.in', '.txt'},
    'cmake': {'.cmake'},
    'framework/tools': {'.py', '.cmake'},
    'framework/launcher': {'.cs', '.manifest', '.ico', '.png', '.md'},
    'framework/installer': {'.cs', '.manifest', '.md'},
    'framework/player': {'.ps1', '.cmd', '.md'},
    'framework/schemas': {'.json'},
    'framework/release': {'.md', '.bbcode'},
    'tests': {'.cpp', '.cs', '.py'},
}
EXCLUDED_FILES = {'src/Config.hpp', 'framework/tools/export_patch.py'}
SOURCE_IGNORE = '''*.zip
*.exe
*.dll
*.o
*.obj
*.a
*.lib
*.pdb
*.pyc
*.baronysave
*.key
*.pem
*.pfx
.env
.env.*
__pycache__/
/build/
/modding-evidence/
/framework/runtime/
/framework/profiles/
/framework/backups/
/framework/patch-baseline/
/framework/raid-handoff/
/framework/dist/
/framework/github/
/framework/audio-tools/
/framework/audio-models/
/framework/audio/
/src/Config.hpp
'''


def export_sources(destination, source_root=ROOT):
    destination = Path(destination).resolve()
    source_root = Path(source_root).resolve()
    if destination == source_root:
        raise ValueError('Source export destination must be separate from the development workspace')
    destination.mkdir(parents=True, exist_ok=True)
    records = []

    def copy(path, relative=None):
        if path.is_symlink() or path.is_junction():
            raise ValueError('Source export must not follow links: ' + str(path))
        relative = relative or path.relative_to(source_root).as_posix()
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        records.append(dict(path=relative, sha256=hashlib.sha256(target.read_bytes()).hexdigest()))

    for group, extensions in SOURCE_GROUPS.items():
        for path in sorted((source_root / group).rglob('*')):
            if not path.is_file() or path.suffix.lower() not in extensions:
                continue
            if '__pycache__' in path.parts or path.relative_to(source_root).as_posix() in EXCLUDED_FILES:
                continue
            copy(path)
    for name in ('CMakeLists.txt', 'framework/LICENSE-RMF.txt', 'framework/slots.json'):
        copy(source_root / name)
    engine_license = source_root / 'LICENSE.txt'
    if not engine_license.is_file():
        engine_license = source_root / 'LICENSE-Barony.txt'
    copy(engine_license, 'LICENSE-Barony.txt')
    copy(source_root / 'framework/LICENSE-RMF.txt', 'LICENSE')
    notices = sorted(source_root.glob('LICENSE.*.txt')) or sorted((source_root / 'licenses/upstream').glob('LICENSE.*.txt'))
    for path in notices:
        copy(path, 'licenses/upstream/' + path.name)
    for path in sorted((source_root / 'mods').rglob('*')):
        if path.is_file():
            if path.suffix.lower() in {'.exe', '.dll', '.baronysave', '.pyc'}:
                raise ValueError('Unexpected binary/private file in authored mod content: ' + str(path))
            copy(path)
    copy(source_root / 'framework/release/github/SOURCE.md', 'SOURCE.md')
    copy(source_root / 'framework/release/github/FRAMEWORK-SOURCE-README.md', 'framework/README.md')
    module_license = source_root / 'framework/release/github/licenses/LGPL-2.1.txt'
    if not module_license.is_file():
        module_license = source_root / 'licenses/LGPL-2.1.txt'
    copy(module_license, 'licenses/LGPL-2.1.txt')
    # Resource scripts use our original icon, not extracted retail artwork.
    for name in ('barony.rc', 'editor.rc'):
        (destination / name).write_text('1 ICON "framework/launcher/DungeonDelve.ico"\n', encoding='utf-8', newline='\n')
        records.append(dict(path=name, sha256=hashlib.sha256((destination / name).read_bytes()).hexdigest()))
    (destination / '.gitignore').write_text(SOURCE_IGNORE, encoding='utf-8', newline='\n')
    manifest = dict(format_version=1, native_version='0.17.2', framework_api=1, protocol=28,
                    upstream='https://github.com/TurningWheel/Barony',
                    upstream_commit='not recorded in the supplied workspace',
                    scope='Current source, build tools and authored content; no retail data or compiled binaries',
                    files=sorted(records, key=lambda entry: entry['path']))
    (destination / 'SOURCE-MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    return dict(files=len(records), cpp_files=sum(entry['path'].endswith('.cpp') for entry in records),
                csharp_files=sum(entry['path'].endswith('.cs') for entry in records), destination=str(destination))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True, help='new source staging directory')
    output = parser.parse_args().output.resolve()
    if output.exists():
        raise FileExistsError('Refusing to reuse an existing source staging directory: ' + str(output))
    print(json.dumps(export_sources(output), indent=2))
