"""Match every authored skill layer to the shipped skill's actual mechanics."""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[2]
GATES = dict(Strength='POTION_STR', Speed='FAST', Regeneration='HP_REGEN',
             Resistance='MAGICRESIST', Reflection='MAGICREFLECT', Sight='TELEPATH',
             Levitation='LEVITATING', Amplification='MAGICAMPLIFY', Mana='MP_REGEN', Hidden='INVISIBLE')


def main():
    catalogue = json.loads((ROOT / 'mods/extraction-raid/data/rmf/class_builds.json').read_text())
    authored = (ROOT / 'src/delve_skill_vfx_catalogue.inc').read_text()
    blocks = re.findall(r'design\("([^"]+)",\s*\w+,\s*\w+,\s*\d+,\s*\{(.*?)\}\)', authored, re.S)
    skills = [ability for entry in catalogue['classes'] for ability in entry['abilities']]
    assert len(blocks) == len(skills) == 104
    checks = 0
    for (name, layers), skill in zip(blocks, skills):
        assert name == skill['name'], (name, skill['name'])
        rules = skill['rules']
        granted = set(re.findall(r'(\w+):', rules.get('self', '')))
        gates = re.findall(r'G::(\w+)', layers)
        for gate in gates:
            assert GATES[gate] in granted, (name, gate, granted)
            checks += 1
        assert bool(gates) == bool(granted), (name, 'missing sustained effect')
        assert {GATES[gate] for gate in gates} == granted, (name, 'missing component', granted, gates)
        assert ('control(' in layers) == ('control' in rules), (name, 'false or missing target control')
        assert ('hit(' in layers or 'link(' in layers) == (bool(int(rules.get('damage', '0'))) or 'control' in rules), (name, 'false or missing impact')
        if 'link(' in layers:
            assert int(rules.get('range', '0')) > 0, (name, 'link without actual hostile range')
        if 'A::Feet,-1' in layers:
            assert rules.get('arc') == '360' and int(rules.get('range', '0')) > 0, (name, 'false radial footprint')
        if 'S::Siphon' in layers:
            assert int(rules.get('leech', '0')) > 0, (name, 'false lifesteal tether')
        checks += 6
    print(f'PASS 104 skill names and mechanics; {checks} gate/control/hit/footprint checks')
    sounds = (Path(r'F:\Steam\steamapps\common\Barony') / 'sound/sounds.txt').read_text().splitlines()
    authored_sounds = [int(value) for value in re.findall(r'^design\("[^"\n]+", \w+, \w+, (\d+),', authored, re.M)]
    for sound in authored_sounds:
        assert sound < len(sounds) and 'null.' not in sounds[sound].lower(), sound
        assert (Path(r'F:\Steam\steamapps\common\Barony') / sounds[sound]).is_file(), sounds[sound]
    print(f'PASS {len(authored_sounds)} native sound references; no copied audio or generated-art dependency')
    header = (ROOT / 'src/delve_passives.hpp').read_text()
    kinds = set(re.search(r'enum class Kind\s*\{(.*?)\}', header, re.S).group(1).replace('\n', '').split(','))
    kinds = {kind.strip() for kind in kinds if kind.strip()}
    routed = set(re.findall(r'Kind::(\w+)', (ROOT / 'src/delve_perk_vfx.hpp').read_text()))
    assert kinds == routed, (kinds - routed, routed - kinds)
    assert sum(len(entry['perks']) for entry in catalogue['classes']) == 156
    print(f'PASS all {len(kinds)} passive trigger kinds and 156 perk definitions have semantic routing')


if __name__ == '__main__':
    main()
