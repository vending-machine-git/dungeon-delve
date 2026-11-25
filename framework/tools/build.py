"""Build RMF with the local WSL toolchain; output never replaces Steam files."""
from pathlib import Path
import argparse
import hashlib
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[2]
def linux(path):
    p=Path(path).resolve()
    return '/mnt/' + p.drive[0].lower() + p.as_posix()[2:]
def run(*args):
    subprocess.run(['wsl.exe', '-d', 'Ubuntu-24.04', '--exec', *args], check=True)
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--platform', choices=['windows','linux'], default='windows')
    parser.add_argument('--configure-only', action='store_true')
    parser.add_argument('--smoke-tests', action='store_true', help='build a separate raid automation executable')
    parser.add_argument('--audio', choices=['fmod', 'none'], help='Windows defaults to native FMOD; none is for explicit silent development builds')
    parser.add_argument('--jobs', default='6')
    args=parser.parse_args()
    audio=args.audio or ('fmod' if args.platform=='windows' else 'none')
    fmod='/root/.cache/reyes-mod-framework/fmod-2.02.14'
    if audio=='fmod':
        if args.platform!='windows':
            parser.error('The pinned FMOD import adapter is Windows x64 only; use --audio none for Linux')
        from prepare_fmod import DLL_HASH
        if hashlib.sha256(Path(r'F:\Steam\steamapps\common\Barony\fmod.dll').read_bytes()).hexdigest()!=DLL_HASH:
            raise ValueError('The owned FMOD DLL differs from the pinned native import adapter')
        present=subprocess.run(['wsl.exe','-d','Ubuntu-24.04','--exec','test','-f',fmod+'/lib/libfmod.dll.a'])
        if present.returncode:
            subprocess.run([sys.executable,str(ROOT/'framework/tools/prepare_fmod.py')],check=True)
    source='/root/.cache/reyes-mod-framework/source'
    run('mkdir','-p','/root/.cache/reyes-mod-framework')
    run('ln','-sfnT',linux(ROOT),source)
    build='/root/.cache/reyes-mod-framework/' + args.platform + ('-audio' if audio=='fmod' else '') + ('-smoke-safe' if args.smoke_tests else '-safe')
    cmd=['cmake','-S',source,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
         '-DFMOD_ENABLED='+('ON' if audio=='fmod' else 'OFF'),'-DOPENAL_ENABLED=OFF','-DSTEAMWORKS_ENABLED=OFF','-DEOS_ENABLED=OFF','-DEDITOR_ENABLED=OFF', '-DBARONY_SMOKE_TESTS=' + ('ON' if args.smoke_tests else 'OFF')]
    if audio=='fmod':
        cmd += ['-DFMOD_INCLUDE_DIR='+fmod+'/include','-DFMOD_LIBRARY='+fmod+'/lib/libfmod.dll.a']
    if args.platform=='windows':
        prefix='/root/.cache/reyes-mod-framework/windows-prefix'
        cmd += ['-DCMAKE_TOOLCHAIN_FILE='+source+'/framework/tools/mingw-toolchain.cmake',
                '-DCMAKE_PREFIX_PATH='+prefix, '-DSDL2_INCLUDE_DIR='+prefix+'/include/SDL2',
                '-DSDL2_LIBRARY='+prefix+'/lib/libSDL2.dll.a',
                '-DSDL2MAIN_LIBRARY='+prefix+'/lib/libSDL2main.a',
                '-DSDL2_IMAGE_INCLUDE_DIR='+prefix+'/include/SDL2',
                '-DSDL2_IMAGE_LIBRARY='+prefix+'/lib/libSDL2_image.dll.a',
                '-DSDL2_NET_INCLUDE_DIR='+prefix+'/include/SDL2',
                '-DSDL2_NET_LIBRARY='+prefix+'/lib/libSDL2_net.dll.a',
                '-DSDL2_TTF_INCLUDE_DIR='+prefix+'/include/SDL2',
                '-DSDL2_TTF_LIBRARY='+prefix+'/lib/libSDL2_ttf.dll.a',
                '-DPHYSFS_INCLUDE_DIR='+prefix+'/include',
                '-DPHYSFS_LIBRARY='+prefix+'/lib/libphysfs.dll.a',
                '-DRAPID_JSON_INCLUDE_DIR='+prefix+'/include',
                '-DPNG_INCLUDE_DIR='+prefix+'/include',
                '-DPNG_LIBRARY='+prefix+'/lib/libpng16.dll.a',
                '-DZLIB_INCLUDE_DIR='+prefix+'/include',
                '-DZLIB_LIBRARY='+prefix+'/lib/libz.dll.a']
    run(*cmd)
    if not args.configure_only:
        run('cmake','--build',build,'--target','barony','--parallel',args.jobs)
        if args.platform=='windows':
            dest=ROOT/'framework/runtime'; dest.mkdir(parents=True,exist_ok=True)
            run('cp',build+'/barony.exe',linux(dest/('barony-raid-smoke.exe' if args.smoke_tests else 'barony-rmf.exe')))
if __name__=='__main__': main()
