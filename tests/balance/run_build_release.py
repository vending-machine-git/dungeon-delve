"""Run the focused class-build release checks in unique profiles."""
from pathlib import Path
import argparse,json,subprocess,sys,uuid
ROOT=Path(__file__).resolve().parents[2]
def main():
    run=ROOT/'modding-evidence'/('class-builds-release-'+uuid.uuid4().hex[:10]);run.mkdir();print('RELEASE',run,flush=True)
    cases={
      'class-picker':['tests/balance/check_class_picker.py'],
      'all-options':['tests/balance/run_balance_audit.py','--builds','--capture'],
      'normal-scope':['tests/balance/run_balance_audit.py','--normal-scope'],
      'control-contracts':['tests/balance/run_balance_audit.py','--contracts'],
      'mixed-builds-three-floors':['tests/smoke/run_raid_smoke.py','--scenario','progression','--builds','--classes','16,7,15,9','--races','4,1,2,5','--capture'],
      'raid-quality-combat-restart':['tests/smoke/run_raid_smoke.py','--scenario','quality','--scenario','combat','--scenario','restart','--capture'],
      'normal-mods':['tests/regression/run_regression.py','--case','ring-normal','--case','gear-save-resume-direct-ip','--case','join-four-full-loss'],
      'packages':['tests/regression/check_packages.py'],
    }
    ap=argparse.ArgumentParser();ap.add_argument('--case',choices=list(cases),action='append');args=ap.parse_args()
    if args.case:cases={k:v for k,v in cases.items() if k in args.case}
    report=dict(status='RUNNING',cases={})
    for name,cmd in cases.items():
      print('START',name,flush=True)
      log=run/(name+'.log')
      with log.open('w') as f:r=subprocess.run([sys.executable,*cmd],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT)
      report['cases'][name]=dict(status='PASS' if r.returncode==0 else 'FAIL',log=str(log))
      (run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
      print(report['cases'][name]['status'],name,flush=True)
      if r.returncode:print(log.read_text(errors='replace')[-4000:],flush=True);break
    report['status']='PASS' if len(report['cases'])==len(cases) and all(x['status']=='PASS' for x in report['cases'].values()) else 'FAIL'
    (run/'result.json').write_text(json.dumps(report,indent=2)+'\n');print('RESULT',report['status'],run/'result.json',flush=True)
    return int(report['status']!='PASS')
if __name__=='__main__':sys.exit(main())
