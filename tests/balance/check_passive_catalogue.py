"""Check authored perk coverage and reproducible generated runtime/package data."""
from pathlib import Path
import json
import runpy
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'framework/tools'))


def main():
    module = runpy.run_path(str(ROOT / 'framework/tools/make_class_builds.py'))
    classes = module['CLASSES']
    assert len(classes) == 26
    assert len({entry['name'] for entry in classes}) == 26
    names = set()
    header = (ROOT / 'src/delve_passives.hpp').read_text(encoding='utf-8')
    for entry in classes:
        assert len(entry['perks']) == 6 and len(entry['abilities']) == 4
        for passive in entry['perks']:
            assert passive['name'] not in names
            names.add(passive['name'])
            assert 'modifiers' not in passive
            assert passive['kind'] in header and passive['reward'] in header
            assert passive['description'] and all(passive[field] >= 0 for field in ('value', 'duration', 'cooldown'))
            assert passive['filter'] in (-1, 1, 6, 7, 8, 9)
        for ability in entry['abilities']:
            assert int(ability['rules'].get('hpCost', 0)) == 0
    assert len(names) == 156
    outputs = ('src/delve_passives_catalogue.inc', 'src/delve_builds_catalogue.inc',
               'mods/extraction-raid/data/rmf/class_builds.json', 'DELVE_CLASS_BUILDS.md')
    with tempfile.TemporaryDirectory(prefix='delve-passive-catalogue-') as temporary:
        temporary_root = Path(temporary)
        for output in outputs:
            (temporary_root / output).parent.mkdir(parents=True, exist_ok=True)
        module['main'].__globals__['ROOT'] = temporary_root
        module['main']()
        for output in outputs:
            assert (temporary_root / output).read_bytes() == (ROOT / output).read_bytes(), output
    package = json.loads((ROOT / outputs[2]).read_text(encoding='utf-8'))
    assert package['version'] == 2
    print('PASS 156 behavioral passives, 104 penalty-free abilities, reproducible generated outputs')


if __name__ == '__main__':
    main()
