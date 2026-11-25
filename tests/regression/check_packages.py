"""Validate Workshop roots and launcher feature/dependency boundaries without launching games."""
from pathlib import Path
import os
import importlib.util,json,subprocess,sys,time,uuid
ROOT=Path(__file__).resolve().parents[2]
GAME=Path(r'F:\Steam\steamapps\common\Barony')
def main():
 run=ROOT/'modding-evidence'/('package-checks-'+uuid.uuid4().hex[:10]);run.mkdir();report=dict(status='RUNNING',cases={});print('ARTIFACTS '+str(run),flush=True)
 source=ROOT/'framework/profiles/gear-smoke-08544ff23b4e/client/import.baronysave'
 tests={
  'framework':([],True,['vending_machine_framework']),
  'removed-ring-reject':(['--ring'],False,None),
  'removed-ring-mod-reject':(['--mod','game-dev-ring'],False,None),
  'raid':(['--raid'],True,['vending_machine_framework','extraction-raid']),
  'gear':(['--gear-import',str(source)],True,['vending_machine_framework','bring-your-gear']),
  'join':(['--join-in-progress'],True,['vending_machine_framework','join-in-progress']),
  'join-gear':(['--join-in-progress','--gear-import',str(source)],True,['vending_machine_framework','join-in-progress','bring-your-gear']),
  'deduplicate':(['--mod','vending_machine_framework'],True,['vending_machine_framework']),
  'raid-gear-reject':(['--raid','--bring-gear'],False,None),
  'raid-join-reject':(['--raid','--join-in-progress'],False,None),
  'manifest-modes-reject':(['--mod','extraction-raid','--mod','join-in-progress'],False,None),
  'legacy-import-reject':(['--raid-import',str(source)],False,None),
  'missing-import-reject':(['--gear-import',str(run/'missing.baronysave')],False,None),
  'path-traversal-reject':(['--mod','../src'],False,None),
  'missing-mod-reject':(['--mod','does-not-exist'],False,None),
  'selftest-mode-reject':(['--selftest','--join-in-progress'],False,None),
  'steam-invite-code':(['--connect-lobby','steam:109775241323497676'],True,['vending_machine_framework']),
  'steam-lobby-id-join':(['--join-in-progress','--connect-lobby','109775241323497676'],True,['vending_machine_framework','join-in-progress']),
  'steam-invite-code-reject':(['--connect-lobby','steam:not-a-lobby'],False,None),
  'steam-invites-off':(['--no-steam-invites'],True,['vending_machine_framework']),
 }
 for name,(args,good,mods) in tests.items():
  r=subprocess.run([sys.executable,str(ROOT/'framework/tools/launch.py'),*args,'--dry-run'],cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  ok=(r.returncode==0)==good
  if good and r.returncode==0:ok=ok and json.loads(r.stdout)['mods']==mods
  report['cases'][name]=dict(status='PASS' if ok else 'FAIL',output=r.stdout.strip());print(report['cases'][name]['status']+' '+name,flush=True)
 for folder in sorted((ROOT/'mods').iterdir()):
  if not folder.is_dir():continue
  r=subprocess.run([str(Path(os.environ.get('UNIVERSAL_MODDER', str(Path.home() / 'Documents/Projects/[Tools]/universal-modder/bin/um.cmd')))),'publish','check',str(folder),'--game',str(GAME)],cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  files=[x.relative_to(folder).as_posix() for x in folder.rglob('*') if x.is_file()]
  ok=r.returncode==0 and len(files)==(23 if folder.name=='extraction-raid' else 5) and not any(x.endswith(('.cpp','.hpp','.exe','.dll','.baronysave')) for x in files)
  if folder.name=='extraction-raid':ok=ok and {'images/rmf/cooldown-disc.png','images/rmf/cooldown-ring.png','images/rmf/rarity-smoke.png','images/rmf/rarity-slot.png','music/rmf/embers-before-the-delve.wav','music/rmf/dungeon-intro.wav','data/rmf/intro.json','data/rmf/intro.srt','music/rmf/dungeon-floor-2.wav','music/rmf/dungeon-floor-3.wav','data/rmf/intro-floor-2.json','data/rmf/intro-floor-3.json','data/rmf/intro-floor-2.srt','data/rmf/intro-floor-3.srt'}<=set(files)
  report['cases']['package-'+folder.name]=dict(status='PASS' if ok else 'FAIL',files=files,output=r.stdout.strip());print(report['cases']['package-'+folder.name]['status']+' package-'+folder.name,flush=True)
 # Test dependency validation against synthetic manifests, never mutate real mod folders.
 spec=importlib.util.spec_from_file_location('barony_launch',ROOT/'framework/tools/launch.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);module.ROOT=run/'synthetic'
 for name,requires in [('a',{'b':1}),('b',{'a':1})]:
  folder=module.ROOT/'mods'/name;folder.mkdir(parents=True);(folder/'mod.json').write_text(json.dumps(dict(id=name,api_version=1,requires=requires)))
 for name,selected in [('cycle-rejected',['a']),('invalid-id-rejected',['../escape'])]:
  try:module.resolve_mods(selected);ok=False
  except ValueError:ok=True
  report['cases'][name]=dict(status='PASS' if ok else 'FAIL')
 report['status']='PASS' if all(x['status']=='PASS' for x in report['cases'].values()) else 'FAIL'
 (run/'result.json').write_text(json.dumps(report,indent=2)+'\n');print('RESULT '+report['status']+' '+str(run/'result.json'),flush=True);return int(report['status']!='PASS')
if __name__=='__main__':sys.exit(main())
