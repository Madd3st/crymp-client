"""Compile and execute REAL small Brain methods in a mocked world, not copied decision logic."""
from pathlib import Path
import argparse
import subprocess

def main():
    ROOT = Path(__file__).resolve().parents[1]
    ap = argparse.ArgumentParser()
    ap.add_argument('--source-file', type=Path, default=ROOT/'Code/CryGame/Bots/BotBrain.cpp')
    ap.add_argument('--out', type=Path, default=ROOT/'units/weapon-goal-caller')
    args = ap.parse_args()
    source = args.source_file.read_text(encoding='utf-8-sig')
    out = args.out.resolve()
    out.relative_to(ROOT)  # Never write generated files outside the explicit repository workspace.
    out.mkdir(parents=True, exist_ok=True)

    def method(signature):
        start = source.index(signature)
        left = source.index('{', start)
        depth = 0
        for i in range(left, len(source)):
            if source[i] == '{':
                depth += 1
            elif source[i] == '}':
                depth -= 1
                if not depth:
                    return source[start:i+1]
        raise ValueError('unclosed method: '+signature)

    cpp = r'''
    #include "CryGame/Bots/BotWeaponGoal.h"
    #include <cstdio>
    #include <cstdint>
    #define WB_LOG(...) ((void)0)
    using EntityId=uint32_t;
    struct CPlayer {};
    struct BotConfig {int sniperReach=0,firstPrimary=1;static BotConfig& Get(){static BotConfig c;return c;}};
    struct BotWeapons {static bool HoldsSniper(CPlayer&){return false;}};
    class BotBrain {
    public:
     EntityId m_upgradeTarget=0,m_upgradeTracked=0,candidate=77;
     float m_upgradeBest=1e9f,m_upgradeBestAt=0,m_upgradePathSig=0,m_upgradeSince=0,m_now=1,m_nextUpgradeSearch=0;
     int m_number=1,scans=0;
     float PickupRange(){return 20;}
     EntityId UpgradeCandidate(CPlayer&,bool,bool,bool){++scans;return candidate;}
     void SetUpgradeTarget(EntityId);
     bool TryAcquireUpgradeGoal(CPlayer&);
    };
    ''' + method('void BotBrain::SetUpgradeTarget(') + '\n' + method('bool BotBrain::TryAcquireUpgradeGoal(') + r'''
    int main(){
     CPlayer actor;BotBrain b;
     if(!b.TryAcquireUpgradeGoal(actor)||b.scans!=1||b.m_upgradeTarget!=77)return 1;
     b.m_now=2;b.candidate=88;
     if(b.TryAcquireUpgradeGoal(actor)||b.scans!=1)return 2;
     b.m_now=4;
     if(!b.TryAcquireUpgradeGoal(actor)||b.scans!=2||b.m_upgradeTarget!=88){
      std::puts("FAIL: unchosen candidate did not refresh after the search throttle");return 7;
     }
     b.m_upgradeSince=4;b.m_now=8;b.candidate=99;
     if(b.TryAcquireUpgradeGoal(actor)||b.scans!=2||b.m_upgradeTarget!=88)return 3;
     b.SetUpgradeTarget(0);b.m_now=9;b.candidate=0;
     if(b.TryAcquireUpgradeGoal(actor)||b.scans!=3||b.m_upgradeTarget!=0)return 4;
     b.m_now=10;b.candidate=100;
     if(b.TryAcquireUpgradeGoal(actor)||b.scans!=3)return 5;
     b.m_now=12;
     if(!b.TryAcquireUpgradeGoal(actor)||b.scans!=4||b.m_upgradeTarget!=100)return 6;
     std::puts("PASS real Brain callers: candidate refresh, pursuit hold, empty-search throttle");return 0;
    }
    '''
    unit = out/'caller.cpp'
    exe = out/'caller.exe'
    unit.write_text(cpp, encoding='utf-8')
    compiled = subprocess.run(['cl', '/nologo', '/std:c++17', '/EHsc', '/I', str(ROOT/'Code'),
                               str(unit), '/Fo:'+str(out)+'/', '/Fe:'+str(exe)], capture_output=True)
    print((compiled.stdout+compiled.stderr).decode(errors='replace'))
    if compiled.returncode:
        raise SystemExit(compiled.returncode)
    result = subprocess.run([str(exe)], capture_output=True)
    print((result.stdout+result.stderr).decode(errors='replace'))
    raise SystemExit(result.returncode)

if __name__ == '__main__':
    main()
