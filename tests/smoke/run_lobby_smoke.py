"""Real Direct IP tavern arrivals, disconnect/rejoin, editor, music and deployment.

Uses only disposable profiles and per-window capture, with no desktop input.
"""
from pathlib import Path
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
import uuid
from run_delve_smoke import capture, menu_window
from dlc_ownership import installed_mask

ROOT = Path(__file__).resolve().parents[2]
GAME = Path(r'F:\Steam\steamapps\common\Barony')
UM = Path(os.environ.get('UNIVERSAL_MODDER', str(Path.home() / 'Documents/Projects/[Tools]/universal-modder/bin/um.cmd')))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--size', default='1280x720')
    parser.add_argument('--port', type=int, default=57321)
    parser.add_argument('--record', action='store_true')
    parser.add_argument('--quick', action='store_true', help='capture only the host for visual iteration')
    parser.add_argument('--fresh-ready', action='store_true', help='verify Ready on fresh peers without the separate rejoin probe')
    parser.add_argument('--muted', action='store_true', help='exercise -nosound during a quick visual capture')
    parser.add_argument('--audio-controls', action='store_true', help='record real mute/unmute and lobby-exit playback')
    parser.add_argument('--dlc-masks', help='four comma-separated restriction masks; only hide genuine ownership')
    parser.add_argument('--dlc-classes', default='0,13,17,0', help='four class selections for mixed-ownership check')
    parser.add_argument('--dlc-races', default='0,1,5,0', help='four race selections for mixed-ownership check')
    args = parser.parse_args()
    masks = list(map(int,args.dlc_masks.split(','))) if args.dlc_masks else None
    classes = list(map(int,args.dlc_classes.split(',')))
    races = list(map(int,args.dlc_races.split(',')))
    if masks is not None:
        assert args.fresh_ready and not args.quick, 'Mixed-ownership test requires full fresh native Ready flow'
        assert len(masks)==len(classes)==len(races)==4 and all(0<=m<=7 for m in masks)
        genuine=installed_mask()
        for m,c,r in zip(masks,classes,races):
            assert m & genuine == m, 'Native fixtures may only restrict real ownership'
            assert 0<=c<26 and 0<=r<18
            class_pack=0 if c<13 else 1 if c<17 else 2 if c<21 else 4
            race_pack=0 if r==0 else 1 if r<=4 else 2 if r<=12 else 4
            assert m & (class_pack|race_pack) == (class_pack|race_pack), 'Fixture selection requires DLC hidden from that peer'


    run = ROOT/'framework/profiles'/('delve-tavern-'+uuid.uuid4().hex[:10])
    (run/'sync').mkdir(parents=True)
    print('ARTIFACTS '+str(run), flush=True)
    processes, streams = {}, []
    folders = {}
    report = dict(status='RUNNING', checks={}, size=args.size, run=str(run))

    def log(role):
        file = folders[role]/'log.txt'
        return file.read_text(errors='replace') if file.exists() else ''

    def wait_for(role, pattern, timeout=80):
        deadline = time.monotonic()+timeout
        while time.monotonic() < deadline:
            matches = re.findall(pattern, log(role))
            if matches: return matches[-1]
            if processes[role].poll() is not None:
                raise RuntimeError(role+' exited '+str(processes[role].returncode))
            time.sleep(.2)
        raise RuntimeError(role+' timeout: '+pattern)

    def check(name, condition):
        report['checks'][name] = bool(condition)
        print(('PASS ' if condition else 'FAIL ')+name, flush=True)
        assert condition, name

    def launch(slot, rejoin=False):
        role = 'host' if slot == 0 else 'client'+str(slot)
        folder = run/(role+('-rejoined' if rejoin else ''))
        folders[role] = folder
        (folder/'config').mkdir(parents=True)
        width, height = map(int, args.size.split('x'))
        settings = dict(version=25, skipintro=True, mods=[], port_number=args.port,
                        master_volume=35, music_volume=50 if slot == 0 else 0,
                        gameplay_volume=0 if args.audio_controls else 100,
                        ambient_volume=0 if args.audio_controls else 100,
                        environment_volume=0 if args.audio_controls else 100,
                        notification_volume=0 if args.audio_controls else 100,
                        video=dict(version=1, window_mode=0, resolution_x=width,
                                   resolution_y=height, vsync_enabled=False))
        (folder/'config/config.json').write_text(json.dumps(settings))
        for mod in ('vending_machine_framework', 'extraction-raid'):
            for directory in ('data', 'maps', 'images', 'music'):
                source = ROOT/'mods'/mod/directory
                if source.is_dir(): shutil.copytree(source, folder/directory, dirs_exist_ok=True)
        books = GAME/'books/compiled_books.json'
        if books.exists():
            (folder/'books').mkdir(); shutil.copy2(books, folder/'books/compiled_books.json')
        env = os.environ.copy()
        env['PATH'] = str(GAME)+os.pathsep+env.get('PATH', '')
        env.update(BARONY_SMOKE_AUTOPILOT='1', BARONY_SMOKE_TAVERN='1',
                   BARONY_SMOKE_ROLE='host' if slot == 0 else 'client',
                   BARONY_SMOKE_CLASS=str(slot), BARONY_SMOKE_RACE=str(slot),
                   BARONY_SMOKE_RAID_SYNC=str(run/'sync'),
                   BARONY_SMOKE_CONNECT_ADDRESS='127.0.0.1:'+str(args.port))
        if masks is not None:
            env.update(BARONY_SMOKE_DLC_RESTRICT=str(masks[slot]),
                       BARONY_SMOKE_CLASS=str(classes[slot]),BARONY_SMOKE_RACE=str(races[slot]))
        stream = (folder/'process.log').open('wb'); streams.append(stream)
        proc = subprocess.Popen([str(ROOT/'framework/runtime/barony-raid-smoke.exe'),
            *(['-nosound'] if slot or args.muted else []),
            '-windowed', '-size='+args.size, '-datadir='+str(GAME),
            '-raid-profile='+str(folder/'delve-profile.json')], cwd=folder, env=env,
            stdout=stream, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
        processes[role] = proc
        print(role+' pid='+str(proc.pid), flush=True)
        return role

    def shot(stage):
        folder = run/stage; folder.mkdir()
        capture({'host':processes['host']}, folder)

    try:
        launch(0)
        wait_for('host', 'host_ready')
        wait_for('host', r'\[DELVE-TAVERN-TEST\] seats=1 models=1 music=0 frames=0' if args.muted else
                 r'\[DELVE-TAVERN-TEST\] seats=1 models=1 music=1 frames=([1-9]\d*)')
        check('host_seated_with_nosound' if args.muted else 'host_seated_with_playing_audio', True)
        check('tabletop_support_and_clearance', '[DELVE-LOBBY] check PASS tabletop_support_and_clearance' in log('host'))
        shot('one-player')
        if args.audio_controls:
            import numpy as np
            trace=(run/'audio-record.log').open('wb')
            recorder=subprocess.Popen([str(UM),'win','record','--hwnd',str(menu_window(processes['host'])),
                '--pid',str(processes['host'].pid),'--out',str(run/'audio-controls'),
                '--seconds','18','--fps','15'],stdout=trace,stderr=subprocess.STDOUT)
            raw=run/'audio-controls.audio.raw'
            def until_audio(seconds):
                deadline=time.monotonic()+45
                while time.monotonic()<deadline:
                    if raw.exists() and raw.stat().st_size>=seconds*48000*2*4:return
                    assert recorder.poll() is None, 'Audio recorder exited early'
                    time.sleep(.1)
                raise TimeoutError('Audio capture did not advance')
            until_audio(6)
            (run/'sync/tavern-volume').write_text('0')
            wait_for('host',r'\[DELVE-TAVERN-TEST\] music volume=0')
            until_audio(11)
            (run/'sync/tavern-volume').write_text('100')
            wait_for('host',r'\[DELVE-TAVERN-TEST\] music volume=100')
            assert recorder.wait(timeout=40)==0;trace.close()
            samples=np.fromfile(raw,dtype='<f4').reshape(-1,2)
            energies={name:float(np.sqrt(np.mean(samples[start*48000:end*48000]**2)))
                      for name,start,end in [('before',2,4),('muted',8,10),('after',13,15)]}
            report['recorded_rms']=energies
            check('recorded_music_audible',energies['before']>.0001)
            check('music_slider_zero_is_silent',energies['muted']<.000001)
            check('music_slider_restores_audio',energies['after']>.0001)
            (run/'sync/tavern-leave-0').write_text('leave')
            wait_for('host',r'\[DELVE-LOBBY\] music stopped frames=([1-9]\d*)')
            check('leaving_lobby_stops_music',True)
            report['status']='PASS'
            return 0
        if args.quick:
            if args.muted:check('nosound_never_opens_audio', '[DELVE-LOBBY] music playing' not in log('host'))
            report['status']='PASS_VISUAL_CAPTURE'
            return 0
        for slot in range(1, 4):
            launch(slot)
            mask = (1 << (slot+1))-1
            wait_for('host', rf'\[DELVE-TAVERN-TEST\] seats={mask} models={slot+1} music=1 frames=([1-9]\d*)')
            wait_for('client'+str(slot), rf'\[DELVE-TAVERN-TEST\] seats={mask} models={slot+1} music=0 frames=0')
            check('arrival_'+str(slot+1)+'_fills_unique_seat_on_both_peers', True)
            shot(str(slot+1)+'-players')
        folder=run/'guest-view';folder.mkdir()
        capture({'client3':processes['client3']}, folder)
        if masks is not None:
            for slot in range(4):
                role='host' if slot==0 else 'client'+str(slot)
                expected=rf'\[DELVE-DLC-TEST\] local={masks[slot]} classes='+','.join(map(str,classes))+' races='+','.join(map(str,races))
                wait_for(role,expected)
                check(role+'_retains_own_entitlements_and_receives_full_mixed_roster',True)
            report['mixed_ownership']=dict(restriction_masks=masks,classes=classes,races=races,
                scope='Only intersects genuine ownership; no fake positive entitlement enters native games')
        if args.record:
            subprocess.run([str(UM),'win','record','--hwnd',str(menu_window(processes['host'])),
                '--pid',str(processes['host'].pid),'--out',str(run/'tavern-showcase'),
                '--seconds','15','--fps','30'],check=True)
        if not args.fresh_ready:
            (run/'sync/tavern-leave-2').write_text('leave')
            wait_for('host', r'\[DELVE-TAVERN-TEST\] seats=11 models=3 music=1 frames=([1-9]\d*)')
            check('disconnect_vacates_seat', True)
            wait_for('client2', r'\[DELVE-TAVERN-TEST\] native disconnect')
            check('nosound_guest_never_opens_audio', '[DELVE-LOBBY] music playing' not in log('client2'))
            processes['client2'].terminate(); processes['client2'].wait(timeout=15)
            shot('vacated-seat')
            launch(2, rejoin=True)
            deadline=time.monotonic()+80
            while time.monotonic()<deadline:
                rows=re.findall(r'\[DELVE-TAVERN-TEST\] seats=(\d+) models=(\d+)',log('host'))
                if rows and rows[-1]==('15','4'): break
                time.sleep(.2)
            check('rejoin_restores_vacant_seat', rows[-1]==('15','4'))
            shot('rejoined')
        (run/'sync/tavern-edit-0').write_text('edit')
        wait_for('host', r'\[DELVE-TAVERN-TEST\] editor opened')
        time.sleep(1)
        check('native_character_editor_available', True)
        shot('character-editor')
        # Let every native character/flags update settle before anyone readies.
        # Editing a character legitimately invalidates earlier ready acknowledgments.
        for slot in (1, 2, 3):
            (run/('sync/tavern-edit-'+str(slot))).write_text('edit')
            wait_for('client'+str(slot), r'\[DELVE-TAVERN-TEST\] editor opened')
        time.sleep(3)
        if args.fresh_ready:
            for slot in (1, 2, 3, 0):
                (run/('sync/tavern-ready-'+str(slot))).write_text('ready')
                role = 'host' if slot == 0 else 'client'+str(slot)
                wait_for(role, r'\[DELVE-TAVERN-TEST\] native ready activated')
            check('all_four_native_ready_buttons_activate', True)
        else:
            (run/'sync/tavern-start').write_text('start')
            check('host_start_after_rejoin_requested', True)
        wait_for('host', r'\[DELVE-LOBBY\] music stopped frames=([1-9]\d*)')
        wait_for('host', r'\[DELVE\] floor_ready floor=1',timeout=90)
        check('music_stops_when_match_starts', True)
        if masks is not None:
            for slot in range(1,4):
                wait_for('client'+str(slot),r'\[DELVE\] load floor=1 portals=1',timeout=90)
                wait_for('host',rf'\[DELVE\] equipment_ready slot={slot}',timeout=90)
                check('client'+str(slot)+'_mixed_ownership_deploys',True)
            for slot in range(4):
                role='host' if slot==0 else 'client'+str(slot)
                check(role+'_no_invalid_license_key_logged',
                    'DLC license key invalid' not in log(role) and 'DELVE-CLASS-TEST] FAIL' not in log(role))
        report['status']='PASS'
    except Exception as error:
        report['status']='FAIL';report['error']=repr(error)
        print('FAIL '+repr(error), flush=True)
    finally:
        for proc in processes.values():
            if proc.poll() is None: proc.terminate()
        for proc in processes.values():
            try:proc.wait(timeout=15)
            except subprocess.TimeoutExpired:proc.kill();proc.wait()
        for stream in streams:stream.close()
        (run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print('RESULT '+report['status']+' '+str(run/'result.json'),flush=True)
    return int(report['status']!='PASS')


if __name__ == '__main__':
    sys.exit(main())
