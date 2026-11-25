"""Validate generated and initialized Delve rosters independently of interaction fixtures."""
from collections import Counter
from pathlib import Path
import argparse
import json
import re

ROOT = Path(__file__).resolve().parents[2]
BAGS = [
    dict(RAT=4, BAT_SMALL=1, SKELETON=5, SLIME=2),
    dict(RAT=3, BAT_SMALL=1, SKELETON=4, SLIME=2, GOBLIN=1, GHOUL=1),
    dict(RAT=2, BAT_SMALL=1, SKELETON=4, SLIME=1, GOBLIN=2, GHOUL=1, TROLL=1),
]


def validate(profile):
    report = dict(status='RUNNING', checks=[])

    def check(condition, label):
        report['checks'].append(dict(label=label, passed=bool(condition)))
        assert condition, label

    try:
        source = (ROOT / 'src/monster.hpp').read_text()
        enum = re.search(r'enum Monster : int\s*\{(.*?)\};', source, re.S)[1]
        names = [name.strip() for name in re.sub(r'//[^\n]*', '', enum).split(',') if name.strip()]
        monster_ids = {name: index for index, name in enumerate(names)}
        log = (profile / 'host/log.txt').read_text(errors='replace')
        check('check PASS population_3000_seeded_rosters' in log, '3000 native seeded roster checks')
        check('check FAIL population_' not in log and 'check FAIL low_tier_' not in log, 'no native roster failures')
        check('check PASS outpost_physical_guards_no_random_minibosses' in log, 'physical-only first-floor guards')
        check('check PASS outpost_has_no_goblins' in log, 'no first-floor goblins')
        layouts = json.loads((ROOT / 'mods/extraction-raid/data/rmf/raid.json').read_text())['layouts']
        check([layout['guard_target'] for layout in layouts] == [56, 56, 56], '56 regular enemies per floor')
        peers = sorted(folder for folder in profile.iterdir() if (folder / 'delve-generated-floor1.json').exists())
        check(len(peers) == 4, 'four-peer progression evidence')
        for floor, layout in enumerate(layouts, 1):
            target = layout['guard_target']
            generated = [json.loads((peer / f'delve-generated-floor{floor}.json').read_text()) for peer in peers]
            check(all(world['size'] == 90 for world in generated), f'floor {floor}: all peers use 90x90 canvas')
            encounters = generated[0]['encounters']
            check(all(world['encounters'] == encounters for world in generated), f'floor {floor}: peer rosters agree')
            check(len(encounters) == target, f'floor {floor}: regular count {target}')
            weights = {monster_ids[name]: weight for name, weight in BAGS[floor - 1].items()}
            counts = Counter(encounter['monster'] for encounter in encounters)
            check(set(counts) == set(weights), f'floor {floor}: exact intended species set')
            for species, weight in weights.items():
                minimum = target // 12 * weight
                maximum = minimum + (weight if target % 12 else 0)
                check(minimum <= counts[species] <= maximum, f'floor {floor}: {names[species]} quota')
            check(not any(counts[monster_ids[species]] for species in
                          ('INSECTOID', 'INCUBUS', 'DEMON', 'VAMPIRE', 'CRYSTALGOLEM')),
                  f'floor {floor}: no late-campaign enemies')
            distance = lambda a, b: abs(a['x']-b['x']) + abs(a['y']-b['y'])
            guardian = generated[0]['boss']
            check(all(distance(encounter, spawn) >= 12 for encounter in encounters
                      for spawn in generated[0]['spawns']), f'floor {floor}: 12-tile protected starts')
            check(all(distance(guardian, spawn) >= 12 for spawn in generated[0]['spawns']),
                  f'floor {floor}: guardian outside protected starts')
            check(all(distance(a, b) >= 3 for i, a in enumerate(encounters) for b in encounters[i+1:]),
                  f'floor {floor}: encounters separated by three tiles')
            check(all(distance(encounter, guardian) >= 4 for encounter in encounters),
                  f'floor {floor}: guardian has four-tile encounter clearance')
            for encounter in encounters:
                species = encounter['monster']
                kind = ('vermin' if species in (monster_ids['RAT'], monster_ids['BAT_SMALL']) else
                        'elite' if species in (monster_ids['TROLL'], monster_ids['CRYSTALGOLEM']) else 'patrol')
                check(encounter['kind'] == kind, f'floor {floor}: encounter classification')
            boss = monster_ids[('SKELETON', 'GOBLIN', 'TROLL')[floor - 1]]
            hp, level = ((60, 3), (90, 6), (140, 12))[floor - 1]
            check((guardian['monster'], guardian['hp'], guardian['level']) == (boss, hp, level),
                  f'floor {floor}: authored low-level guardian')
            check(f'guardian floor={floor} type={boss} hp={hp} level={level}' in log,
                  f'floor {floor}: native initialized guardian stats')
            expected = counts.copy()
            expected[boss] += 1
            census = re.findall(rf'roster floor={floor} type=(\d+) expected=(\d+) actual=(\d+)', log)
            check({int(species): int(count) for species, count, actual in census} == expected,
                  f'floor {floor}: native requested census matches generated roster plus guardian')
            check(all(count == actual for species, count, actual in census), f'floor {floor}: actual native census matches')
            check(f'population_checked floor={floor} ' in log, f'floor {floor}: initialized census completed')
        for label in ('population_physical_guards_on_every_floor', 'population_starter_appropriate_attack_ceiling',
                      'population_exactly_one_guardian', 'population_low_level_guardian_stats'):
            check(log.count('check PASS '+label) == 3, label+' checked on all three floors')
        report['status'] = 'PASS'
    except BaseException as error:
        report.update(status='FAIL', error=repr(error))
        raise
    finally:
        result = profile / 'result.json'
        if result.exists():
            report['full_scenario_status'] = json.loads(result.read_text())['status']
        (profile / 'population-validation.json').write_text(json.dumps(report, indent=2) + '\n')
        print(report['status'], len(report['checks']), 'population checks;', profile, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profiles', type=Path, nargs='+')
    for profile in parser.parse_args().profiles:
        validate(profile.resolve())


if __name__ == '__main__':
    main()
