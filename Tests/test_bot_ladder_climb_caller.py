"""Run the actual native climb tail: physics submission versus manual-animation readiness."""
from pathlib import Path
import argparse, subprocess


def main():
    root=Path(__file__).resolve().parents[1]
    ap=argparse.ArgumentParser();ap.add_argument('--revision');ap.add_argument('--out',type=Path,default=root/'units/ladder-climb-caller')
    args=ap.parse_args();path='Code/CryGame/Actors/Player/PlayerMovement.cpp'
    source=(subprocess.check_output(['git','-C',str(root),'show',f'{args.revision}:{path}']).decode('utf-8-sig')
            if args.revision else (root/path).read_text(encoding='utf-8-sig'))
    begin=source.index('void CPlayerMovement::ProcessMovementOnLadder(')
    axis=source[source.index('\tVec3 move(',begin):source.index('\n\tfloat topDist',begin)]
    start=source.index('\tmove *= m_movement.desiredVelocity.y',begin)
    end=source.index('\n}',source.index('\tm_request.velocity = move;',start))+2
    tail=source[start:end]
    h=source.index('static bool WarsBotLadderServer(');helper=source[h:source.index('\n}',h)+2]
    cpp=r'''
#define WARSBOTS_EXPERIMENTAL_SPAWNER 1
#define WB_LOG(...) ((void)0)
#include <cmath>
#include <cstdio>
#include <algorithm>
using std::max;
float cry_floorf(float f){return std::floor(f);} float cry_fabsf(float f){return std::fabs(f);}
struct Vec3 {float x=0,y=0,z=0;Vec3(){} Vec3(float a,float b,float c):x(a),y(b),z(c){}
 Vec3 operator-(const Vec3&v)const{return Vec3(x-v.x,y-v.y,z-v.z);}
 float len2()const{return x*x+y*y+z*z;} float len()const{return std::sqrt(len2());}
 void NormalizeSafe(){float n=len();if(n>0)*this/=n;}
 Vec3& operator/=(float a){x/=a;y/=a;z/=a;return *this;}
 Vec3& operator*=(float a){x*=a;y*=a;z*=a;return *this;}
 Vec3& operator+=(const Vec3&v){x+=v.x;y+=v.y;z+=v.z;return *this;}
};
enum {STANCE_STAND,ACTION_SPRINT=1,ACTION_JUMP=2,NANOMODE_SPEED=3,eCMT_Fly=4};
struct Timer{float GetFrameTime(){return .016f;}};
struct Renderer{void Draw2dLabel(...) {}};
struct Env{bool bServer=true;Timer timer;Renderer renderer;Timer*pTimer=&timer;Renderer*pRenderer=&renderer;}env,*gEnv=&env;
struct Cvars{int pl_debug_ladders=0;}cvars,*g_pGameCVars=&cvars;
namespace WarsBots{struct BotConfig{int ladder=1;static BotConfig&Get(){static BotConfig c;return c;}};}
struct Manager{bool owns=true;bool Owns(unsigned){return owns;}};
struct Game{Manager mgr;Manager&GetBotManager(){return mgr;}}game,*g_pGame=&game;
struct Suit{int GetMode(){return 0;}float GetSprintMultiplier(bool){return 1;}};
struct CPlayer{bool bot=true,ready=false,frozen=false;int health=100;Suit suit;
 enum {eLS_Climb,eLDIR_Up,eLDIR_Down,eLAT_Jump}; using ELadderDirection=int;
 bool IsWarsBotActor()const{return bot;}unsigned GetEntityId()const{return 7;}
 int GetHealth(){return health;}bool IsFrozen(){return frozen;}float GetStanceMaxSpeed(int){return 4.5f;}
 Suit*GetNanoSuit(){return &suit;}void RequestLeaveLadder(int){}
 bool UpdateLadderAnimation(int,int,float){return ready;}
};
''' + helper + r'''
struct Movement{
 CPlayer&m_player;struct{bool isRagDoll=false;Vec3 ladderTop{0,0,20},ladderBottom{0,0,10};}m_stats;
 struct{Vec3 desiredVelocity{0,1,0};}m_movement;
 struct{int type=0;Vec3 velocity;}m_request;
 int m_actions=0;
 Movement(CPlayer&p):m_player(p){}
 void AdjustPlayerPositionOnLadder(CPlayer&){}
 void Climb(CPlayer&player){float bottomDist=0;
''' + axis + tail + r'''
};
bool run(CPlayer&p,bool expected){Movement m(p);m.Climb(p);return (m.m_request.type==eCMT_Fly && m.m_request.velocity.z>0)==expected;}
int main(){CPlayer p;
 if(!run(p,true)){std::puts("FAIL: owned native climb waits for cosmetic FIFO");return 1;}
 p.ready=true;if(!run(p,true))return 2;p.ready=false;
 p.bot=false;if(!run(p,false))return 3;p.bot=true;
 game.mgr.owns=false;if(!run(p,false))return 4;game.mgr.owns=true;
 env.bServer=false;if(!run(p,false))return 5;env.bServer=true;
 WarsBots::BotConfig::Get().ladder=0;if(!run(p,false))return 6;WarsBots::BotConfig::Get().ladder=1;
 p.health=0;if(!run(p,false))return 7;p.health=100;
 p.frozen=true;if(!run(p,false))return 8;p.frozen=false;
 Movement dead(p);dead.m_stats.isRagDoll=true;dead.Climb(p);if(dead.m_request.type!=0)return 9;
 Movement hold(p);hold.m_movement.desiredVelocity={0,0,0};hold.Climb(p);if(hold.m_request.velocity.len2()!=0)return 10;
 Movement down(p);down.m_movement.desiredVelocity={0,-1,0};down.Climb(p);if(down.m_request.velocity.z>=0)return 11;
 p.bot=false;p.ready=true;if(!run(p,true))return 12;
 std::puts("Actual native climb/animation gate controls PASS");return 0;
}
'''
    out=args.out.resolve();out.mkdir(parents=True,exist_ok=True);unit=out/'climb.cpp';unit.write_bytes(cpp.encode())
    comp=subprocess.run(['cl','/nologo','/std:c++17','/EHsc',str(unit),'/Fo:'+str(out)+'/', '/Fe:'+str(out/'climb.exe')],capture_output=True)
    (out/'compile.log').write_bytes(comp.stdout+comp.stderr)
    if comp.returncode:raise RuntimeError((comp.stdout+comp.stderr).decode(errors='replace'))
    result=subprocess.run([str(out/'climb.exe')],capture_output=True);(out/'run.log').write_bytes(result.stdout+result.stderr)
    print(result.stdout.decode());return result.returncode


if __name__=='__main__':
    raise SystemExit(main())
