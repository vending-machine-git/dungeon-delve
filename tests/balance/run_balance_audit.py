"""Measure Delve starter kits and controlled PvP hits in the separate smoke runtime."""
from pathlib import Path
from collections import Counter
import json
import os
import shutil
import subprocess
import time
import uuid
import argparse
import re

ROOT=Path(__file__).resolve().parents[2]
GAME=Path(r'F:\Steam\steamapps\common\Barony')


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--resurrection-only',action='store_true');ap.add_argument('--verify-fixes',action='store_true')
    ap.add_argument('--contracts',action='store_true');ap.add_argument('--normal-scope',action='store_true')
    ap.add_argument('--matrix',action='store_true',help='all 26 classes x 14 races, with innate abilities enabled and disabled')
    ap.add_argument('--baseline',type=Path,help='preserved matrix profile for class identity comparison')
    ap.add_argument('--builds',action='store_true',help='audit all 260 native build options and live activation packets')
    ap.add_argument('--capture',action='store_true',help='capture the owned host/client game windows before cleanup')
    ap.add_argument('--effects-capture',action='store_true',help='capture every skill on both native peers; requires --builds')
    ap.add_argument('--effects-selection',help='comma-separated gallery ordinals 0-103 for focused visual iteration; native checks still cover every skill')
    ap.add_argument('--effects-pitch',type=float,default=.1,help='camera pitch in radians for native effect alignment captures (-1.1 to 1.1)')
    ap.add_argument('--effects-motion',action='store_true',help='play full cosmetic timelines rather than midpoint poses; each skill dwells at least three seconds')
    ap.add_argument('--effects-settled',action='store_true',help='capture after the actual cast expires, without cosmetic replays')
    ap.add_argument('--effects-video',action='store_true',help='record an owned host-window motion gallery with Universal Modder')
    ap.add_argument('--sound',action='store_true',help='enable native host audio; the guest remains muted to avoid duplicate local playback')
    ap.add_argument('--samples',choices=['melee','projectiles'],help='run combat measurements in a fresh session without repeating the catalog')
    ap.add_argument('--idle-seconds',type=int,default=0,help='check a long idle Direct IP session without reinitializing fixture characters')
    ap.add_argument('--mana',type=int,default=75);ap.add_argument('--cause',choices=['melee','collapse'],default='melee')
    args=ap.parse_args()
    if args.effects_capture and not args.builds: ap.error('--effects-capture requires --builds')
    if args.effects_motion and not args.effects_capture: ap.error('--effects-motion requires --effects-capture')
    if args.effects_video and not args.effects_motion: ap.error('--effects-video requires --effects-motion')
    if args.effects_settled and (not args.effects_capture or args.effects_motion): ap.error('--effects-settled requires --effects-capture without --effects-motion')
    if not -1.1 <= args.effects_pitch <= 1.1: ap.error('--effects-pitch must be between -1.1 and 1.1')
    effects_selection = list(range(104))
    if args.effects_selection:
        effects_selection = [int(value) for value in args.effects_selection.split(',')]
        if not args.effects_capture or not effects_selection or len(set(effects_selection)) != len(effects_selection) or any(value < 0 or value >= 104 for value in effects_selection):
            ap.error('--effects-selection requires --effects-capture and unique ordinals 0-103')
    run=ROOT/'framework/profiles'/('balance-audit-'+uuid.uuid4().hex[:10])
    (run/'sync').mkdir(parents=True)
    print('ARTIFACTS '+str(run),flush=True)
    processes={};streams=[];recorders=[];result=dict(status='RUNNING',profile=str(run))
    try:
        for slot,role in enumerate(['host','client']):
            profile=run/role;(profile/'config').mkdir(parents=True)
            (profile/'config/config.json').write_text(json.dumps(dict(skipintro=True,mods=[],port_number=57525,music_volume=1)))
            for mod in ['vending_machine_framework','extraction-raid']:
                for directory in ['data','maps','images']:
                    source=ROOT/'mods'/mod/directory
                    if source.is_dir():shutil.copytree(source,profile/directory,dirs_exist_ok=True)
            (profile/'books').mkdir();shutil.copy2(GAME/'books/compiled_books.json',profile/'books/compiled_books.json')
            env=os.environ.copy();env['PATH']=str(GAME)+os.pathsep+env.get('PATH','')
            env.update(BARONY_SMOKE_CLASS='0',BARONY_SMOKE_AUTOPILOT='1',BARONY_SMOKE_ROLE=role,BARONY_SMOKE_PLAYERS='2',
                       BARONY_SMOKE_DELVE='balance',BARONY_SMOKE_RAID_SYNC=str(run/'sync'),BARONY_SMOKE_CONNECT_ADDRESS='127.0.0.1:57525')
            if args.builds:env.update(BARONY_BALANCE_BUILDS='1',BARONY_SMOKE_CLASS='1')
            if args.effects_capture:env.update(BARONY_EFFECTS_CAPTURE='1',BARONY_EFFECTS_PITCH=str(args.effects_pitch))
            if args.effects_selection:env['BARONY_EFFECTS_SELECTION']=args.effects_selection
            if args.effects_motion:env['BARONY_EFFECTS_MOTION']='1'
            if args.effects_settled:env['BARONY_EFFECTS_SETTLED']='1'
            if args.effects_video:env['BARONY_EFFECTS_RECORD']='1'
            if args.builds:
                (profile/'delve-builds.json').write_text(json.dumps(dict(version=1,classes=[[0,1,8,9] for character_class in range(26)])))
            if args.resurrection_only:
                env.update(BARONY_BALANCE_RESURRECTION_ONLY='1',BARONY_SMOKE_RACE=str(slot))
            env.update(BARONY_BALANCE_MANA=str(args.mana),BARONY_BALANCE_CAUSE=args.cause)
            if args.verify_fixes:env['BARONY_BALANCE_VERIFY_FIXES']='1'
            if args.contracts:env['BARONY_BALANCE_CONTRACTS']='1'
            if args.matrix:env['BARONY_BALANCE_MATRIX']='1'
            if args.samples:env['BARONY_BALANCE_SAMPLES']=args.samples
            if args.idle_seconds:env['BARONY_BALANCE_IDLE_SECONDS']=str(args.idle_seconds)
            if args.normal_scope:
                env.update(BARONY_BALANCE_NORMAL_SCOPE='1',BARONY_SMOKE_RACE=str(slot),BARONY_SMOKE_CLASS='7' if slot==0 else '0')
            stream=(profile/'process.log').open('wb');streams.append(stream)
            command=[str(ROOT/'framework/runtime/barony-raid-smoke.exe'),'-windowed','-size=960x540','-nosound','-datadir='+str(GAME)]
            if args.sound and slot == 0:command.remove('-nosound')
            if not args.normal_scope:command+=['-raid-profile='+str(profile/'delve-profile.json')]
            processes[role]=subprocess.Popen(command,cwd=profile,env=env,
                stdout=stream,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
            print(role+' pid='+str(processes[role].pid),flush=True)
            if slot==0:
                deadline=time.monotonic()+50
                while True:
                    path=profile/'log.txt'
                    if path.exists() and 'host_ready' in path.read_text(errors='replace'):break
                    if processes[role].poll() is not None:raise RuntimeError('Host exited')
                    if time.monotonic()>deadline:raise RuntimeError('Lobby timeout')
                    time.sleep(.2)
        deadline=time.monotonic()+(900 if args.effects_capture else 240);previous=0;captured_effects=set()
        while not (run/'host/balance-complete').exists():
            for role,process in processes.items():
                if process.poll() is not None:raise RuntimeError(role+' exited '+str(process.returncode))
            data=run/'host/balance.jsonl'
            count=len(data.read_text().splitlines()) if data.exists() else 0
            if count>=previous+5:previous=count;print('Measured rows '+str(count),flush=True)
            log=(run/'host/log.txt').read_text(errors='replace')
            if '[BALANCE] missing starter' in log:raise RuntimeError(log[log.index('[BALANCE] missing starter'):].splitlines()[0])
            if 'eliminated slot=1 status=Abandoned' in log:raise RuntimeError('Measurement peer disconnected; preserve this failed session')
            if args.effects_video and not recorders and (run/'effects-record-ready').exists():
                import sys
                sys.path.insert(0,str(ROOT/'tests/smoke'))
                from run_delve_smoke import menu_window, UM
                stream=(run/'motion-record.log').open('wb');streams.append(stream)
                command=[str(UM),'win','record','--hwnd',str(menu_window(processes['host'])),'--pid',str(processes['host'].pid),
                         '--seconds',str(len(effects_selection)*5+10),'--fps','30','--out',str(run/'effects-motion-host')]
                if not args.sound:command.append('--no-audio')
                recorders.append(subprocess.Popen(command,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW))
                time.sleep(1)
                (run/'effects-recording').write_text('ready')
            if args.effects_capture and (run/'effects-view-ready').exists():
                ordinal=int((run/'effects-view-ready').read_text().strip() or '-1')
                if 0<=ordinal<104 and ordinal not in captured_effects:
                    import sys
                    sys.path.insert(0,str(ROOT/'tests/smoke'))
                    from run_delve_smoke import capture
                    destination=run/'skill-effects'/f'{ordinal:03d}'
                    destination.mkdir(parents=True)
                    capture(processes,destination)
                    captured_effects.add(ordinal)
                    (run/f'effects-captured-{ordinal}').write_text('1')
            if time.monotonic()>deadline:raise RuntimeError('Audit timeout')
            time.sleep(.2)
        for recorder in recorders:
            assert recorder.wait(timeout=len(effects_selection)*6+30)==0, 'Universal Modder motion recording failed'
        rows=[json.loads(line) for line in (run/'host/balance.jsonl').read_text().splitlines()]
        if args.capture:
            import sys
            sys.path.insert(0,str(ROOT/'tests/smoke'))
            from run_delve_smoke import capture
            capture(processes,run)
        if args.builds:
            audits=[json.loads(line) for line in (run/'host/build-audit.jsonl').read_text().splitlines()]
            assert len([r for r in audits if r['name'].startswith('perk_')])==156
            assert len([r for r in audits if r['name'].startswith('ability_')])==104
            assert len([r for r in audits if r['name'].startswith('skill_effect_cast_')])==104
            assert len([r for r in audits if r['name'].startswith('skill_effect_duration_')])==104
            assert len([r for r in audits if r['name'].startswith('skill_effect_rejected_cast_inert_')])==104
            assert len([r for r in audits if r['name'].startswith('passive_effect_')])==156
            assert len([r for r in audits if r['name'].startswith('tooltip_description_')])==104
            assert len([r for r in audits if r['name'].startswith('skill_resources_capped_')])==104
            assert len([r for r in audits if r['name'].startswith('skill_exact_mana_budget_')])==104
            skill_measurements=[json.loads(line) for line in (run/'host/skill-balance.jsonl').read_text().splitlines()]
            assert len(skill_measurements)==104
            assert len({(row['class'],row['index']) for row in skill_measurements})==104
            assert all(row['damage']>=0 and row['healing']>=0 for row in skill_measurements)
            assert all(r['pass'] for r in audits), [r for r in audits if not r['pass']]
            assert len([r for r in audits if r['name'].startswith('penalty_free_perk_')])==156
            assert len([r for r in audits if r['name'].startswith('penalty_free_ability_')])==104
            assert len([r for r in audits if r['name'].startswith('passive_trigger_')])==156
            assert len([r for r in audits if r['name'].startswith('passive_bounds_')])==156
            assert all(r['pass'] for r in rows),rows
            assert '[DELVE-BUILD-TEST] client_ack' in (run/'client/log.txt').read_text(errors='replace')
            assert '[DELVE-PASSIVE-TEST] guest_state_ack' in (run/'client/log.txt').read_text(errors='replace')
            assert '[DELVE-EFFECT-TEST] guest_event_and_state_ack' in (run/'client/log.txt').read_text(errors='replace')
            if args.effects_capture:assert captured_effects == set(effects_selection), 'Missing native skill captures'
            for role in ('host','client'):
                migrated=json.loads((run/role/'delve-builds.json').read_text())
                assert migrated['version']==2 and migrated['classes']==[[-1,-1,8,9] for character_class in range(26)]
            result['selection_migration']='PASS; old passive slots cleared, active slots retained'
            result.update(status='PASS',build_checks=len(audits),measurements=rows,skill_measurements=skill_measurements,
                          gallery_selection=effects_selection if args.effects_capture else [],gallery_mode='settled' if args.effects_settled else 'motion' if args.effects_motion else 'midpoint',
                          gallery_pitch=args.effects_pitch if args.effects_capture else None,
                          motion_video=str(run/'effects-motion-host.mkv') if args.effects_video else None);return
        if args.idle_seconds:
            assert len(rows)==1 and rows[0]['pass']==1
            result.update(status='PASS',measurements=rows);return
        if args.matrix:
            assert len(rows)==26*14*2
            assert len({(r['kind'],r['class'],r['race']) for r in rows})==len(rows)
            assert all(r['gold']==0 and r['hp']>=30 for r in rows)
            if args.verify_fixes:
                for row in rows:
                    assert 50<=row['hp']<=60, (row['class'],row['race'],row['hp'])
                    totals=Counter()
                    for item in row['inventory']:
                        assert item['count']>0
                        totals[item['name']]+=item['count']
                    assert totals['potion of healing']==1, row
                    assert totals['scroll of teleportation']<=1 and totals['beartrap']<=1
                    assert all(n<=12 for name,n in totals.items() if name.startswith('quiver'))
                    assert totals['potion of acid']<=(2 if row['class']==16 else 1)
                    for item in row['inventory']:
                        if item['category'] in [3,4,11]:
                            assert totals[item['name']]<=(2 if row['class']==16 and item['name']=='potion of acid' else 1), (row['class'],row['race'],item)
                        if item['category']==9:assert totals[item['name']]<=2
                    if row['kind']=='matrix_innate' and row['race']==1:assert totals['potion of restore magic']>=1
                    elif row['kind']=='matrix_innate' and row['race']==2:assert totals['vial of blood']>=1
                    elif row['kind']=='matrix_innate' and row['race']==5:assert totals['metal scrap']>=4
                    else:assert totals['bread']>=1
                    if row['kind']=='matrix_innate' and row['race']==4:assert not row['asleep']
                    equipped=next((x['name'] for x in row['equipment'] if x['slot']=='weapon'),None)
                    if row['class'] in [9,15,16]:assert equipped=={9:'slingshot',15:'magicstaff of cold',16:'iron axe'}[row['class']]
            if args.baseline:
                before={ (r['kind'],r['class'],r['race']):r for r in json.loads((args.baseline/'result.json').read_text())['measurements'] }
                for row in rows:
                    base=before[(row['kind'],row['class'],row['race'])]
                    assert row['skills']==base['skills'] and row['spells']==base['spells'], (row['class'],row['race'],'class identity changed')
                    assert row['mp']==base['mp'] and row['starting_mp']==base['starting_mp']
                result['baseline']=str(args.baseline)
            result.update(status='PASS',measurements=rows)
            print('PASS all 728 class/race/innate combinations',flush=True);return
        if args.normal_scope:
            row=rows[0];assert row['kind']=='normal_scope' and row['raid_active']==0
            assert row['normal_xp_multiplier']==2 and row['normal_skill_xp']==2 and row['delve_ability_rejected']==1
            assert row['normal_xp_multiplier_inert']==1
            assert row['normal_skill_penalty_retained']==1 and row['normal_tooltip_penalty_retained']==1
            assert row['normal_spell_preparation_retained']==1
            assert row['normal_passives_inert']==1
            assert row['normal_shopkeeper_race_rules_retained']==1 and row['normal_cooldown_hud_inert']==1
            assert row['normal_appraisal_retained']==1
            assert row['normal_skill_feedback_inert']==1
            assert row['normal_blessing_prices_retained']==1 and row['normal_concealment_inert']==1
            assert row['normal_blessed_background_retained']==1
            assert row['normal_gem_potion_trap_rarity_inert']==1
            assert row['normal_equipment_affixes_inert']==1
            assert row['normal_equipment_roll_quality_inert']==1
            assert row['normal_blessed_equipment_affixes_inert']==1
            assert row['normal_ground_tooltip_retained']==1
            assert row['hp']==20 and row['reflection']==2 and row['guest_race']==1
            assert abs(row['native_sword']-.5)<.001 and row['paralysis_ticks']==420
            assert row['fear_ticks']==400 and row['web_ticks']==400 and row['confusion_ticks']==600
            result.update(status='PASS',measurements=rows);print('PASS normal-campaign scope',flush=True);return
        if args.contracts:
            assert len(rows)>=16 and all(row['kind']=='contract' and row['pass']==1 for row in rows), rows
            result.update(status='PASS',measurements=rows);print('PASS PvP/PvE control and damage contracts',flush=True);return
        if args.resurrection_only:
            assert rows[0]['guest_race']==1
            assert '[BALANCE] peer_identity race=1' in (run/'client/log.txt').read_text(errors='replace')
            assert sum(r['kind']=='resurrection' for r in rows)==1
            if args.verify_fixes:
                checks=[r for r in rows if r['kind']=='contract'];assert checks and all(r['pass']==1 for r in checks), checks
                report=json.loads((run/'host/delve-results.json').read_text());assert report['complete']
                assert sum(r['players_killed'] for r in report['ranking'])==(1 if args.mana<75 and args.cause=='melee' else 0)
                guest=next(r for r in report['ranking'] if r['slot']==1)
                assert guest['status']==('Eliminated' if args.mana<75 else 'Winner')
                result['results']=report
                if args.cause=='collapse':
                    assert any(row.get('name')=='storm_native_crush_applied' and row['pass']==1 for row in checks)
            result.update(status='PASS',measurements=rows)
            print('PASS matched Skeleton identity/death probe',flush=True)
            return
        assert sum(r['kind']=='class' for r in rows)==(0 if args.samples else 26)
        assert sum(r['kind']=='race' for r in rows)==(0 if args.samples else 14)
        assert all(r['gold']==0 for r in rows if r['kind'] in ['class','race'])
        assert sum(r['kind']=='melee' for r in rows)==(0 if args.samples=='projectiles' else 14)
        assert all(len(r['damage'])==16 for r in rows if r['kind']=='melee')
        assert sum(r['kind']=='projectile' for r in rows)==(0 if args.samples=='melee' else 24)
        measured={r['name']:r for r in rows if 'name' in r}
        # A native ranged projectile may miss. A zero from one attempt is not
        # evidence of zero weapon damage and must not fail the whole assessment.
        assert args.samples=='melee' or any(r['target_damage']>0 for r in rows if r['kind']=='projectile')
        assert all(r['target_damage']>=0 for r in rows if r['kind']=='projectile')
        assert not any(r['kind']=='resurrection' for r in rows), 'Use a matching-race peer for revival tests'
        if args.verify_fixes and not args.samples:
            kits=[r for r in rows if r['kind']=='class'];assert all(r['hp']>=30 for r in kits)
            for kit in kits:
                names=[x['name'] for x in kit['inventory']]
                assert not any(x in names for x in ['crystal sword','crystal battleaxe','amulet of magic reflection','potion of firestorm','potion of icestorm','potion of thunderstorm'])
                assert all(x['count']<=1 for x in kit['inventory'] if x['name'] in ['beartrap','scroll of teleportation'])
                assert all(x['count']<=12 for x in kit['inventory'] if x['name'].startswith('quiver'))
            wizard=next(r for r in kits if r['class']==7);assert wizard['reflection']==0 and wizard['magic_resistance']>=1
            for race in (r for r in rows if r['kind']=='race'):
                assert all(.799<v<1.201 for v in race['pvp_incoming_multipliers'])
            assert 0<measured['sexton_sleep']['sleep_ticks']<=150
            assert 0<measured['ninja_paralysis_potion']['paralysis_ticks']<=75
        if args.verify_fixes and args.samples=='projectiles':
            for name,key,cap in [('shaman_web','web_ticks',100),('shaman_fear','fear_ticks',100),('joker_confuse','confusion_ticks',150)]:
                assert 0<measured[name][key]<=cap, (name,measured[name])
        result.update(status='PASS',measurements=rows)
    except Exception as exc:
        result.update(status='FAIL',error=str(exc));raise
    finally:
        for recorder in recorders:
            if recorder.poll() is None:recorder.terminate();recorder.wait(timeout=10)
        for process in processes.values():
            if process.poll() is None:process.terminate()
        for process in processes.values():
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
        for stream in streams:stream.close()
        (run/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS '+str(run/'result.json'),flush=True)


if __name__=='__main__':main()
