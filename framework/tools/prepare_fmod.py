"""Prepare a local MinGW import adapter for the owner's FMOD DLL; ship no SDK files."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import urllib.request

CACHE = '/root/.cache/reyes-mod-framework/fmod-2.02.14'
GAME_DLL = '/mnt/f/Steam/steamapps/common/Barony/fmod.dll'
DLL_HASH = 'f0fb763aa0db5ff0b40930828d9078292add03386e76cec89956c58cc12b6920'
HEADER_REVISION = 'f32fbd4811e90c2433bf159aa40fb8f8aceb8b58'
HEADER_ROOT = 'https://raw.githubusercontent.com/ejoy/ant/' + HEADER_REVISION + '/3rd/fmod/windows/core/inc/'
HEADERS = ('fmod.h', 'fmod.hpp', 'fmod_common.h', 'fmod_codec.h', 'fmod_dsp.h',
           'fmod_dsp_effects.h', 'fmod_errors.h', 'fmod_output.h')


def run(*arguments, data=None):
    result = subprocess.run(['wsl.exe', '-d', 'Ubuntu-24.04', '--exec', *arguments],
                            input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    return result.stdout.decode('utf-8')


def write_cache(path, data):
    run('tee', path, data=data.encode('utf-8') if isinstance(data, str) else data)


def main():
    dll = Path(r'F:\Steam\steamapps\common\Barony\fmod.dll')
    if hashlib.sha256(dll.read_bytes()).hexdigest() != DLL_HASH:
        raise ValueError('The owned FMOD DLL does not match the tested 2.02.14 build')
    run('mkdir', '-p', CACHE + '/include', CACHE + '/lib')
    headers = {}
    for name in HEADERS:
        with urllib.request.urlopen(HEADER_ROOT + name, timeout=45) as response:
            data = response.read()
        headers[name] = data
        write_cache(CACHE + '/include/' + name, data)
    if b'#define FMOD_VERSION    0x00020214' not in headers['fmod_common.h']:
        raise ValueError('Wrong FMOD header API version')
    methods = []
    owner = None
    for line in headers['fmod.hpp'].decode('utf-8').splitlines():
        declaration = re.match(r'\s*class\s+(\w+)(?:\s*:\s*public\s+\w+)?\s*$', line)
        if declaration:
            owner = declaration.group(1)
        method = re.search(r'FMOD_RESULT\s+F_API\s+(\w+)\s*\(', line)
        if method:
            methods.append((owner, method.group(1)))
    if len(methods) != len(set(methods)) or not methods:
        raise ValueError('Unexpected overloaded or missing FMOD declarations')
    probe = '#include <fmod.hpp>\n' + '\n'.join(
        'auto probe_' + owner + '_' + name + ' = &FMOD::' + owner + '::' + name + ';'
        for owner, name in methods) + '\n'
    write_cache(CACHE + '/probe.cpp', probe)
    run('x86_64-w64-mingw32-g++', '-I' + CACHE + '/include', '-c', CACHE + '/probe.cpp', '-o', CACHE + '/probe.o')
    symbols = [line.split()[-1] for line in run('x86_64-w64-mingw32-nm', '--undefined-only', CACHE + '/probe.o').splitlines()]
    demangled = run('x86_64-w64-mingw32-c++filt', data=('\n'.join(symbols) + '\n').encode('ascii')).splitlines()
    export_table = run('x86_64-w64-mingw32-objdump', '-p', GAME_DLL)
    ordinal_base = int(re.search(r'Ordinal Base\s+(\d+)', export_table).group(1))
    exports = {}
    c_exports = []
    for line in export_table.splitlines():
        entry = re.match(r'\s*\[\s*(\d+)\]\s+([^\s]+)\s*$', line)
        if not entry:
            continue
        name = entry.group(2)
        ordinal = ordinal_base + int(entry.group(1))
        cpp = re.match(r'\?(\w+)@(\w+)@FMOD@@', name)
        if cpp:
            key = (cpp.group(2), cpp.group(1))
            if key in exports:
                raise ValueError('Ambiguous native FMOD method: ' + str(key))
            exports[key] = (name, ordinal)
        elif name.startswith('FMOD_'):
            c_exports.append((name, ordinal))
    aliases = []
    for symbol, readable in zip(symbols, demangled):
        method = re.match(r'FMOD::(\w+)::(\w+)\(', readable)
        if not method or method.groups() not in exports:
            raise ValueError('Cannot map FMOD method: ' + readable)
        aliases.append('    ' + symbol + ' @' + str(exports[method.groups()][1]) + ' NONAME')
    definition = 'LIBRARY fmod.dll\nEXPORTS\n' + '\n'.join('    ' + name + ' @' + str(ordinal) for name, ordinal in c_exports)
    definition += '\n' + '\n'.join(aliases) + '\n'
    write_cache(CACHE + '/fmod.def', definition)
    run('x86_64-w64-mingw32-dlltool', '-m', 'i386:x86-64', '-d', CACHE + '/fmod.def', '-l', CACHE + '/lib/libfmod.dll.a')
    provenance = dict(header_revision=HEADER_REVISION, header_root=HEADER_ROOT,
                      header_hashes={name: hashlib.sha256(data).hexdigest() for name, data in headers.items()},
                      owned_dll_sha256=DLL_HASH, cpp_methods=len(aliases), c_exports=len(c_exports),
                      target='Hash-pinned Windows x64 DLL ordinals; native C++ entrypoints, no C ABI bool conversions')
    write_cache(CACHE + '/provenance.json', json.dumps(provenance, indent=2) + '\n')
    print(json.dumps(dict(cache=CACHE, **provenance), indent=2))


if __name__ == '__main__':
    main()
