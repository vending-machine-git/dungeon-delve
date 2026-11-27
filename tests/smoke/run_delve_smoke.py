"""Exercise Dungeon Delve through real Direct IP clients in disposable profiles.

Movement and clock advancement are smoke fixtures; interactions use native CKIR.
Production builds exclude the fixtures. Never loads the user's campaign saves.
"""
from pathlib import Path
import argparse
from collections import Counter
import ctypes
from ctypes import wintypes
from check_delve_generation import check_generation
import json
import os
import re
import shutil
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
GAME = Path(r'F:\Steam\steamapps\common\Barony')
UM = Path(os.environ.get('UNIVERSAL_MODDER', str(Path.home() / 'Documents/Projects/[Tools]/universal-modder/bin/um.cmd')))


def check_tuned_roster(layout, floor):
    check_generation(layout)
    assert len(layout.get('kiosks', [])) == 3, 'Expected exactly three shopkeepers per floor'
    distances = [(a['x']-b['x'])**2+(a['y']-b['y'])**2 for i,a in enumerate(layout['kiosks']) for b in layout['kiosks'][i+1:]]
    assert min(distances) >= 22**2, 'Shopkeepers are clustered together'
    assert max(distances) <= 4*min(distances), 'Shopkeepers are not evenly spread'
    for kiosk in layout['kiosks']:
        assert (kiosk['width'],kiosk['height'])==(5,5), 'Kiosk must have room for its furnished interior'
        assert (kiosk['entrance_x'],kiosk['entrance_y'])==(kiosk['x'],kiosk['y']+2)
        assert (kiosk['approach_x'],kiosk['approach_y'])==(kiosk['x'],kiosk['y']+3)
        assert (kiosk['merchant_x'],kiosk['merchant_y'])==(kiosk['x'],kiosk['y']+1)
        assert len(set(kiosk['torch_uids']))==4 and kiosk['table_uid']!=kiosk['chair_uid']
    assert len(layout['descents']) == 1 and len(layout['portals']) == 3, 'Expected one hatch and three portal sites'
    species = Counter(encounter['monster'] for encounter in layout['encounters'])
    assert sum(species.values()) == 56 and all(species[kind] > 0 for kind in (2, 4, 6))
    # 3 goblin, 7 spider, 9 skeleton, 10 scorpion, 28 insectoid.
    if floor == 1:
        assert species[9] > 0 and species[3] == 0 and species[28] == 0
    elif floor == 2:
        assert species[9] > 0 and species[3] >= 3 * species[9] and species[28] == 0, 'Jungle goblins must replace skeletons'
    else:
        assert species[9] == 0 and all(species[kind] > 0 for kind in (7, 10, 28)), 'Caves must use mid-tier insect monsters'
    assert species[5] == (0, 5, 9)[floor - 1], 'Wrong harder-enemy replacement count'


def menu_window(proc):
    handles = []
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def visit(hwnd, unused):
        pid = wintypes.DWORD()
        ctypes.windll.user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value == proc.pid and ctypes.windll.user32.IsWindowVisible(hwnd):
            handles.append(hwnd)
        return True
    callback = callback_type(visit)
    ctypes.windll.user32.EnumWindows(callback, 0)
    assert len(handles) == 1, 'Could not identify owned game window'
    return handles[0]


def menu_keys(proc, keys):
    """Post menu keys only to this fixture's window; leave desktop input alone."""
    hwnd = menu_window(proc)
    for key in keys:
        scan = ctypes.windll.user32.MapVirtualKeyW(key, 0)
        flags = 1 | (scan << 16) | ((1 << 24) if 0x21 <= key <= 0x2e else 0)
        ctypes.windll.user32.PostMessageW(hwnd, 0x100, key, flags)
        time.sleep(.15)
        ctypes.windll.user32.PostMessageW(hwnd, 0x101, key, flags | (3 << 30))
        time.sleep(.35)


def menu_click(proc, x, y):
    """Click a native menu in virtual 1280x720 coordinates, in the owned window."""
    hwnd = menu_window(proc)
    rect = wintypes.RECT()
    assert ctypes.windll.user32.GetClientRect(hwnd, ctypes.byref(rect))
    point = round(x*(rect.right-rect.left)/1280) | (round(y*(rect.bottom-rect.top)/720) << 16)
    ctypes.windll.user32.PostMessageW(hwnd, 0x200, 0, point)
    time.sleep(.1)
    ctypes.windll.user32.PostMessageW(hwnd, 0x201, 1, point)
    time.sleep(.05)
    ctypes.windll.user32.PostMessageW(hwnd, 0x202, 0, point)
    time.sleep(.5)


def capture(processes, run):
    handles = {}
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def visit(hwnd, unused):
        pid = wintypes.DWORD()
        ctypes.windll.user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        for role, proc in processes.items():
            if pid.value == proc.pid and ctypes.windll.user32.IsWindowVisible(hwnd):
                handles[role] = hwnd
        return True
    callback = callback_type(visit)
    ctypes.windll.user32.EnumWindows(callback, 0)
    for role, hwnd in handles.items():
        from PIL import Image
        output = run / (role + '.png')
        for attempt in range(3):
            subprocess.run([str(UM), 'win', 'shot', '--hwnd', str(hwnd), '--scale', '0.75', str(output)], check=True)
            with Image.open(output) as screenshot:
                if screenshot.convert('RGB').getbbox() is not None:
                    break
            if attempt == 2:
                raise RuntimeError('Window capture returned three black frames for ' + role)
            time.sleep(.15)


def scenario(args, name):
    run = args.output.resolve()/('delve-'+name+'-'+uuid.uuid4().hex[:10])
    (run/'sync').mkdir(parents=True)
    print('ARTIFACTS '+str(run), flush=True)
    peers = 4 if name in ('battle-royale', 'battle-wipe', 'battle-host-win', 'progression', 'population', 'concealment', 'results', 'revenants') else 2
    if args.players is not None: peers=args.players
    processes, streams, logs = {}, [], {}
    result = dict(status='RUNNING', scenario=name, players=peers)
    try:
        for slot in range(peers):
            role = 'host' if slot == 0 else 'client'+str(slot)
            folder = run/role
            (folder/'config').mkdir(parents=True)
            width, height = map(int, args.size.split('x'))
            settings = dict(version=25, skipintro=True, mods=[], port_number=args.port, music_volume=1, ui_scale=float(args.ui_scale),
                video=dict(version=1, window_mode=0, resolution_x=width, resolution_y=height, vsync_enabled=False))
            if name == 'abilities':
                settings['bindings'] = dict(version=0, players=[dict(kb_mouse_bindings=[
                    dict(binding='Delve Ability 1', input='F7'), dict(binding='Delve Ability 2', input='F8')],
                    gamepad_bindings=[], joystick_bindings=[]) for player in range(4)])
            (folder/'config/config.json').write_text(json.dumps(settings))
            for mod in ('vending_machine_framework', 'extraction-raid'):
                for directory in ('data', 'maps', 'images', 'music'):
                    source = ROOT/'mods'/mod/directory
                    if source.is_dir(): shutil.copytree(source, folder/directory, dirs_exist_ok=True)
            if name == 'quality-unidentified' or (name == 'appraisal-disabled' and slot == 0) or (name == 'appraisal' and slot > 0):
                settings=folder/'data/rmf/raid.json'
                config=json.loads(settings.read_text()); config['instant_identification']=False
                settings.write_text(json.dumps(config))
            books = GAME/'books/compiled_books.json'
            if books.is_file():
                (folder/'books').mkdir(); shutil.copy2(books, folder/'books/compiled_books.json')
            env = os.environ.copy()
            env['PATH'] = str(GAME)+os.pathsep+env.get('PATH', '')
            selected_class=args.classes[slot] if args.classes else slot if name == 'progression' else 0
            selected_race=args.races[slot] if args.races else 0
            env.update(BARONY_SMOKE_CLASS=str(selected_class), BARONY_SMOKE_RACE=str(selected_race), BARONY_SMOKE_AUTOPILOT='1', BARONY_SMOKE_ROLE='host' if slot == 0 else 'client',
                       BARONY_SMOKE_PLAYERS=str(peers), BARONY_SMOKE_DELVE=name,
                       BARONY_SMOKE_DELVE_TOUR='1' if args.tour_capture and slot == 0 else '0',
                       BARONY_SMOKE_RAID_SYNC=str(run/'sync'), BARONY_SMOKE_CONNECT_ADDRESS='127.0.0.1:'+str(args.port))
            for floor in (1,2,3):
                forced=getattr(args,'floor'+str(floor)+'_seed')
                if forced is not None:env['BARONY_SMOKE_DELVE_FLOOR'+str(floor)+'_SEED']=str(forced)
            if args.builds or name == 'abilities':env['BARONY_SMOKE_EQUIP_BUILD']='1'
            # A stale/invalid prototype kit must never affect fresh class selection.
            (folder/'delve-profile.json').write_text('{"legacy_profile_must_not_be_loaded":true}')
            log = (folder/'process.log').open('wb'); streams.append(log)
            processes[role] = subprocess.Popen([str(args.exe), '-windowed', '-size='+args.size, '-nosound',
                '-datadir='+str(GAME), '-raid-profile='+str(folder/'delve-profile.json')], cwd=folder,
                env=env, stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
            print(role+' pid='+str(processes[role].pid), flush=True)
            if slot == 0:
                deadline = time.monotonic()+args.timeout
                while True:
                    engine = folder/'log.txt'
                    if engine.exists() and 'host_ready' in engine.read_text(errors='replace'): break
                    if processes[role].poll() is not None: raise RuntimeError('Host exited before lobby ready')
                    if time.monotonic()>deadline: raise RuntimeError('Lobby timed out')
                    time.sleep(.2)
        deadline = time.monotonic()+args.timeout
        retry_verified = False
        captured_views=set()
        captured_scenes=set()
        captured_gates=set()
        restarted=set()
        while True:
            for role, proc in processes.items():
                path = run/role/'log.txt'
                logs[role] = path.read_text(errors='replace') if path.exists() else ''
                if proc.poll() is not None: raise RuntimeError(role+' exited '+str(proc.returncode))
            if name == 'rarity' and (run/'sync/rarity-smoke-ready').exists() and 'smoke' not in captured_views:
                time.sleep(.8)
                if args.capture:
                    destination = run/'smoke-pillar'
                    destination.mkdir(parents=True, exist_ok=True)
                    capture({'client1':processes['client1']}, destination)
                    menu_keys(processes['client1'], [0x09])
                    destination = run/'rarity-inventory'
                    destination.mkdir(parents=True, exist_ok=True)
                    capture({'client1':processes['client1']}, destination)
                    menu_keys(processes['client1'], [0x09])
                (run/'sync/rarity-smoke-captured').write_text('1')
                captured_views.add('smoke')
            if name == 'shops' and (run/'sync/gate-view').exists():
                view=(run/'sync/gate-view').read_text().strip()
                if view and view not in captured_gates:
                    if args.capture:
                        time.sleep(.35)
                        destination=run/'gate-scenes'/view;destination.mkdir(parents=True)
                        capture({'client1':processes['client1']},destination)
                    (run/'sync'/('gate-captured-'+view)).write_text('1')
                    captured_gates.add(view)
            if name == 'shops' and (run/'sync/shop-scene-view').exists():
                view=(run/'sync/shop-scene-view').read_text().strip()
                if view and view not in captured_scenes:
                    if args.capture:
                        time.sleep(.35)
                        destination=run/'shop-scenes'/view;destination.mkdir(parents=True)
                        capture({'client1':processes['client1']},destination)
                    (run/'sync'/('shop-scene-captured-'+view)).write_text('1')
                    captured_scenes.add(view)
            if name == 'shops' and (run/'sync/shop-view').exists():
                view=(run/'sync/shop-view').read_text().strip()
                if view and view not in captured_views:
                    if args.capture:
                        destination=run/'shops'/view;destination.mkdir(parents=True)
                        capture({'client1':processes['client1']},destination)
                    (run/'sync'/('shop-captured-'+view)).write_text('1')
                    captured_views.add(view)
            if name == 'revenants':
                for view in ('return-menu-first', 'return-menu-rat'):
                    if (run/'sync'/view).exists() and view not in captured_views:
                        if args.capture:
                            time.sleep(.6)
                            destination = run/view
                            destination.mkdir(parents=True, exist_ok=True)
                            capture(processes, destination)
                        (run/'sync'/(view+'-captured')).write_text('1')
                        captured_views.add(view)
            if name.startswith('battle-') and (run/'sync/battle-jungle-view-ready').exists() and 'jungle' not in captured_views:
                destination = run/'jungle-view'
                destination.mkdir(parents=True)
                capture({'host': processes['host']}, destination)
                (run/'sync/battle-jungle-captured').write_text('1')
                captured_views.add('jungle')
            if name == 'restart':
                for round_text, action in re.findall(r'restart_menu round=(\d+) action=(\w+)', logs['host']):
                    round_number = int(round_text)
                    if round_number in restarted: continue
                    destination = run/'rounds'/round_text
                    destination.mkdir(parents=True)
                    for role in processes:
                        for filename in ('delve-results.json', 'delve-generated-floor1.json'):
                            source = run/role/filename
                            if source.exists(): shutil.copy2(source, destination/(role+'-'+filename))
                    for target in (run/'sync').glob('target-*'): target.unlink()
                    # Let the native death animation/menu finish before activating it.
                    time.sleep(8 if action == 'death' else 1)
                    if args.capture: capture({'host':processes['host']}, destination)
                    if action == 'pause':
                        menu_keys(processes['host'], [0x1b])
                    session = re.findall(r'restart_ready_match round=\d+ session=(\d+)', logs['host'])[-1]
                    (run/'sync/restart-click').write_text(session)
                    restarted.add(round_number)
                    print(f'Requested native {action} Restart controls after round {round_number}.', flush=True)
            if name == 'faults' and not retry_verified and '[RAID] extraction save failed:' in logs['client1']:
                assert json.loads((run/'client1/delve-profile.json').read_text()) == dict(legacy_profile_must_not_be_loaded=True), 'Failed banking changed the prior profile'
                blocker = run/'client1/delve-profile.json.tmp'
                (blocker/'fault-blocker').unlink(); blocker.rmdir()
                retry_verified = True
                print('Bank failure retained unbanked state; retry enabled.', flush=True)
            view_file=run/'sync/view'
            ability_file = run/'sync/ability-stage'
            if name == 'abilities' and ability_file.exists():
                stage = ability_file.read_text().strip()
                if stage in {'0', '1', '2', '3', '4', '5', '6'} and stage not in captured_views:
                    time.sleep(.6)
                    destination = run/'abilities'/stage
                    destination.mkdir(parents=True, exist_ok=True)
                    capture(processes, destination)
                    captured_views.add(stage)
                    if stage == '0': menu_keys(processes['host'], [0x51])
                    elif stage == '1':
                        time.sleep(.7)
                        menu_keys(processes['host'], [0x45])
                    elif stage == '2': menu_keys(processes['client1'], [0x51])
                    elif stage == '3':
                        time.sleep(.7)
                        menu_keys(processes['client1'], [0x45])
                    else: (run/'sync/ability-command').write_text(str(int(stage) + 1))
            cooldown_file=run/'sync/cooldown' 
            if name == 'cooldowns' and cooldown_file.exists():
                stage = cooldown_file.read_text().strip()
                if stage and stage not in captured_views:
                    destination=run/'cooldowns'/stage;destination.mkdir(parents=True,exist_ok=True)
                    capture({'host':processes['host']},destination);captured_views.add(stage)
            if args.tour_capture and view_file.exists():
                view=view_file.read_text().strip()
                if view and view not in captured_views:
                    destination=run/'views'/view;destination.mkdir(parents=True,exist_ok=True)
                    capture({'host':processes['host']},destination);captured_views.add(view)
            if name == 'results' and 'initial' not in captured_views and all('[SMOKE][DELVE] results_ready role=' in log for log in logs.values()):
                if args.capture: capture(processes, run)
                captured_views.add('initial')
                (run/'sync/results-dismiss').write_text('1')
            if all('[SMOKE][DELVE] complete role=' in log for log in logs.values()): break
            if time.monotonic()>deadline: raise RuntimeError('Timed out waiting for '+name)
            time.sleep(.2)
        reports = {role: json.loads((run/role/'delve-results.json').read_text()) for role in processes}
        assert len({json.dumps(r, sort_keys=True) for r in reports.values()}) == 1, 'Results disagree across peers'
        report = reports['host']; assert report['complete'] and len(report['ranking']) == peers
        assert peers==1 or ('enemy=1 friend=0 friendlyfire=1' in logs['host'] or (name == 'rarity' and 'check PASS rarity_ffa_enforced' in logs['host']) or (name.startswith('battle-') and 'check PASS battle_ffa_enforced' in logs['host'])), 'FFA is not enforced'
        assert '[SMOKE][DELVE] check FAIL' not in logs['host'], 'Gameplay check failed'
        for role,text in logs.items():
            limits=re.findall(r'\[DELVE\] load floor=\d+ portals=\d+ limit=(\d+)',text)
            assert limits and all(int(limit)==360 for limit in limits), (role, 'Six-minute floor timer not loaded', limits)
        if not name.startswith('battle-') and name not in ('restart', 'revenants', 'rarity', 'shops', 'storm-wipe'):
            assert 'check PASS outpost_physical_guards_no_random_minibosses' in logs['host'], 'First-floor loadouts were not validated'
            assert 'check PASS outpost_has_no_goblins' in logs['host'], 'First-floor goblin exclusion was not verified'
        if not name.startswith('battle-') and name not in ('restart', 'results', 'cooldowns', 'abilities', 'appraisal', 'appraisal-disabled', 'revenants', 'rarity', 'shops', 'storm-wipe'):
            assert 'check PASS population_3000_seeded_rosters' in logs['host'], 'Seeded population distribution was not validated'
        if name.startswith('battle-'):
            assert all('check FAIL' not in text for text in logs.values())
            layouts = json.loads((ROOT/'mods/extraction-raid/data/rmf/raid.json').read_text())['layouts']
            for floor, layout in enumerate(layouts, 1):
                generated = [json.loads((run/role/f'delve-generated-floor{floor}.json').read_text()) for role in processes]
                assert all(data['size'] == 90 and data['theme'] == layout['theme'] for data in generated)
                assert len({data['geometry_fingerprint'] for data in generated}) == 1, 'Peers generated different terrain'
                for key in ('spawns', 'boss', 'descents', 'portals', 'encounters', 'kiosks'):
                    assert len({json.dumps(data[key], sort_keys=True) for data in generated}) == 1, 'Peer layout mismatch: '+key
                assert len(generated[0]['spawns']) == 4 and generated[0]['reachable_tiles'] >= 150
                assert len(generated[0]['encounters']) == layout['guard_target']
                check_tuned_roster(generated[0], floor)
                assert len(generated[0]['chests']) >= layout['chest_target']
                assert len(generated[0]['drops']) == layout['drop_target']
                if floor == 2:
                    assert layout['theme'] == 'swamp' and all(data['spell_traps'] == 0 for data in generated)
            assert logs['host'].count('check PASS battle_equal_full_size_floors') == 3
            assert 'check PASS battle_jungle_has_no_spell_traps' in logs['host']
            expected_winner = -1 if name == 'battle-wipe' else 0 if name == 'battle-host-win' else 1
            assert sum(row['status'] == 'Winner' for row in report['ranking']) == int(expected_winner >= 0)
            if expected_winner >= 0: assert report['ranking'][0]['status'] == 'Winner'
            assert report['last_standing_slot'] == expected_winner
            for role in processes:
                assert json.loads((run/role/'delve-profile.json').read_text()) == dict(legacy_profile_must_not_be_loaded=True)
                assert logs[role].count('[DELVE] load floor=') == 3
                assert all(logs[role].count('[DELVE-INTRO] dungeon-start floor='+str(floor)) == 1 for floor in (1, 2, 3))
                assert logs[role].count('[DELVE-INTRO] playing') == 3
                assert '[DELVE-INTRO] stopped frames=' in logs[role]
            for label in ('battle_dead_guest_returns_starter', 'battle_descent_preserves_backpack',
                          'battle_storm_breaks_wall_and_prop', 'battle_one_hatch_three_final_portals',
                          'battle_descent_single_use', 'battle_final_portal_hidden_before_last_minute',
                          'battle_guest_monster_stays_slain_this_floor', 'battle_monster_cannot_descend_with_loot',
                          'battle_guest_monster_returns_normal_next_floor', 'battle_host_same_floor_monster_return',
                          'battle_live_host_monster_does_not_block_floor', 'battle_host_monster_returns_normal_next_floor'):
                assert 'check PASS ' + label in logs['host'], label
            for label in ('battle_one_hatch_three_portal_sites', 'battle_one_early_hatch_only', 'battle_xp_one_point_seven_five',
                          'battle_portal_hidden_before_last_minute', 'battle_one_portal_at_one_minute_left',
                          'battle_overtime_portals_hidden_before_zero', 'battle_two_more_portals_at_zero',
                          'battle_final_hatch_cannot_win_early', 'battle_final_overtime_portals_hidden_before_zero',
                          'battle_final_two_more_portals_at_zero',
                          'battle_final_no_storm_before_last_minute', 'battle_final_storm_starts_last_minute',
                          'battle_dead_guest_retains_progression', 'battle_wipe_retains_progression'):
                assert 'check PASS ' + label in logs['host'], label
            assert logs['host'].count('check PASS battle_wipe_returns_starters') == 4
            for label in ('battle_used_hatch_removed_immediately', 'battle_used_portal_removed',
                          'battle_spent_exits_stay_removed_at_timeout'):
                assert 'check PASS ' + label in logs['host'], label
            if expected_winner >= 0:
                assert all('check PASS battle_final_exit_removed' in log for log in logs.values())
            if name == 'battle-royale':
                assert 'check PASS battle_guest_first_claim_reserved' in logs['host']
                assert 'check PASS battle_pending_victory_hidden_without_light' in logs['host']
                assert 'check PASS battle_other_exit_cannot_steal_pending_victory' in logs['host']
            assert logs['host'].count('floor_return slot=') == 5
            for role in ('client1', 'client2', 'client3'):
                assert 'check PASS battle_guest_used_hatch_removed' in logs[role]
                assert 'check PASS battle_guest_used_portal_removed' in logs[role]
                assert 'check PASS battle_guest_storm_wall_replicated' in logs[role]
                assert logs[role].count('check PASS battle_guest_retains_progression') == 2
        elif name == 'storm-wipe':
            assert all('check FAIL' not in log for log in logs.values())
            assert logs['host'].count('check PASS storm_collision_killed_player')==peers*3
            assert logs['host'].count('check PASS storm_death_obituary')==peers*3
            assert logs['host'].count('check PASS storm_return_three_reachable_spaced_shops')==3
            assert logs['host'].count('check PASS storm_return_alive_class_starter')==peers*3
            assert report['last_standing_slot']==-1
            for floor in (1,2,3):
                generated=[json.loads((run/role/f'delve-generated-floor{floor}.json').read_text()) for role in processes]
                assert len({data['geometry_fingerprint'] for data in generated})==1
                for data in generated: check_tuned_roster(data,floor)
            if args.floor2_seed is not None:
                assert json.loads((run/'host/delve-generated-floor2.json').read_text())['seed']==args.floor2_seed
        elif name == 'shops':
            assert logs['host'].count('check PASS gate_guest_lever_native_interaction_opens_original_gate') == len(captured_gates)
            assert logs['client1'].count('check PASS gate_guest_sees_native_gate_motion_and_collision') == len(captured_gates)
            assert len(captured_scenes)==9, 'Missing furnished kiosk scene snapshots'
            assert len(captured_views)==9, 'Missing shop inventory snapshots'
            assert logs['host'].count('check PASS shops_three_spaced_kiosks_and_floor_rarity_stock')==3
            assert logs['host'].count('check PASS shops_host_removes_exact_rarity_item_and_charges_gold')==9
            assert logs['client1'].count('check PASS shops_guest_exact_stock_appearance_status_quantity_and_slots')==9
            assert logs['client1'].count('check PASS shops_guest_native_purchase_preserves_rarity_and_rolls')==9
            for check_name in ('shops_hover_actual_effects_and_rolls', 'shops_hover_visible_beside_shop', 'shops_hover_keeps_native_price_and_purchase_prompt'):
                assert logs['client1'].count('check PASS '+check_name)==9, 'Missing shop hover checks: '+check_name
            assert all('check FAIL' not in log for log in logs.values())
            for floor in (1,2,3):
                check_tuned_roster(json.loads((run/'host'/f'delve-generated-floor{floor}.json').read_text()),floor)
        elif name == 'rarity':
            for role in processes:
                for label in ('spell_cast_base_preparation_double_vanilla',
                              'spell_cast_learned_speed_bonus_after_slower_base',
                              'spell_cast_held_book_speed_bonus_after_slower_base'):
                    assert 'check PASS ' + label in logs[role], (role, label)
            assert 'check PASS rarity_host_complete' in logs['host']
            assert 'check PASS rarity_guest_complete' in logs['client1']
            assert all('check FAIL' not in text for text in logs.values())
            assert 'check PASS rarity_guest_equipment_server_synchronized' in logs['host']
            assert 'check PASS rarity_guest_itmu_full_appearance' in logs['client1']
            assert 'check PASS affix_guest_exact_host_rolls_networked' in logs['client1']
            assert 'check PASS spellbook_guest_cast_exact_snapshot_on_host' in logs['host']
            for role in ('host', 'client1'):
                for check_name in ('spellbook_first_read_creates_memorized_variant',
                                   'spellbook_save_metadata_roundtrip', 'spellbook_native_healing_power',
                                   'spellbook_held_cast_uses_held_rolls_without_merging_memory',
                                   'spellbook_broken_held_book_inert',
                                   'spellbook_read_legendary_inherits_exact_blessed_rolls',
                                   'spellbook_replacement_keeps_selection_and_single_spell',
                                   'spellbook_duplicate_read_replaces_all_rolls_including_downgrade',
                                   'spellbook_inflight_copy_keeps_old_rolls',
                                   'spellbook_plain_book_clears_previous_rolls'):
                    assert 'check PASS '+check_name in logs[role], 'Missing book check: '+role+'/'+check_name
            for index in range(28):
                for prefix in ('affix_native_effect_', 'affix_unequip_broken_inert_', 'affix_blessed_native_effect_', 'affix_blessed_unequip_broken_inert_'):
                    assert 'check PASS '+prefix+str(index)+'\n' in logs['host'], 'Missing affix check: '+prefix+str(index)
            for check_name in ('affix_different_seed_cannot_stack', 'affix_native_zero_zero_one_two_three_rolls', 'affix_deduplication_never_rerolls', 'affix_blessing_tooltips_and_seed_stability', 'affix_blessing_live_max_hp_no_free_healing'):
                assert 'check PASS '+check_name in logs['host'], 'Missing affix check: '+check_name
            for check_name in ('rarity_gem_quality_flawless_is_rare', 'rarity_potion_native_drink_healing_consumption',
                               'rarity_trap_native_deploy_metadata', 'rarity_trap_native_trigger_damage_bleeding',
                               'rarity_trap_native_recovery_retains_tier', 'rarity_empty_bottles_excluded'):
                assert 'check PASS '+check_name in logs['host'], 'Missing rarity check: '+check_name
            for check_name in ('rarity_guest_gems_potions_traps_all_tiers_networked',
                               'rarity_guest_gem_potion_trap_world_smoke_and_grey_exclusion'):
                assert 'check PASS '+check_name in logs['client1'], 'Missing guest rarity check: '+check_name
            assert all('check PASS rarity_blessed_background_removed' in text for text in logs.values())
        elif name == 'revenants':
            for label in ('revenant_native_rat_attack_uncapped', 'revenant_passive_rat_damage_uncapped',
                          'revenant_rat_cooldown_45_seconds', 'revenant_rat_cooldown_last_tick_blocked',
                          'revenant_rat_respawn_after_exact_cooldown', 'revenant_cheese_resets_after_death',
                          'revenant_rat_can_pick_up_cheese_without_counting', 'revenant_rejected_food_not_counted',
                          'revenant_nine_cheese_not_evolved', 'revenant_tenth_cheese_native_algernon_stats',
                          'revenant_algernon_evolution_once_only', 'revenant_algernon_not_clamped_to_fragile_rat',
                          'revenant_guest_cheese_host_authoritative', 'revenant_new_rat_starts_without_cheese'):
                assert 'check PASS ' + label in logs['host'], label
            assert 'check PASS revenant_menu_cooldown_countdown' in logs['host']
            for label in ('revenant_menu_readable_card_layout', 'revenant_menu_rat_only_view',
                          'revenant_guest_nine_cheese_not_evolved', 'revenant_guest_algernon_stats_networked',
                          'revenant_guest_algernon_native_visual'):
                assert any('check PASS ' + label in text for role, text in logs.items() if role != 'host'), label
            assert sum('check PASS revenant_algernon_observer_visual' in text for role, text in logs.items() if role != 'host') == 2
            assert 'check PASS revenants_host_complete' in logs['host']
            assert logs['host'].count('check PASS revenant_catalogue_native_visual') == 17
            assert any('check PASS revenant_guest_body_synchronized' in text for role, text in logs.items() if role != 'host')
            assert any('check PASS revenant_guest_second_npc_locked_ui' in text for role, text in logs.items() if role != 'host')
            assert all('check FAIL' not in text for text in logs.values())
            assert all(row['status'] == 'Eliminated' for row in report['ranking'])
        elif name in ('progression', 'host-early', 'population'):
            for floor in (1, 2, 3):
                layout=json.loads((ROOT/'mods/extraction-raid/data/rmf/raid.json').read_text())['layouts'][floor-1]
                world=re.search(rf'world floor={floor} chests=(\d+) drops=(\d+)',logs['host'])
                assert world and int(world[1])>=layout['chest_target'] and int(world[2])>=layout['drop_target'], 'Missing generated loot on floor '+str(floor)
                population=re.search(rf'floor_ready floor={floor} .* monsters=(\d+)',logs['host'])
                assert population and int(population[1])>=layout['guard_target']+1, 'Enemy population below target on floor '+str(floor)
                generated=[json.loads((run/role/f'delve-generated-floor{floor}.json').read_text()) for role in processes]
                assert len({g['geometry_fingerprint'] for g in generated})==1, 'Peers generated different native geometry'
                assert len({json.dumps(g['spawns']) for g in generated})==1, 'Peers generated different start markers'
                assert len({json.dumps(g['encounters'], sort_keys=True) for g in generated})==1, 'Peers generated different enemy rosters'
                assert len(generated[0]['encounters'])==layout['guard_target'], 'Enemy count changed'
                check_tuned_roster(generated[0], floor)
                assert f'population_checked floor={floor} ' in logs['host'], 'Missing initialized enemy census'
                assert all(g['size']==layout['size'] and g['theme']==layout['theme'] for g in generated)
                for role, text in logs.items():
                    if role != 'host':
                        slot = int(re.search(r'client_floor slot=(\d+)', text).group(1))
                        assert f'client_floor slot={slot} floor={floor}' in text, 'Peer missed floor'
            assert logs['host'].count('stairs=1 portals=3') == 3, 'Floors must each have one hatch and three portal sites'
            if name == 'progression':
                assert logs['host'].count('protected=1') == 2, 'Staging failed to protect players'
                spawns = re.findall(r'spawn floor=1 slot=\d+ x=(\d+) y=(\d+)', logs['host'])
                assert len(set(spawns)) == 4, 'Starts overlap'
                classes=re.findall(r'spawn floor=1 .* class=(\d+)',logs['host'])
                assert sorted(map(int,classes))==sorted(args.classes if args.classes else range(4)), 'Selected classes were not preserved'
                if args.races:
                    identities=re.findall(r'spawn floor=1 slot=(\d+) .* class=(\d+) .* race=(\d+) hp=(\d+) maxhp=(\d+)',logs['host'])
                    assert len(identities)==4
                    # Direct IP slots follow connection order, which can differ from process launch order.
                    expected_pairs=sorted(zip(args.classes or list(range(4)),args.races))
                    assert sorted((int(c),int(r)) for slot,c,r,hp,mhp in identities)==expected_pairs,identities
                    catalogue=json.loads((ROOT/'mods/extraction-raid/data/rmf/class_builds.json').read_text())
                    for slot,class_id,race,hp,maxhp in identities:
                        class_id=int(class_id)
                        native_hp=60 if class_id in (0,4,12,16) else 55 if class_id in (11,19) else 50
                        if args.builds:
                            assert catalogue['version']==2 and all('modifiers' not in passive for passive in catalogue['classes'][class_id]['perks'])
                        assert int(maxhp)==native_hp and int(hp)>0, identities
                assert sum(r['status'] == 'Escaped' for r in report['ranking']) == 3
            elif name == 'host-early':
                assert reports['host']['ranking'][0]['status'] == 'Escaped'
                assert 'extraction confirmed slot=0 ' in logs['host'], 'Host could not escape early'
        elif name == 'faults':
            assert retry_verified
            assert 'early_portal_denied=1' in logs['host'] and 'spent_portal_denied=1' in logs['host']
            assert 'duplicate extraction approval ignored' in logs['client1']
            assert logs['client1'].count('[RAID] extraction committed slot=1 ') == 1
            assert report['ranking'][0]['slot'] == 1 and report['ranking'][0]['status'] == 'Escaped'
        elif name == 'restart':
            assert restarted == {1, 2, 3}, 'Missed a native restart path'
            for role, log in logs.items():
                sessions = re.findall(r'\[DELVE\] new_match session=(\d+)', log)
                assert len(sessions)==4 and len(set(sessions))==4, role+' missed a fresh match'
                assert log.count('[RAID] deployed slot=')==4, role+' did not deploy four fresh kits'
                assert '[SMOKE][DELVE] check FAIL' not in log, role+' failed restart checks'
                if role != 'host':
                    assert log.count('Received order to restart game')==3, 'Guest missed RSTR'
            populations = re.findall(r'floor_ready floor=1 .* monsters=(\d+)',logs['host'])
            target = json.loads((ROOT/'mods/extraction-raid/data/rmf/raid.json').read_text())['layouts'][0]['guard_target']+1
            assert len(populations)==4 and all(int(count)>=target for count in populations), 'Restart skipped enemy population'
            assert logs['host'].count('equipment_ready slot=1')==11, 'Guest kit handshake did not reset across four matches and eleven floors'
            assert logs['host'].count('restart_ready_match round=')==4
            assert logs['host'].count('restart_button action=death')==1
            assert logs['host'].count('restart_button action=pause')==2
            assert logs['host'].count('restart_button confirm=pause')==2
            for round_number in (1, 2, 3, 4):
                folder = run/'rounds'/str(round_number) if round_number<4 else None
                worlds = [json.loads(((folder/(role+'-delve-generated-floor1.json')) if folder else
                                     (run/role/'delve-generated-floor1.json')).read_text()) for role in processes]
                assert len({w['geometry_fingerprint'] for w in worlds})==1, 'Restarted peers generated different geometry'
            assert 'floor_ready floor=2 ' in logs['host'], 'Did not restart from a deeper floor'
            initial=json.loads((run/'rounds/1/host-delve-results.json').read_text())
            banked=json.loads((run/'rounds/2/host-delve-results.json').read_text())
            assert sum(r['monsters_slain'] for r in initial['ranking'])==1
            assert any(r['status']=='Winner' and r['extracted_gold']==2345 for r in banked['ranking'])
            assert all(r['status']=='Eliminated' and r['extracted_worth']==0 and r['extracted_gold']==0 and
                       r['monsters_slain']==0 and r['players_killed']==0 for r in report['ranking']), 'Old results leaked into restarted match'
        elif name == 'results':
            for proc in processes.values():
                ctypes.windll.user32.PostMessageW(menu_window(proc), 0x200, 0, 15 | (15 << 16))
            time.sleep(.3)
            if args.capture:
                destination = run/'dismissed'
                destination.mkdir()
                capture(processes, destination)
            for role, text in logs.items():
                assert '[SMOKE][DELVE] check FAIL' not in text, role
                for label in ('results_native_banner', 'results_clear_footer', 'results_row_fits',
                              'results_name_fits', 'results_stats_present', 'results_native_controls_preserved', 'results_native_dismiss'):
                    assert 'check PASS ' + label in text, (role, label)
        elif name == 'concealment':
            for role,text in logs.items():
                assert '[SMOKE][DELVE] check FAIL' not in text, role
                for label in ('invisibility_preserves_owner','invisibility_hides_remote_body','invisibility_hides_remote_equipment'):
                    assert 'check PASS '+label in text, (role,label)
            assert 'check PASS invisibility_restores_visibility_guest' in logs['client1']
            assert 'check PASS blessing_scroll_quarter_value' in logs['host']
        elif name == 'abilities':
            assert captured_views == {'0', '1', '2', '3', '4', '5', '6'}, 'Missing ability HUD/input phases'
            assert '[SMOKE][DELVE] check FAIL' not in logs['host'] + logs['client1']
            for label in ('legacy_hotkeys_migrated_host', 'hud_q_key_activates_host', 'hud_e_key_activates_host',
                          'hud_q_key_activates_guest_packet', 'hud_e_key_activates_guest_packet', 'hud_abilities_absent_from_inventory'):
                assert 'check PASS ' + label in logs['host'], label
            assert 'check PASS legacy_hotkeys_migrated_guest' in logs['client1']
        elif name == 'cooldowns':
            assert captured_views == {'0','1','2','3','4'}, 'Missing cooldown HUD phases'
        elif name in ('appraisal', 'appraisal-disabled'):
            for label in ('appraisal_created_before_pickup', 'appraisal_native_ground_state', 'appraisal_ground_preview',
                          'appraisal_properties_preserved', 'appraisal_ground_entity_update', 'appraisal_cache_contents'):
                assert 'check PASS ' + label in logs['host'], label
            for label in ('guest_appraisal_host_setting', 'guest_appraisal_ground_state', 'guest_appraisal_ground_preview'):
                assert 'check PASS ' + label in logs['client1'], label
            assert '[SMOKE][DELVE] check FAIL' not in logs['client1']
            assert 'check PASS hover_stats_host_complete' in logs['host']
            assert 'check PASS hover_stats_guest_complete' in logs['client1']
        elif name.startswith('quality'):
            assert '[SMOKE][DELVE] check FAIL' not in logs['host'], 'Gameplay check failed'
            assert logs['host'].count('[SMOKE][DELVE] check PASS') >= 12
        else:
            assert '[SMOKE][DELVE] check FAIL' not in logs['host'], 'Gameplay check failed'
            assert 'melee hp=0' in logs['host'], 'PvP melee did not kill'
            assert '[SMOKE][DELVE] check PASS rival_loot_recovered' in logs['host'], 'Killer did not recover loot'
            assert '[SMOKE][DELVE] check PASS death_inventory_drop' in logs['host'], 'Earned inventory did not drop on death'
            assert sum(r['players_killed'] for r in report['ranking']) == 1, 'PvP kill accounting failed'
        if args.tour_capture:
            if name.startswith('battle-'):
                assert 'jungle' in captured_views, 'Missing rendered jungle view'
            else:
                assert len(captured_views)==12, 'Missing rendered tour views'
        if args.capture and name != 'results': capture(processes, run)
        if args.builds and name=='progression':
            staged=re.findall(r'\[DELVE-BUILD\] applied slot=(\d+) class=(\d+) choices=0,1,6,7',logs['host'])
            assert len(staged)==peers and {int(row[0]) for row in staged}==set(range(peers)), ('Perks were missing or applied more than once',staged)
            result['builds_staged_once']=staged
        result.update(status='PASS', results=report, save_retry=retry_verified)
        if args.hold: time.sleep(args.hold)
    except Exception as exc:
        result.update(status='FAIL', error=str(exc))
        raise
    finally:
        (run/'result.json').write_text(json.dumps(result, indent=2)+'\n')
        for proc in processes.values():
            if proc.poll() is None: proc.terminate()
        for proc in processes.values():
            try: proc.wait(timeout=5)
            except subprocess.TimeoutExpired: proc.kill(); proc.wait(timeout=5)
        for stream in streams: stream.close()
    print('PASS '+name, flush=True)
    return dict(scenario=name, profile=str(run), status=result['status'])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--exe', type=Path, default=ROOT/'framework/runtime/barony-raid-smoke.exe')
    ap.add_argument('--output', type=Path, default=ROOT/'framework/profiles')
    ap.add_argument('--scenario', choices=['storm-wipe', 'shops', 'battle-royale', 'battle-wipe', 'battle-host-win', 'rarity', 'revenants', 'population', 'concealment', 'results', 'progression', 'host-early', 'faults', 'combat', 'quality', 'quality-unidentified', 'restart', 'cooldowns', 'abilities', 'appraisal', 'appraisal-disabled'], action='append')
    ap.add_argument('--builds',action='store_true',help='equip recommended class builds and verify one-time host staging across floors')
    ap.add_argument('--faults', action='store_true')
    for floor in (1,2,3):
        ap.add_argument('--floor'+str(floor)+'-seed', type=int, help='reproduce a map seed in the smoke runtime only')
    ap.add_argument('--players', type=int, choices=(1,2,4), help='storm-wipe only: exercise solo or matched Direct IP deaths')
    ap.add_argument('--capture', action='store_true')
    ap.add_argument('--size', choices=['1024x720', '1280x720', '1920x1080', '3440x1440'], default='1280x720', help='owned test window resolution; native minimum is 1024x720')
    ap.add_argument('--ui-scale', type=int, choices=[50, 75, 100], default=100, help='native UI scale for isolated HUD layout checks')
    ap.add_argument('--tour-capture', action='store_true', help='capture four authored views per floor during progression')
    ap.add_argument('--port', type=int, default=57465)
    ap.add_argument('--timeout', type=int, default=130)
    ap.add_argument('--hold', type=int, default=0, help='keep completed matches open briefly for capture')
    ap.add_argument('--classes',type=lambda text:list(map(int,text.split(','))),help='four selected class IDs for a mixed progression test')
    ap.add_argument('--races',type=lambda text:list(map(int,text.split(','))),help='four selected race IDs for a mixed progression test')
    args = ap.parse_args()
    if args.players is not None and args.scenario!=['storm-wipe']: ap.error('--players is only supported by --scenario storm-wipe')
    if args.classes and (len(args.classes)!=4 or any(c<0 or c>=26 for c in args.classes)):ap.error('--classes needs four IDs in 0..25')
    if args.races and (len(args.races)!=4 or any(r not in [0,1,2,3,4,5,6,7,8,13,14,15,16,17] for r in args.races)):ap.error('--races needs four selectable race IDs')
    names = args.scenario or (['battle-royale', 'battle-wipe', 'battle-host-win'] if args.faults else ['battle-royale'])
    results = [scenario(args, name) for name in names]
    print(json.dumps(results, indent=2), flush=True)


if __name__ == '__main__':
    main()
