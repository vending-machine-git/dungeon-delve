"""Run Delve balance engine checks sequentially in disposable Direct IP profiles."""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys
import uuid

ROOT=Path(__file__).resolve().parents[2]


def main():
    cases={
        'matrix':['--matrix','--verify-fixes'],
        'contracts':['--contracts','--capture'],
        'melee':['--samples','melee','--verify-fixes'],
        'projectiles':['--samples','projectiles','--verify-fixes'],
        'long-session':['--idle-seconds','150'],
        'revival':['--resurrection-only','--verify-fixes'],
        'death':['--resurrection-only','--verify-fixes','--mana','74'],
        'collapse':['--resurrection-only','--verify-fixes','--cause','collapse'],
        'normal':['--normal-scope'],
    }
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--case',choices=list(cases),action='append')
    args=ap.parse_args()
    run=ROOT/'modding-evidence'/('balance-suite-'+uuid.uuid4().hex[:10]);run.mkdir()
    print('BALANCE_SUITE '+str(run),flush=True)
    report=dict(status='RUNNING',cases={})
    for name,arguments in cases.items():
        if args.case and name not in args.case:continue
        print('START '+name,flush=True)
        output=run/(name+'.log')
        with output.open('w') as log:
            result=subprocess.run([sys.executable,str(ROOT/'tests/balance/run_balance_audit.py'),*arguments],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        profiles=re.findall(r'^ARTIFACTS (.+)$',output.read_text(),re.M)
        row=dict(status='PASS' if result.returncode==0 else 'FAIL',log=str(output))
        if profiles:row['profile']=profiles[-1]
        report['cases'][name]=row
        print(row['status']+' '+name,flush=True)
        if result.returncode:print(output.read_text()[-2500:],flush=True)
        (run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    report['status']='PASS' if all(r['status']=='PASS' for r in report['cases'].values()) else 'FAIL'
    (run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print('RESULT '+report['status']+' '+str(run/'result.json'),flush=True)
    return 0 if report['status']=='PASS' else 1


if __name__=='__main__':sys.exit(main())
