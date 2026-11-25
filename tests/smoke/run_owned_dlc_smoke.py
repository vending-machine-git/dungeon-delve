"""Launch the production runtime through its real launcher in a disposable profile.

Requires genuine local Steam ownership; never supplies or overrides entitlements.
Preserves the user's last-launch metadata and closes only this test's game PID.
"""
from pathlib import Path
import os
import argparse
import re
import json
import subprocess
import sys
import time
import uuid
from dlc_ownership import installed_mask

ROOT=Path(__file__).resolve().parents[2]
UM=Path(os.environ.get('UNIVERSAL_MODDER', str(Path.home() / 'Documents/Projects/[Tools]/universal-modder/bin/um.cmd')))


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--expect-mask',type=int,choices=range(8),help='optional independent expected ownership mask')
    args=parser.parse_args()
    expected_mask=args.expect_mask
    profile=ROOT/'framework/profiles'/('owned-dlc-'+uuid.uuid4().hex[:10])
    (profile/'config').mkdir(parents=True)
    (profile/'config/config.json').write_text(json.dumps(dict(version=25,skipintro=True,mods=[])))
    metadata=ROOT/'framework/last-launch.json'
    before=metadata.read_bytes() if metadata.exists() else None
    report=dict(status='RUNNING',profile=str(profile));pid=None
    print('PROFILE '+str(profile),flush=True)
    try:
        with (profile/'launcher.log').open('wb') as stream:
            launcher=subprocess.Popen([sys.executable,str(ROOT/'framework/tools/launch.py'),
                '--raid','--profile',str(profile)],cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW)
            assert launcher.wait(timeout=20)==0, 'Framework launcher failed'
        launch=json.loads(metadata.read_text());pid=launch['pid']
        assert Path(launch['profile'])==profile
        report['pid']=pid
        deadline=time.monotonic()+80
        while time.monotonic()<deadline:
            file=profile/'log.txt';log=file.read_text(errors='replace') if file.exists() else ''
            ownership=re.findall(r'Steam installed DLC: myths=([01]) legends=([01]) deserters=([01])',log)
            if ownership:
                values=tuple(map(int,ownership[-1]))
                mask=sum(v<<i for i,v in enumerate(values))
                oracle=installed_mask() if expected_mask is None else expected_mask
                assert mask==oracle,(mask,oracle)
                report.update(status='PASS',ownership=dict(zip(('myths','legends','deserters'),map(bool,values))),mask=mask,
                    launcher='Real production launcher started/contacted Steam, staged content and launched the non-smoke executable')
                print('PASS production launcher recognizes owned Steam DLC',flush=True)
                break
            time.sleep(.2)
        else:raise TimeoutError('Production ownership query did not recover')
    except Exception as error:
        report.update(status='FAIL',error=repr(error));raise
    finally:
        if pid is not None:
            subprocess.run([str(UM),'win','kill',str(pid)],cwd=ROOT,stdout=subprocess.DEVNULL,stderr=subprocess.STDOUT)
        if before is not None and metadata.exists():
            current=json.loads(metadata.read_text())
            if current.get('pid')==pid:metadata.write_bytes(before)
        (profile/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        print('REPORT '+str(profile/'result.json'),flush=True)


if __name__=='__main__':main()
