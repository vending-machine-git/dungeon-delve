"""Independent genuine ownership oracle used by native regression runners."""
from pathlib import Path
import ctypes
import os

GAME = Path(r'F:\Steam\steamapps\common\Barony')


def installed_mask():
    previous = os.environ.get('SteamAppId')
    os.environ['SteamAppId'] = '371970'
    api = ctypes.CDLL(str(GAME/'steam_api64.dll'))
    api.SteamAPI_InitSafe.restype = ctypes.c_bool
    api.SteamAPI_SteamApps_v008.restype = ctypes.c_void_p
    api.SteamAPI_ISteamApps_BIsDlcInstalled.argtypes = [ctypes.c_void_p,ctypes.c_uint32]
    api.SteamAPI_ISteamApps_BIsDlcInstalled.restype = ctypes.c_bool
    initialized = False
    try:
        initialized = api.SteamAPI_InitSafe()
        if not initialized:
            raise RuntimeError('Independent ownership oracle requires a running, signed-in Steam client')
        apps = api.SteamAPI_SteamApps_v008()
        if not apps:
            raise RuntimeError('Real Steam Apps interface unavailable')
        return sum(int(api.SteamAPI_ISteamApps_BIsDlcInstalled(apps,1010820+i))<<i for i in range(3))
    finally:
        if initialized:
            api.SteamAPI_Shutdown()
        if previous is None:
            os.environ.pop('SteamAppId',None)
        else:
            os.environ['SteamAppId']=previous
