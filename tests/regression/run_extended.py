"""Additional real-engine regressions for faults, ring persistence, and later floors."""
from pathlib import Path
import argparse,json,re,subprocess,sys,time,uuid
ROOT=Path(__file__).resolve().parents[2]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--case',action='append',default=[]);args=ap.parse_args()
 run=ROOT/'modding-evidence'/('extended-regression-'+time.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:6]);run.mkdir();print('REGRESSION '+str(run),flush=True)
 source=ROOT/'framework/profiles/gear-smoke-08544ff23b4e'
 save=json.loads((source/'client/import.baronysave').read_text());stats=save['players'][save['player_num']]['stats']
 header=(ROOT/'src/items.hpp').read_text().split('enum ItemType')[1].split('{',1)[1].split('}',1)[0]
 names=[re.sub(r'//.*','',x).strip().split('=')[0].strip() for x in header.split(',')]
 index=next(x['second'] for x in stats['player_equipment'] if x['first']=='ring')
 stats['inventory'][index].update(type=names.index('RMF_FIRST_ITEM'),status=4,beatitude=0,count=1,identified=True)
 fixture=run/'ring-fixture.baronysave';fixture.write_text(json.dumps(save))
 join=str(ROOT/'tests/smoke/run_join_smoke.py')
 cases={
  'snapshot-host-recovery':[join,'--snapshot-write-failure','host'],
  'snapshot-client-recovery':[join,'--snapshot-write-failure','client'],
  'checksum-rejection-retry':[join,'--corrupt-transfer','--retry-abort'],
  'queued-arrival':[join,'--floor','1','--simultaneous','--extra-guests','1','--loss'],
  'ring-save-resume-multiplayer':[str(ROOT/'tests/smoke/run_gear_smoke.py'),'--ring','--host-import',str(source/'host/import.baronysave'),'--guest-import',str(fixture)],
  'ring-join-floor':[join,'--floor','1','--ring','--host-gear','--gear-import',str(fixture),'--advance-floor','--capture'],
  'join-swamp-floor5':[join,'--floor','5','--loss'],
  'join-boss-floor10':[join,'--floor','10','--loss'],
 }
 unknown=set(args.case)-set(cases)
 if unknown:ap.error('Unknown case '+str(unknown))
 report=dict(status='RUNNING',cases={})
 for name,command in cases.items():
  if args.case and name not in args.case:continue
  print('START '+name,flush=True);began=time.monotonic();path=run/(name+'.log')
  with path.open('w') as log:process=subprocess.run([sys.executable,*command],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
  content=path.read_text();result=dict(status='PASS' if process.returncode==0 else 'FAIL',exit_code=process.returncode,elapsed_seconds=round(time.monotonic()-began,2),log=str(path))
  profiles=re.findall(r'^ARTIFACTS (.+)$',content,re.M)
  if profiles:
   profile=Path(profiles[-1]);result['profile']=str(profile)
   if (profile/'result.json').exists():result['engine_result']=json.loads((profile/'result.json').read_text())
  report['cases'][name]=result;print(result['status']+' '+name,flush=True)
  if process.returncode:print(content[-2200:],flush=True)
  (run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
 report['status']='PASS' if all(x['status']=='PASS' for x in report['cases'].values()) else 'FAIL'
 (run/'result.json').write_text(json.dumps(report,indent=2)+'\n');print('RESULT '+report['status']+' '+str(run/'result.json'),flush=True);return int(report['status']!='PASS')
if __name__=='__main__':sys.exit(main())
