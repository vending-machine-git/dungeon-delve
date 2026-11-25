"""Open the real native build creator in a disposable smoke profile and capture it."""
from pathlib import Path
import os
import argparse,ctypes,json,os,shutil,subprocess,time,uuid
ROOT=Path(__file__).resolve().parents[2]
GAME=Path(r'F:\Steam\steamapps\common\Barony')
UM=Path(os.environ.get('UNIVERSAL_MODDER', str(Path.home() / 'Documents/Projects/[Tools]/universal-modder/bin/um.cmd')))
def window(pid):
    found=[]
    @ctypes.WINFUNCTYPE(ctypes.c_bool,ctypes.c_void_p,ctypes.c_void_p)
    def callback(hwnd,_):
        value=ctypes.c_ulong();ctypes.windll.user32.GetWindowThreadProcessId(hwnd,ctypes.byref(value))
        if value.value==pid and ctypes.windll.user32.IsWindowVisible(hwnd):found.append(int(hwnd))
        return True
    ctypes.windll.user32.EnumWindows(callback,0)
    return found[0] if found else None
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--profile',type=Path);ap.add_argument('--class-id',type=int,default=7);ap.add_argument('--close',action='store_true');args=ap.parse_args()
    profile=args.profile or ROOT/'framework/profiles'/('build-creator-'+uuid.uuid4().hex[:10]);(profile/'config').mkdir(parents=True,exist_ok=True)
    (profile/'config/config.json').write_text(json.dumps(dict(skipintro=True,mods=[],port_number=57535)))
    for mod in ['vending_machine_framework','extraction-raid']:
        for sub in ['data','maps','images']:
            p=ROOT/'mods'/mod/sub
            if p.is_dir():shutil.copytree(p,profile/sub,dirs_exist_ok=True)
    (profile/'books').mkdir(exist_ok=True);shutil.copy2(GAME/'books/compiled_books.json',profile/'books/compiled_books.json')
    env=os.environ.copy();env['PATH']=str(GAME)+os.pathsep+env.get('PATH','')
    env.update(BARONY_SMOKE_AUTOPILOT='1',BARONY_SMOKE_ROLE='host',BARONY_SMOKE_BUILD_CREATOR='1',BARONY_SMOKE_CLASS=str(args.class_id))
    stream=(profile/'process.log').open('wb')
    launched_at=time.time()
    proc=subprocess.Popen([str(ROOT/'framework/runtime/barony-raid-smoke.exe'),'-windowed','-size=1280x720','-nosound','-datadir='+str(GAME),'-raid-profile='+str(profile/'delve-profile.json')],cwd=profile,env=env,stdout=stream,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
    print('PROFILE',profile,'PID',proc.pid,flush=True)
    try:
        until=time.monotonic()+60
        while time.monotonic()<until:
            p=profile/'log.txt'
            if p.exists() and p.stat().st_mtime>=launched_at and 'creator_ready' in p.read_text(errors='replace'):break
            if proc.poll() is not None:raise RuntimeError('Creator exited')
            time.sleep(.2)
        else:raise RuntimeError('Creator timeout')
        hwnd=window(proc.pid);assert hwnd
        time.sleep(2)
        subprocess.run([str(UM),'win','shot',str(profile/'creator.png'),'--hwnd',str(hwnd),'--scale','0.7'],check=True)
        (ROOT/'modding-evidence/build-creator-session.json').write_text(json.dumps(dict(profile=str(profile),pid=proc.pid,hwnd=hwnd),indent=2))
        print('READY',profile/'creator.png',flush=True)
        if args.close:proc.terminate();proc.wait(timeout=10)
    except BaseException:
        proc.terminate();proc.wait(timeout=10);raise
if __name__=='__main__':main()
