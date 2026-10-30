"""Compile the actual missing-clip predicate, collider lifecycle and physics exit adapter.

Engine boundaries are mocked; movement/clearance/landing decisions are extracted from production.
This proves refusal contracts and requested displacements, not real engine collision or route success.
"""
from pathlib import Path
import argparse
import subprocess


def method(source, signature):
    start = source.index(signature)
    left = source.index('{', start)
    depth = 0
    for i in range(left, len(source)):
        depth += (source[i] == '{') - (source[i] == '}')
        if not depth:
            return source[start:i + 1]
    raise ValueError(signature)


CPP = r'''
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "@HEADER@"
#define WB_LOG(...) ((void)0)
using std::min; using std::max;
float fabs_tpl(float v){return std::fabs(v);}
struct Vec3 {
 float x=0,y=0,z=0; Vec3(){} Vec3(float a,float b,float c):x(a),y(b),z(c){}
 Vec3 operator+(Vec3 b)const{return {x+b.x,y+b.y,z+b.z};}
 Vec3 operator-(Vec3 b)const{return {x-b.x,y-b.y,z-b.z};}
 Vec3 operator-()const{return {-x,-y,-z};}
 Vec3 operator*(float f)const{return {x*f,y*f,z*f};}
 Vec3 operator/(float f)const{return *this*(1.f/f);}
 float Dot(Vec3 b)const{return x*b.x+y*b.y+z*b.z;}
 float len()const{return std::sqrt(Dot(*this));}
 bool IsValid()const{return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z);}
 void zero(){x=y=z=0;} Vec3 GetNormalized()const{return *this/len();} void Normalize(){*this=GetNormalized();}
};
struct pe_player_dimensions {Vec3 sizeCollider={.3f,.3f,.6f};float heightCollider=1.f,heightPivot=0;int bUseCapsule=1;};
struct IPhysicalEntity {bool dimensionsOK=true;pe_player_dimensions dims;int GetParams(pe_player_dimensions* p){*p=dims;return dimensionsOK;}};
namespace primitives {struct primitive{};struct cylinder:primitive {enum{type=5};Vec3 center,axis;float r,hh;};struct capsule:cylinder {enum{type=6};};}
struct geom_contact{};struct ray_hit{Vec3 pt,n;};
enum {ent_static=1,ent_terrain=2,ent_rigid=4,ent_sleeping_rigid=8,ent_living=16,rwi_stop_at_pierceable=32,geom_colltype_player=64,rwi_colltype_bit=8};
struct World {
 bool floor=true,edge=false;float floorZ=9.6f,normal=1,overlap=0,sweep=0;int rays=0,overlaps=0,sweeps=0;bool skipOK=true;
 int RayWorldIntersection(Vec3 origin,Vec3,int mask,int,ray_hit* hit,int,IPhysicalEntity** skip,int count){
  ++rays;skipOK&=count==1&&skip&&skip[0]&&!(mask&ent_living);if(!floor||(edge&&origin.x>1.3f))return 0;
  hit->pt={origin.x,origin.y,floorZ};hit->n={0,0,normal};return 1;
 }
 float PrimitiveWorldIntersection(int,primitives::primitive*,Vec3 delta,int mask,geom_contact**,int,int,void*,void*,int,IPhysicalEntity** skip,int count){
  skipOK&=count==1&&skip&&skip[0]&&(mask&ent_living);if(delta.len()<.0001f){++overlaps;return overlap;}++sweeps;return sweep;
 }
} world;
struct Env {bool bServer=true;World* pPhysicalWorld=&world;} env,*gEnv=&env;
struct Manager {bool owns=true;bool Owns(unsigned){return owns;}};
struct Game {Manager mgr;Manager& GetBotManager(){return mgr;}} game,*g_pGame=&game;
namespace WarsBots {struct BotConfig {int ladder=1;static BotConfig& Get(){static BotConfig c;return c;}};}
struct Animations {int clip=-1;int GetAnimIDByName(const char*){return clip;}};
struct Character {Animations a;Animations* GetIAnimationSet(){return &a;}};
using ICharacterInstance=Character;using IAnimationSet=Animations;
struct Entity {Vec3 pos={0,0,8};IPhysicalEntity physics;Character ch;Character* GetCharacter(int){return &ch;}IPhysicalEntity* GetPhysics(){return &physics;}Vec3 GetWorldPos(){return pos;}};
enum {eColliderMode_Undefined=0,eColliderMode_Pushable=3,eColliderModeLayer_Game=1,eCMT_Fly=1,eCMT_Normal=2};
struct AC {int mode=-1,calls=0;int GetPhysicalColliderMode(){return mode;}void RequestPhysicalColliderMode(int value,int,const char*){mode=value;++calls;}};
struct Coherent{bool value=true;bool Value()const{return value;}};
struct Stats {Coherent isOnLadder;bool isExitingLadder=true,isRagDoll=false;float onGround=0;Vec3 ladderTop={0,0,10},ladderBottom={0,0,0},ladderOrientation={-1,0,0};};
struct CPlayer {
 bool bot=true,frozen=false,m_warsBotLadderExitPhysics=false,m_warsBotLadderExitSettling=false;int health=100;mutable Entity entity;Stats m_stats;AC ac;AC* m_pAnimatedCharacter=&ac;
 bool IsWarsBotActor()const{return bot;}unsigned GetEntityId()const{return 7;}int GetHealth()const{return health;}bool IsFrozen(){return frozen;}
 Entity* GetEntity()const{return &entity;} AC* GetAnimatedCharacter(){return m_pAnimatedCharacter;} bool WarsBotLadderExitSettling()const{return m_warsBotLadderExitSettling;}
 @NEEDS@
 @COLLIDER@
};
struct CPlayerMovement {
 Stats& m_stats;float m_frameTime=.03f;struct{int type=0;Vec3 velocity={100,100,100};}m_request;
 CPlayerMovement(CPlayer& p):m_stats(p.m_stats){}
 void ProcessWarsBotLadderPhysicsExit(CPlayer&,bool&);
};
@ADAPTER@
int failures=0;
void check(bool v,const char* s){if(!v){std::printf("FAIL %s\n",s);++failures;}}
void reset(){world=World();env=Env();game=Game();g_pGame=&game;gEnv=&env;WarsBots::BotConfig::Get().ladder=1;}
int main(){
 {reset();CPlayer p;check(p.NeedsWarsBotLadderPhysicsExit(),"owned missing asset eligible");
  p.entity.ch.a.clip=12;check(!p.NeedsWarsBotLadderPhysicsExit(),"registered asset preserves native animation");p.entity.ch.a.clip=-1;
  p.bot=false;check(!p.NeedsWarsBotLadderPhysicsExit(),"human");p.bot=true;game.mgr.owns=false;check(!p.NeedsWarsBotLadderPhysicsExit(),"foreign");game.mgr.owns=true;
  env.bServer=false;check(!p.NeedsWarsBotLadderPhysicsExit(),"replica");env.bServer=true;WarsBots::BotConfig::Get().ladder=0;check(!p.NeedsWarsBotLadderPhysicsExit(),"OFF");WarsBots::BotConfig::Get().ladder=1;
  p.health=0;check(!p.NeedsWarsBotLadderPhysicsExit(),"dead");p.health=100;p.frozen=true;check(!p.NeedsWarsBotLadderPhysicsExit(),"frozen");p.frozen=false;
  p.m_stats.isRagDoll=true;check(!p.NeedsWarsBotLadderPhysicsExit(),"ragdoll");p.m_stats.isRagDoll=false;p.m_stats.isExitingLadder=false;check(!p.NeedsWarsBotLadderPhysicsExit(),"not exit");}
 {reset();CPlayer p;CPlayerMovement m(p);bool landed=true;m.ProcessWarsBotLadderPhysicsExit(p,landed);
  check(!landed&&m.m_request.type==eCMT_Fly&&m.m_request.velocity.z>0,"request real native-axis rise");check(world.rays==5&&world.overlaps==1&&world.sweeps==1&&world.skipOK,"support footprint/fullbody checks and own body skipped");check(p.ac.mode==eColliderMode_Pushable&&p.m_warsBotLadderExitPhysics,"world collider acquired");
  p.SetWarsBotLadderExitPhysics(false,false);check(p.ac.mode==eColliderMode_Undefined&&!p.m_warsBotLadderExitPhysics,"cleanup releases owned collider layer");}
 {reset();CPlayer p;p.entity.ch.a.clip=1;CPlayerMovement m(p);bool landed=true;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(!landed&&world.rays==0&&world.overlaps==0&&p.ac.calls==0&&m.m_request.velocity.x==100,"registered clip path untouched by fallback");}
 for(int fault=0;fault<7;++fault){reset();CPlayer p;CPlayerMovement m(p);bool landed=true;
  if(fault==0)world.floor=false;if(fault==1)world.edge=true;if(fault==2)world.normal=.4f;if(fault==3)world.overlap=1;if(fault==4)world.sweep=.02f;if(fault==5)p.entity.physics.dimensionsOK=false;if(fault==6)m.m_frameTime=NAN;
  m.ProcessWarsBotLadderPhysicsExit(p,landed);check(!landed&&m.m_request.velocity.len()==0&&!p.m_warsBotLadderExitPhysics,"blocked/missing floor/ledge/slope/overlap/wall/dimensions/dt refuse without success or motion");}
 {reset();CPlayer p;CPlayerMovement m(p);m.m_frameTime=.12f;bool landed=false;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(m.m_request.velocity.len()*.12f<=1.7f*.05f+.0001f,"slow frame displacement stays inside swept interval");}
 {reset();CPlayer p;p.entity.pos={0,0,9.67f};CPlayerMovement m(p);bool landed=true;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(!landed&&m.m_request.velocity.x>0&&m.m_request.velocity.z==0,"rise then native forward egress");}
 {reset();CPlayer p;p.entity.pos={1.1f,0,9.6f};CPlayerMovement m(p);bool landed=true;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(!landed,"position alone never completes");p.m_stats.onGround=.2f;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(landed,"real upper floor plus unchanged physical egress plus grounded");}
 {reset();CPlayer p;p.entity.pos={1.1f,0,9.6f};p.m_stats.onGround=.2f;world.floor=false;CPlayerMovement m(p);bool landed=true;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(!landed,"stale grounded at missing floor never completes");}
 {reset();CPlayer p;p.entity.pos={1.1f,0,9.6f};p.m_stats.onGround=.2f;p.m_warsBotLadderExitSettling=true;p.ac.mode=0;CPlayerMovement m(p);bool landed=true;m.ProcessWarsBotLadderPhysicsExit(p,landed);check(!landed,"foreign collider override never completes from stale grounded");}
 std::printf("Actual physics exit caller: failures=%d\n",failures);return failures?1:0;
}
'''


def main():
    root = Path(__file__).resolve().parents[1]
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', type=Path, default=root / 'units/ladder-physics-exit')
    args = ap.parse_args()
    player = (root / 'Code/CryGame/Actors/Player/Player.cpp').read_text(encoding='utf-8-sig')
    move = (root / 'Code/CryGame/Actors/Player/PlayerMovement.cpp').read_text(encoding='utf-8-sig')
    restore = method(player, 'void CPlayer::RestoreWarsBotLadderGraph()')
    assert restore.index('SetWarsBotLadderExitPhysics(false,false)') < restore.index('if (!m_warsBotLadderGraphOverride)')
    cpp = CPP.replace('@HEADER@', (root / 'Code/CryGame/Bots/BotLadder.h').as_posix())
    for token, source, signature in [('@NEEDS@', player, 'bool CPlayer::NeedsWarsBotLadderPhysicsExit()'),
                                     ('@COLLIDER@', player, 'void CPlayer::SetWarsBotLadderExitPhysics('),
                                     ('@ADAPTER@', move, 'void CPlayerMovement::ProcessWarsBotLadderPhysicsExit(')]:
        body = method(source, signature)
        if token != '@ADAPTER@':
            body = body.replace('CPlayer::', '')
        cpp = cpp.replace(token, body)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    unit, exe = out / 'caller.cpp', out / 'caller.exe'
    unit.write_text(cpp, encoding='utf-8')
    comp = subprocess.run(['cl', '/nologo', '/std:c++17', '/EHsc', str(unit), '/Fo:' + str(out) + '/', '/Fe:' + str(exe)], capture_output=True)
    (out / 'compile.log').write_bytes(comp.stdout + comp.stderr)
    if comp.returncode:
        print((comp.stdout + comp.stderr).decode(errors='replace'))
        raise RuntimeError('Compilation failure is never behavioral evidence')
    result = subprocess.run([str(exe)], capture_output=True)
    (out / 'run.log').write_bytes(result.stdout + result.stderr)
    print(result.stdout.decode(errors='replace'))
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
