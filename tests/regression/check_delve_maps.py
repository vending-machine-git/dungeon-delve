"""Check original Delve canvases and runtime biome/size configuration."""
from pathlib import Path
import json
import struct

ROOT = Path(__file__).resolve().parents[2]


def main():
    config = json.loads((ROOT/'mods/extraction-raid/data/rmf/raid.json').read_text())
    assert config['layout_version'] == 4
    assert config['rules_version'] == 7 and config['floor_seconds'] == 360
    assert config['xp_multiplier'] == config['experience_multiplier'] == 1.75
    assert config['shopkeepers']['count_per_floor'] == 3
    assert config['shopkeepers']['minimum_spacing_tiles'] == 22
    assert config['shopkeepers']['stock_slots'] == 16
    assert config['shopkeepers']['stock_rarity'] == 'floor-loot-weights'
    assert config['shopkeepers']['footprint_tiles'] == [5, 5]
    assert config['shopkeepers']['mob_relationship'] == 'mutually-neutral'
    assert config['shopkeepers']['raid_protocol'] == 35
    assert config['descent_sites_per_floor'] == 1 and config['portal_sites_per_floor'] == 3
    assert config['portal_unlock_seconds_remaining'] == [60, 0, 0]
    assert config['final_hatch_seconds_remaining'] == 60 and config['victory_portals'] == 3
    assert config['enemy_balance']['harder_replacements_per_floor'] == [0, 5, 9]
    assert config['enemy_balance']['raid_protocol'] == 28
    assert config['rat_returns']['respawn_cooldown_seconds'] == 45
    assert config['rat_returns']['algernon_cheese'] == 10
    assert config['rat_returns']['damage'] == 'native-uncapped'
    expected = [('mine', 36, 52, 56), ('swamp', 30, 44, 56), ('caves', 24, 36, 56)]
    assert len(config['layouts']) == len(expected)
    for floor, (layout, budgets) in enumerate(zip(config['layouts'], expected), 1):
        theme, chests, drops, guards = budgets
        assert layout['theme'] == theme and layout['size'] == 90
        assert (layout['chest_target'], layout['drop_target'], layout['guard_target']) == (chests, drops, guards)
        data = (ROOT/f'mods/extraction-raid/maps/delve{floor}.lmp').read_bytes()
        assert data[:6] == b'BARONY'
        assert data[6:38].rstrip(b'\0').decode() == layout['name']
        assert data[38:70].rstrip(b'\0') == b'vending_machine'
        assert struct.unpack_from('<II', data, 70) == (90, 90)
        assert len(data) == 78 + 90*90*3*4 + 4
        assert struct.unpack_from('<I', data, len(data)-4)[0] == 0
    print('PASS equal 90x90 original canvases, jungle biome, gentler population and preserved loot budgets')


if __name__ == '__main__':
    main()
