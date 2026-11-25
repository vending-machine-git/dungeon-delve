"""Exercise invalid imported saves and launcher boundaries in real isolated runtimes."""
from pathlib import Path
import argparse,copy,json,os,subprocess,sys,time,uuid
ROOT=Path(__file__).resolve().parents[2]
GAME=Path(r'F:\Steam\steamapps\common\Barony')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--exe',type=Path,default=ROOT/'framework/runtime/barony-rmf.exe');args=ap.parse_args()
 run=ROOT/'framework/profiles'/('input-checks-'+uuid.uuid4().hex[:12]);run.mkdir();print('ARTIFACTS '+str(run),flush=True)
 source=json.loads((ROOT/'framework/profiles/gear-smoke-08544ff23b4e/client/import.baronysave').read_text())
 def edited(edit):
  value=copy.deepcopy(source);edit(value['players'][value['player_num']]['stats']);return json.dumps(value)
 cases={
  'valid':(json.dumps(source),True),
  'truncated':('{"magic_cookie":',False),
  'wrong-root':('[]',False),
  'missing-item-type':(edited(lambda s:s['inventory'][0].pop('type')),False),
  'wrong-item-count-type':(edited(lambda s:s['inventory'][0].__setitem__('count','oops')),False),
  'invalid-item-id':(edited(lambda s:s['inventory'][0].__setitem__('type',999999)),False),
  'duplicate-equipment-slot':(edited(lambda s:s['player_equipment'].append(s['player_equipment'][0])),False),
  'out-of-range-equipment':(edited(lambda s:s['player_equipment'][0].__setitem__('second',123456)),False),
  'missing-stats':(json.dumps(dict(magic_cookie='BARONYJSONSAVE',game_version=502,player_num=0,players=[dict(char_class=0,race=0)])),False),
 }
 report=dict(status='RUNNING',cases={})
 for name,(payload,expected) in cases.items():
  folder=run/name;(folder/'config').mkdir(parents=True);(folder/'config/config.json').write_text(json.dumps(dict(skipintro=True,mods=[])))
  (folder/'import.baronysave').write_text(payload)
  env=os.environ.copy();env['PATH']=str(GAME)+os.pathsep+env.get('PATH','')
  with (folder/'stdout.log').open('wb') as out:
   process=subprocess.Popen([str(args.exe),'-windowed','-size=960x540','-nosound','-datadir='+str(GAME),'-quickstart=barbarian','-bring-gear','-gear-import='+str(folder/'import.baronysave')],cwd=folder,env=env,stdout=out,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
   try:
    deadline=time.monotonic()+60
    while True:
     log=(folder/'log.txt').read_text(errors='replace') if (folder/'log.txt').exists() else ''
     accepted='[GEAR] imported slot=' in log;rejected='[GEAR] import failed:' in log
     if accepted or rejected:
      status='PASS' if accepted==expected else 'FAIL';report['cases'][name]=dict(status=status,accepted=accepted,expected_accept=expected);print(status+' '+name+' accepted='+str(accepted),flush=True);break
     if process.poll() is not None:raise RuntimeError('Runtime crashed/exited '+str(process.returncode))
     if time.monotonic()>deadline:raise RuntimeError('Import outcome timed out')
     time.sleep(.2)
   except Exception as exc:report['cases'][name]=dict(status='FAIL',error=str(exc));print('FAIL '+name+': '+str(exc),flush=True)
   finally:
    if process.poll() is None:process.terminate()
    try:process.wait(timeout=5)
    except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
  (run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
 report['status']='PASS' if all(x['status']=='PASS' for x in report['cases'].values()) else 'FAIL'
 (run/'result.json').write_text(json.dumps(report,indent=2)+'\n');return int(report['status']!='PASS')
if __name__=='__main__':sys.exit(main())
