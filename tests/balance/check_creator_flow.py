"""Exercise native Delve creator controls through real mouse input in an isolated profile."""
from pathlib import Path
import os
import argparse,ctypes,json,re,subprocess,sys,time,uuid
ROOT=Path(__file__).resolve().parents[2]
UM=Path(os.environ.get('UNIVERSAL_MODDER', str(Path.home() / 'Documents/Projects/[Tools]/universal-modder/bin/um.cmd')))
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--session',action='store_true');args=ap.parse_args()
    if not args.session:
        subprocess.run([sys.executable,str(ROOT/'tests/balance/check_build_creator.py'),'--class-id','0'],cwd=ROOT,check=True)
    session=json.loads((ROOT/'modding-evidence/build-creator-session.json').read_text())
    profile=Path(session['profile']); report=dict(status='RUNNING',profile=str(profile),checks=[],screenshots=[])
    def drive(*commands):
        subprocess.run([str(UM),'win','drive','--proc','barony-raid-smoke','focus',*commands],check=True,capture_output=True,text=True)
    def click(x,y):drive(f'click {x} {y}','sleep 180')
    def shot(name):
        path=profile/(name+'.png')
        subprocess.run([str(UM),'win','shot','--hwnd',str(session['hwnd']),str(path)],check=True,capture_output=True)
        report['screenshots'].append(str(path))
    def log():return (profile/'log.txt').read_text(errors='replace')
    def check(ok,name):
        assert ok,name
        report['checks'].append(name);print('PASS',name,flush=True)
    def build(c):return json.loads((profile/'delve-builds.json').read_text())['classes'][c]
    def card():click(38,80)
    def picker():click(163,566)
    def builds():click(163,618)
    try:
        shot('grouped-empty')
        click(84,664);check(build(0)==[0,1,6,7],'Recommended equips two skills and two abilities')
        click(229,164);click(160,244);check(build(0)==[0,2,6,7],'Passive skill replaces only selected passive slot')
        click(229,385);click(160,490);check(build(0)==[0,2,6,9],'Active ability replaces only selected active slot')
        shot('grouped-equipped')
        card();shot('prominent-button');picker();shot('class-picker')
        click(230,482);click(160,674)
        check('class confirmed slot=0 class=7' in log(),'Clickable class name and Choose Class commit Wizard')
        builds();check('creator opened slot=0 class=7' in log(),'Build panel follows confirmed Wizard class')
        click(84,664);check(build(7)==[0,1,6,7] and build(0)==[0,2,6,9],'Per-class builds stay independent')
        shot('wizard-build')
        card();picker();click(68,428)
        # Toggle class details with class zero selected, then return to the grid.
        click(128,615);shot('class-zero-details');click(128,615);click(160,674)
        check('class confirmed slot=0 class=0' in log(),'Class zero survives details/grid round trip and confirmation')
        builds();check(build(0)==[0,2,6,9],'Returning to Barbarian retains custom build')
        card();picker();click(230,482);click(38,298)
        check('class cancelled slot=0 restored=0' in log(),'Back cancels class preview and restores committed class')
        builds();check(log().rfind('creator opened slot=0 class=0')>log().rfind('class cancelled'),'Cancelled preview does not change build class')
        card();picker();click(258,615);click(128,615)
        matches=re.findall(r'class picker opened slot=0 class=(\d+) details=1',log());assert matches
        random_class=int(matches[-1]);click(128,615);click(160,674)
        confirmed=re.findall(r'class confirmed slot=0 class=(\d+)',log())
        check(int(confirmed[-1])==random_class,'Randomized class remains selected through details and confirmation')
        shot('random-class-confirmed')
        report['status']='PASS'
    except BaseException as exc:
        report['status']='FAIL';report['error']=repr(exc)
        shot('failure');raise
    finally:
        out=ROOT/'modding-evidence/delve-creator-flow.json';out.write_text(json.dumps(report,indent=2)+'\n')
        handle=ctypes.windll.kernel32.OpenProcess(1,False,session['pid'])
        if handle:ctypes.windll.kernel32.TerminateProcess(handle,0);ctypes.windll.kernel32.CloseHandle(handle)
        print('REPORT',out,flush=True)
if __name__=='__main__':main()
