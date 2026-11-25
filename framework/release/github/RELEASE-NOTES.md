# Dungeon Delve 0.20.3 - vanilla-style gates

Gates now behave like vanilla Barony. They start closed (native inverted gates
still start open) and their lever sits a few tiles away against a wall, off the
doorway, instead of right beside the gate. Reward-room gates have one lever
outside. Any other gate that divides the floor has a lever on each side, and
either lever opens or closes it, so a closed gate can never trap a player.

Optional rooms without loot keep their lock and receive a cache chest (or
gear/treasure when no chest fits) from the existing floor loot budget. Only
structurally broken doorways are removed.

Crowded chests, doorway blockers and native full shops remain fixed. Three
furnished kiosks per floor, floor loot/rarity pools, XP, timing and death/return
rules remain. All peers need runtime/content 0.20.3, protocol 35 and API 1.
Restart from the root Dungeon Delve.exe and begin a new match.
