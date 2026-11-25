"""Exercise native class confirmation after hover, details, randomization and cancellation."""
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

ROOT = Path(__file__).resolve().parents[2]
GAME = Path(r'F:\Steam\steamapps\common\Barony')
sys.path.insert(0,str(ROOT/'tests/smoke'))
from dlc_ownership import installed_mask


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--expect-owned-dlc', action='store_true', help='require exactly the classes reported by genuine Steam ownership')
    parser.add_argument('--restrict-dlc', type=int, choices=range(8), help='hide DLC outside this bitmask; cannot grant ownership')
    parser.add_argument('--steam-retry', action='store_true', help='defer initial availability, then require genuine Steam recovery')
    args = parser.parse_args()
    expected_mask=installed_mask()
    if args.restrict_dlc is not None:expected_mask &= args.restrict_dlc
    profile = ROOT / 'framework/profiles' / ('class-picker-' + uuid.uuid4().hex[:10])
    (profile / 'config').mkdir(parents=True)
    (profile / 'config/config.json').write_text(json.dumps(dict(skipintro=True, mods=[], port_number=57536)))
    for mod in ['vending_machine_framework', 'extraction-raid']:
        for directory in ['data', 'maps', 'images']:
            source = ROOT / 'mods' / mod / directory
            if source.is_dir():
                shutil.copytree(source, profile / directory, dirs_exist_ok=True)
    (profile / 'books').mkdir()
    shutil.copy2(GAME / 'books/compiled_books.json', profile / 'books/compiled_books.json')
    env = os.environ.copy()
    env['PATH'] = str(GAME) + os.pathsep + env.get('PATH', '')
    env.update(BARONY_SMOKE_AUTOPILOT='1', BARONY_SMOKE_ROLE='host',
               BARONY_SMOKE_BUILD_CREATOR='1', BARONY_SMOKE_CLASS_PICKER='1', BARONY_SMOKE_CLASS='0')
    if args.steam_retry:env['BARONY_SMOKE_DLC_DELAY_MS']='3000'
    if args.restrict_dlc is not None:env['BARONY_SMOKE_DLC_RESTRICT']=str(args.restrict_dlc)
    report = dict(status='RUNNING', profile=str(profile))
    print('PROFILE', profile, flush=True)
    with (profile / 'process.log').open('wb') as stream:
        process = subprocess.Popen([
            str(ROOT / 'framework/runtime/barony-raid-smoke.exe'), '-windowed', '-size=1280x720',
            '-nosound', '-datadir=' + str(GAME), '-raid-profile=' + str(profile / 'delve-profile.json')
        ], cwd=profile, env=env, stdout=stream, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline:
                log = (profile / 'log.txt').read_text(errors='replace') if (profile / 'log.txt').exists() else ''
                checks = re.findall(r'\[DELVE-CLASS-TEST\] (PASS|FAIL) class=(\d+) ([^\r\n]+)', log)
                assert not any(status == 'FAIL' for status, _, _ in checks), checks
                complete = re.search(r'\[DELVE-CLASS-TEST\] complete checks=(\d+)', log)
                if complete:
                    assert len(checks) == int(complete[1]) and len(checks) >= 101, checks
                    assert all(entry[0]=='PASS' for entry in checks), checks
                    report.update(status='PASS', checks=checks, classes=sorted({int(entry[1]) for entry in checks if entry[2].startswith('confirm ')}))
                    if args.expect_owned_dlc:
                        ownership=re.findall(r'Steam installed DLC: myths=([01]) legends=([01]) deserters=([01])',log)
                        assert ownership, 'Genuine ownership was never queried'
                        owned=tuple(map(int,ownership[-1]))
                        assert sum(v<<i for i,v in enumerate(owned))==expected_mask, 'Native detection disagrees with independent Steam query'
                        expected=list(range(13))+([13,14,15,16] if owned[0] else [])+([17,18,19,20] if owned[1] else [])+([21,22,23,24,25] if owned[2] else [])
                        assert report['classes']==expected, (report['classes'],expected)
                        report['ownership']=owned
                    if args.steam_retry:
                        assert 'Smoke fixture defers Steam availability' in log
                        assert re.search(r'Steam installed DLC: myths=[01] legends=[01] deserters=[01]',log)
                        report['steam_retry']='PASS: deferred initial query recovered through real Steam API'
                    print('PASS', len(checks), 'native checks;', len(report['classes']), 'available classes', flush=True)
                    break
                assert process.poll() is None, 'Native audit exited early'
                time.sleep(.2)
            else:
                raise TimeoutError('Native class picker audit timed out')
        except BaseException as error:
            report.update(status='FAIL', error=repr(error))
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
            (profile / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
            print('REPORT', profile / 'result.json', flush=True)


if __name__ == '__main__':
    main()
