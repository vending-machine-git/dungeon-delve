"""Check dungeon-entry narration on real host/guest clients, not story screens."""
from pathlib import Path
import argparse
import json
import os
import re
import shutil
import subprocess
import time
import uuid
from run_delve_smoke import ROOT, GAME, UM, menu_window


def scenario(name, record=False, floors=False):
    run = ROOT/'framework/profiles'/('dungeon-intro-'+name+'-'+uuid.uuid4().hex[:10])
    (run/'sync').mkdir(parents=True)
    authored = {floor: json.loads((ROOT/'mods/extraction-raid/data/rmf'/('intro.json' if floor == 1 else f'intro-floor-{floor}.json')).read_text()) for floor in (1, 2, 3)}
    processes, streams, logs = {}, [], {}
    report = dict(status='RUNNING', scenario=name, profile=str(run), floors=[1, 2, 3] if floors else [1])
    recorders = {}
    print('ARTIFACTS '+str(run), flush=True)
    try:
        for slot in range(2):
            role = 'host' if slot == 0 else 'guest'
            profile = run/role
            (profile/'config').mkdir(parents=True)
            settings = json.loads((GAME/'config/config.json').read_text(encoding='utf-8'))
            settings.update(version=25, skipintro=True, mods=[], port_number=57169,
                            master_volume=0.0 if name == 'muted' else 70.0,
                            gameplay_volume=100.0, music_volume=0.0)
            settings['video'].update(window_mode=0, resolution_x=1280, resolution_y=720, vsync_enabled=False)
            (profile/'config/config.json').write_text(json.dumps(settings))
            for mod in ('vending_machine_framework', 'extraction-raid'):
                for directory in ('data', 'maps', 'images', 'music'):
                    source = ROOT/'mods'/mod/directory
                    if source.is_dir():
                        shutil.copytree(source, profile/directory, dirs_exist_ok=True)
            if name == 'missing':
                (profile/('music/rmf/dungeon-floor-2.wav' if floors else 'music/rmf/dungeon-intro.wav')).unlink()
            env = os.environ.copy()
            env['PATH'] = str(GAME)+os.pathsep+env.get('PATH', '')
            env.update(BARONY_SMOKE_AUTOPILOT='1', BARONY_SMOKE_ROLE='host' if slot == 0 else 'client',
                       BARONY_SMOKE_PLAYERS='2', BARONY_SMOKE_CONNECT_ADDRESS='127.0.0.1:57169')
            env.pop('BARONY_SMOKE_INTRO', None)
            env.pop('BARONY_SMOKE_DELVE', None)
            if floors:
                env.update(BARONY_SMOKE_DELVE='narration', BARONY_SMOKE_RAID_SYNC=str(run/'sync'))
            command = [str(ROOT/'framework/runtime/barony-raid-smoke.exe'), '-windowed', '-size=1280x720',
                       '-datadir='+str(GAME), '-raid-profile='+str(profile/'delve-profile.json')]
            if name == 'nosound':
                command.append('-nosound')
            stream = (profile/'process.log').open('wb')
            streams.append(stream)
            processes[role] = subprocess.Popen(command, cwd=profile, env=env, stdout=stream,
                stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
            if slot == 0:
                deadline = time.monotonic()+45
                while time.monotonic() < deadline:
                    file = profile/'log.txt'
                    if file.exists() and 'host_ready' in file.read_text(errors='replace'):
                        break
                    assert processes[role].poll() is None, 'Host exited before lobby'
                    time.sleep(.1)
                else:
                    raise TimeoutError('Lobby timed out')
        deadline = time.monotonic()+(180 if floors else 85)
        captured = set()
        deployed = None
        while time.monotonic() < deadline:
            for role, process in processes.items():
                assert process.poll() is None, role+' exited '+str(process.returncode)
                file = run/role/'log.txt'
                logs[role] = file.read_text(errors='replace') if file.exists() else ''
                assert 'check FAIL' not in logs[role]
            for floor in ((1, 2, 3) if floors else (1,)):
                started = f'[DELVE-INTRO] dungeon-start floor={floor}' in logs['host']
                if record and floor not in recorders and started:
                    stream = (run/f'record-floor-{floor}.log').open('wb')
                    streams.append(stream)
                    recorders[floor] = subprocess.Popen([str(UM), 'win', 'record', '--hwnd', str(menu_window(processes['host'])),
                        '--pid', str(processes['host'].pid), '--out', str(run/f'narration-floor-{floor}'),
                        '--seconds', str(int(authored[floor]['duration_seconds'])+2), '--fps', '20'],
                        stdout=stream, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
                for cue in (0, len(authored[floor]['cues'])-1):
                    marker = re.search(r'\[DELVE-INTRO\] subtitle='+str(cue)+r' position=[0-9.]+ floor='+str(floor)+r'\b', logs['host'])
                    if record and (floor, cue) not in captured and marker:
                        subprocess.run([str(UM), 'win', 'shot', '--hwnd', str(menu_window(processes['host'])),
                            '--scale', '1', str(run/f'floor-{floor}-subtitle-{cue}.png')], check=True, capture_output=True)
                        captured.add((floor, cue))
            if floors:
                if all('[INTRO-FLOORS] complete slot=' in log for log in logs.values()):
                    break
            elif name == 'missing':
                if all('[RAID] deployed' in log for log in logs.values()):
                    if deployed is None:
                        deployed = time.monotonic()
                    if time.monotonic()-deployed > 4:
                        break
            elif all('[DELVE-INTRO] stopped frames=' in log for log in logs.values()):
                break
            time.sleep(.05)
        else:
            raise TimeoutError('Dungeon narration timed out')
        observed = {}
        for role, log in logs.items():
            assert '[RAID] deployed' in log
            expected_floors = [1, 2, 3] if floors else [1]
            missing_floor = (2 if floors else 1) if name == 'missing' else 0
            played_floors = [floor for floor in expected_floors if floor != missing_floor]
            starts = list(map(int, re.findall(r'\[DELVE-INTRO\] dungeon-start floor=(\d+)', log)))
            assert starts == played_floors, (role, starts)
            assert log.count('[DELVE-INTRO] playing') == len(played_floors)
            assert log.count('You hear a voice inside your head...') == len(played_floors)
            assert log.count('[DELVE-INTRO] stopped frames=') == len(played_floors)
            cues = [(int(floor), int(index), float(position)) for index, position, floor in
                    re.findall(r'\[DELVE-INTRO\] subtitle=(\d+) position=([0-9.]+) floor=(\d+)', log)]
            for floor in expected_floors:
                entries = authored[floor]['cues'] if floor in played_floors else []
                timed = [(index, position) for cue_floor, index, position in cues if cue_floor == floor]
                assert [index for index, position in timed] == list(range(len(entries))), (role, floor, timed)
                assert all(abs(position-entries[index]['start']) < .2 for index, position in timed), (role, floor, timed)
                for entry in entries:
                    dialogue = '"'+entry['text'].replace('\n', ' ')+'"'
                    assert dialogue in log, (role, floor, dialogue)
            if played_floors:
                expected_audio = '0' if name == 'nosound' else '1'
                assert log.count('playing audio='+expected_audio) == len(played_floors)
                if name == 'muted':
                    assert log.count('playing audio=1 gain=0') == len(played_floors)
            observed[role] = cues
        for floor, recorder in recorders.items():
            assert recorder.wait(timeout=35) == 0
            assert (run/f'narration-floor-{floor}.audio.raw').stat().st_size > 1000000
        report.update(status='PASS', observed_subtitles=observed)
    except Exception as error:
        report.update(status='FAIL', error=repr(error))
        raise
    finally:
        for recorder in recorders.values():
            if recorder.poll() is None:
                recorder.terminate()
                recorder.wait(timeout=10)
        for process in processes.values():
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
        for stream in streams:
            stream.close()
        (run/'result.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS '+name, flush=True)
    return report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--scenario', action='append', choices=['complete', 'nosound', 'muted', 'missing'])
    parser.add_argument('--record', action='store_true')
    parser.add_argument('--floors', action='store_true', help='exercise all three floor entries via native wipe transitions')
    args = parser.parse_args()
    results = [scenario(name, args.record and name == 'complete', args.floors) for name in
               args.scenario or ['complete', 'nosound', 'muted', 'missing']]
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
