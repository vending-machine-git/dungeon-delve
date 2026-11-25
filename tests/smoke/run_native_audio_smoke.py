"""Record native FMOD effects and mute controls from a disposable game process."""
from pathlib import Path
import json
import os
import re
import shutil
import subprocess
import time
import uuid

import numpy as np
from run_delve_smoke import ROOT, GAME, UM, menu_window


def main():
    evidence = ROOT / 'framework/profiles' / ('native-audio-' + uuid.uuid4().hex[:10])
    sync = evidence / 'sync'
    sync.mkdir(parents=True)
    report = dict(status='RUNNING', profile=str(evidence), checks={})
    processes = []
    streams = []
    print('ARTIFACTS ' + str(evidence), flush=True)

    def launch(name, muted=False):
        profile = evidence / name
        (profile / 'config').mkdir(parents=True)
        settings = dict(version=25, skipintro=True, mods=[], master_volume=50,
                        music_volume=50, gameplay_volume=100, out_of_focus_audio_enabled=True,
                        video=dict(version=1, window_mode=0, resolution_x=1280,
                                   resolution_y=720, vsync_enabled=False))
        (profile / 'config/config.json').write_text(json.dumps(settings))
        for mod in ('vending_machine_framework', 'extraction-raid'):
            for directory in ('data', 'maps', 'images', 'music'):
                source = ROOT / 'mods' / mod / directory
                if source.is_dir():
                    shutil.copytree(source, profile / directory, dirs_exist_ok=True)
        environment = os.environ.copy()
        environment['PATH'] = str(GAME) + os.pathsep + environment.get('PATH', '')
        environment['BARONY_SMOKE_AUDIO_SYNC'] = str(sync)
        stream = (profile / 'process.log').open('wb')
        streams.append(stream)
        process = subprocess.Popen([str(ROOT / 'framework/runtime/barony-raid-smoke.exe'),
            *(['-nosound'] if muted else []), '-windowed', '-size=1280x720',
            '-datadir=' + str(GAME), '-raid-profile=' + str(profile / 'delve-profile.json')],
            cwd=profile, env=environment, stdout=stream, stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW)
        processes.append(process)
        log = profile / 'log.txt'
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise AssertionError('Game exited during audio startup: ' + str(process.returncode))
            if log.exists() and 'giving keyboard to player 0' in log.read_text(errors='replace'):
                return process, log
            time.sleep(.2)
        raise TimeoutError('Native audio startup')

    def check(name, condition):
        report['checks'][name] = bool(condition)
        print(('PASS ' if condition else 'FAIL ') + name, flush=True)
        assert condition, name

    def send(log, master, sound=139):
        previous = log.read_text(errors='replace').count('[NATIVE-AUDIO-TEST]')
        (sync / 'native-audio-command').write_text(f'{master} 0 1 {sound}\n')
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            content = log.read_text(errors='replace')
            if content.count('[NATIVE-AUDIO-TEST]') > previous:
                return re.findall(r'\[NATIVE-AUDIO-TEST\] (.+)', content)[-1]
            time.sleep(.05)
        raise TimeoutError('Native sound command')

    try:
        process, log = launch('enabled')
        startup = log.read_text(errors='replace')
        check('native_fmod_initialized', '[FMOD]: Current audio device:' in startup and 'DISABLING AUDIO' not in startup)
        send(log, .8, -1)
        trace = (evidence / 'recording.log').open('wb')
        streams.append(trace)
        recording = subprocess.Popen([str(UM), 'win', 'record', '--hwnd', str(menu_window(process)),
            '--pid', str(process.pid), '--out', str(evidence / 'native-controls'),
            '--seconds', '18', '--fps', '15'], stdout=trace, stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW)
        processes.append(recording)
        raw = evidence / 'native-controls.audio.raw'

        def until(seconds):
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline:
                if raw.exists() and raw.stat().st_size >= seconds * 48000 * 2 * 4:
                    return
                assert recording.poll() is None, 'Audio recorder exited early'
                time.sleep(.1)
            raise TimeoutError('Audio capture did not advance')

        until(2)
        for sample in range(4):
            check('native_effect_plays_' + str(sample), 'playing=1 nosound=0 group=0.800' in send(log, .8))
            until(3 + sample)
        for sample in range(6):
            send(log, 0)
            until(7 + sample)
        for sample in range(5):
            send(log, .8)
            until(13 + sample)
        assert recording.wait(timeout=45) == 0
        trace.close()
        samples = np.fromfile(raw, dtype='<f4').reshape(-1, 2)
        energies = {name: float(np.sqrt(np.mean(samples[start * 48000:end * 48000] ** 2)))
                    for name, start, end in [('audible', 2, 5), ('muted', 8, 10), ('restored', 14, 17)]}
        report['recorded_rms'] = energies
        check('recorded_native_effects_audible', energies['audible'] > .0001)
        check('master_mute_is_silent', energies['muted'] < .000001)
        check('unmute_restores_native_effects', energies['restored'] > .0001)
        process.terminate()
        process.wait(timeout=15)
        muted, muted_log = launch('nosound', muted=True)
        result = send(muted_log, .8)
        check('nosound_does_not_crash', muted.poll() is None)
        check('nosound_suppresses_native_channel', 'playing=0 nosound=1 group=0.000' in result)
        report['status'] = 'PASS'
        return 0
    except Exception as error:
        report['status'] = 'FAIL'
        report['error'] = str(error)
        raise
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=15)
        for stream in streams:
            stream.close()
        (evidence / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
        print('RESULT ' + report['status'] + ' ' + str(evidence / 'result.json'), flush=True)


if __name__ == '__main__':
    raise SystemExit(main())
