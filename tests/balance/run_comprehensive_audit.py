"""Run supported native regressions sequentially with durable per-case evidence."""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]


def main():
    source = ROOT / 'framework/profiles/gear-smoke-08544ff23b4e'
    cases = {
        'class-picker': ['tests/balance/check_class_picker.py'],
        'skills-perks': ['tests/balance/run_balance_audit.py', '--builds', '--capture', '--effects-capture'],
        'battle': ['tests/smoke/run_raid_smoke.py', '--faults', '--capture', '--timeout', '180'],
        'mixed-builds': ['tests/smoke/run_delve_smoke.py', '--scenario', 'battle-royale', '--builds', '--classes', '16,7,15,9', '--races', '4,1,2,5', '--capture', '--timeout', '180'],
        'rarity': ['tests/smoke/run_delve_smoke.py', '--scenario', 'rarity', '--capture', '--timeout', '180'],
        'returns': ['tests/smoke/run_delve_smoke.py', '--scenario', 'revenants', '--capture', '--timeout', '180'],
        'restart': ['tests/smoke/run_delve_smoke.py', '--scenario', 'restart', '--capture', '--timeout', '240'],
        'population-concealment': ['tests/smoke/run_delve_smoke.py', '--scenario', 'population', '--scenario', 'concealment', '--capture'],
        'abilities-appraisal': ['tests/smoke/run_delve_smoke.py', '--scenario', 'abilities', '--scenario', 'appraisal', '--scenario', 'appraisal-disabled', '--capture'],
        'cooldowns-results': ['tests/smoke/run_delve_smoke.py', '--scenario', 'cooldowns', '--scenario', 'results', '--capture'],
        'ui-small': ['tests/smoke/run_delve_smoke.py', '--scenario', 'abilities', '--scenario', 'cooldowns', '--scenario', 'results', '--size', '1024x720', '--ui-scale', '50', '--capture'],
        'ui-wide': ['tests/smoke/run_delve_smoke.py', '--scenario', 'abilities', '--scenario', 'cooldowns', '--scenario', 'results', '--size', '3440x1440', '--ui-scale', '75', '--capture'],
        'intro': ['tests/smoke/run_intro_smoke.py', '--floors'],
        'gear': ['tests/smoke/run_gear_smoke.py', '--host-import', str(source / 'host/import.baronysave'), '--guest-import', str(source / 'client/import.baronysave')],
        'join-four-loss': ['tests/smoke/run_join_smoke.py', '--extra-guests', '2', '--check-full', '--loss', '--capture'],
        'join-recovery': ['tests/regression/run_extended.py', '--case', 'snapshot-host-recovery', '--case', 'snapshot-client-recovery', '--case', 'checksum-rejection-retry', '--case', 'queued-arrival', '--case', 'join-swamp-floor5', '--case', 'join-boss-floor10'],
        'join-abort': ['tests/smoke/run_join_smoke.py', '--abort-transfer', '--retry-abort'],
        'join-equipment-abort': ['tests/smoke/run_join_smoke.py', '--abort-after-equipment', '--retry-abort'],
        'invalid-imports': ['tests/regression/run_input_checks.py'],
        'packages': ['tests/regression/check_packages.py'],
    }
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=list(cases), action='append')
    args = parser.parse_args()
    selected = {name: command for name, command in cases.items() if not args.case or name in args.case}
    run = ROOT / 'modding-evidence' / ('comprehensive-audit-' + uuid.uuid4().hex[:10])
    run.mkdir()
    print('AUDIT ' + str(run), flush=True)
    report = dict(status='RUNNING', cases={})
    for name, command in selected.items():
        print('START ' + name, flush=True)
        log = run / (name + '.log')
        started = time.monotonic()
        with log.open('w', encoding='utf-8') as stream:
            result = subprocess.run([sys.executable, *command], cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
        text = log.read_text(encoding='utf-8', errors='replace')
        row = dict(status='PASS' if result.returncode == 0 else 'FAIL', exit_code=result.returncode,
                   seconds=round(time.monotonic() - started, 2), log=str(log))
        row['profiles'] = re.findall(r'^ARTIFACTS (.+)$', text, re.M)
        if not result.returncode and name in ('abilities-appraisal', 'cooldowns-results', 'ui-small', 'ui-wide'):
            validators = {
                'delve-abilities-': 'check_ability_hud.py',
                'delve-appraisal-': 'check_ground_item_hud.py',
                'delve-cooldowns-': 'check_cooldown_hud.py',
                'delve-results-': 'check_results_ui.py',
            }
            row['screenshot_validation'] = []
            for profile in row['profiles']:
                script = next((script for prefix, script in validators.items() if Path(profile).name.startswith(prefix)), None)
                if script is None:
                    raise RuntimeError('No screenshot validator for ' + profile)
                with log.open('a', encoding='utf-8') as stream:
                    validation = subprocess.run([sys.executable, str(ROOT / 'tests/balance' / script), profile],
                                                cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
                row['screenshot_validation'].append(dict(profile=profile, script=script, exit_code=validation.returncode))
                if validation.returncode:
                    row['status'] = 'FAIL'
        report['cases'][name] = row
        print(row['status'] + ' ' + name, flush=True)
        if row['status'] == 'FAIL':
            print(log.read_text(encoding='utf-8', errors='replace')[-2500:], flush=True)
        (run / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    report['status'] = 'PASS' if all(row['status'] == 'PASS' for row in report['cases'].values()) else 'FAIL'
    (run / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('RESULT ' + report['status'] + ' ' + str(run / 'result.json'), flush=True)
    return int(report['status'] != 'PASS')


if __name__ == '__main__':
    sys.exit(main())
