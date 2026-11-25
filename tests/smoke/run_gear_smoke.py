"""Real-engine tests for independent normal-game Bring Your Gear."""
from pathlib import Path
import argparse
from collections import Counter
import hashlib
import json
import os
import re
import shutil
import subprocess
import time
import uuid

ROOT=Path(__file__).resolve().parents[2]
FIELDS=('type','status','appearance','beatitude','count','identified','x','y')

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--exe',type=Path,default=ROOT/'framework/runtime/barony-raid-smoke.exe')
 ap.add_argument('--data',type=Path,default=Path(r'F:\Steam\steamapps\common\Barony'))
 ap.add_argument('--host-import',type=Path,required=True)
 ap.add_argument('--guest-import',type=Path,required=True)
 ap.add_argument('--timeout',type=int,default=90)
 args=ap.parse_args()
 run=ROOT/'framework/profiles'/('gear-smoke-'+uuid.uuid4().hex[:12]);run.mkdir(parents=True)
 print('ARTIFACTS '+str(run),flush=True)
 originals={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [args.host_import,args.guest_import]}
 header=(ROOT/'src/items.hpp').read_text().split('enum ItemType')[1].split('{',1)[1].split('}',1)[0]
 names=[re.sub(r'//.*','',x).strip().split('=')[0].strip() for x in header.split(',')]
 spell=names.index('SPELL_ITEM')
 key=names.index('TOOL_SKELETONKEY')
 def expected(source):
  save=json.loads(source.read_text());s=save['players'][save['player_num']]['stats']
  inventory=s['inventory'];remap={i:n for n,i in enumerate(i for i,x in enumerate(inventory) if x['type']!=spell)}
  return dict(inventory=[x for x in inventory if x['type']!=spell],player_equipment=[dict(first=x['first'],second=remap.get(x['second'],4294967295)) for x in s['player_equipment']])
 def signature(item,coordinates=True):return tuple(item[k] for k in (FIELDS if coordinates else FIELDS[:-2]))
 def check_kit(actual,wanted):
  assert Counter(map(signature,actual['inventory']))==Counter(map(signature,wanted['inventory'])),'Physical item metadata or coordinates differ from import'
  def equip(kit):return {x['first']:signature(kit['inventory'][x['second']]) for x in kit['player_equipment'] if x['second']!=4294967295}
  assert equip(actual)==equip(wanted),'Equipment assignments differ'
 def setup(name,source):
  folder=run/name;(folder/'config').mkdir(parents=True)
  (folder/'config/config.json').write_text(json.dumps(dict(skipintro=True,mods=[],port_number=57166,music_volume=1)))
  (folder/'savegames').mkdir();shutil.copy2(source,folder/'import.baronysave')
  return folder
 def start(folder,role,quick=False,resume=False):
  for name in ['log.txt','gear-observed.json']:
   p=folder/name
   if p.exists():p.unlink()
  env=os.environ.copy();env['PATH']=str(args.data)+os.pathsep+env.get('PATH','')
  env.update(BARONY_SMOKE_GEAR='1',BARONY_SMOKE_GEAR_RESUME='1' if resume else '0',BARONY_SMOKE_ROLE=role,BARONY_SMOKE_CONNECT_ADDRESS='127.0.0.1:57166')
  if not quick:env['BARONY_SMOKE_AUTOPILOT']='1'
  cmd=[str(args.exe),'-windowed','-size=960x540','-nosound','-datadir='+str(args.data),'-bring-gear','-gear-import='+str(folder/'import.baronysave')]
  if quick:cmd+=['-quickstart=barbarian']
  output=(folder/('resume.stdout.log' if resume else 'stdout.log')).open('wb')
  p=subprocess.Popen(cmd,cwd=folder,env=env,stdout=output,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
  print(role+' pid='+str(p.pid),flush=True)
  return p,output
 def wait(players,condition):
  deadline=time.monotonic()+args.timeout
  while True:
   logs={name:(folder/'log.txt').read_text(errors='replace') if (folder/'log.txt').exists() else '' for name,(folder,process) in players.items()}
   for name,(_,process) in players.items():
    if '[SMOKE][GEAR] fail' in logs[name] or '[GEAR] import failed' in logs[name]:raise RuntimeError('Gear fixture failed: '+name)
    if process.poll() is not None:raise RuntimeError(name+' exited: '+str(process.returncode))
   if any('[SMOKE][RING] fail' in value for value in logs.values()):raise RuntimeError('Ring persistence/resource check failed')
   if condition(logs):return logs
   if time.monotonic()>deadline:raise RuntimeError('Timed out waiting for '+','.join(players))
   time.sleep(.2)
 def stop(process,stream):
  if process.poll() is None:process.terminate()
  try:process.wait(timeout=5)
  except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
  stream.close()
 results=[]
 try:
  solo=setup('solo',args.guest_import)
  wanted=expected(args.guest_import)
  p,stream=start(solo,'solo',quick=True)
  try:
   logs=wait({'solo':(solo,p)},lambda d:'[SMOKE][GEAR] ordinary_save result=0' in d['solo'])
   actual=json.loads((solo/'gear-observed.json').read_text());check_kit(actual,wanted)
   assert actual['name']=='Avatar' and actual['class']==0 and actual['race']==0,'Gear import replaced selected identity'
   assert 'level=1 floor=0 saves=1' in logs['solo'],'Normal progression or saving changed'
   shutil.copy2(solo/'gear-observed.json',solo/'fresh-observed.json')
   shutil.copy2(solo/'log.txt',solo/'fresh.engine.log')
   save=json.loads((solo/'savegames/savegame0.baronysave').read_text())
   inv=save['players'][save['player_num']]['stats']['inventory']
   assert sum(x['count'] for x in inv if x['type']==key and x['beatitude']==7)==1,'Normal save lost earned marker item'
   results.append('normal single-player import preserves full physical metadata and equipment, selected identity, floor, level, and normal saving')
  finally:stop(p,stream)
  # Resume with the import still selected: earned loot must survive and the import must not replace it.
  p,stream=start(solo,'solo-resume',resume=True)
  try:
   logs=wait({'solo':(solo,p)},lambda d:'[SMOKE][GEAR] ordinary_save result=0' in d['solo'])
   observed=json.loads((solo/'gear-observed.json').read_text())
   marker=[x for x in observed['inventory'] if x['type']==key and x['beatitude']==7]
   assert len(marker)==1 and marker[0]['count']==1,'Resume reapplied import or duplicated earned loot'
   assert '[GEAR] ordinary resumed inventory' in logs['solo'] and '[GEAR] imported slot=' not in logs['solo'],'Resume invoked import'
   results.append('ordinary campaign resume retains earned loot without reimporting')
  finally:stop(p,stream)
  host=setup('host',args.host_import);guest=setup('client',args.guest_import)
  hp,hs=start(host,'host');gp=gs=None
  try:
   wait({'host':(host,hp)},lambda d:'[SMOKE][RAID] host_ready' in d['host'])
   gp,gs=start(guest,'client')
   logs=wait({'host':(host,hp),'client':(guest,gp)},lambda d:all('[SMOKE][GEAR] ordinary_save result=0' in value for value in d.values()))
   check_kit(json.loads((host/'gear-observed.json').read_text()),expected(args.host_import))
   check_kit(json.loads((guest/'gear-observed.json').read_text()),expected(args.guest_import))
   remote=re.search(r'\[SMOKE\]\[GEAR\] remote ac=(-?\d+) hash=(\d+)',logs['host'])
   local=re.search(r'\[SMOKE\]\[GEAR\] observed role=client .*? ac=(-?\d+) hash=(\d+)',logs['client'])
   assert remote and local and remote.groups()==local.groups(),'Host guest equipment/armor class disagreement'
   assert all('level=1 floor=0 saves=1' in value and '[RAID] deployed' not in value for value in logs.values()),'QoL imported into raid or changed normal progression'
   assert all(not (folder/'raid-profile.json').exists() for folder in [solo,host,guest]),'QoL wrote raid persistence'
   results.append('private Direct IP peers import separate kits, agree on guest combat equipment, and use normal campaign saves')
  finally:
   if gp:stop(gp,gs)
   stop(hp,hs)
  for path,digest in originals.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,'Original source save changed'
  (run/'result.json').write_text(json.dumps(dict(status='PASS',checks=results,source_hashes=originals),indent=2)+'\n')
  print('PASS: '+json.dumps(results),flush=True)
 except Exception as exc:
  (run/'result.json').write_text(json.dumps(dict(status='FAIL',error=str(exc),checks=results),indent=2)+'\n')
  raise
if __name__=='__main__':main()
