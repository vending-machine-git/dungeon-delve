"""Steam invite + relay transport for a normal two-player campaign.

Two isolated smoke clients load the local Steam stand-in (steam_fixture.py), so
nothing touches the owner's real Steam account. The host's own invite control
opens the (mock) overlay invite dialog; the guest accepts through the ordinary
Steam callback path and plays a normal campaign with every packet carried by the
Steam relay layer (no Direct IP address is ever given to the guest).

Scenarios:
  --method invite|joingame|code|argument   how the guest receives the invite
  --no-overlay      host overlay unavailable: invite code fallback
  --drop N          drop every Nth unreliable relay packet
  --retail-lobby    invite to a lobby without framework data is refused cleanly
  --mode-mismatch   a Join In Progress guest is refused by a normal host lobby
"""
from pathlib import Path
import argparse, json, os, subprocess, time, uuid
from steam_fixture import SteamFixture

ROOT = Path(__file__).resolve().parents[2]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', type=Path, default=ROOT/'framework/runtime/barony-raid-smoke.exe')
    ap.add_argument('--data', type=Path, default=Path(r'F:\Steam\steamapps\common\Barony'))
    ap.add_argument('--method', choices=['invite', 'joingame', 'code', 'argument'], default='invite')
    ap.add_argument('--no-overlay', action='store_true')
    ap.add_argument('--drop', type=int, default=0)
    ap.add_argument('--retail-lobby', action='store_true')
    ap.add_argument('--mode-mismatch', action='store_true')
    ap.add_argument('--timeout', type=int, default=90)
    args = ap.parse_args()
    run = ROOT/'framework/profiles'/('steam-invite-' + uuid.uuid4().hex[:10])
    run.mkdir(parents=True)
    print('ARTIFACTS ' + str(run), flush=True)
    method = 'manual' if args.method == 'argument' or args.retail_lobby else args.method
    fixture = SteamFixture(run, method=method, click=False if args.retail_lobby else 'card', overlay=not args.no_overlay, drop=args.drop)
    players, streams = {}, []

    def start(role, extra=()):
        folder = run/role
        (folder/'config').mkdir(parents=True)
        (folder/'config/config.json').write_text(json.dumps(dict(skipintro=True, mods=[], port_number=57168)))
        env = os.environ.copy()
        env['PATH'] = str(args.data) + os.pathsep + env.get('PATH', '')
        env.update(BARONY_SMOKE_AUTOPILOT='1', BARONY_SMOKE_ROLE='host' if role == 'host' else 'client',
                   BARONY_BALANCE_NORMAL_SCOPE='1', BARONY_SMOKE_PLAYERS='2',
                   BARONY_SMOKE_CLASS='0' if role == 'host' else '1', BARONY_SMOKE_RACE='0')
        env.update(fixture.env(role, folder, host=role == 'host'))
        cmd = [str(args.exe), '-windowed', '-size=960x540', '-nosound', '-datadir=' + str(args.data), *extra]
        stream = (folder/'stdout.log').open('wb')
        streams.append(stream)
        p = subprocess.Popen(cmd, cwd=folder, env=env, stdout=stream, stderr=subprocess.STDOUT,
                             creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        players[role] = (folder, p)
        print(role + ' pid=' + str(p.pid), flush=True)

    def log(role):
        path = players[role][0]/'log.txt'
        return path.read_text(errors='replace') if path.exists() else ''

    def report(role):
        path = players[role][0]/'steam-report.json'
        try:
            return json.loads(path.read_text())
        except (OSError, ValueError):
            return None

    def wait(condition, what):
        deadline = time.monotonic() + args.timeout
        while True:
            fixture.check()
            for role, (_, p) in players.items():
                if p.poll() is not None:
                    raise RuntimeError(role + ' exited: ' + str(p.returncode))
            value = condition()
            if value:
                return value
            if time.monotonic() > deadline:
                raise RuntimeError('Timed out waiting for ' + what + '; see ' + str(run))
            time.sleep(.2)

    result = dict(status='FAIL', scenario=vars(args) | dict(exe=str(args.exe), data=str(args.data)))
    try:
        start('host')
        wait(lambda: '[STEAM-INVITE] lobby ready id=' in log('host'), 'host Steam lobby')
        if args.retail_lobby:
            # A lobby with retail Barony's keys but no framework data.
            lobby = 109775249999999999
            folder = fixture.dir/('lobby-' + str(lobby))
            folder.mkdir()
            (folder/'data.txt').write_bytes(b'__owner=76561190000000999\n__max=4\n__type=1\n__joinable=1\nver=v5.0.2\nname=retail\n')
            start('client')
            wait(lambda: '[SMOKE][STEAM] waiting for invite' in log('client'), 'guest at menu')
            fixture.deliver('client', lobby)
            wait(lambda: '[STEAM-INVITE] join failed: This invite is for unmodded Barony' in log('client'), 'retail refusal')
            time.sleep(2)
            assert '[STEAM-INVITE] relaying to host' not in log('client'), 'Guest connected to a retail lobby'
            result.update(status='PASS', retail_lobby_refused=True)
            print('PASS: retail lobby invite refused before any connection.', flush=True)
            return
        if args.no_overlay:
            wait(lambda: '[STEAM-INVITE] overlay unavailable; copied invite code' in log('host'), 'invite code fallback')
        else:
            wait(lambda: '[STEAM-INVITE] opened overlay invite dialog' in log('host'), 'invite button')
        assert '[SMOKE][STEAM] pressing invite card slot=1' in log('host'), 'Lobby card invite button was not used'
        lobby = fixture.wait_lobby()
        extra = ['+connect_lobby', str(lobby)] if args.method == 'argument' else []
        if args.mode_mismatch:
            extra.append('-join-in-progress')
        start('client', extra)
        if args.mode_mismatch:
            wait(lambda: '[STEAM-INVITE] join failed: The host is playing a normal campaign' in log('client'), 'mode refusal')
            time.sleep(2)
            h = report('host')
            assert not h or not h['connected'][1], 'Mismatched guest reached the game'
            assert '[STEAM-INVITE] relaying to host' not in log('client'), 'Mismatched guest connected'
            result.update(status='PASS', mode_mismatch_refused=True)
            print('PASS: mismatched mode refused before connecting.', flush=True)
            return
        both = wait(lambda: (lambda h, c: (h, c) if h and c and all(h['bodies'][:2]) and all(c['bodies'][:2]) else None)
                    (report('host'), report('client')), 'both peers in game over relay')
        host, guest = both
        # Let gameplay traffic flow for a few seconds, then confirm it still agrees.
        time.sleep(5)
        host, guest = report('host'), report('client')
        assert host['clientnum'] == 0 and guest['clientnum'] == 1, 'Unexpected player slots'
        assert host['key'] == guest['key'] and host['level'] == guest['level'], 'Peers disagree on the game'
        assert host['bodies'][:2] == guest['bodies'][:2], 'Peers disagree on player entities'
        assert guest['virtual_server'] and not host['virtual_server'], 'Guest did not reach the host through Steam'
        assert host['steam_peers'] >= 1 and guest['steam_peers'] >= 1
        assert host['lobby_closed'], 'Normal campaign lobby stayed open after gameplay began'
        assert 'joinable lobby=%d value=0' % lobby in fixture.mock_log('host'), 'Lobby not closed in Steam'
        hostlog, guestlog = log('host'), log('client')
        assert '[STEAM-INVITE] accepted relay session ' + str(fixture.user('client')) in hostlog
        source = {'invite': 'invite', 'joingame': 'join-game', 'code': 'code', 'argument': 'command-line'}[args.method]
        assert '[STEAM-INVITE] join requested lobby=%d source=%s' % (lobby, source) in guestlog, 'Wrong invite path'
        result.update(status='PASS', lobby=str(lobby), host=host, guest=guest, steam=fixture.summary())
        print('PASS: %s joined a normal campaign over the Steam relay layer.' % args.method, flush=True)
    except Exception as exc:
        result['error'] = str(exc)
        raise
    finally:
        (run/'result.json').write_text(json.dumps(result, indent=2) + '\n')
        fixture.close()
        for _, p in players.values():
            if p.poll() is None:
                p.terminate()
        for _, p in players.values():
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait(timeout=5)
        for stream in streams:
            stream.close()


if __name__ == '__main__':
    main()
