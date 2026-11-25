"""Build an assessment from recorded engine measurements; never alter game balance."""
from pathlib import Path
import argparse
import hashlib
import json
import statistics

ROOT=Path(__file__).resolve().parents[2]
CLASSES=['Barbarian','Warrior','Healer','Rogue','Wanderer','Cleric','Merchant','Wizard','Arcanist','Joker','Sexton','Ninja','Monk','Conjurer','Accursed','Mesmer','Brewer','Machinist','Punisher','Shaman','Hunter','Bard','Sapper','Scion','Hermit','Paladin']
RACES={0:'Human',1:'Skeleton',2:'Vampire',3:'Succubus',4:'Goatman',5:'Automaton',6:'Incubus',7:'Goblin',8:'Insectoid',13:'Gnome',14:'Gremlin',15:'Dryad',16:'Myconid',17:'Salamander'}


def main():
    ap=argparse.ArgumentParser();ap.add_argument('profile',type=Path);ap.add_argument('skeleton_profile',type=Path);args=ap.parse_args()
    result=json.loads((args.profile/'result.json').read_text())
    assert (args.profile/'host/balance-complete').exists()
    skeleton=json.loads((args.skeleton_profile/'result.json').read_text());assert skeleton['status']=='PASS'
    rows=[json.loads(line) for line in (args.profile/'host/balance.jsonl').read_text().splitlines()]
    classes=[r for r in rows if r['kind']=='class'];races=[r for r in rows if r['kind']=='race']
    assert len(classes)==26 and len(races)==14
    assert sum(r['kind']=='melee' and len(r['damage'])==16 for r in rows)==14
    assert sum(r['kind']=='projectile' for r in rows)==21
    melee={r['name']:{**r,'mean':statistics.mean(r['damage']),'min':min(r['damage']),'max':max(r['damage'])} for r in rows if r['kind']=='melee'}
    projectiles={r['name']:r for r in rows if r['kind']=='projectile'}
    data=dict(status='ASSESSMENT_COMPLETE_WITH_LIMITS',profile=str(args.profile),recorded_run_status=result['status'],recorded_run_error=result.get('error'),classes=classes,races=races,melee=melee,projectiles=projectiles,
              matched_skeleton_probe=skeleton,production_balance_changed=False,
              production_sha256=hashlib.sha256((ROOT/'framework/runtime/barony-rmf.exe').read_bytes()).hexdigest(),
              method='Fresh native class initialization followed by the real Delve starter nerf. Human attackers; stationary unarmored targets, 1000 HP to survive measurement. Sixteen fresh-kit full-charge guaranteed-contact melee samples per case. Single projectile attempts with native spread; three-second observation, aim/cast windup/recovery/blocking excluded. No win-rate or DPS claims. Race multipliers use the native engine with innate abilities enabled.',
              limits=['Single projectile probes are examples, not averages. A zero-damage arrow attempt is inconclusive and may be a miss.','Potion storms assume the target remains in the effect.','All 26 class kits and 14 race baselines were inspected, not every class/race combination or progression stage.','No human movement, aim, shield, latency, or match-win-rate balancing was performed.'])
    (ROOT/'modding-evidence/delve-balance-audit.json').write_text(json.dumps(data,indent=2)+'\n')
    lines=['# Dungeon Delve PvP balance assessment — runtime/content 0.2.3','',
        'The current starters have material PvP power gaps. Race also changes the weapon matchup substantially. This is an assessment; production balance and packages are unchanged.','',
        '## Findings','',
        '- **Ninja is a clear opening melee outlier.** Its crystal sword, 60 Sword proficiency, speed equipment, five daggers, three beartraps, paralysis and invisibility potions, and three teleport scrolls survive the generic starter nerf. Decrepit quality does not equalize weapon tier or remove special effects.',
        '- **Wizard and Arcanist have dangerous early burst.** Starter books/staves still produce substantial damage. Wizard also keeps an excellent magic-reflection amulet: in the isolated fire-staff reflection probe, the defender took zero damage while the attacker took '+str(int(projectiles['fire_staff_vs_reflection']['caster_damage']))+' over three seconds. Reflection can degrade its item; this is a strong starting counter, not permanent immunity.',
        '- **Brewer has a large consumable spike despite weak axe damage.** Its acid and firestorm potions remain excellent. The stationary-target firestorm probe caused '+str(int(projectiles['brewer_firestorm']['target_damage']))+' damage over three seconds. Movement and leaving the effect were excluded.',
        '- **Player crowd control is long.** Sexton Sleep reached '+str(projectiles['sexton_sleep']['sleep_ticks']/50)+' seconds on the neutral target; damage can wake a sleeping player. Ninja Paralysis reached '+str(round(projectiles['ninja_paralysis_potion']['paralysis_ticks']/50,2))+' seconds in this sample. These durations need PvP-specific review before flat damage tuning.',
        '- **PvE utility does not translate uniformly into PvP.** Mesmer Charm staff did not charm/damage the player in the probe, although its cold staff did damage and slow. Merchant loses the normal starting gold advantage, and default instant identification reduces the value of high Appraisal. These are opening combat disadvantages, not proof those classes cannot extract successfully.',
        '- **Race is mechanically important.** Skeleton and Vampire have 0.5 ordinary ranged damage multipliers; Skeleton also has 0.5 Sword resistance but 1.4 Mace and 1.3 Polearm vulnerability. Special ammunition/effects can override the base relationship. Goatman takes 1.4 base magic damage; drunkenness adds at least four STR and multiplies incoming damage by 0.8. Myconid has 0.7 base magic damage but 1.3 Sword damage. Teleport/levitation/dash and other innate abilities add further matchup differences.',
        '- **Confirmed Skeleton lifecycle defect.** With a real Skeleton guest selected through the lobby on both peers, a lethal native melee hit at 75 MP consumed the mana and restored 40 HP, but Delve had already recorded the player as Slain. That prevents normal escape/descent participation. It needs a lifecycle fix before Skeleton balance can be judged. No production fix was applied during this assessment.','',
        '## Controlled charged melee samples','',
        'Stationary, unarmored targets; sixteen freshly initialized kits per row, maximum charge, frontal unless named rear. Guaranteed contact excludes accuracy and timing. These numbers are HP per hit, not DPS or win rates. Brewer uses its carried iron axe; Punisher uses its carried crystal battleaxe.','',
        '| Attack | Minimum | Mean | Maximum |','|---|---:|---:|---:|']
    for name,r in melee.items():lines.append(f"| {name.replace('_',' ')} | {r['min']} | {r['mean']:.1f} | {r['max']} |")
    lines+=['','## Priorities for a balance pass','',
        '1. Fix Skeleton revival/elimination ordering and test a real lethal PvP hit, collapse, and floor transition.',
        '2. Normalize starter weapon tiers and exceptional item budgets while preserving class stats/proficiencies. Review Ninja and Punisher crystal equipment, Wizard reflection, Hunter special ammunition, traps, and excellent offensive potions.',
        '3. Give hostile player paralysis/sleep a shorter duration and reapplication protection; preserve the existing PvE effects. Check that fragile 20-HP starters have practical counterplay to burst.',
        '4. Playtest physical/ranged/caster class-race pairs on all three floors before changing racial tables. Consider narrower player-versus-player resistance extremes if counters dominate outcomes. Preserve normal campaigns.',
        '5. Measure actual match outcomes with equally skilled players, gear acquisition and mobility. The controlled samples identify tuning targets; they do not establish a complete class tier list.','',
        '## All human class starter baselines','',
        'Captured after one simulation second using native initialization and the production Delve kit nerf. Innate class activations and reading starter books are not automatically performed. ATK is the native stat, not observed hit damage.','',
        '| Class | HP | MP | AC | ATK | Equipped weapon |','|---|---:|---:|---:|---:|---|']
    for r in classes:
        weapon=next((p['name'] for p in r['equipment'] if p['slot']=='weapon'),'unarmed')
        lines.append(f"| {CLASSES[int(r['class'])]} | {int(r['hp'])} | {int(r['mp'])} | {int(r['ac'])} | {int(r['attack'])} | {weapon} |")
    lines+=['','## Racial incoming damage multipliers','',
        'Barbarian starter, innate abilities enabled, no active drunkenness or other skill use. Below 1 reduces damage; above 1 increases it. Equipment, special ammo, armor, blocking and spell-specific effects can modify the final hit. Rat/Troll/Spider/Imp IDs are forms rather than selectable races.','',
        '| Race | Sword | Mace | Axe | Polearm | Ranged | Magic | Unarmed |','|---|---:|---:|---:|---:|---:|---:|---:|']
    for r in races:lines.append('| '+RACES[int(r['race'])]+' | '+' | '.join(f'{v:.2f}' for v in r['incoming_multipliers'])+' |')
    lines+=['','## Evidence and limits','',data['method'],'',*['- '+x for x in data['limits']],'',
        '- Final completed measurement profile: `'+str(args.profile)+'`. The original runner result is '+result['status']+' because of an overstrict single-arrow hit assertion: '+str(result.get('error','none'))+'. The original result is preserved; report validation checks the completed raw dataset. Native one-shot ranged misses are excluded from damage conclusions.',
        '- Matched Skeleton death probe: `'+str(args.skeleton_profile)+'`.',
        '- Raw JSON: `modding-evidence/delve-balance-audit.json`. Repeatable runner: `tests/balance/run_balance_audit.py`.',
        '- Pilot `balance-audit-ea5be91812` toggled off already-equipped weapons in some projectile cases; those zeros were not treated as class damage results. Corrected `balance-audit-a7ad1c96e4` completed measurements but failed an overstrict assertion requiring a single Rogue bow attempt to hit. Zero-damage single attempts are now reported as inconclusive, rather than silently labeled weak damage.','',
        'No gameplay nerfs, Workshop package changes or publication were performed.']
    (ROOT/'BALANCE_AUDIT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    series=[]
    for label,human,skeleton_name in [('Ninja · crystal sword','ninja_melee','ninja_vs_skeleton'),('Warrior · spear','warrior_melee','warrior_vs_skeleton'),('Monk · unarmed','monk_melee','monk_vs_skeleton')]:
        series.append(dict(label=label,human={**melee[human],'mean':round(melee[human]['mean'],1)},skeleton={**melee[skeleton_name],'mean':round(melee[skeleton_name]['mean'],1)}))
    html='''<!doctype html><html><head><meta charset="utf-8"><style>
body{margin:0;font-family:var(--font-sans);color:var(--foreground)}p{margin:0 0 12px}small{color:var(--muted-foreground);line-height:1.5;display:block}button{color:var(--foreground);background:var(--secondary);border:1px solid var(--border);border-radius:var(--radius);padding:6px 9px;font:inherit;font-size:12px;cursor:pointer}.legend{display:flex;gap:15px;align-items:center;font-size:12px;margin-bottom:18px}.dot{display:inline-block;width:10px;height:10px;border-radius:2px;margin-right:5px}.group{display:grid;grid-template-columns:145px 1fr;gap:8px;margin-bottom:18px}.label{font-size:13px;align-self:center}.plot{position:relative}.row{height:22px;position:relative;margin:4px 0}.bar{height:16px;position:absolute;top:3px;border-radius:2px}.value{position:absolute;font-size:12px;line-height:22px;margin-left:6px;font-variant-numeric:tabular-nums}.range{display:none;position:absolute;top:10px;height:2px;background:var(--foreground)}.ranges .range{display:block}.axis{display:flex;justify-content:space-between;margin-left:153px;margin-right:35px;font-size:11px;color:var(--muted-foreground);margin-bottom:14px}.groups{padding-right:35px}.foot{margin-top:8px;font-size:12px}@media(max-width:450px){.group{grid-template-columns:105px 1fr}.axis{margin-left:113px}.label{font-size:12px}.legend{gap:8px;flex-wrap:wrap}}
</style></head><body><p><strong>Charged melee damage: race changes the matchup</strong></p><div class="legend"><span><i class="dot" style="background:var(--chart-1)"></i>Human target</span><span><i class="dot" style="background:var(--chart-2)"></i>Skeleton target</span><button id="range" aria-pressed="false">Show ranges</button></div><div class="axis"><span>0 HP</span><span>10</span><span>20</span><span>30</span><span>40</span></div><div class="groups" id="groups"></div><small class="foot">Mean HP lost per fully charged frontal hit, 16 fresh-kit samples per case. Unarmored stationary targets. Attack speed, blocking and player aim are excluded.</small><script>
const data=DATA;const root=document.getElementById('groups');for(const s of data){const g=document.createElement('div');g.className='group';const l=document.createElement('span');l.className='label';l.textContent=s.label;g.append(l);const p=document.createElement('div');p.className='plot';for(const [key,color] of [['human','--chart-1'],['skeleton','--chart-2']]){const d=s[key],r=document.createElement('div');r.className='row';r.title=key+': mean '+d.mean.toFixed(1)+', range '+d.min+'–'+d.max+' HP';const b=document.createElement('span');b.className='bar';b.style.background='var('+color+')';b.style.width=(d.mean/40*100)+'%';const v=document.createElement('span');v.className='value';v.style.left=(d.mean/40*100)+'%';v.textContent=d.mean.toFixed(1);const range=document.createElement('span');range.className='range';range.style.left=(d.min/40*100)+'%';range.style.width=((d.max-d.min)/40*100)+'%';r.append(b,range,v);p.append(r)}g.append(p);root.append(g)}document.getElementById('range').onclick=function(){const show=root.classList.toggle('ranges');this.setAttribute('aria-pressed',String(show));this.textContent=show?'Hide ranges':'Show ranges'};
</script></body></html>'''.replace('DATA',json.dumps(series))
    (ROOT/'modding-evidence/delve-balance-chart.html').write_text(html,encoding='utf-8')
    print(ROOT/'BALANCE_AUDIT.md')


if __name__=='__main__':main()
