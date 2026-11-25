"""Repeat DLC regression coverage without ever granting ownership in a game.

Build normal/smoke runtimes first. The isolated C++ suite covers all eight DLC
combinations; native clients intersect the local account's genuine entitlements.
"""
from pathlib import Path
import json
import re
import subprocess
import sys
import uuid
from dlc_ownership import installed_mask

ROOT=Path(__file__).resolve().parents[2]


def main():
    run=ROOT/'modding-evidence'/('dlc-regression-'+uuid.uuid4().hex[:10])
    run.mkdir()
    genuine=installed_mask()
    report=dict(status='RUNNING',genuine_mask=genuine,cases={})
    print('ARTIFACTS '+str(run),flush=True)

    def check(name, script, *args):
        result=subprocess.run([sys.executable,str(ROOT/script),*map(str,args)],cwd=ROOT,
            text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (run/(name+'.log')).write_text(result.stdout)
        refs=re.findall(r'(?:REPORT|RESULT PASS) (.+result.json)',result.stdout)
        report['cases'][name]=dict(status='PASS' if result.returncode==0 else 'FAIL',reports=refs)
        print(result.stdout,flush=True)
        assert result.returncode==0,name

    try:
        check('isolated-all-eight','tests/regression/check_dlc_entitlements.py')
        submasks=[m for m in range(8) if m&genuine==m]
        for mask in submasks:
            check('native-classes-'+str(mask),'tests/balance/check_class_picker.py',
                '--expect-owned-dlc','--steam-retry','--restrict-dlc',mask)
        # Vary host and guest ownership independently. An account with fewer DLCs
        # can exercise fewer native sets; the full isolated matrix still runs.
        peers=(submasks*4)[:4]
        for i,masks in enumerate((peers,peers[-1:]+peers[:-1])):
            classes=[21 if m&4 else 17 if m&2 else 13 if m&1 else 0 for m in masks]
            races=[13 if m&4 else 5 if m&2 else 1 if m&1 else 0 for m in masks]
            check('mixed-lobby-'+str(i),'tests/smoke/run_lobby_smoke.py','--fresh-ready',
                '--dlc-masks',','.join(map(str,masks)),'--dlc-classes',','.join(map(str,classes)),
                '--dlc-races',','.join(map(str,races)),'--port',57335+i)
        check('production-launcher','tests/smoke/run_owned_dlc_smoke.py','--expect-mask',genuine)
        check('packages','tests/regression/check_packages.py')
        report['status']='PASS'
    except BaseException as error:
        report.update(status='FAIL',error=repr(error))
        raise
    finally:
        (run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        print('REPORT '+str(run/'result.json'),flush=True)


if __name__=='__main__':
    main()
