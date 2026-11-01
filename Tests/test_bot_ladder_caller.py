"""DS-108: compile and execute REAL WarsBots ladder caller blocks from two git revisions.

This is an engine-free actual-caller regression: it extracts the real method bodies / smallest real blocks from
BotBrain.cpp, BotLocomotion.cpp, PlayerMovement.cpp, BotManager.cpp and BotLadder.h, then compiles them against a
small mock world. It does not copy the decision logic into the fixture and it never links the game/SDK.

Usage:
  python ds108_ladder_caller.py --repo <QA/build/ds076-code> --old 01e15a93... --new 0b02dc27... --out <temp dir>

Exit 0 only when the new revision passes and the old revision reproduces the intended RED controls. Generated files
are written only to --out.
"""
from pathlib import Path
import argparse
import subprocess
import sys


def method(text: str, signature: str) -> str:
    start = text.index(signature)
    left = text.index('{', start)
    depth = 0
    for i in range(left, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise ValueError('unclosed method: ' + signature)


def block(text: str, marker: str) -> str:
    start = text.index(marker)
    left = text.index('{', start)
    depth = 0
    for i in range(left, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise ValueError('unclosed block: ' + marker)


def prefix_to(text: str, signature: str, stop: str) -> str:
    start = text.index(signature)
    end = text.index(stop, start)
    return text[start:end]


CPP = r'''
#define WARSBOTS_EXPERIMENTAL_SPAWNER 1
#include "CryGame/Bots/BotLadder.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <initializer_list>
using namespace WarsBots;
using EntityId = unsigned;

#define WB_LOG(...) ((void)0)
template<class T> T fabs_tpl(T v){return (T)std::fabs((double)v);}
enum EStance { STANCE_STAND = 0 };
enum ENanoMode { NANOMODE_DEFENSE = 3 };
enum class BotGait { Walk, Run, Sprint };
enum class BotJump { None, Normal, Strength };
enum class BotCombatIntent { None, Aim, Fire };

struct Vec3 { float x=0,y=0,z=0; Vec3(){} Vec3(float a,float b,float c):x(a),y(b),z(c){}
 Vec3 operator-(const Vec3&o)const{return Vec3(x-o.x,y-o.y,z-o.z);} Vec3 operator+(const Vec3&o)const{return Vec3(x+o.x,y+o.y,z+o.z);}
 float len()const{return std::sqrt(x*x+y*y+z*z);} float GetLengthSquared2D()const{return x*x+y*y;} };
struct JVec { float x=0,y=0,z=0; JVec(){} JVec(float a,float b,float c):x(a),y(b),z(c){} };
struct CoherentBool { bool v=false; bool Value()const{return v;} void operator=(bool b){v=b;} };
struct SPlayerStats {
 CoherentBool isOnLadder, isFrozen, inFreefall;
 bool isExitingLadder=false, isRagDoll=false, isRagdoll=false, inZeroG=false, isGrabbed=false;
 int spectatorMode=0, flyMode=0;
 Vec3 ladderTop, ladderBottom, ladderOrientation;
};
struct CAnimation { struct { int m_nAnimID[1]={0}; } m_LMG0; float m_fAnimTime=0.f; };
struct IAnimationSet { int GetAnimIDByName(const char*) {return 1;} const char* GetNameByAnimID(int) {return "mock";} };
struct ISkeletonAnim { int GetNumAnimsInFIFO(int) {return 0;} CAnimation& GetAnimFromFIFO(int,int) {static CAnimation a;return a;} };
struct ICharacterInstance { int GetFlags(){return 0;} IAnimationSet* GetIAnimationSet() {return nullptr;} ISkeletonAnim* GetISkeletonAnim() {return nullptr;} const char* GetFilePath() {return "mock";} };
struct Entity { Vec3 p; Vec3 GetWorldPos()const{return p;} ICharacterInstance* GetCharacter(int){return nullptr;} };
struct IAnimationGraphState {
 using InputID=unsigned;const char* out=nullptr;const char* QueryOutput(const char*){return out;}
 const char* GetCurrentStateName(){return "Mock";}
 InputID GetInputId(const char*n){return std::strcmp(n,"Signal")==0?1u:2u;}
 const char* GetInputName(InputID n){return n==1?"Signal":"PseudoSpeed";}
 void GetInput(InputID,char*b){std::strcpy(b,"none");}float GetInputAsFloat(InputID){return .5f;}
};
struct IAnimatedCharacter { IAnimationGraphState* ag=nullptr; IAnimationGraphState* GetAnimationGraphState(){return ag;} };
struct CPlayer;
struct CActor { CPlayer* p=nullptr; };
struct CPlayer : CActor {
 bool NeedsWarsBotLadderPhysicsExit() const {return false;}
 void SetWarsBotLadderExitPhysics(bool,bool) {}
 void RestoreWarsBotLadderGraph(){}
 bool TryWarsBotLadderExitRetry(IAnimationGraphState*){return false;} // isolated route tests; real retry has a dedicated caller
 enum ELadderActionType { eLAT_None, eLAT_ReachedEnd, eLAT_ExitTop, eLAT_Jump, eLAT_StrafeLeft, eLAT_StrafeRight, eLAT_Use, eLAT_Die };
 Entity e; SPlayerStats st; IAnimatedCharacter ac; IAnimationGraphState ag;
 CPlayer(){p=this;}
 SPlayerStats* GetPlayerStats(){return &st;}
 Entity* GetEntity(){return &e;}
 IAnimatedCharacter* GetAnimatedCharacter(){return &ac;}
 EntityId GetEntityId()const{return 1;}
 bool IsFrozen()const{return st.isFrozen.Value();}
 bool IsClient()const{return false;}
 bool IsWarsBotActor()const{return warsBot;}
 bool warsBot=true;
 bool ladderUsable=false; int isLadderCalls=0;
 int grabs=0, leaves=0, lastLeave=0;
 bool IsLadderUsable(){++isLadderCalls; return ladderUsable;}
 void RequestGrabOnLadder(ELadderActionType){++grabs; st.isOnLadder=true;}
 void RequestLeaveLadder(ELadderActionType r){++leaves; lastLeave=r; st.isOnLadder=false; st.isExitingLadder=false;}
 void SetWarsBotActions(bool,bool,int){}
 float GetStanceMaxSpeed(int)const{return 2.5f;}
 float CalculatePseudoSpeed(bool)const{return 1.0f;} // native value separately extracted by exit-retry caller
 void* GetLinkedEntity(){return nullptr;}
 void* GetLinkedVehicle(){return nullptr;}
};
struct NavPathLink { int corner=0, kind=0; float a[3]={},b[3]={}; unsigned long long ref=0; };
enum NavLinkKind { NavLinkDrop=0, NavLinkJump, NavLinkStrength, NavLinkLift, NavLinkLadder };
inline const char* NavLinkName(int k){static const char* n[]={"drop","jump","strength","lift","ladder"};return n[k];}
struct NavMeshCore { int bans=0, notes=0; bool BanLink(unsigned long long){++bans;return true;} bool NoteLinkFailure(unsigned long long){++notes;return true;} };
namespace WarsBots { struct BotConfig { int ladder=1, jumpLinks=1, navKneeFix=0; static BotConfig& Get(){static BotConfig c;return c;} }; }
using WarsBots::BotConfig;
struct CGameRules { using TPlayers=std::vector<int>; };
struct MockNav { int audit=-1, resets=0; void StartAudit(int v){audit=v;} void Reset(){++resets;} };
struct MockRules { void GetPlayers(CGameRules::TPlayers&){} };
struct IConsoleCmdArgs;
struct MockManager;
struct MockGame { MockManager* mgr=nullptr; MockRules rules; MockManager& GetBotManager(); MockRules* GetGameRules(){return &rules;} };
struct MockManager {
 bool owns=true; int count=0; MockNav m_nav;
 bool Owns(EntityId)const{return owns;} bool CanRunLocalIA()const{return true;} int Count()const{return count;}
 void NavAuditCommand(IConsoleCmdArgs*);
 static MockManager* self; static MockManager* Self(){return self;}
};
using BotManager = MockManager;
MockManager* MockManager::self=nullptr;
MockManager& MockGame::GetBotManager(){return *mgr;}
struct MockEnv { bool bServer=true; bool bClient=false; } g_env;
MockEnv* gEnv=&g_env;
MockGame g_game; MockGame* g_pGame=&g_game;
static bool ParseInt(const char* s,int& v){return s && std::sscanf(s,"%d",&v)==1;}

// Extracted actual WarsBotLadderServer helper.
@WARSBOT_HELPER@

#if HAVE_EXIT_HELPER
using WarsBots::NativeLadderExitPhysicalReady;
#else
static bool NativeLadderExitPhysicalReady(const LVec&,const LVec&,const LVec&){return true;}
#endif

struct BotBlackboard { Vec3 position, velocity; bool grounded=true, canMove=true, suitActive=true; int mode=3; float energy=100; };
struct BotCombat { void StopFire(){} void Reset(){} };
struct BotIntent {
 bool move=false; Vec3 destination, aim; BotGait gait=BotGait::Run; BotJump jump=BotJump::None;
 int suitMode=NANOMODE_DEFENSE; BotCombatIntent combat=BotCombatIntent::None;
 bool linkJump=false, meshPath=false; float arrive=0.8f;
 struct Why { short suitMode=0,jump=0,gait=0,aim=0,move=0; } why;
};
#define WBI(field,value) (m_intent.field=(value), m_intent.why.field=(short)__LINE__)

struct BotMovement {
 int moves=0; float lastSpeed=0; bool lastJump=false;
 bool Move(CPlayer&,const Vec3&,const Vec3&,float,float speed,EStance,float,bool jumpTraverse,bool,float,bool,float,bool)
 { ++moves; lastSpeed=speed; lastJump=jumpTraverse; return true; }
 void Aim(CPlayer&,const Vec3&){} void Look(CPlayer&,const Vec3&){} void Stop(CPlayer&){}
};
struct MockLoco {
 BotMovement m_movement;
 void Look(CPlayer&,const Vec3&){} void Stop(CPlayer&){} bool Execute(CPlayer&,const BotIntent&,BotBlackboard&,float,float){return true;}
 @EXECUTE_LADDER@
};
struct Trap { void Reset(){} };
enum class LinkMove { Drop=0, Jump, Strength };
struct BotInstance { CActor a; CActor* GetActor(){return &a;} };
struct IConsoleCmdArgs { virtual ~IConsoleCmdArgs(){} virtual int GetArgCount()const=0; virtual const char* GetArg(int)const=0; };
struct MockLink { bool active=false; int starts=0; LinkMove move=LinkMove::Drop; bool Active()const{return active;}
 void Reset(){active=false;} void Start(float,LinkMove m,JVec,JVec){++starts;move=m;active=true;} };
struct MockBrain {
 BotLadder m_ladder; NavPathLink m_ladderNow, m_linkNow; bool m_ladderNativeLocked=false, m_ladderCancelPending=false;
 std::vector<Vec3> m_path; std::vector<NavPathLink> m_pathLinks; size_t m_pathIndex=0, m_cornerFor=size_t(-1);
 float m_nextRepath=0,m_minRepath=0,m_now=0,m_number=1; bool m_meshPathActive=true,m_verbose=false,m_running=true;
 BotIntent m_intent; BotBlackboard m_blackboard; MockLoco m_locomotion; BotCombat m_combat; MockLink m_link;
 Trap m_trap; float m_trapStepUntil=0; NavMeshCore mesh; NavPathLink cornerLink;
 int cancelRunningSeen=-1; bool lastCancelBan=false; int cancels=0;
 NavMeshCore* CurrentNavMesh(){return &mesh;}
 const NavPathLink* LinkAtCorner(size_t c){return c==0?&cornerLink:nullptr;}
 @CANCEL_LADDER@
 void CancelLadder(CPlayer& a,const char* w,bool b){cancelRunningSeen=m_running?1:0;lastCancelBan=b;++cancels;CancelLadderBody(a,w,b);}
 @PROBE_BLOCK@
 @GRAB_BLOCK@
 @APPLY_LINK@
 @STOP_PREFIX@
};
struct MockMovement { SPlayerStats& m_stats; CPlayer& m_player; MockMovement(CPlayer& p):m_stats(p.st),m_player(p){} @EXIT_BLOCK@ };
struct MockArgs : IConsoleCmdArgs { std::vector<std::string> a; int GetArgCount()const{return (int)a.size();} const char* GetArg(int i)const{return a[i].c_str();} };
struct MockManager;
@NAV_AUDIT@
'''

NAV_AUDIT = r'''
void MockManager::NavAuditCommand(IConsoleCmdArgs* args) {
@NAV_BODY@
}
'''

MAIN = r'''
static int fails=0;
static void check(bool ok,const char* what){ if(!ok){std::printf("FAIL %s\n",what);++fails;} }
int main(){
 MockManager mgr; MockManager::self=&mgr; g_game.mgr=&mgr;
 CPlayer actor; actor.st.ladderBottom=Vec3(0,0,10); actor.st.ladderTop=Vec3(0,0,20); actor.st.ladderOrientation=Vec3(0,1,0);
 actor.e.p=Vec3(0,0,10);

 // 1. Grab-lock: probe at 1.7 m then walk to 1.1 m; new grabs once, old never grabs.
 {
  MockBrain b; b.cornerLink.a[0]=0;b.cornerLink.a[1]=0;b.cornerLink.a[2]=10;
  b.cornerLink.b[0]=0;b.cornerLink.b[1]=0;b.cornerLink.b[2]=20;b.cornerLink.kind=NavLinkLadder;
  b.m_ladder.Start(0,LVec{0,0,10},LVec{0,0,20});
  actor.ladderUsable=true; actor.grabs=0; actor.isLadderCalls=0;
  Vec3 center(0,0,11.2f);
  for(int tick=0;tick<3;++tick){
   LadderInput in; in.now=0; in.grounded=true; // actual UpdateLadder constructs fresh input every tick
   actor.e.p=Vec3(0,0,10); in.feet=LVec{0,0,10};
   if(tick==0) in.feet=LVec{0,1.7f,10}; if(tick==1||tick==2) in.feet=LVec{0,1.1f,10};
   in.onLadder=actor.st.isOnLadder.Value(); in.exiting=actor.st.isExitingLadder;
   b.ProbeBlock(actor,in,center);
   LadderOutput o=b.m_ladder.Update(in);
   b.GrabBlock(actor,o,in);
  }
  check(actor.grabs==1,"grab-lock: new must grab exactly once");
  check(b.m_ladderNativeLocked,"grab-lock: frame locked after native attach");
  const int calls=actor.isLadderCalls;
  actor.ladderUsable=false;
  LadderInput climb; climb.now=1; climb.grounded=true; climb.onLadder=true; climb.feet=LVec{0,1.1f,10};
  b.ProbeBlock(actor,climb,center);
  check(actor.isLadderCalls==calls,"grab-lock: no ray mutation while climbing");
 }

 // 2. Cancel pending is consumed once; old early return leaves it set.
 {
  MockBrain b; b.m_ladder.Reset(); b.m_ladderNativeLocked=false; b.m_ladderCancelPending=true;
  actor.st.isOnLadder=false; actor.st.isExitingLadder=false;
  b.CancelLadder(actor,"reset",false);
  check(!b.m_ladderCancelPending,"cancel: pending must be cleared once");
 }

 // 3. Fail bans once; stop-style cancel does not ban a valid link.
 {
  MockBrain b; b.m_ladder.Start(0,LVec{0,0,10},LVec{0,0,20}); b.m_ladderNow.ref=42;
  b.CancelLadder(actor,"fail",true);
  check(b.mesh.bans==1,"cancel: real failure bans the link once");
  MockBrain c; c.m_ladder.Start(0,LVec{0,0,10},LVec{0,0,20}); c.m_ladderNow.ref=42;
  c.CancelLadder(actor,"stop",false);
  check(c.mesh.bans==0,"cancel: stop must not ban a valid link");
  MockBrain d; d.m_ladder.Reset(); d.m_ladderNow.ref=42;
  d.CancelLadder(actor,"no_grab",true);
  check(d.mesh.bans==1,"cancel: failed inactive policy still bans its actual route");
  d.CancelLadder(actor,"no_grab",true);
  check(d.mesh.bans==1,"cancel: duplicate cleanup must not ban twice");
 }

 // 4. Native exit graph: new rejects empty/0-before-egress; human legacy still leaves.
 {
  CPlayer p; p.st.isExitingLadder=true; p.st.ladderBottom=Vec3(0,0,10); p.st.ladderTop=Vec3(0,0,20);
  p.ac.ag=&p.ag; p.e.p=Vec3(0,0,10);
  MockMovement m(p);
  p.ag.out=nullptr; m.ExitBlock(p); check(p.leaves==0,"exit: null output must not leave");
  p.ac.ag=nullptr; m.ExitBlock(p); check(p.leaves==0,"exit: missing graph must not leave");p.ac.ag=&p.ag;
  p.ag.out="invalid"; m.ExitBlock(p); check(p.leaves==0,"exit: invalid output must not leave");
  p.ag.out=""; m.ExitBlock(p); check(p.leaves==0,"exit: empty graph output must not leave");
  p.ag.out="0"; p.e.p=Vec3(0,0,10); m.ExitBlock(p); check(p.leaves==0,"exit: graph0 before physical egress must not leave");
  p.e.p=Vec3(0.6f,0.6f,19.5f); m.ExitBlock(p); check(p.leaves==1,"exit: graph0 after physical egress leaves");
  p.leaves=0;p.st.isExitingLadder=true;p.st.isOnLadder=true;p.ag.out="";m.ExitBlock(p);check(p.leaves==1,"exit: empty native default completes only after physical egress");
  p.leaves=0; p.warsBot=false; gEnv->bClient=true; p.st.isExitingLadder=true; p.ag.out="0"; p.e.p=Vec3(0,0,10); m.ExitBlock(p);
  check(p.leaves==1,"exit: human/client legacy behavior preserved"); gEnv->bClient=false;
 }

 // 5. ExecuteLadder: new requires native attach and sends stance speed; old bypasses without attach and uses 1.0.
 {
  MockLoco l; CPlayer p; p.st.isOnLadder=true; p.st.isExitingLadder=false;
  const bool ok=l.ExecuteLadder(p,Vec3(0,0,10),Vec3(0,0,11),0.1f,true);
  check(ok && l.m_movement.moves==1,"execute: attached new runs");
  check(l.m_movement.lastSpeed>1.0f,"execute: new uses stance/full forward speed");
  check(l.m_movement.lastJump,"execute: attached uses the SafeStep exception");
  p.st.isOnLadder=false; l.m_movement.moves=0;
  const bool ok2=l.ExecuteLadder(p,Vec3(0,0,10),Vec3(0,0,11),0.1f,true);
  check(!ok2 && l.m_movement.moves==0,"execute: new refuses non-attached bypass");
 }

 // 6. Policy settle: 1.5 m is not Done on new (0.8 m), old accepts it; reattach is Lost.
 {
  BotLadder L; L.Start(0,LVec{0,0,10},LVec{0,0,20}); L.SetNative(LVec{0,0,10},LVec{0,0,20},LVec{0,1,0});
  LadderInput in; in.now=1; in.grounded=true; in.feet=LVec{0,0,10}; L.Update(in);
  in.onLadder=true; in.feet=LVec{0,0,19.5f}; L.Update(in); in.exiting=true; L.Update(in);
  in.onLadder=false; in.exiting=false; in.grounded=true; in.feet=LVec{0,1.5f,19.5f};
  LadderOutput o=L.Update(in);
  check(o.phase!=LadderPhase::Done,"settle: 1.5 m is not a real arrival");
  BotLadder R; R.Start(0,LVec{0,0,10},LVec{0,0,20}); R.SetNative(LVec{0,0,10},LVec{0,0,20},LVec{0,1,0});
  LadderInput ri; ri.now=0; ri.grounded=true; ri.feet=LVec{0,0,10};
  R.Update(ri); ri.onLadder=true; ri.feet=LVec{0,0,19.5f}; ri.exiting=true; R.Update(ri);
  ri.onLadder=false; ri.exiting=false; ri.grounded=true; ri.feet=LVec{0,1.5f,19.5f}; R.Update(ri);
  ri.onLadder=true; o=R.Update(ri); check(o.phase==LadderPhase::Failed && o.fail==LadderFail::Lost,"settle: reattach is not Done");
 }

 // 7. Generic jump start rejects Ladder/Lift; Drop/Jump/Strength still start.
 {
  MockBrain b; b.m_intent.move=true; b.m_blackboard.position=Vec3(0,0,10);
  const int kinds[]={NavLinkDrop,NavLinkJump,NavLinkStrength,NavLinkLadder,NavLinkLift};
  b.cornerLink.a[0]=0;b.cornerLink.a[1]=0;b.cornerLink.a[2]=10;
  b.cornerLink.b[0]=0;b.cornerLink.b[1]=0;b.cornerLink.b[2]=20;
  for(int k:kinds){ b.cornerLink.kind=k; b.m_link.starts=0; b.m_link.active=false; b.ApplyLinkStart(actor);
   if(k==NavLinkLadder||k==NavLinkLift) check(b.m_link.starts==0,"link: unsupported kind must not start as Drop");
   else check(b.m_link.starts==1,"link: supported kind still starts"); }
 }

 // 8. Stop prefix sets m_running=false before native cleanup (new) and keeps valid ladder un-banned.
 {
  MockBrain b; b.m_running=true; b.m_ladder.Start(0,LVec{0,0,10},LVec{0,0,20}); b.m_ladderNow.ref=42;
  BotInstance bi; bi.a.p=&actor; b.StopPrefix(bi);
  check(b.cancelRunningSeen==0,"stop: new must clear running before native cleanup");
  check(b.lastCancelBan==false,"stop: must not globally ban a valid ladder");
 }

 // 9. Net-zero nav audit variants: default/0/1/2/stop and invalid/no-actor guards.
 {
  MockManager m; MockManager::self=&m; g_game.mgr=&m; m.count=0;
  MockArgs a0; a0.a={"wb_nav_audit"}; m.NavAuditCommand(&a0); check(m.m_nav.audit==0,"nav: default variant 0");
  MockArgs a1; a1.a={"wb_nav_audit","1"}; m.NavAuditCommand(&a1); check(m.m_nav.audit==1,"nav: variant 1");
  MockArgs a2; a2.a={"wb_nav_audit","2"}; m.NavAuditCommand(&a2); check(m.m_nav.audit==2,"nav: variant 2");
  MockArgs as; as.a={"wb_nav_audit","stop"}; m.NavAuditCommand(&as); check(m.m_nav.resets==1,"nav: stop resets");
  MockArgs bad; bad.a={"wb_nav_audit","3"}; m.m_nav.audit=-1; m.NavAuditCommand(&bad); check(m.m_nav.audit==-1,"nav: invalid ignored");
  MockArgs a1b; a1b.a={"wb_nav_audit","1"}; m.count=1; m.m_nav.audit=-1; m.NavAuditCommand(&a1b); check(m.m_nav.audit==-1,"nav: actors guard");
 }
 if(fails){std::printf("DS108 caller controls FAILED (%d)\n",fails);return 1;}
 std::printf("DS108 real caller controls PASS\n"); return 0;
}
'''


def build_harness(repo: Path, rev: str, out: Path) -> Path:
    def show(path: str) -> str:
        if rev == 'WORKTREE':
            return (repo / path).read_text(encoding='utf-8-sig')
        r = subprocess.run(['git', '-C', str(repo), 'show', f'{rev}:{path}'], capture_output=True, check=True)
        return r.stdout.decode('utf-8-sig', errors='replace')

    brain = show('Code/CryGame/Bots/BotBrain.cpp')
    loco = show('Code/CryGame/Bots/BotLocomotion.cpp')
    move = show('Code/CryGame/Actors/Player/PlayerMovement.cpp')
    manager = show('Code/CryGame/Bots/BotManager.cpp')
    ladder_h = show('Code/CryGame/Bots/BotLadder.h')

    cancel = method(brain, 'void BotBrain::CancelLadder(').replace('void BotBrain::CancelLadder(', 'void CancelLadderBody(')
    execute = method(loco, 'bool BotLocomotion::ExecuteLadder(').replace('bool BotLocomotion::ExecuteLadder(', 'bool ExecuteLadder(')
    nav = method(manager, 'void BotManager::NavAuditCommand(').replace('void BotManager::NavAuditCommand(', 'void MockManager::NavAuditCommand(')
    probe = block(brain, 'if ((phase==LadderPhase::Approach || phase==LadderPhase::Grab)')
    grab = block(brain, 'if (o.grab)')
    exit_block = block(move, 'if (m_stats.isExitingLadder)')
    helper = method(move, 'static bool WarsBotLadderServer(')
    apply_prefix = prefix_to(brain, 'void BotBrain::ApplyLinkExecutor(', '\tLinkInput in;')
    apply_prefix = apply_prefix.replace('void BotBrain::ApplyLinkExecutor(', 'void ApplyLinkStart(')
    stop_prefix = prefix_to(brain, 'void BotBrain::Stop(', '\tm_combat.Reset();') + '\tm_combat.Reset();\n}'
    stop_prefix = stop_prefix.replace('void BotBrain::Stop(', 'void StopPrefix(')

    # Wrap extracted blocks as members.
    probe = ('void ProbeBlock(CPlayer& actor,LadderInput& in,const Vec3& center){\n'
             ' SPlayerStats* stats=actor.GetPlayerStats();\n'
             ' const LadderPhase phase=m_ladder.Phase();\n' + probe + '\n}\n')
    grab = ('void GrabBlock(CPlayer& actor,const LadderOutput& o,LadderInput& in){\n'
            ' SPlayerStats* stats=actor.GetPlayerStats();\n' + grab + '\n}\n')
    exit_block = ('void ProcessWarsBotLadderPhysicsExit(CPlayer&,bool& landed){landed=false;}\nvoid ExitBlock(CPlayer& player){\n' + exit_block + '\n}\n')

    cpp = CPP.replace('@WARSBOT_HELPER@', helper)
    cpp = cpp.replace('@EXECUTE_LADDER@', execute)
    cpp = cpp.replace('@CANCEL_LADDER@', cancel)
    cpp = cpp.replace('@PROBE_BLOCK@', probe)
    cpp = cpp.replace('@GRAB_BLOCK@', grab)
    cpp = cpp.replace('@APPLY_LINK@', apply_prefix + '\n}')
    cpp = cpp.replace('@STOP_PREFIX@', stop_prefix)
    cpp = cpp.replace('@EXIT_BLOCK@', exit_block)
    cpp = cpp.replace('@NAV_AUDIT@', NAV_AUDIT.replace('@NAV_BODY@', nav.split('{', 1)[1].rsplit('}', 1)[0]))
    cpp = cpp.replace('@HAVE_EXIT_HELPER@', '1' if 'NativeLadderExitPhysicalReady' in ladder_h else '0')
    cpp = cpp.replace('#if HAVE_EXIT_HELPER', '#if ' + ('1' if 'NativeLadderExitPhysicalReady' in ladder_h else '0'))
    cpp = cpp + MAIN
    out.mkdir(parents=True, exist_ok=True)
    unit = out / 'ds108_caller.cpp'
    unit.write_text(cpp, encoding='utf-8')
    # The reference must compile its own policy, not silently use the current checkout's header.
    header = out / 'include/CryGame/Bots/BotLadder.h'
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text(ladder_h, encoding='utf-8')
    return unit


def run(repo: Path, rev: str, out: Path) -> int:
    unit = build_harness(repo, rev, out)
    exe = out / 'ds108_caller.exe'
    inc = repo / 'Code'
    comp = subprocess.run(['cl', '/nologo', '/std:c++17', '/EHsc', '/I', str(out / 'include'), '/I', str(inc), str(unit),
                           '/Fo:' + str(out) + '\\', '/Fe:' + str(exe)], capture_output=True)
    sys.stdout.buffer.write(comp.stdout + comp.stderr)
    (out / 'compile.log').write_bytes(comp.stdout + comp.stderr)
    if comp.returncode:
        raise RuntimeError('Caller fixture must compile; compiler failure is not semantic RED')
    res = subprocess.run([str(exe)], capture_output=True)
    sys.stdout.buffer.write(res.stdout + res.stderr)
    (out / 'run.log').write_bytes(res.stdout + res.stderr)
    return res.returncode


def main():
    ap = argparse.ArgumentParser()
    root = Path(__file__).resolve().parents[1]
    ap.add_argument('--repo', type=Path, default=root, help='local git checkout')
    ap.add_argument('--old', help='optional reference revision; requires semantic RED')
    ap.add_argument('--new', default='HEAD', help='current revision SHA')
    ap.add_argument('--out', type=Path, default=root / 'units/ladder-caller', help='private output directory')
    args = ap.parse_args()
    out = args.out.resolve()
    old = None
    if args.old:
        print('=== OLD', args.old, '===')
        old = run(args.repo, args.old, out / 'old')
    print('=== NEW', args.new, '===')
    new = run(args.repo, args.new, out / 'new')
    print('old_exit=', old, 'new_exit=', new)
    return 0 if new == 0 and (old is None or old != 0) else 1


if __name__ == '__main__':
    raise SystemExit(main())
