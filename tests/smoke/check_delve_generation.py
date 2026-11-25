"""Independent topology checks against terrain and fixtures from the native game."""
from collections import deque
from pathlib import Path
import argparse
import json


def xy(value):
    return value['x'], value['y']


def neighbors(point):
    x, y = point
    return ((x-1, y), (x+1, y), (x, y-1), (x, y+1))


def flood(start, tiles):
    reached = {start} if start in tiles else set()
    queue = deque(reached)
    while queue:
        for point in neighbors(queue.popleft()):
            if point in tiles and point not in reached:
                reached.add(point)
                queue.append(point)
    return reached


def check_generation(layout):
    assert layout['native_shop_tiles'] == 0, 'Retail full shop survived generation'
    chests, doors = layout['chests'], layout['doors']
    assert len(chests) == layout['chest_target'], 'Cache budget is not exact'
    assert len({xy(c) for c in chests}) == len(chests)
    for i, chest in enumerate(chests):
        x, y = xy(chest)
        assert all(abs(x-c['x'])+abs(y-c['y']) >= 4 for c in chests[i+1:]), 'Clustered chests'
        assert all(max(abs(x-d['x']), abs(y-d['y'])) > 2 for d in doors), 'Chest blocks doorway/approach'
    terrain = {(x, y) for y, row in enumerate(layout['diagram_tiles']) for x, cell in enumerate(row)
               if cell == '.' and 1 < x < layout['size']-2 and 10 <= y < layout['size']-2}
    blocked = {xy(b) for b in layout['static_blockers']}
    walkable = terrain - blocked
    start = xy(layout['spawns'][0])
    opened = flood(start, walkable)
    essential = [xy(p) for key in ('spawns', 'descents', 'portals') for p in layout[key]]
    essential += [xy(layout['boss'])]
    essential += [(k['entrance_x'], k['entrance_y']) for k in layout['kiosks']]
    essential += [(k['approach_x'], k['approach_y']) for k in layout['kiosks']]
    assert set(essential) <= opened, 'Essential route disconnected by furniture'
    for chest in chests:
        assert any(p in opened for p in neighbors(xy(chest))), 'Unreachable chest interaction'
        # Opening a chest's occupied tile must not expose a hidden disconnected
        # region: a chest cannot itself be the sole bridge to another room.
        unblocked = flood(start, walkable | {xy(chest)})
        assert unblocked - opened <= {xy(chest)}, 'Chest severs a passage'
    assert layout['converted_native_gates'] == 0, 'Native gate was converted'
    assert sum(d['kind'] == 'gate' for d in doors) == layout['preserved_native_gates']
    groups = {}
    for door in doors:
        groups.setdefault(door['group'], []).append(door)
    locked_groups = [group for group in groups.values() if group[0]['locked']]
    assert len(locked_groups) == layout['reward_locks']
    assert sum(group[0]['kind'] != 'gate' for group in locked_groups) <= 3
    locks = {xy(d) for group in locked_groups for d in group}
    closed = flood(start, walkable - locks)
    assert set(essential) <= closed, 'Locked door blocks essential destination'
    rewards = set()
    for door in doors:
        x, y = xy(door)
        step = (1, 0) if door['axis'] == 0 else (0, 1)
        assert (x-step[0], y-step[1]) in opened and (x+step[0], y+step[1]) in opened, 'Door faces a wall or blocked tile'
        assert xy(door) in opened
        if door['kind'] == 'gate':
            assert (door['lever_x'], door['lever_y']) in opened, 'Gate has no reachable lever'
            if door['locked']:
                assert (door['lever_x'], door['lever_y']) in closed, 'Closed gate control is behind another barrier'
            assert door['starts_open'] != door['locked']
        if not door['locked']:
            assert door['reward_index'] == -1
            continue
    for group in locked_groups:
        door = group[0]
        group_cells = {xy(d) for d in group}
        assert any(p in closed for d in group_cells for p in neighbors(d)), 'Barrier has no reachable approach'
        kind, index = door['reward_kind'], door['reward_index']
        assert kind in ('chest', 'drop') and (kind, index) not in rewards, 'Invalid/reused vault reward'
        rewards.add((kind, index))
        reward = layout['chests' if kind == 'chest' else 'drops'][index]
        assert kind != 'chest' or reward['role'] != 0, 'Starter cache is behind a lock'
        reward_tiles = set(neighbors(xy(reward))) if kind == 'chest' else {xy(reward)}
        assert not reward_tiles & closed, 'Barrier has no exclusive reward'
        unlocked = flood(start, walkable - (locks - group_cells))
        assert reward_tiles & unlocked, 'Reward requires another barrier'
        assert all(d['reward_kind'] == kind and d['reward_index'] == index for d in group)
    assert sum(group[0]['reward_generated'] for group in groups.values()) == layout['generated_room_rewards']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profiles', nargs='+', type=Path)
    args = parser.parse_args()
    count = 0
    for root in args.profiles:
        paths = [root] if root.is_file() else sorted(root.rglob('delve-generated-floor*.json'))
        for path in paths:
            check_generation(json.loads(path.read_text()))
            count += 1
    assert count, 'No generated native maps supplied'
    print(f'PASS {count} native floor layouts: shops, chest spacing/door clearance, connectivity and exclusive lock rewards')


if __name__ == '__main__':
    main()
