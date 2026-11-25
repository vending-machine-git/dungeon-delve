"""Record the completed Delve balance pass against the preserved 0.2.3 audit."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import statistics

ROOT=Path(__file__).resolve().parents[2]


def main():
    ap=argparse.ArgumentParser();ap.add_argument('after',type=Path)
    ap.add_argument('--before',type=Path,default=ROOT/'framework/profiles/balance-audit-18e8d23ca7');args=ap.parse_args()
    before=[json.loads(s) for s in (args.before/'host/balance.jsonl').read_text().splitlines()]
    result=json.loads((args.after/'result.json').read_text());assert result['status']=='PASS'
    after=result['measurements']
    def index(rows,kind,key):return {r[key]:r for r in rows if r['kind']==kind}
    bm=index(before,'melee','name');am=index(after,'melee','name')
    bc=index(before,'class','class');ac=index(after,'class','class')
    bp=index(before,'projectile','name');apj=index(after,'projectile','name')
    br=index(before,'race','race');ar=index(after,'race','race')
    assert all(r['hp']>=30 for r in ac.values())
    assert all(ar[r]['incoming_multipliers']==br[r]['incoming_multipliers'] for r in br), 'PvE racial baselines changed'
    assert all(.799<x<1.201 for r in ar.values() for x in r['pvp_incoming_multipliers'])
    changes={}
    for key in ['barbarian_melee','warrior_melee','ninja_melee','ninja_vs_skeleton','warrior_vs_skeleton']:
        changes[key]={'before_mean':statistics.mean(bm[key]['damage']),'after_mean':statistics.mean(am[key]['damage']),
                      'before_range':[min(bm[key]['damage']),max(bm[key]['damage'])],
                      'after_range':[min(am[key]['damage']),max(am[key]['damage'])]}
    assert changes['ninja_melee']['after_mean']<changes['ninja_melee']['before_mean']
    cases={
        'control-and-scope-contracts':'balance-audit-81087b55df',
        'melee-revival-descent-extraction':'balance-audit-8472c3af13',
        'insufficient-mana-death-credit':'balance-audit-da31827504',
        'collapse-revival-descent-extraction':'balance-audit-e0278ad451',
        'normal-campaign-native-rules':'balance-audit-b4013aa54e',
    }
    verified={}
    for name,folder in cases.items():
        p=ROOT/'framework/profiles'/folder;d=json.loads((p/'result.json').read_text());assert d['status']=='PASS',name
        verified[name]=dict(status='PASS',profile=str(p))
    collapse_log=(ROOT/'framework/profiles'/cases['collapse-revival-descent-extraction']/'host/log.txt').read_text()
    pulses=re.findall(r'collapse slot=0 floor=1 damage=10',collapse_log);assert 2<=len(pulses)<=4
    log=(ROOT/'modding-evidence/balance-fix-raid-suite.log').read_text(encoding='utf-8-sig',errors='replace')
    suite={}
    for path in re.findall(r'^ARTIFACTS (.+)$',log,re.M):
        p=Path(path.strip());d=json.loads((p/'result.json').read_text());assert d['status']=='PASS',str(p)
        suite[d['scenario']]=dict(status='PASS',profile=str(p))
    assert set(suite)=={'progression','host-early','faults','combat','quality','quality-unidentified','restart'}
    report=dict(status='PASS',runtime='0.2.4',content='0.2.4',api=1,raid_protocol=3,
                before_profile=str(args.before),after_profile=str(args.after),changes=changes,
                wizard_hp=dict(before=bc[7]['hp'],after=ac[7]['hp']),
                sleep_ticks=dict(before=bp['sexton_sleep']['sleep_ticks'],after=apj['sexton_sleep']['sleep_ticks']),
                paralysis_ticks=dict(before=bp['ninja_paralysis_potion']['paralysis_ticks'],after=apj['ninja_paralysis_potion']['paralysis_ticks']),
                targeted=verified,raid_suite=suite,collapse_pulses_in_three_seconds=len(pulses),
                binaries={n:hashlib.sha256((ROOT/'framework/runtime'/n).read_bytes()).hexdigest() for n in ['barony-rmf.exe','barony-raid-smoke.exe']},
                limits=['Controlled melee samples and single projectile attempts are not human win rates or DPS.','Native spread can make a single ranged attempt miss.','Later-floor balance, human traversal/controller and prior audio/Steam/WAN release validation remain.'])
    (ROOT/'modding-evidence/delve-balance-fix-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    lines=['# Dungeon Delve balance fixes — 0.2.4','',
        'This pass implements the owner-approved audit recommendations in Delve. Native normal campaigns and PvE retain their original rules. Framework API 1 remains; raid protocol 3 requires updated runtimes on every peer.','',
        '- Native Skeleton revival resolves before elimination and player kill credit. A revived guest descended to floor two and extracted; a 74-MP guest died and awarded one kill. Collapse revival also retained participation.',
        '- Starters have at least 30 HP. Crystal swords/battleaxes become iron, speed boots become ordinary leather, and Wizard starts with a magic-resistance ring instead of full reflection.',
        '- Starter storm potions become worn acid; offensive acid/sickness are worn. Thrown stacks cap at two, beartraps/teleport scrolls at one, and special quivers at twelve. Class attributes/proficiencies/spells and later loot remain native.',
        '- Hostile-player sleep caps at 3 seconds and paralysis at 1.5 seconds. Their shared recovery lasts through the applied duration plus 2 seconds, including after curing early. Owned traps/minions follow the same PvP rules. Monster and self effects keep native duration.',
        '- Racial base damage multipliers between rival players clamp to 0.8–1.2 before equipment/effect adjustments. The full native racial table remains for PvE. Equipment resistance and Shadow Tag overrides were explicitly checked.',
        '- Collapse now pulses once per elapsed floor second. It previously repeated damage between rendered frames while the simulation tick remained unchanged.','',
        '## Controlled comparison','',
        'Sixteen fresh-kit, fully charged frontal melee samples per row against a stationary unarmored target. Numbers are HP per hit, excluding aim, attack timing, blocking and player movement.','',
        '| Attack | 0.2.3 mean | 0.2.4 mean |','|---|---:|---:|']
    for name,c in changes.items():lines.append(f"| {name.replace('_',' ')} | {c['before_mean']:.1f} | {c['after_mean']:.1f} |")
    lines+=['','## Verification','',
        '- All 26 class starters and 14 selectable race baselines checked; 224 charged melee samples and 21 single projectile/control attempts recorded.',
        '- Native spell Sleep and thrown paralysis potion confirmed their new player caps. Sixteen additional control/recovery/racial/PvE contracts passed, including a real player-owned beartrap.',
        '- Correctly selected Skeleton peer passed lethal melee revival, actual guest descent/extraction, insufficient-mana death/scoring and collapse revival checks.',
        '- Normal Direct IP campaign retained Wizard 20 HP/full reflection, Skeleton Sword multiplier 0.5 and 420-tick native paralysis.',
        '- Seven current raid smoke scenarios passed: four-player/all-floor progression, early host escape, banking faults, combat/loot recovery, both identification modes, and four consecutive matches through actual Restart controls.',
        '- Consolidated evidence: `modding-evidence/delve-balance-fix-validation.json`.','',
        'The earlier 0.2.3 audit is preserved in BALANCE_AUDIT.md. The first 0.2.4 contract fixture omitted the native monster ally index; corrected before the minion check passed. The collapse fixture exposed and retained the real repeated-pulse failure. The long damage audit needed its measurement health buffer on both peers, and its synthetic host-only race-change revival was replaced by correctly matched race tests. Failed profiles/logs remain preserved.','',
        'These results establish the intended behavior and controlled damage changes. They do not certify complete human class/race balance. No Workshop publication was performed.']
    (ROOT/'BALANCE_CHANGES.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(ROOT/'BALANCE_CHANGES.md')


if __name__=='__main__':main()
