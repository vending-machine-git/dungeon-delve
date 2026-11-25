"""Generate the native Delve catalogue and player reference from authored rules."""
from pathlib import Path
import json
from delve_passive_rules import PASSIVES
ROOT=Path(__file__).resolve().parents[2]
CLASSES=[]
def add(name,actives):
    assert len(PASSIVES[name])==6 and len(actives)==4
    abilities=[]
    for ability_name, authored_rules in actives:
        rules=dict(pair.split('=') for pair in authored_rules.split())
        for field in ('self','control'):
            if field in rules:
                rules[field]=','.join(effect+':'+format(float(seconds)*1.25*(1.25 if effect=='INVISIBLE' else 1), 'g')
                                     for effect,seconds in (pair.split(':') for pair in rules[field].split(',')))
        abilities.append(dict(name=ability_name,rules=rules))
    CLASSES.append(dict(name=name,perks=PASSIVES[name],abilities=abilities))

add("Barbarian",[
("Bloodrage","cd=32 self=POTION_STR:5,FAST:4"),("War Cry","cd=28 manaCost=4 range=64 arc=360 control=FEAR:2"),("Cleave","cd=18 range=40 arc=100 damage=17 kind=physical"),("Second Wind","cd=45 heal=14 cleanse=1")])
add("Warrior",[
("Shield Advance","cd=26 self=FAST:4,MAGICRESIST:5"),("Sweeping Strike","cd=20 range=44 arc=120 damage=15 kind=physical"),("Iron Resolve","cd=38 self=HP_REGEN:4 cleanse=1"),("Challenge","cd=30 range=72 arc=75 damage=8 kind=physical control=SLOW:3")])
add("Healer",[
("Emergency Mend","cd=32 manaCost=8 heal=20"),("Purifying Breath","cd=24 manaCost=3 cleanse=1 self=MAGICRESIST:4"),("Life Ward","cd=40 manaCost=8 self=HP_REGEN:5,MAGICRESIST:6"),("Merciful Rebuke","cd=22 manaCost=5 range=88 arc=25 damage=14 control=SLOW:2")])
add("Rogue",[
("Vanish","cd=34 manaCost=4 self=INVISIBLE:5"),("Dirty Cut","cd=18 range=32 arc=65 damage=12 kind=physical control=SLOW:4"),("Quickstep","cd=20 self=FAST:3 cleanse=1"),("Pocket Sand","cd=28 range=40 arc=85 control=CONFUSED:3")])
add("Wanderer",[
("Trailblaze","cd=25 self=FAST:5"),("Camp Remedy","cd=42 heal=12 mana=8"),("Hunter's Sweep","cd=20 range=50 arc=100 damage=13 kind=physical"),("Read the Wild","cd=35 manaCost=3 self=TELEPATH:7,MAGICRESIST:3")])
add("Cleric",[
("Lay on Hands","cd=38 manaCost=6 heal=16 cleanse=1"),("Hallowed Guard","cd=32 manaCost=6 self=MAGICRESIST:7,HP_REGEN:3"),("Judgment","cd=24 manaCost=7 range=80 arc=30 damage=19"),("Pilgrim's March","cd=28 manaCost=4 self=FAST:4,LEVITATING:3")])
add("Merchant",[
("Hostile Negotiation","cd=30 manaCost=4 range=64 arc=60 control=CONFUSED:3 damage=6"),("Caravan Sprint","cd=24 self=FAST:4"),("Insurance Policy","cd=48 heal=18 self=MAGICRESIST:3"),("Flash Buyout","cd=26 manaCost=6 self=INVISIBLE:3,TELEPATH:5")])
add("Wizard",[
("Arcane Lance","cd=18 manaCost=8 range=128 arc=12 damage=24"),("Frost Ring","cd=28 manaCost=10 range=52 arc=360 damage=8 control=SLOW:4"),("Mirror Ward","cd=40 manaCost=10 self=MAGICREFLECT:4"),("Ley Siphon","cd=45 mana=18")])
add("Arcanist",[
("Runic Bolt","cd=20 manaCost=6 range=112 arc=16 damage=20"),("Spellguard","cd=30 manaCost=5 self=MAGICRESIST:6,FAST:2"),("Charged Break","cd=28 manaCost=7 range=64 arc=55 damage=12 control=WEBBED:2"),("Battle Recharge","cd=40 mana=12 self=MAGICAMPLIFY:3")])
add("Joker",[
("Vanishing Act","cd=30 manaCost=5 self=INVISIBLE:3,FAST:3"),("Punchline","cd=24 manaCost=4 range=56 arc=100 damage=8 control=CONFUSED:3"),("Juggler's Flurry","cd=18 range=72 arc=70 damage=13 kind=physical"),("Encore","cd=48 heal=10 mana=10 cleanse=1")])
add("Sexton",[
("Grave Silence","cd=32 manaCost=7 range=64 arc=70 control=ASLEEP:3"),("Ashen Ward","cd=32 manaCost=6 self=MAGICRESIST:5,INVISIBLE:2"),("Gravestone Blow","cd=22 range=36 arc=70 damage=17 kind=physical control=SLOW:2"),("Wake the Dead","cd=44 mana=14 self=TELEPATH:6")])
add("Ninja",[
("Smoke Veil","cd=28 self=INVISIBLE:4"),("Shuriken Fan","cd=18 range=80 arc=75 damage=14 kind=physical"),("Nerve Strike","cd=30 range=30 arc=45 damage=8 kind=physical control=PARALYZED:1.5"),("Wind Step","cd=24 self=FAST:4,LEVITATING:2")])
add("Monk",[
("Palm Shock","cd=20 range=34 arc=75 damage=16 kind=physical control=SLOW:2"),("Centering Breath","cd=40 heal=12 mana=6 cleanse=1"),("Wind Walk","cd=30 manaCost=4 self=FAST:4,LEVITATING:4"),("Diamond Skin","cd=34 manaCost=5 self=MAGICRESIST:7")])
add("Conjurer",[
("Binding Pulse","cd=28 manaCost=8 range=64 arc=110 damage=9 control=WEBBED:2"),("Master's Aegis","cd=36 manaCost=7 self=MAGICRESIST:6,HP_REGEN:2"),("Soul Exchange","cd=42 mana=22"),("Spectral Survey","cd=30 manaCost=4 self=TELEPATH:8,INVISIBLE:2")])
add("Accursed",[
("Blood Price","cd=28 self=POTION_STR:4,FAST:3"),("Crimson Drain","cd=25 manaCost=6 range=64 arc=25 damage=16 leech=50"),("Night Shroud","cd=34 manaCost=5 self=INVISIBLE:4,MAGICRESIST:3"),("Dread Pulse","cd=34 range=52 arc=360 damage=5 control=FEAR:2")])
add("Mesmer",[
("Mind Fracture","cd=28 manaCost=6 range=96 arc=20 damage=10 control=CONFUSED:3"),("False Opening","cd=30 manaCost=6 self=INVISIBLE:3,FAST:4"),("Cold Snap","cd=22 manaCost=7 range=80 arc=50 damage=14 control=SLOW:3"),("Psychic Bastion","cd=38 manaCost=8 self=MAGICREFLECT:3,TELEPATH:5")])
add("Brewer",[
("Combat Tonic","cd=38 heal=10 self=POTION_STR:3"),("Caustic Splash","cd=24 manaCost=4 range=64 arc=90 damage=16"),("Antidote Batch","cd=28 cleanse=1 self=HP_REGEN:3"),("Rocket Brew","cd=30 self=FAST:5,MAGICRESIST:3")])
add("Machinist",[
("Overcharge","cd=30 manaCost=6 self=FAST:3,MAGICAMPLIFY:4"),("Scatter Volley","cd=20 range=96 arc=65 damage=16 kind=physical"),("Insulation Field","cd=36 manaCost=6 self=MAGICRESIST:7 cleanse=1"),("Arc Discharge","cd=30 manaCost=8 range=56 arc=360 damage=12 control=SLOW:2")])
add("Punisher",[
("Sentence","cd=26 range=40 arc=55 damage=24 kind=physical"),("Relentless Chase","cd=26 self=FAST:5 cleanse=1"),("Chain Snare","cd=30 manaCost=5 range=88 arc=18 damage=6 kind=physical control=WEBBED:2"),("Dread Verdict","cd=34 manaCost=6 range=60 arc=100 control=FEAR:2 self=POTION_STR:3")])
add("Shaman",[
("Spirit Surge","cd=30 manaCost=6 self=FAST:4,MP_REGEN:3"),("Entangling Roots","cd=30 manaCost=7 range=64 arc=120 control=WEBBED:2 damage=5"),("Ancestral Remedy","cd=40 manaCost=7 heal=14 cleanse=1"),("Primal Roar","cd=35 manaCost=5 range=48 arc=360 control=FEAR:2 self=MAGICRESIST:4")])
add("Hunter",[
("Piercing Shot","cd=22 range=160 arc=10 damage=23 kind=physical"),("Hunter's Mark","cd=30 manaCost=4 self=TELEPATH:9"),("Crippling Shot","cd=28 range=120 arc=14 damage=10 kind=physical control=SLOW:4"),("Trail Escape","cd=30 self=FAST:4,INVISIBLE:2")])
add("Bard",[
("Battle Ballad","cd=32 manaCost=6 self=POTION_STR:4,FAST:3"),("Lullaby","cd=34 manaCost=7 range=56 arc=110 control=ASLEEP:3"),("Restorative Verse","cd=42 manaCost=6 heal=10 self=HP_REGEN:3"),("Dissonant Chord","cd=24 manaCost=5 range=72 arc=90 damage=12 control=CONFUSED:2")])
add("Sapper",[
("Breach Charge","cd=26 range=56 arc=110 damage=22 kind=physical"),("Concussion Burst","cd=32 manaCost=5 range=44 arc=360 damage=7 kind=physical control=CONFUSED:3"),("Blast Shelter","cd=36 self=MAGICRESIST:6 cleanse=1"),("Fuse Sprint","cd=22 self=FAST:5")])
add("Scion",[
("Ancestral Blade","cd=24 manaCost=7 range=48 arc=80 damage=21"),("Bloodline Ward","cd=38 manaCost=7 self=MAGICREFLECT:3,MAGICRESIST:3"),("Noble Resolve","cd=40 heal=12 mana=8 cleanse=1"),("Royal Pursuit","cd=28 manaCost=5 self=FAST:5,POTION_STR:2")])
add("Hermit",[
("Quiet Restoration","cd=45 heal=14 mana=10"),("Stone Meditation","cd=34 manaCost=4 self=MAGICRESIST:8"),("Cave Echo","cd=28 manaCost=5 range=60 arc=360 damage=10 control=SLOW:3"),("Leave No Trace","cd=32 manaCost=5 self=INVISIBLE:4,LEVITATING:3")])
add("Paladin",[
("Radiant Smite","cd=24 manaCost=7 range=40 arc=70 damage=20"),("Dawn Aegis","cd=38 manaCost=8 self=MAGICRESIST:6,HP_REGEN:4"),("Oath Renewal","cd=42 manaCost=6 heal=16 cleanse=1"),("Crusader's Advance","cd=30 manaCost=5 self=FAST:4,POTION_STR:3")])

LABELS={"HP":"max HP","MP":"max MP","STR":"STR","DEX":"DEX","CON":"CON","INT":"INT","PER":"PER","CHR":"CHR"}
PROS=["LOCKPICKING","STEALTH","TRADING","APPRAISAL","THAUMATURGY","LEADERSHIP","MYSTICISM","SORCERY","RANGED","SWORD","MACE","AXE","POLEARM","SHIELD","UNARMED","ALCHEMY"]
EFFECTS={"FAST":"haste","POTION_STR":"strength","MAGICRESIST":"magic resistance","MAGICREFLECT":"spell reflection","INVISIBLE":"invisibility","LEVITATING":"levitation","TELEPATH":"telepathy","HP_REGEN":"health regeneration","MP_REGEN":"mana regeneration","MAGICAMPLIFY":"spell amplification","SLOW":"slow","FEAR":"fear","CONFUSED":"confusion","ASLEEP":"sleep","WEBBED":"web","PARALYZED":"paralysis"}
def quote(s):return json.dumps(s)
def main():
    assert len(CLASSES)==26
    names=set();rows=[];passive_rows=[];doc=["# Dungeon Delve class builds (0.4.0)","", "Choose two perks and two active abilities in the creator's Skills & Abilities tab. All ten options per class are available immediately. Empty slots are allowed. Selections are remembered separately for every class and locked for the entire match. Default slots are empty, preserving the native starter builds until you equip options.","", "Equipped abilities are dedicated native HUD prompts next to the Space/Defend prompt, with their icon and actual remappable keycap underneath (Q for ability 1, E for ability 2 by default). They are not inventory spells and never occupy hotbar slots. Old F7/F8 default bindings migrate once to Q/E; custom or intentionally unbound controls remain unchanged. Ability shortcuts take priority over follower actions sharing their input during Delve gameplay only. Icons dim and show remaining seconds while recovering; the left-side round cooldown indicators remain. Ability commands are also available as /delveability 1 and /delveability 2. All effects are self-only or hostile: FFA opponents never receive your healing. Cooldowns run only during active, unpaused gameplay and persist across floors. Each activation has a short shared recovery. No activation during staging, extraction, death, sleep, paralysis or other control locks.","", "XP from native combat and skill gains, plus cache exploration rewards, is multiplied by exactly three in Delve; normal campaigns are unchanged. Pets retain native XP sharing and receive the same threefold multiplier once.","", "Direct attack abilities use range, facing and line of sight. Physical damage respects armor; magic damage respects native magic resistance and reflect wards. Both respect native racial resistance bounded by Delve PvP rules. Damage gains up to +6 from level and up to +6 from the relevant STR/INT attribute. Native PvP control caps and shared recovery apply. All 156 passives now react to combat, spells, blocking, item use or exploration instead of modifying base stats. The HUD shows readiness, charges and recovery. Old stat-perk selections are cleared once; active spell selections are retained. Barriers cap at 10 points, haste at 10%, and next-cast discounts at 2 MP. Skills never reduce stats and abilities never consume HP. Low skill never causes cast failure, reduced spell efficiency or random additional MP consumption in Delve. Normal MP costs and cooldowns still apply; abilities cost no items or hunger. Proficiency perk bonuses cap at 99 and cannot grant capstone spells. Recommended builds select the first two perks and first two abilities; Clear removes all four choices.",""]
    doc[0] = '# Dungeon Delve class builds (0.4.2)'
    doc += ['## Shops and cooldown HUD (0.4.2)', '',
            'Shopkeepers welcome every player race and native shapeshift in Dungeon Delve without requiring a disguise. Actual aggression and wanted behavior still apply; ordinary campaign shopkeepers are unchanged. Timed recovery appears as round indicators on the left below the match card and trap warning: gold A1/A2 for active abilities and cyan P1/P2 for passive skills. Each ring drains with remaining cooldown and shows seconds plus the skill name. Only recovering equipped skills appear. Ready skills vanish, and native menus, staging, death and match results hide the indicators. Both host and guests use the host-authoritative cooldown state. No cooldown or balance values change in this update. All peers require runtime/content 0.4.2, raid protocol 9; API 1 and saved build selections remain unchanged.', '']
    doc += ['## November 20 invisibility duration update', '',
            'Every class ability that grants invisibility now keeps that effect 25% longer than the previous 0.4.2 build. Other effects on the same ability retain their durations. Descriptions and the native effect timers use the same generated rules; cooldowns and MP costs retain their existing values.', '']
    doc += ['## Class identity and duration update', '',
            'Release 0.4.1 replaces 18 overlapping passives with class-oriented counters, ranged/spell relays, summon follow-ups, execution and recovery mechanics. Timed passive effects, marks, charges, combo windows and active ability effects last 25% longer than 0.4.0. Cooldowns, MP costs, damage, healing totals and trigger thresholds are unchanged except where a passive is explicitly redesigned. Instant abilities have no duration to extend. Native PvP control caps and recovery remain enforced, so capped control may not last 25% longer against players. All peers require current runtime/content 0.4.2 (raid protocol 9; framework API 1). Saved slot selections remain valid.', '']
    for c in CLASSES:
        doc += ["## "+c['name'],""]
        for i,p in enumerate(c['perks']):
            assert p['name'] not in names;names.add(p['name'])
            assert all(p[field] >= 0 for field in ('value','duration','cooldown'))
            desc=p['description']
            rows.append('{'+','.join([quote(p['name']),quote(desc),'true','{}','{}','{}'])+'}')
            passive_rows.append('{Kind::'+p['kind']+',Reward::'+p['reward']+','+','.join(str(round(p[field]*50) if field in ('duration','cooldown') else p[field]) for field in ('value','duration','cooldown','filter'))+'}')
            doc += [f"- **Perk - {p['name']}:** {desc}"]
        for p in c['abilities']:
            assert p['name'] not in names;names.add(p['name']);r=p['rules'];assert int(r.get('hpCost',0)) == 0;parts=[r['cd']+"s cooldown"]
            for k,label in [('manaCost','MP cost'),('hpCost','HP cost'),('heal','HP restored'),('mana','MP restored')]:
                if k in r:parts.append(r[k]+" "+label)
            if 'damage' in r:parts.append(r['damage']+" base "+r.get('kind','magic')+" damage")
            if 'range' in r:parts.append(r['range']+" range / "+r['arc']+" degree arc")
            if 'control' in r:
                e,s=r['control'].split(':');parts.append(EFFECTS[e]+" for "+s+"s on hit")
            if 'self' in r:
                for pair in r['self'].split(','):
                    e,s=pair.split(':');parts.append(EFFECTS[e]+" for "+s+"s (self)")
            if 'cleanse' in r:parts.append("cures poison, bleeding, slow, web, confusion, fear, sleep and paralysis (self)")
            if 'leech' in r:parts.append(r['leech']+"% of damage dealt restored as HP")
            desc="; ".join(parts)+".";p['description']=desc
            pairs=[x.split(':') for x in r.get('self','').split(',') if x];pairs += [('',0)]*(2-len(pairs));assert len(pairs)==2
            effect=lambda e:'EFF_'+e if e else '-1'
            selfrows='{{'+','.join('{'+effect(e)+','+str(round(float(s)*50))+'}' for e,s in pairs)+'}}'
            ctrl,duration=r.get('control',':0').split(':')
            vals=[int(r.get(k,0)) for k in ['cd','manaCost','hpCost','heal','mana','damage','range','arc','cleanse','leech']]
            # Durations are expressed in seconds here and converted by runtime with TICKS_PER_SECOND.
            a='{'+','.join(map(str,vals))+','+str(r.get('kind')=='physical').lower()+','+effect(ctrl)+','+str(round(float(duration)*50))+','+selfrows+'}'
            rows.append('{'+','.join([quote(p['name']),quote(desc),'false','{}','{}',a])+'}')
            doc += [f"- **Ability ? {p['name']}:** {desc}"]
        doc += [""]
    doc += ['## Current validation (0.4.2)', '',
            'All 1,482 native build checks pass, including every playable race, native shapeshifts, real-aggression hostility, active/passive cooldown access and existing build/network contracts. At 1280x720 and 1920x1080, all 56 screenshot assertions pass for full/half radial progress, remaining seconds, ready-state disappearance, inventory hiding and return. Normal-campaign racial shopkeeper rules and inert cooldown access pass; all 22 launcher/package checks pass. See modding-evidence/shops-cooldowns-validation.json. This is controlled engine/rendering validation, not a claim of measured human balance or physical controller testing.', '']
    doc += ['## Previous validation (0.4.1)', '',
            'All 1,197 native build checks pass, including the 156 passive triggers, class-specific rejection paths, exact mark/haste expiry, extended Stabilize recovery with unchanged healing total, all 104 abilities and tooltip layouts, packet authentication and real Direct IP guest synchronization. Normal-campaign scope and all 22 launcher/package checks pass. Static generation checks verify 156 passive timings and 106 active status durations against the authored 1.25 multiplier, without scaling ability costs or cooldowns. See modding-evidence/passive-identity-validation.json for release evidence and broader smoke-test limitations.', '']
    assert len(rows)==260 and len(passive_rows)==156
    (ROOT/'src/delve_passives_catalogue.inc').write_text(',\n'.join(passive_rows)+'\n',encoding='utf-8')
    doc += ["## Verification", "", "Release 0.4.0 passed 1,076 native checks, including every behavioral passive trigger and bound, 104 zero-HP-cost abilities, 32 zero-skill casts, native shield-block Riposte, native cold-projectile Shatter, real potion use and packet authentication. A real Direct IP guest received host-authoritative passive charges and haste as well as an ability cooldown acknowledgment. Old passive selections migrated to empty slots on both peers while active spell choices were retained. Four-player mixed class/race progression passed all three floors. Normal campaigns retain native spellcasting and no passive effects. All 20 launcher/package checks passed.", "", "Evidence: modding-evidence/delve-passives-validation.json. Per-class values are an initial balance pass; controlled engine checks do not establish human win-rate balance. Physical controller input was not exercised; remapping uses the existing native Settings system.", ""]
    (ROOT/'src/delve_builds_catalogue.inc').write_text('// Generated by framework/tools/make_class_builds.py; edit the authored rules there.\n'+',\n'.join(rows)+'\n')
    (ROOT/'mods/extraction-raid/data/rmf/class_builds.json').write_text(json.dumps(dict(version=2,classes=CLASSES),indent=2)+'\n')
    (ROOT/'DELVE_CLASS_BUILDS.md').write_text('\n'.join(doc)+'\n')
    print('Generated 260 options across 26 classes')
if __name__=='__main__':main()
