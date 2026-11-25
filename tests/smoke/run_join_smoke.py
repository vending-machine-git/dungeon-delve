"""Join a running, altered game and verify live state plus ordinary pickup."""
from pathlib import Path
import os
import argparse,json,os,subprocess,time,uuid,shutil,hashlib
from collections import Counter
ROOT=Path(__file__).resolve().parents[2]

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--exe',type=Path,default=ROOT/'framework/runtime/barony-raid-smoke.exe')
 ap.add_argument('--data',type=Path,default=Path(r'F:\Steam\steamapps\common\Barony'))
 ap.add_argument('--floor',type=int,default=0)
 ap.add_argument('--loss',action='store_true')
 ap.add_argument('--capture',action='store_true')
 ap.add_argument('--simultaneous',action='store_true',help='start additional guests during the first transfer')
 ap.add_argument('--retry-abort',action='store_true',help='after a failed transfer, verify its slot is free for a new arrival')
 ap.add_argument('--corrupt-transfer',action='store_true',help='reject a deliberately corrupted snapshot without applying it')
 ap.add_argument('--extra-guests',type=int,choices=[0,1,2],default=0)
 ap.add_argument('--check-full',action='store_true')
 ap.add_argument('--abort-transfer',action='store_true')
 ap.add_argument('--abort-after-equipment',action='store_true')
 ap.add_argument('--snapshot-write-failure',choices=['host','client'])
 ap.add_argument('--advance-floor',action='store_true')
 ap.add_argument('--host-dead',action='store_true')
 ap.add_argument('--gear-import',type=Path,help='copy this save into each importing late guest profile')
 ap.add_argument('--reject-import',action='store_true',help='verify host without Bring Your Gear rejects importing guests')
 ap.add_argument('--host-gear',action='store_true',help='enable Bring Your Gear on the host')
 ap.add_argument('--plain-extra-guests',action='store_true',help='additional guests use class starters without Bring Your Gear')
 ap.add_argument('--timeout',type=int,default=75)
 args=ap.parse_args()
 if args.simultaneous and args.extra_guests!=1:ap.error('--simultaneous currently uses exactly one additional guest')
 if args.gear_import and not (args.host_gear or args.reject_import):ap.error('--gear-import requires --host-gear or --reject-import')
 if args.reject_import and (args.host_gear or not args.gear_import):ap.error('--reject-import needs an import and a host without Bring Your Gear')
 original_hash=hashlib.sha256(args.gear_import.read_bytes()).hexdigest() if args.gear_import else None
 run=ROOT/'framework/profiles'/('jip-smoke-'+uuid.uuid4().hex[:12]);run.mkdir(parents=True)
 sync=run/'sync';sync.mkdir();players={};streams=[]
 print('ARTIFACTS '+str(run),flush=True)
 def start(role):
  folder=run/role;(folder/'config').mkdir(parents=True)
  (folder/'config/config.json').write_text(json.dumps(dict(skipintro=True,mods=[],port_number=57167,music_volume=1)))
  env=os.environ.copy();env['PATH']=str(args.data)+os.pathsep+env.get('PATH','')
  env.update(BARONY_SMOKE_AUTOPILOT='1',BARONY_SMOKE_ROLE=role,BARONY_SMOKE_CONNECT_ADDRESS='127.0.0.1:57167',BARONY_SMOKE_JIP_SYNC=str(sync),BARONY_SMOKE_JIP_FLOOR=str(args.floor))
  if role=='host':env['BARONY_SMOKE_JIP_SOLO_HOST']='1'
  if args.loss and role.startswith('client'):env['BARONY_SMOKE_JIP_LOSS']='1'
  if args.snapshot_write_failure==role:(folder/'jip').write_text('test-owned file blocking snapshot directory')
  if args.simultaneous and role=='client':env['BARONY_SMOKE_JIP_DELAY']='1'

  if args.abort_after_equipment and role=='client':env['BARONY_SMOKE_JIP_ABORT_EQUIPMENT']='1'
  if args.abort_transfer and role=='client':env['BARONY_SMOKE_JIP_ABORT']='1'
  if args.corrupt_transfer and role=='client':env['BARONY_SMOKE_JIP_CORRUPT']='1'
  stream=(folder/'stdout.log').open('wb');streams.append(stream)
  cmd=[str(args.exe),'-windowed','-size=1280x720','-nosound','-datadir='+str(args.data),'-join-in-progress']
  importing=args.gear_import and role.startswith('client') and not (args.plain_extra_guests and role!='client')
  if (role=='host' and args.host_gear) or importing:cmd+=['-bring-gear']
  if importing:
   shutil.copy2(args.gear_import,folder/'import.baronysave');cmd+=['-gear-import='+str(folder/'import.baronysave')]
  p=subprocess.Popen(cmd,cwd=folder,env=env,stdout=stream,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
  players[role]=(folder,p);print(role+' pid='+str(p.pid),flush=True)
 def wait(condition,allow_cancel=False):
  deadline=time.monotonic()+args.timeout
  while True:
   logs={role:(folder/'log.txt').read_text(errors='replace') if (folder/'log.txt').exists() else '' for role,(folder,_) in players.items()}
   for role,(folder,p) in players.items():
    if p.poll() is not None:raise RuntimeError(role+' exited: '+str(p.returncode))
    if '[SMOKE][JIP] fail' in logs[role] or ('[JIP] join failed:' in logs[role] and not (args.corrupt_transfer and 'World transfer failed its checksum.' in logs[role]) and not args.snapshot_write_failure) or ('[JIP] sync cancelled' in logs[role] and not allow_cancel):raise RuntimeError(role+' reported join failure; see '+str(folder/'log.txt'))
   if condition(logs):return logs
   if time.monotonic()>deadline:raise RuntimeError('Timed out waiting for live join; see '+str(run))
   time.sleep(.2)
 try:
  start('host')
  wait(lambda logs:'[SMOKE][JIP] ongoing ' in logs['host'] and (sync/'fixture.json').exists())
  fixture=json.loads((sync/'fixture.json').read_text())
  assert fixture['floor']==args.floor
  print('Host is already playing: '+json.dumps(fixture),flush=True)
  start('client')
  if args.simultaneous:
   wait(lambda logs:'[JIP] receiving nonce=' in logs['client'])
   for index in range(2,2+args.extra_guests):start('client'+str(index))
  if args.reject_import:
   logs=wait(lambda logs:'connection attempt denied by server, error code: 5' in logs['client'])
   assert '[JIP] reserved slot=' not in logs['host'],'Non-opted-in host reserved a slot for imported gear'
   assert hashlib.sha256(args.gear_import.read_bytes()).hexdigest()==original_hash,'Source campaign changed'
   (run/'result.json').write_text(json.dumps(dict(status='PASS',import_rejected=True,fixture=fixture),indent=2)+'\n')
   print('PASS: host without Bring Your Gear rejected importing guest before reserving a slot.',flush=True)
   return
  if args.snapshot_write_failure:
   blocked=args.snapshot_write_failure
   marker='[JIP] sync cancelled' if blocked=='host' else '[JIP] join failed: Could not create the world snapshot directory.'
   wait(lambda logs:marker in logs[blocked],allow_cancel=True)
   folder,p=players.pop('client');p.terminate();p.wait(timeout=5)
   if blocked=='client':wait(lambda logs:'[JIP] sync cancelled' in logs['host'],allow_cancel=True)
   blocker=run/blocked/'jip';assert blocker.is_file();blocker.unlink()
   start('client-retry')
   wait(lambda logs:'[JIP] live slot=1 ' in logs['client-retry'] and (run/'client-retry/jip-observed.json').exists(),allow_cancel=True)
   recovered=json.loads((run/'client-retry/jip-observed.json').read_text())
   assert all(recovered[k]==fixture[k] for k in ['floor','seed','key','host','hash']),'Snapshot failure changed host state'
   (run/'result.json').write_text(json.dumps(dict(status='PASS',snapshot_write_failure=blocked,retry_slot=1,guest=recovered),indent=2)+'\n')
   print('PASS: snapshot directory failure reported safely; fresh guest joined unchanged host.',flush=True);return
  if args.abort_transfer or args.corrupt_transfer or args.abort_after_equipment:
   if args.abort_after_equipment:wait(lambda logs:'[JIP] equipment staged slot=1' in logs['host'])
   else:wait(lambda logs:('[JIP] join failed: World transfer failed its checksum.' if args.corrupt_transfer else '[JIP] receiving nonce=') in logs['client'],allow_cancel=True)
   folder,p=players.pop('client');p.terminate();p.wait(timeout=5)
   wait(lambda logs:(run/'host/jip-aborted.json').exists(),allow_cancel=True)
   aborted=json.loads((run/'host/jip-aborted.json').read_text())
   assert all(aborted[field]==fixture[field] for field in ['floor','seed','key','host','hash']),'Aborted transfer changed the host world'
   result=dict(status='PASS',aborted_transfer=True,fixture=fixture,host=aborted)
   (run/'result.json').write_text(json.dumps(result,indent=2)+'\n')
   print('PASS: abandoned/corrupted synchronization released the host unchanged.',flush=True)
   if args.retry_abort:
    wait(lambda logs:(run/'host/jip-slot-free.json').exists(),allow_cancel=True)
    freed=json.loads((run/'host/jip-slot-free.json').read_text())
    assert freed['loot_bags']==0,'Failed join left a loot bag from an uncommitted character'
    args.abort_transfer=False;args.corrupt_transfer=False;args.abort_after_equipment=False
    start('client-retry')
    wait(lambda logs:'[JIP] live slot=1 ' in logs['client-retry'],allow_cancel=True)
    result['retry_slot']=1;result['cleanup']=freed
    (run/'result.json').write_text(json.dumps(result,indent=2)+'\n')
   return
  logs=wait(lambda logs:(run/'client/jip-done.json').exists() and (run/'host/jip-host-observed.json').exists())
  observed=json.loads((run/'client/jip-observed.json').read_text())
  host=json.loads((run/'host/jip-host-observed.json').read_text())
  done=json.loads((run/'client/jip-done.json').read_text())
  for field in ['floor','seed','key','host','hash']:
   assert observed[field]==fixture[field] and host[field]==fixture[field],f'World mismatch: {field}'
  assert observed['own']!=fixture['host'] and observed['slot']==1 and observed['own']>0,'New player body is invalid'
  assert not observed['removed_present'] and observed['loot_present'] and observed['marked']==0,'Live world deletion or spawn was not transferred'
  assert host['host_gold']==4321,'Host character was reset'
  world=json.loads((run/'host/jip/outgoing.json').read_text())
  assert world['roster'][0]['name']==fixture['name'],'Host character name changed during join'
  assert done['marked']==1,'Ordinary item pickup failed or duplicated'
  assert '[JIP] joined slot=1 ' in logs['host'] and '[JIP] live slot=1 ' in logs['client'],'Join was not committed'
  if args.simultaneous:assert '[JIP] join queued until synchronization/level change finishes' in logs['host'],'Concurrent arrival did not exercise the queue'
  if args.loss:assert '[SMOKE][JIP] dropped first chunk=' in logs['client'],'Loss fixture did not run'
  if args.host_dead:
   assert args.extra_guests>0,'Host-dead test needs an additional late guest'
   (sync/'host-die').write_text('1')
   wait(lambda logs:(sync/'host-dead').exists())
  additional=[]
  for index in range(2,2+args.extra_guests):
   role='client'+str(index)
   if role not in players:start(role)
   wait(lambda logs:f'[JIP] live slot={index} ' in logs[role] and (run/role/'jip-observed.json').exists() and (run/'client/jip-peers.json').exists() and json.loads((run/'client/jip-peers.json').read_text())['players'][index]>0)
   new=json.loads((run/role/'jip-observed.json').read_text())
   peers=json.loads((run/'client/jip-peers.json').read_text())
   assert new['slot']==index and new['host']==(0 if args.host_dead else fixture['host']) and new['key']==fixture['key'],'Additional join reset the host or took the wrong slot'
   assert peers['players'][index]==new['own'] and peers['players'][1]==observed['own'],'Existing guest did not see the new body or was replaced'
   additional.append(new)
  full_rejected=False
  if args.check_full:
   assert args.extra_guests==2,'Full test requires all four player slots'
   start('client-full')
   wait(lambda logs:'connection attempt denied by server, error code: 4' in logs['client-full'])
   full_rejected=True
  advanced=None
  if args.advance_floor:
   (sync/'advance').write_text('1')
   wait(lambda logs:all((run/role/'jip-floor.json').exists() and json.loads((run/role/'jip-floor.json').read_text())['floor']==args.floor+1 for role in ['host','client']))
   host_floor=json.loads((run/'host/jip-floor.json').read_text())
   guest_floor=json.loads((run/'client/jip-floor.json').read_text())
   assert host_floor==guest_floor and host_floor['key']==fixture['key'],'Ordinary floor transition lost synchronization'
   advanced=host_floor
  gear_checks=[]
  if args.host_gear:
   def signature(x):return tuple(x[k] for k in ('type','status','appearance','beatitude','count','identified','x','y'))
   def equipment(kit):return {x['first']:kit['inventory'][x['second']] for x in kit['player_equipment'] if x['second']<len(kit['inventory'])}
   def equipment_hash(kit):
    items=equipment(kit);value=2166136261
    for slot in ('weapon','shield','mask','helmet','gloves','shoes','breastplate','cloak','amulet','ring'):
     x=items.get(slot);fields=[1,x['type'],x['status'],x['appearance'],x['beatitude'],x['count']] if x else [0]*6
     for field in fields:value=((value^(field&0xffffffff))*16777619)&0xffffffff
    return value
   for slot in range(1,2+args.extra_guests):
    role='client' if slot==1 else 'client'+str(slot)
    wait(lambda logs:(run/'host'/('jip-equipment-slot'+str(slot)+'.json')).exists())
    kit=json.loads((run/role/'jip-observed.json-gear.json').read_text())
    remote=json.loads((run/'host'/('jip-equipment-slot'+str(slot)+'.json')).read_text())
    assert remote['hash']==equipment_hash(kit),'Host combat equipment differs from late guest: '+role
    importing=args.gear_import and not (args.plain_extra_guests and slot>1)
    if importing:
     source=json.loads(args.gear_import.read_text());wanted=source['players'][source['player_num']]['stats']
     header=(ROOT/'src/items.hpp').read_text().split('enum ItemType')[1].split('{',1)[1].split('}',1)[0]
     import re
     names=[re.sub(r'//.*','',x).strip().split('=')[0].strip() for x in header.split(',')]
     physical=[x for x in wanted['inventory'] if x['type']!=names.index('SPELL_ITEM')]
     assert Counter(map(signature,kit['inventory']))==Counter(map(signature,physical)),'Late join lost or changed imported backpack'
     assert {k:signature(v) for k,v in equipment(kit).items()}=={k:signature(v) for k,v in equipment(wanted).items()},'Late join changed equipped slots'
    gear_checks.append(dict(slot=slot,imported=bool(importing),host_equipment_hash=remote['hash']))
   if args.gear_import:assert hashlib.sha256(args.gear_import.read_bytes()).hexdigest()==original_hash,'Source campaign changed'
  if args.capture:
   import ctypes,sys
   from ctypes import wintypes
   handles={};CALLBACK=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
   def window_callback(hwnd,_):
    pid=wintypes.DWORD();ctypes.windll.user32.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
    for role,(_,p) in players.items():
     if p.pid==pid.value and ctypes.windll.user32.IsWindowVisible(hwnd):handles[role]=hwnd
    return True
   callback=CALLBACK(window_callback);ctypes.windll.user32.EnumWindows(callback,0)
   for role,hwnd in handles.items():subprocess.run([os.environ.get('UNIVERSAL_MODDER', str(Path.home() / 'Documents/Projects/[Tools]/universal-modder/bin/um.cmd')),'win','shot','--hwnd',str(hwnd),'--scale','0.5',str(run/(role+'.png'))],check=True)
  result=dict(status='PASS',gear_checks=gear_checks,floor=args.floor,loss=args.loss,additional_guests=additional,full_rejected=full_rejected,host_dead=args.host_dead,advanced_floor=advanced,fixture=fixture,guest=observed,host=host,pickup=done)
  (run/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS: joined ongoing floor with live terrain/entities, preserved host, and picked up one item.',flush=True)
 except Exception as exc:
  (run/'result.json').write_text(json.dumps(dict(status='FAIL',error=str(exc)),indent=2)+'\n');raise
 finally:
  for _,p in players.values():
   if p.poll() is None:p.terminate()
  for _,p in players.values():
   try:p.wait(timeout=5)
   except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
  for stream in streams:stream.close()
if __name__=='__main__':main()
