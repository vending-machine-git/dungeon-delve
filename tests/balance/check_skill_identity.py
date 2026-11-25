"""Check the class-identity redesign and exact authored duration multiplier."""
import ast
from pathlib import Path
import runpy
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'framework/tools'))


def main():
    module = runpy.run_path(str(ROOT / 'framework/tools/make_class_builds.py'))
    classes = {entry['name']: entry for entry in module['CLASSES']}
    authored = ast.parse((ROOT / 'framework/tools/make_class_builds.py').read_text())
    timed_effects = 0
    for node in authored.body:
        if not isinstance(node, ast.Expr) or not isinstance(node.value, ast.Call):
            continue
        if not isinstance(node.value.func, ast.Name) or node.value.func.id != 'add':
            continue
        class_name, abilities = map(ast.literal_eval, node.value.args)
        for (ability_name, text), generated in zip(abilities, classes[class_name]['abilities']):
            assert generated['name'] == ability_name
            rules = dict(pair.split('=') for pair in text.split())
            for field, value in rules.items():
                if field not in ('self', 'control'):
                    assert generated['rules'][field] == value
                    continue
                expected = [(effect, float(seconds) * 1.25 * (1.25 if effect == 'INVISIBLE' else 1))
                            for effect, seconds in (pair.split(':') for pair in value.split(','))]
                actual = [(effect, float(seconds)) for effect, seconds in
                          (pair.split(':') for pair in generated['rules'][field].split(','))]
                assert actual == expected, (class_name, ability_name, field)
                timed_effects += len(actual)
    authored_perks = ast.parse((ROOT / 'framework/tools/delve_passive_rules.py').read_text())
    perks = {perk['name']: perk for entry in classes.values() for perk in entry['perks']}
    for node in ast.walk(authored_perks):
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == 'perk':
            values = list(map(ast.literal_eval, node.args))
            generated = perks[values[0]]
            assert generated['duration'] == values[4] * 1.25
            assert generated['cooldown'] == values[5]
    identities = {
        'Heavy Reprisal': 'WoundedBlock', 'Hard Counteroffer': 'Cache',
        'Runic Cadence': 'Crossfire', 'Tricksters Exit': 'Variety',
        'Wakeful Guard': 'ControlledKill', 'Shadow Route': 'ThrownRear',
        'Centered Counter': 'UnarmedCounter', 'Soul Cadence': 'SummonEcho',
        'Night Return': 'BloodHunt', 'Illusion Opening': 'ControlledStrike',
        'Chain Rhythm': 'Execution', 'Hunters Remedy': 'RangedKill',
        'Refrain': 'Refrain', 'Demolition Entry': 'Breacher',
        'Noble Riposte': 'DuelCounter', 'Quiet Opening': 'PatientStrike',
        'Renewed Resolve': 'CleanseBarrier', 'Oath Overflow': 'HealingOath',
    }
    for name, kind in identities.items():
        assert perks[name]['kind'] == kind
    assert perks['Refrain']['description'].count('10s') == 1
    assert perks['Reactive Frost']['duration'] == 0.9375
    assert '0.9375s' in perks['Reactive Frost']['description']
    assert '6.25s' in perks['Soul Cadence']['description']
    assert len(perks) == 156 and timed_effects > 0
    print(f'PASS 18 redesigned passives, 156 passive timings, {timed_effects} active effect timings; costs/cooldowns unchanged')


if __name__ == '__main__':
    main()
