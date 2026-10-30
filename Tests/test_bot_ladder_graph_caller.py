"""Actual native-animation prefix and graph preparation/restoration, without engine linking."""
from pathlib import Path
import argparse,subprocess


def method(source,signature):
    start=source.index(signature);left=source.index('{',start);depth=0
    for i in range(left,len(source)):
        if source[i]=='{':depth+=1
        elif source[i]=='}':
            depth-=1
            if not depth:return source[start:i+1]
    raise ValueError(signature)


def main():
    root=Path(__file__).resolve().parents[1]
    ap=argparse.ArgumentParser();ap.add_argument('--revision');ap.add_argument('--out',type=Path,default=root/'units/ladder-graph-caller');args=ap.parse_args()
    path='Code/CryGame/Actors/Player/Player.cpp'
    source=(subprocess.check_output(['git','-C',str(root),'show',f'{args.revision}:{path}']).decode('utf-8-sig')
            if args.revision else (root/path).read_text(encoding='utf-8-sig'))
    native=method(source,'bool CPlayer::UpdateLadderAnimation(')
    prefix=native[native.index('{')+1:native.index('\tswitch (eLS)')]
    if 'bool CPlayer::PrepareWarsBotLadderGraph(' in source:
        prepare=method(source,'bool CPlayer::PrepareWarsBotLadderGraph(')
        restore=method(source,'void CPlayer::RestoreWarsBotLadderGraph(')
    else:
        # Old native prefix contains no call. These declarations only make the same mock world compile;
        # the RED control executes the real old prefix, which never prepares the graph.
        prepare='bool CPlayer::PrepareWarsBotLadderGraph(){return false;}'
        restore='void CPlayer::RestoreWarsBotLadderGraph(){}'
    cpp=r'''
#define WARSBOTS_EXPERIMENTAL_SPAWNER 1
#define WB_LOG(...) ((void)0)
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdint>
using string=std::string;using uint8=uint8_t;
struct Env{bool bServer=true;}env,*gEnv=&env;
namespace WarsBots{struct BotConfig{int ladder=1;static BotConfig&Get(){static BotConfig c;return c;}};}
struct Manager{bool owns=true;bool Owns(unsigned){return owns;}};
struct Game{Manager mgr;Manager&GetBotManager(){return mgr;}}game,*g_pGame=&game;
struct IAnimationGraphState{
 using InputID=uint8;string value="combat";bool active=false,def=false,valid=true,reject=false;int writes=0,clears=0;
 InputID GetInputId(const char*){return 3;}const char*GetInputName(InputID){return valid?"Stance":nullptr;}
 void GetInput(InputID,char*buf){std::strcpy(buf,value.c_str());}
 bool IsDefaultInputValue(InputID){return def;}bool GetAnimationActivation(){return active;}
 void SetAnimationActivation(bool v){active=v;++writes;}
 bool SetInput(InputID,const char*v){++writes;if(reject)return false;value=v;def=false;return true;}
 void ClearInput(InputID){++writes;++clears;value="combat";def=true;}
};
struct IAnimatedCharacter{IAnimationGraphState*graph=nullptr;IAnimationGraphState*GetAnimationGraphState(){return graph;}};
struct Flag{bool v=true;bool Value(){return v;}};
struct CPlayer{
 enum{eLS_Climb=1,eLS_ExitTop=2,eLS_Exit=3};bool bot=true,frozen=false;int health=100,physicalStance=0;
 struct{Flag isOnLadder;bool isRagDoll=false;}m_stats;
 IAnimationGraphState graph;IAnimatedCharacter ac;IAnimatedCharacter*m_pAnimatedCharacter=&ac;
 IAnimationGraphState*m_warsBotLadderGraph=nullptr;
 bool m_warsBotLadderGraphOverride=false,m_warsBotLadderGraphWasActive=false,m_warsBotLadderGraphStanceDefault=true;
 uint8 m_warsBotLadderGraphStanceId=0;string m_warsBotLadderGraphStance;
 CPlayer(){ac.graph=&graph;}
 bool IsWarsBotActor(){return bot;}unsigned GetEntityId(){return 7;}int GetHealth(){return health;}bool IsFrozen(){return frozen;}
 void ResetWarsBotLadderExitRetry(){} void ClearWarsBotLadderExitRetry(){} // isolated graph snapshot test; actual retry lifecycle has its own caller
 void SetWarsBotLadderExitPhysics(bool,bool){} bool PrepareWarsBotLadderGraph();void RestoreWarsBotLadderGraph();void NativePrefix(int);
};
''' + prepare + '\n' + restore + '\nvoid CPlayer::NativePrefix(int eLS){\n'+prefix+'\n}\n'+r'''
int main(){
 {CPlayer p;p.NativePrefix(CPlayer::eLS_Climb);
  if(!p.graph.active||p.graph.value!="relaxed"||!p.m_warsBotLadderGraphOverride){std::puts("FAIL native prefix does not prepare relaxed active graph");return 1;}
  if(p.physicalStance!=0)return 2;
  p.NativePrefix(CPlayer::eLS_ExitTop);WarsBots::BotConfig::Get().ladder=0;p.RestoreWarsBotLadderGraph();
  if(p.graph.active||p.graph.value!="combat"||p.m_warsBotLadderGraphOverride)return 3;
  int n=p.graph.writes;p.RestoreWarsBotLadderGraph();if(p.graph.writes!=n)return 4;WarsBots::BotConfig::Get().ladder=1;
 }
 {CPlayer p;p.graph.active=true;p.graph.value="crouch";p.NativePrefix(CPlayer::eLS_Climb);p.RestoreWarsBotLadderGraph();if(!p.graph.active||p.graph.value!="crouch")return 5;}
 {CPlayer p;p.graph.def=true;p.NativePrefix(CPlayer::eLS_Climb);p.RestoreWarsBotLadderGraph();if(p.graph.clears!=1||!p.graph.def)return 6;}
 {CPlayer p;p.graph.valid=false;p.NativePrefix(CPlayer::eLS_Climb);if(p.graph.writes||p.m_warsBotLadderGraphOverride)return 7;}
 {CPlayer p;p.graph.reject=true;p.NativePrefix(CPlayer::eLS_Climb);if(p.graph.active||p.graph.value!="combat"||p.m_warsBotLadderGraphOverride)return 8;}
 {CPlayer p;p.NativePrefix(CPlayer::eLS_Climb);IAnimationGraphState replacement;p.ac.graph=&replacement;p.RestoreWarsBotLadderGraph();if(replacement.writes||p.m_warsBotLadderGraphOverride)return 9;}
 {CPlayer p;p.NativePrefix(CPlayer::eLS_Climb);game.mgr.owns=false;int n=p.graph.writes;p.RestoreWarsBotLadderGraph();if(p.graph.writes!=n||p.m_warsBotLadderGraphOverride)return 10;game.mgr.owns=true;}
 {CPlayer p;p.NativePrefix(CPlayer::eLS_Exit);if(p.graph.writes)return 11;}
 for(int k=0;k<8;++k){CPlayer p;
  if(k==0)p.bot=false;if(k==1)env.bServer=false;if(k==2)game.mgr.owns=false;
  if(k==3)WarsBots::BotConfig::Get().ladder=0;if(k==4)p.health=0;if(k==5)p.frozen=true;
  if(k==6)p.m_stats.isRagDoll=true;if(k==7)p.m_stats.isOnLadder.v=false;
  p.NativePrefix(CPlayer::eLS_Climb);if(p.graph.writes||p.m_warsBotLadderGraphOverride)return 12+k;
  env.bServer=true;game.mgr.owns=true;WarsBots::BotConfig::Get().ladder=1;
 }
 {CPlayer p;p.ac.graph=nullptr;p.NativePrefix(CPlayer::eLS_Climb);if(p.m_warsBotLadderGraphOverride)return 20;}
 std::puts("Actual native graph prefix, scope and restoration controls PASS");return 0;
}
'''
    out=args.out.resolve();out.mkdir(parents=True,exist_ok=True);unit=out/'graph.cpp';unit.write_bytes(cpp.encode('utf-8'))
    comp=subprocess.run(['cl','/nologo','/std:c++17','/EHsc',str(unit),'/Fo:'+str(out)+'/', '/Fe:'+str(out/'graph.exe')],capture_output=True)
    (out/'compile.log').write_bytes(comp.stdout+comp.stderr)
    if comp.returncode:raise RuntimeError((comp.stdout+comp.stderr).decode(errors='replace'))
    res=subprocess.run([str(out/'graph.exe')],capture_output=True);(out/'run.log').write_bytes(res.stdout+res.stderr);print(res.stdout.decode());return res.returncode


if __name__=='__main__':
    raise SystemExit(main())
