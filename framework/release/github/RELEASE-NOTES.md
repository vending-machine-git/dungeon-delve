# Dungeon Delve 0.20.2 - populated reward rooms and native gates

Valid gate and iron-door models are preserved. Optional rooms without loot now
keep their lock and receive a cache chest (or gear/treasure when no chest fits)
before their barriers are assigned, using the existing floor loot budgets. Nearby native levers open the original gates,
including rows of connected gate panels. Essential through-routes start open.
Only structurally broken doorways are removed.

Crowded chests, doorway blockers and native full shops remain fixed. Three
furnished kiosks per floor, floor loot/rarity pools, XP, timing and death/return
rules remain. All peers need runtime/content 0.20.2, protocol 34 and API 1.
Restart from the root Dungeon Delve.exe and begin a new match.
