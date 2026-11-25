"""Local Steam stand-in for multi-instance invite/relay smoke tests.

Each game process loads tests/smoke/steam_mock (built to framework/runtime/steam-mock)
instead of the real steam_api64.dll. A background thread plays the part of the
Steam client: once the host's lobby exists (and, with click=True, once the host's
own invite control has opened the overlay invite dialog) it delivers the invite to
each guest that is waiting at the main menu.

Methods: invite (overlay invite accepted), joingame (friends list Join Game via
rich presence), code (pasted steam:<lobby> invite code), manual (the
test delivers the invite itself, e.g. a +connect_lobby launch argument).
"""
from pathlib import Path
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT/'tests/smoke/steam_mock/steam_api_mock.cpp'
DLL = ROOT/'framework/runtime/steam-mock/steam_api64.dll'
BASE_ID = 76561190000000000
METHODS = ('invite', 'joingame', 'code', 'manual')


def linux(path):
    p = Path(path).resolve()
    return '/mnt/' + p.drive[0].lower() + p.as_posix()[2:]


def build():
    if DLL.is_file() and DLL.stat().st_mtime >= SOURCE.stat().st_mtime:
        return DLL
    DLL.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['wsl.exe', '-d', 'Ubuntu-24.04', '--exec', 'x86_64-w64-mingw32-g++', '-std=c++17', '-shared',
                    '-O2', '-Wall', '-static-libgcc', '-static-libstdc++', '-static', '-o', linux(DLL),
                    linux(SOURCE), '-lws2_32'], check=True)
    return DLL


class SteamFixture:
    def __init__(self, run, method='invite', click=True, overlay=True, drop=0):
        assert method in METHODS
        build()
        self.dir = Path(run)/'steam-mock'
        self.dir.mkdir(parents=True, exist_ok=True)
        self.method, self.click, self.overlay, self.drop = method, click, overlay, drop
        self.ids, self.logs, self.invited = {}, {}, {}
        self.deliveries = []
        self.error = None
        self.lock = threading.Lock()
        self.stopping = False
        self.thread = threading.Thread(target=self._loop, daemon=True)
        self.thread.start()

    def user(self, role):
        if role not in self.ids:
            self.ids[role] = BASE_ID + 1 + len(self.ids)
        return self.ids[role]

    def env(self, role, folder, host=False):
        """Environment for one process. Call again when a role relaunches."""
        uid = self.user(role)
        env = dict(BARONY_SMOKE_STEAM_API=str(DLL), MOCK_STEAM_ID=str(uid), MOCK_STEAM_DIR=str(self.dir),
                   MOCK_STEAM_OVERLAY='1' if self.overlay else '0', MOCK_STEAM_DROP=str(self.drop),
                   BARONY_SMOKE_STEAM_REPORT=str(Path(folder)/'steam-report.json'))
        if host:
            self.host = role
            if self.click:
                # 'card' presses the lobby card's invite button; 'game' runs /invite in play.
                env['BARONY_SMOKE_STEAM_INVITE_CLICK'] = self.click if self.click in ('card', 'game') else '1'
        else:
            if self.method == 'code':
                env['BARONY_SMOKE_CONNECT_ADDRESS'] = 'steam:' + str(self.wait_lobby())
            else:
                env['BARONY_SMOKE_CONNECT_ADDRESS'] = 'steam-invite'
            with self.lock:
                self.logs[role] = Path(folder)/'log.txt'
                self.invited[role] = False
        return env

    def mock_log(self, role):
        path = self.dir/('mock-' + str(self.user(role)) + '.log')
        return path.read_text(errors='replace') if path.exists() else ''

    def lobby(self):
        """The host's current open lobby id, if any."""
        if not hasattr(self, 'host'):
            return None
        host = str(self.user(self.host))
        if self.click and self.overlay:
            path = self.dir/('overlay-invite-' + host + '.txt')
            return int(path.read_text()) if path.exists() else None
        best = None
        for folder in self.dir.glob('lobby-*'):
            data = (folder/'data.txt').read_text(errors='replace') if (folder/'data.txt').exists() else ''
            if '__owner=' + host + '\n' in data and '__closed=1' not in data:
                lobby = int(folder.name[6:])
                best = lobby if best is None or lobby > best else best
        return best

    def wait_lobby(self, timeout=60):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            lobby = self.lobby()
            if lobby:
                return lobby
            time.sleep(.2)
        raise RuntimeError('Host never opened a Steam lobby invite')

    def deliver(self, role, lobby, method='invite'):
        """Atomically hand a guest an accepted invite or a Join Game request."""
        uid = str(self.user(role))
        name, text = (('invite-', str(lobby)) if method == 'invite' else ('joingame-', '+connect_lobby ' + str(lobby)))
        temporary = self.dir/(name + uid + '.partial')
        temporary.write_bytes(text.encode())
        temporary.replace(self.dir/(name + uid + '.txt'))

    def _loop(self):
        while not self.stopping:
            try:
                lobby = self.lobby()
                with self.lock:
                    pending = [role for role, done in self.invited.items() if not done]
                for role in pending:
                    if not lobby or self.method in ('code', 'manual'):
                        continue
                    log = self.logs[role]
                    text = log.read_text(errors='replace') if log.exists() else ''
                    # Deliver once per waiting state, including after a relaunch.
                    waiting = text.rfind('[SMOKE][STEAM] waiting for invite')
                    if waiting < 0 or '[STEAM-INVITE] ready' not in text or text.rfind('[STEAM-INVITE] join requested') > waiting:
                        continue
                    self.deliver(role, lobby, self.method)
                    with self.lock:
                        self.invited[role] = True
                    self.deliveries.append(dict(role=role, lobby=lobby, method=self.method))
            except Exception as exc:  # surfaced by check()
                self.error = exc
            time.sleep(.2)

    def check(self):
        if self.error:
            raise RuntimeError('Steam fixture failed: ' + str(self.error))

    def close(self):
        self.stopping = True
        self.thread.join(timeout=2)

    def summary(self):
        return dict(method=self.method, click=self.click, overlay=self.overlay, drop=self.drop,
                    users={role: str(uid) for role, uid in self.ids.items()}, deliveries=self.deliveries)
