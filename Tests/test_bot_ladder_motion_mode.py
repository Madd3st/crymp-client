"""Execute the real PrePhysicsUpdate bot movement-mode override with a small mock actor."""
from pathlib import Path
import argparse
import subprocess


def main():
    root = Path(__file__).resolve().parents[1]
    ap = argparse.ArgumentParser()
    ap.add_argument('--revision', help='optional Git reference for RED controls')
    ap.add_argument('--out', type=Path, default=root / 'units/ladder-motion-mode')
    args = ap.parse_args()
    path = 'Code/CryGame/Actors/Player/Player.cpp'
    source = (subprocess.check_output(['git', '-C', str(root), 'show', f'{args.revision}:{path}']).decode('utf-8-sig')
              if args.revision else (root / path).read_text(encoding='utf-8-sig'))
    start = source.index('params.flags |= eACF_PerAnimGraph;', source.index('void CPlayer::PrePhysicsUpdate'))
    start = source.index('#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)', start)
    end = source.index('#endif', start)
    actual = source[source.index('\n', start):end]
    cpp = r'''
#include <cstdio>
enum {eACF_PerAnimGraph=4, eACF_AlwaysPhysics=2};
struct Coherent { bool v=true; bool Value() const{return v;} };
struct Env {bool bServer=true;} env,*gEnv=&env;
struct Manager {bool owns=true;bool Owns(unsigned){return owns;}};
struct Game {Manager mgr;Manager& GetBotManager(){return mgr;}} game,*g_pGame=&game;
namespace WarsBots {struct BotConfig {int ladder=1;static BotConfig& Get(){static BotConfig c;return c;}};}
struct Player {
 bool m_warsBotPose=true,bot=true,missing=false;
 bool NeedsWarsBotLadderPhysicsExit(){return missing;}
 struct Stats {Coherent isOnLadder;bool isExitingLadder=true;} m_stats;
 bool IsWarsBotActor(){return bot;} unsigned GetEntityId(){return 7;}
 int Mode(){struct {int flags=eACF_PerAnimGraph;} params;
''' + actual + r'''
 return params.flags;}
};
int main(){
 Player p;
 if(p.Mode()!=eACF_PerAnimGraph){std::puts("FAIL: owned native exit lost graph motion");return 1;}
 p.missing=true;if(p.Mode()!=eACF_AlwaysPhysics)return 10;p.missing=false;
 p.m_stats.isExitingLadder=false;if(p.Mode()!=eACF_AlwaysPhysics)return 2;
 p.m_stats.isExitingLadder=true;p.m_stats.isOnLadder.v=false;if(p.Mode()!=eACF_AlwaysPhysics)return 3;
 p.m_stats.isOnLadder.v=true;WarsBots::BotConfig::Get().ladder=0;if(p.Mode()!=eACF_AlwaysPhysics)return 4;
 WarsBots::BotConfig::Get().ladder=1;game.mgr.owns=false;if(p.Mode()!=eACF_AlwaysPhysics)return 5;
 game.mgr.owns=true;p.bot=false;if(p.Mode()!=eACF_AlwaysPhysics)return 6;
 p.bot=true;env.bServer=false;if(p.Mode()!=eACF_AlwaysPhysics)return 7;
 env.bServer=true;g_pGame=nullptr;if(p.Mode()!=eACF_AlwaysPhysics)return 8;
 g_pGame=&game;p.m_warsBotPose=false;if(p.Mode()!=eACF_PerAnimGraph)return 9;
 std::puts("Real ladder exit movement mode PASS");return 0;
}
'''
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    unit, exe = out / 'mode.cpp', out / 'mode.exe'
    unit.write_text(cpp, encoding='utf-8')
    comp = subprocess.run(['cl', '/nologo', '/std:c++17', '/EHsc', str(unit), '/Fo:' + str(out) + '/', '/Fe:' + str(exe)], capture_output=True)
    (out / 'compile.log').write_bytes(comp.stdout + comp.stderr)
    if comp.returncode:
        print((comp.stdout + comp.stderr).decode(errors='replace'))
        raise RuntimeError('Compile failure is not semantic RED')
    result = subprocess.run([str(exe)], capture_output=True)
    (out / 'run.log').write_bytes(result.stdout + result.stderr)
    print(result.stdout.decode(errors='replace'))
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
