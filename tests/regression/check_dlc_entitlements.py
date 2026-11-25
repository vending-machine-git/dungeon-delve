"""Compile the actual detector and native character validator against isolated API fakes.

No fake library or ownership override is installed into a game. Generated test code
runs only in a standalone Linux test process; native game tests may only hide DLC.
"""
from pathlib import Path
import json
import re
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[2]


def block(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def linux(path):
    path = path.resolve()
    return '/mnt/' + path.drive[0].lower() + path.as_posix()[2:]


def main():
    run = ROOT/'modding-evidence'/('dlc-policy-'+uuid.uuid4().hex[:10])
    run.mkdir()
    print('ARTIFACTS '+str(run), flush=True)
    init = (ROOT/'src/init_game.cpp').read_text()
    detector = init[init.index('namespace {\nDLC::Entitlements'):init.index('/*-------------------------------------------------------------------------------',init.index('void refreshInstalledSteamDLC'))]
    game = (ROOT/'src/game.hpp').read_text()
    menu = (ROOT/'src/menu.hpp').read_text()
    enums = '\n'.join(block(text, sig)+';' for text, sig in (
        (game,'enum PlayerClasses : int'), (game,'enum PlayerRaces : int'),
        (menu,'enum CharacterDLCValidation : int')))
    validator = block((ROOT/'src/menu.cpp').read_text(), 'int isCharacterValidFromDLC(Stat& myStats, int characterClass)')
    (run/'dlc_under_test.inc').write_text(enums+'\n'+detector+'\n'+validator+'\n')
    source = (ROOT/'tests/regression/dlc_entitlements_test.cpp').read_text()
    (run/'test.cpp').write_text(source)
    report = dict(status='RUNNING',scope='Isolated actual detector and native validator, with API fakes; no game ownership grants')
    try:
        built = subprocess.run(['wsl.exe','-d','Ubuntu-24.04','--exec','g++','-std=c++17','-Wall','-Wextra','-Werror',
            '-I'+linux(ROOT/'src'),linux(run/'test.cpp'),'-o',linux(run/'dlc-policy-test')],capture_output=True,text=True)
        (run/'build.log').write_text(built.stdout+built.stderr)
        assert built.returncode == 0, built.stderr
        tested = subprocess.run(['wsl.exe','-d','Ubuntu-24.04','--exec',linux(run/'dlc-policy-test')],capture_output=True,text=True)
        (run/'test.log').write_text(tested.stdout+tested.stderr)
        print(tested.stdout,flush=True)
        assert tested.returncode == 0, tested.stderr
        report.update(status='PASS',result=tested.stdout.strip())
    except BaseException as error:
        report.update(status='FAIL',error=repr(error))
        raise
    finally:
        (run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        print('REPORT '+str(run/'result.json'),flush=True)


if __name__ == '__main__':
    main()
