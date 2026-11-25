# Dungeon Delve - Battle Royale 0.20.5

0.20.5 changes the monsters: dungeon monsters no longer fight each other.
Floor 2 (Jungle) swaps most skeletons for melee goblins, so fewer enemies shoot.
Floor 3 (Crystal Caves) drops skeletons for spiders, scorpions and toned-down
insectoids (level 10, no thrown daggers, rare bows), making it a bit harder.
0.20.4 added Steam invites: open lobby seats show INVITE cards, friends join
through Steam's relay without port forwarding, and Direct IP still works.
0.20.3 made gates behave like vanilla: they start closed and their lever sits a
few tiles away against a wall. Gates that divide the floor have a lever on each
side, and either lever toggles them. Empty optional rooms keep their lock and get
a cache chest (or gear/treasure) from the existing floor loot budget. Only
structurally broken doorways are removed. Chest spacing, doorway clearance and
the three furnished kiosks remain.
All peers need protocol 35.

Private 2-4 player PvPvE through the Mines, Jungle and Crystal Caves. Loot, build
your character, outlast rivals and claim a final-floor exit. Death is not the end:
return as monsters/rats while adventurer progression survives for the next floor.
Experimental Windows x64 release. Unofficial; not a Turning Wheel release.

## Combined GitHub release (current)

Download **Dungeon-Delve-0.20.5-windows-x64.zip** from the project's GitHub
Releases page. It already includes Dungeon Delve and vending_machine framework;
no Workshop subscription or separate framework download is required. Extract
into the owned Barony installation, then run
**Dungeon-Delve-0.20.5/Play-Dungeon-Delve.cmd**. Extracting elsewhere also supports
Steam-library discovery. Retail files/saves remain untouched. Follow the README
at the combined ZIP's root for checksums, compatibility, updates and removal.
Use matching releases on every peer; private Direct IP only.

## Historical Workshop installation

The separate-package route below is retained for reference, not required for
the combined GitHub download. Gameplay/networking restrictions still apply.

1. Own/install/run the Steam Windows edition of **Barony**, tested build **5.0.2**.
2. Subscribe to **vending_machine framework 0.20.5** and **Dungeon Delve 0.20.5**.
3. Separately download **vending_machine-framework-0.20.5-windows.zip** from the
   framework Workshop page's release link. Extract outside Barony's game folder.
4. Follow its README and run **Play-Dungeon-Delve.cmd**. No Python/compiler needed.
5. Host/join a fresh private **Direct IP** lobby; everyone joins before starting.
   All peers need runtime/content **0.20.5**, protocol **35**, framework API **1**.

**Subscribing alone cannot enable this native mode.** Launching retail Barony
from Steam does not enable it. The separately downloaded launcher checks the
runtime, installed game and local dependency hashes; matching version labels
alone do not prove compatibility. The original executable is never replaced.
Purchased assets/DLLs are not included; required DLLs are copied from your game
into isolated sessions. Keep Steam running for downloads and DLC checks.

Do not mix raid/retail clients, other mods/frameworks, campaign imports or Join
In Progress. The host stays connected through results. Invite Steam friends from
the tavern's open-seat INVITE cards (they must already have the same release
open), or use Direct IP and check the host's port and firewall/router setup.
WAN and human balance testing remain limited; this is not a hardened public server.
DLC is optional for base-game characters; each player's classes/races require
their own entitlements. A host does not share DLC unlocks with guests.

Choose **Skills & Abilities** during character creation: each of 26 classes has
six passive and four ability options; equip two of each. **Q / E** activate
abilities by default (rebind Delve Ability 1/2 under Settings).

## Current match rules

- Three native-generated **90x90 floors**, each with a **six-minute clock**,
  56 regular enemies and a guardian. Jungle/Crystal Caves replace five/nine weaker
  enemies with trolls. Combat/proficiency XP is **1.75x**.
- Each floor has **one single-use hatch** and **three single-use portal sites**.
  Floors 1-2: hatch available immediately; one portal opens at **1:00 remaining**
  (5:00 elapsed), two more at timeout (6:00 elapsed). These lead to the next floor.
- Final floor: hatch and first portal open at **1:00 remaining**, two more portals
  at timeout. The first accepted final-floor exit claim wins globally. Spent
  exits never admit a second player. A final-floor wipe has no winner.
- Descenders keep loot/gold/progression and wait safely in staging. The floor
  advances when all adventurers descend or die; returned monsters do not block it.
- Exactly **three shopkeepers per floor**, spread across the map with at least
  **22 tiles** between them. Each trades from a tiny roofed **5x5 kiosk**.
  Each shop has **16 randomized stock slots** from that floor's loot pools and
  uses the **same per-floor rarity odds** listed below. Equipment, potions, magic
  and gems are identified; native prices apply. Opening a shop never rerolls stock.
  Shopkeepers and mobs ignore one another, including retaliation and ally alerts.
  Player aggression still uses native shopkeeper rules.
- Death scatters physical loot, retaining rarity/rolls. **Level, partial XP,
  attributes, max HP/MP, proficiencies and level-up state survive death within
  the match**. Next-floor adventurer returns receive fresh class gear, restored
  preserved HP/MP and no previous loot/gold. Spell resets remain. New matches
  start fresh; there are no cross-match loot banks or campaign imports.
- One loot-unlocked monster life from **17 forms** per floor, then unlimited
  **5 HP rats**. Initial monster and first rat are immediate; subsequent rat
  returns have a **45-second cooldown after death**. Rat attacks use normal
  native damage/armor calculations with no forced damage cap.
- Rats can pick up cheese, not other loot. **Eat 10 pieces in one rat life to
  become Algernon** (native 60 HP, level 10). Death resets the count. Returned
  forms cannot use exits/win/carry loot forward; no extra followers or loot perks.
- Perimeter boulder storms begin at timeout on floors 1-2 and **1:00 remaining
  on the final floor**. Escalating waves break walls/props. Exits, boundaries
  and staging are protected; do not linger after timeout.
- Authored enemies/guardians drop randomized **floor-scaled rarity** loot.
  Spellbooks are less common. Spell learning has **no minimum skill/INT**;
  physical/racial reading restrictions, curses and mana costs remain.

## Presentation

A charcoal/brass monster picker shows HP/unlocks and rat cooldowns. Items have
recessed rarity wells/rims and colored world-drop smoke. Delve suppresses the
blue blessed-item background so rarity stays clear; blessings still function
and appear in tooltips. Curses/unidentified cues remain. A furnished Last Hearth
lobby and three owner-recorded floor introductions include timed subtitles.
Normal campaigns retain native mechanics and presentation.

## Settings and support

Shipped `data/rmf/raid.json` has `floor_seconds: 360` (supported range 300-480)
and `instant_identification: true`. For custom clocks, the first portal/final
hatch and final-floor storm activate one minute before timeout; the other two
portals open at timeout. Other metadata mainly describes native rules, not scripts.
Keep everyone on matching unmodified packages when reporting issues.

The portable launcher creates a fresh session for every launch under
`%LOCALAPPDATA%\vending_machine_framework\0.20.5\sessions\`.
Retail saves/config are not copied or edited. Match results/logs remain there;
progress is not a resumable campaign. Read the runtime README for manual paths,
checksums, updates and uninstall. Report versions/reproduction/session logs with
private IPs/usernames removed. Close games before deleting isolated sessions.
Unsubscribe and remove the separate runtime to uninstall; retail stays untouched.

Windows x64 Steam 5.0.2 only. Native FMOD gameplay effects/music use
the owner's verified game DLL. Supplied lobby music/narration use SDL.
Steam invites/relay use the installed Steam client; EOS stays disabled. No public-server hardening or automatic runtime updater.

## Workshop package

This folder is the complete upload root. No source, tools, runtime, DLLs or saves
belong here. The owner must configure **vending_machine framework** as a Workshop
Required Item and link its separately hosted runtime before public release.
Package ID `extraction-raid` stays stable. Nothing publishes automatically.

## Equipment variety

Floor-scaled pools now cover weapons, chest armor, hats/helmets, capes, rings,
necklaces, boots, gloves, shields and masks. Armory/treasury/relic caches and loose
gear use these pools; every cache also adds one accessory-slot roll. These are
native game items with existing models, not redistributed purchased assets.

## Loot rarity

Floor loot and cache equipment can roll Grey (standard), White, Blue (Uncommon),
Purple (Rare), or gold/orange Legendary. Rolled equipment quality matches rarity:
Grey = decrepit, White = worn, Blue = serviceable, Purple/Legendary = excellent.
This covers weapons, thrown weapons, armor, accessories, staves and spellbooks.
Blessings are independent. Later wear/repairs do not change equipment rarity or
reroll affixes. Starter kits and unrolled equipment keep their native condition;
consumable condition is unchanged. Gems continue to derive rarity from quality.

Gems: destroyed/cracked = Grey, rough = White, flawed = Blue (Uncommon),
flawless = Purple (Rare). The exceptionally scarce Legendary roll produces a
Legendary flawless gem. Floor/cache gem rolls set both quality and rarity, so
flawless gems have the Rare/Legendary odds below, not ordinary-loot odds.
Existing and naturally generated gems display the rarity of their actual quality.

| Tier | Random equipment rolls | Gem throwing damage | Value multiplier |
| --- | ---: | ---: | ---: |
| Grey | 0 | 0 | 1x |
| White | 0 | 1 | 1.1x |
| Blue | 1 | 3 | 1.5x |
| Purple | 2 | 6 | 2.5x |
| Legendary | 3 + special effect | 12 | 6x |

Equipment no longer gains automatic rarity damage, spell damage or AC. Condition,
blessings and native equipment properties still matter. Each affix independently
rolls its value within the same range at every rarity; higher rarity gives more
distinct rolls, not guaranteed better individual values. A Blue item can therefore
be better for your build than a Purple item. No duplicate affixes on one item.

Blessed equipment (any positive blessing) doubles every numeric affix roll:
for example, +14 maximum HP becomes +28. Higher blessing levels still give one
2x multiplier, not repeated doubling. Tooltips show the doubled values. Removing
the blessing restores the original rolls without rerolling their seed. Neutral
and cursed gear keep the original affix values. Existing total equipment caps
still apply after adding the doubled rolls. Legendary Siphon/Aegis, native base
stats, gem throwing bonuses and consumable potency are not affix rolls and are
not doubled by this rule. Blessing max-resource gear does not refill HP/MP.

Slot-specific pools contain 28 affixes:

- Weapons: attack charge/ranged speed, armor penetration, rear-hit damage, undead
  or demon damage, HP/MP on kill, Strength, Dexterity and Perception.
- Staves and caster robes/doublets: casting speed, spell mana
  efficiency, healing power, beneficial status duration, maximum MP, MP regen,
  Intelligence, Charisma, undead/demon damage and HP/MP on kill.
- Chest armor: maximum HP/MP, projectile/magic resistance, shorter harmful
  statuses, HP regen, Constitution and Strength.
- Shields: stronger physical blocking, maximum HP, projectile/magic resistance,
  shorter harmful statuses and Constitution.
- Hats/helmets/masks: casting/healing, maximum MP, magic resistance, status
  duration, Intelligence, Perception and Charisma.
- Gloves: attack/casting speed, lockpicking/disarming proficiency, Dexterity,
  Strength and healing. Boots: movement speed, stealth, status resistance,
  Dexterity, lockpicking/disarming and maximum HP.
- Cloaks: movement, stealth, projectile/magic resistance, HP/MP regen, rear-hit
  damage and Perception. Rings/amulets can roll any of the 28 affixes.

All bonuses require equipped, unbroken gear on a living adventurer, not a
same-floor monster/rat return. Held staves contribute their equipment rolls; inventory-only gear does not.
Spellbooks use their own spell-specific rolls described below. Lockpicking/disarming and stealth
increase native skill checks, not an invented interaction timer.

| Affix | Per-item range | Total equipment cap |
| --- | ---: | ---: |
| Attack charge/ranged speed | 3-8% | 20% |
| Casting speed | 4-10% | 25% |
| Movement speed | 1-4% | 10% |
| Armor penetration | 5-16% | 35% |
| Rear-hit damage | 8-20% | 25% |
| Undead / demon damage (each) | 10-23% | 25% |
| HP / MP on hostile kill | 1-4 | 6 HP / 5 MP |
| Maximum HP / MP | 5-16 / 5-18 | 40 each |
| Projectile / magic reduction (each) | 4-10% | 25% |
| Shorter harmful status duration | 7-16% | 35% |
| Physical reduction while blocking | 5-13% | 25% |
| Potion/spell/ability healing | 7-16% | 30% |
| Beneficial status duration | 7-16% | 30% |
| Spell mana efficiency | 5-13% | 25% |
| HP / MP regen per five gameplay seconds | 1-3 | 3 HP / 4 MP |
| STR / DEX / CON (each) | 2-3 | 6 |
| INT | 3-4 | 8 |
| PER | 3-5 | 10 |
| CHR | 4-6 | 12 |
| Stealth / lockpicking proficiency (each) | 5-16 | 25 |

Version 0.12.0 strengthens the less reliable attribute rolls: INT is 3-4 (cap 8),
PER 3-5 (cap 10), CHR 4-6 (cap 12). STR/DEX/CON remain 2-3 (cap 6).
Existing immutable seeds evaluate against the updated ranges; gear is not
rerolled and blessing still doubles once.

Spellbooks have a dedicated pool. Blue grants one distinct roll, Purple two,
and Legendary all three plus Siphon. Grey/White grant no random rolls.

| Book roll | Unblessed range | Cap |
| --- | ---: | ---: |
| Spell power | 7-16% | 30% |
| Casting speed | 4-10% | 25% including equipment |
| Spell mana efficiency | 5-13% | 25% including equipment |

Any positive blessing doubles book rolls once. Spell power contributes to native
spell damage/healing scaling and lengthens beneficial effects; hostile control
limits remain unchanged. Casting and efficiency bonuses share their existing
caps with gear. Neither applies to ability costs or sustained-spell upkeep.

Reading a book memorizes its exact rarity, seed and blessing at that moment.
Reading another book for the same spell replaces **all** previous book rolls,
even if the new book has lower rarity, fewer rolls or no rolls. Books retain
native read restrictions and condition loss. Selection and hotbar entries stay
in place; duplicate spell entries are not created. Changing or dropping the
physical book afterward does not change the memorized spell. Spells already
cast retain their old snapshots. Tooltips show both book and memorized rolls.

Offhand book casting uses that held book's rolls, independently of the memorized
version. Books no longer grant passive equipment rolls to unrelated actions,
and a held copy cannot double its rolls with the memorized version. Legendary
book Siphon shares the existing one-second cooldown with other Siphon sources.
Roll metadata survives native spell copies and save serialization; new class
starters begin with their normal unrolled spells. Normal campaigns are unchanged.

Version 0.9.1 / protocol 19 increases both endpoints of non-attribute ranges by
30% and attribute ranges by 50%, rounded to the nearest whole number (halves
round up). These are unblessed ranges; positive blessings still double the
rolled value once. Equipment caps and rarity roll counts are unchanged. Saved
roll seeds use these updated ranges on evaluation; no additional roll is added.

Attack speed speeds charged melee preparation and ranged draw/reload/throw
timing; it does not speed every frame of a melee animation. Casting speed
shortens native spell preparation. Spell efficiency rounds costs up, never
turning a positive cost into a free spell, and does not discount gold, sustained
spell upkeep or class-ability costs. Armor penetration reduces positive armor
in native melee/arrow/thrown weapon calculations, not magic resistance.
Rear-hit damage applies from the rear 120-degree arc on physical hits. Slayer
rolls affect the target's current undead/demon form, including those player forms.
Damage reductions multiply with native defenses and Aegis; they are not immunity.

Maximum-resource rolls do not heal/refill when equipped; removing them clamps
current resources to the lowered maximum. Regeneration uses gameplay time, stops
under pause/staging/death and cannot be multiplied by repeatedly equipping gear.
Kill rewards apply once per hostile victim, not on hits or friendly kills.
Healing power excludes passive regen, kill healing and Siphon. Status-duration
rolls affect finite supported effects applied through the native effect API:
poison, bleed, slow, webs, confusion, fear, sleep, paralysis, stun, blindness,
root and disorientation; beneficial haste, invisibility, levitation, magic
resistance/reflection, HP/MP regen, strength and combat-stat buffs. Permanent
effects, hunger timers, storm timing and class cooldowns are unchanged.

Legendary weapons and held spell foci **Siphon**: a hostile hit restores 4 HP and
2 MP, at most once per gameplay second per player. Legendary armor/accessories
grant **Aegis**, reducing hostile damage by 15% after armor; multiple Aegis pieces
do not stack. A hit still inflicts at least 1 damage before other barriers.
Siphon shares one cooldown across held weapons/foci. Broken gear supplies neither
affixes nor Legendary effects. All equipment affixes stack only to their caps.

| Floor | Grey | White | Blue | Purple | Legendary |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 65% | 24% | 9% | 1.9% | 0.1% |
| 2 | 48% | 29% | 18% | 4.7% | 0.3% |
| 3 | 32% | 30% | 27% | 10% | 1% |

These are per eligible authored loose-loot/cache stack, not guaranteed quotas.
Weapons, armor, rings, amulets, ordinary staves, spellbooks, thrown weapons and
gems, filled potions and bear traps are eligible. Gems gain throwing damage and
quality-dependent sale value. Potions gain +0/10/35/75/150% base healing, mana restoration,
effect damage and applicable timed effects; curse penalties and native no-effect
cases are unchanged. Bear traps gain the same percentages to direct damage and
bleeding duration, not paralysis (native PvP control caps remain).
Bear traps can appear in loose tool loot and camp-supply caches; deployment and
recovery retain rarity. Food, empty bottles, scrolls, tinkered bombs and other
tools remain standard; special appearance-state
items (including the charged Scepter) are deliberately excluded.
Native monster/random room items and shop stock are not rerolled on pickup.

Inventory, hotbar and menu slots use dark rarity-tinted recessed backgrounds
and fine beveled rims, not banners or icon haze. Original item artwork stays full
size and untinted, with no rarity overlay crossing the icon. Native curse,
quantity and equipped indicators remain above the background; blessings remain in tooltips. Hover names use the rarity color and identified tooltips explain
bonuses/effects. Above-Grey world drops emit small, gently curling smoke pillars
in their rarity color for nearby peers; Grey drops have no smoke. Rarity survives
native pickup/drop, equipment, chests and floor transitions;
different tiers or equipment roll seeds never merge. Pickup, drop, equipment
sync and inventory deduplication preserve exact affixes; they never reroll them.
Old rarity tags remain readable. No campaign import or banked starter kits.

