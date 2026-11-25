"""Verify match progression and normal-game compatibility after a Delve balance build.

The production case drives only its owned native game window and records a clip.
All cases create separate disposable profiles and run sequentially.
"""
from pathlib import Path
import argparse
import json
import subprocess
import sys
import uuid

ROOT=Path(__file__).resolve().parents[2]


def main():
    cases={
        'raid':['tests/smoke/run_raid_smoke.py','--faults','--capture'],
        'mixed':['tests/smoke/run_raid_smoke.py','--scenario','progression','--classes','16,7,15,9','--races','4,1,2,5','--capture'],
        'normal':['tests/regression/run_regression.py','--case','ring-normal','--case','gear-save-resume-direct-ip','--case','join-four-full-loss'],
        'production':['modding-evidence/verify_delve_restart_live.py'],
    }
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--case',choices=list(cases),action='append')
    ap.add_argument('--raid-scenario',choices=['progression','host-early','faults','combat','quality','quality-unidentified','restart'],action='append',help='target specific raid reruns')
    ap.add_argument('--normal-case',choices=['ring-normal','gear-save-resume-direct-ip','join-four-full-loss'],action='append',help='target specific normal-game reruns')
    args=ap.parse_args()
    if args.raid_scenario:
        cases['raid']=['tests/smoke/run_raid_smoke.py','--capture',*[arg for name in args.raid_scenario for arg in ['--scenario',name]]]
    if args.normal_case:
        cases['normal']=['tests/regression/run_regression.py',*[arg for name in args.normal_case for arg in ['--case',name]]]
    run=ROOT/'modding-evidence'/('class-race-release-'+uuid.uuid4().hex[:10]);run.mkdir()
    print('RELEASE_CHECKS '+str(run),flush=True)
    report=dict(status='RUNNING',cases={})
    for name,command in cases.items():
        if args.case and name not in args.case:continue
        print('START '+name,flush=True)
        output=run/(name+'.log')
        with output.open('w') as log:
            result=subprocess.run([sys.executable,*command],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        report['cases'][name]=dict(status='PASS' if result.returncode==0 else 'FAIL',log=str(output))
        print(report['cases'][name]['status']+' '+name,flush=True)
        (run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        if result.returncode:
            print(output.read_text()[-2500:],flush=True)
            report['status']='FAIL';break
    else:report['status']='PASS'
    (run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print('RESULT '+report['status']+' '+str(run/'result.json'),flush=True)
    return 0 if report['status']=='PASS' else 1


if __name__=='__main__':sys.exit(main())
