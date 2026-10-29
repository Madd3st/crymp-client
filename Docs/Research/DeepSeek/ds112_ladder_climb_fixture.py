"""DS112: actual-caller ladder climb gate fixture.

Extracts the real 3cfd CPlayerMovement climb block and CPlayer::UpdateLadderAnimation
from the exact local game checkout, then compiles an engine-free mock caller.

Controls:
   old/no manual FIFO -> RED (no eCMT_Fly request)
  old/manual FIFO     -> GREEN (request set; gate control)
  proposed/no FIFO/owned -> GREEN (request set)
  proposed/no FIFO/owned/zero-input -> GREEN (scoped ACTION_MOVE input fallback)
  proposed/no FIFO/non-owned -> RED (returns; human/flagOFF preserved)

The proposed block adds a scoped WarsBotLadderServer input fallback and keeps the
native velocity past the animation gate. It does not copy the climb arithmetic.

Usage:
  python ds112_ladder_climb_fixture.py --repo QA/build/ds076-code --rev <sha> --out <temp>
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


def actual_climb_block(source: str) -> str:
    start = source.index('move *= m_movement.desiredVelocity.y')
    end = source.index('m_request.velocity = move;', start) + len('m_request.velocity = move;')
    return source[start:end]


CPP = r'''
#define WARSBOTS_EXPERIMENTAL_SPAWNER 1
#include <cstdio>
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstring>
using std::max;
typedef unsigned int uint32;
static float cry_fabsf(float v){return std::fabs(v);}
static float cry_floorf(float v){return std::floor(v);}

struct ColorB { ColorB(int,int,int,int){} };
struct ColorF { ColorF(float,float,float,float){} };
struct IRenderAuxGeom { void DrawSphere(const struct Vec3&,float,ColorB){} void DrawLine(const struct Vec3&,ColorB,float){} };
struct IRenderer { IRenderAuxGeom aux; IRenderAuxGeom* GetIRenderAuxGeom(){return &aux;} void Draw2dLabel(int,int,float,float*,bool,const char*,...){} };
struct Vec3 {
 float x=0,y=0,z=0;
 Vec3(){} Vec3(float a,float b,float c):x(a),y(b),z(c){}
 Vec3& operator*=(float s){x*=s;y*=s;z*=s;return *this;}
 Vec3& operator+=(const Vec3& o){x+=o.x;y+=o.y;z+=o.z;return *this;}
 Vec3& operator/=(float s){x/=s;y/=s;z/=s;return *this;}
 float len() const {return std::sqrt(x*x+y*y+z*z);}
 float len2() const {return x*x+y*y+z*z;}
};
struct Mat34 { Vec3 t; };
struct Quat { Vec3 c1; };
struct IAnimationGraphState { void SetInput(const char*, const char*){} };
struct IAnimatedCharacter { IAnimationGraphState ag; IAnimationGraphState* GetAnimationGraphState(){return &ag;} };
struct CAnimation { struct { unsigned m_nFlags=0; } m_AnimParams; };
struct ISkeletonAnim {
 int count=0; CAnimation anim; bool manualSeek=false;
 int GetNumAnimsInFIFO(int){return count;}
 CAnimation& GetAnimFromFIFO(int,int){return anim;}
 void ManualSeekAnimationInFIFO(int,int,float,bool){manualSeek=true;}
};
struct ICharacterInstance { ISkeletonAnim* sa=nullptr; ISkeletonAnim* GetISkeletonAnim(){return sa;} };
struct Entity {
 Vec3 pos; ICharacterInstance* ch=nullptr;
 Vec3 GetWorldPos() const {return pos;}
 ICharacterInstance* GetCharacter(int){return ch;}
 void SetWorldTM(const Mat34&){}
 Mat34 GetWorldTM() const {Mat34 m;m.t=pos;return m;}
 Quat GetRotation() const {return Quat();}
 void SetRotation(const Quat&){}
};
struct IEntityAreaProxy { };
using EntityId = unsigned;

struct Env { bool bServer=true; struct Timer { float GetFrameTime(){return 1.0f/60.0f;} } timer; Timer* pTimer=&timer; IRenderer renderer; IRenderer* pRenderer=&renderer; };
Env* gEnv=new Env();
struct Manager { bool owns=true; bool Owns(EntityId) const {return owns;} };
struct Game { Manager mgr; Manager& GetBotManager(){return mgr;} };
Game* g_pGame=new Game();
namespace WarsBots { struct BotConfig { int ladder=1; static BotConfig& Get(){static BotConfig c;return c;} }; }
struct NCVars { int pl_debug_ladders=0; };
NCVars* g_pGameCVars=new NCVars();
#define WB_LOG(...) ((void)0)
#define ACTION_SPRINT 1
#define ACTION_JUMP 2
#define ACTION_MOVE 4
#define CA_MANUAL_UPDATE 1u
#define NANOMODE_SPEED 2
enum EStance { STANCE_STAND=0 };
enum ECharMoveType { eCMT_None=0, eCMT_Normal, eCMT_Fly };

struct SPlayerStats { Vec3 ladderBottom, ladderTop, ladderUpDir, ladderOrientation, playerRotation; };
struct SActorFrameMovementParams { Vec3 desiredVelocity; };
struct SCharacterMoveRequest { ECharMoveType type=eCMT_None; Vec3 velocity; };
struct NanoSuit { int GetMode(){return 0;} float GetSprintMultiplier(bool){return 1.f;} };

struct CPlayer {
 enum ELadderState { eLS_ExitTop, eLS_Exit, eLS_Climb };
 enum ELadderDirection { eLDIR_Up, eLDIR_Down };
 enum ELadderActionType { eLAT_None, eLAT_Jump };
 Entity e; IAnimatedCharacter ac; IAnimatedCharacter* m_pAnimatedCharacter=&ac;
 NanoSuit* suit=nullptr; bool warsBot=true; unsigned id=1;
 Entity* GetEntity(){return &e;}
 IAnimatedCharacter* GetAnimationGraphState(){return &ac;}
 float GetStanceMaxSpeed(EStance) const {return 2.5f;}
 NanoSuit* GetNanoSuit(){return suit;}
 void RequestLeaveLadder(ELadderActionType){}
 bool IsWarsBotActor() const {return warsBot;}
 EntityId GetEntityId() const {return id;}
 @UPDATE_LADDER_ANIMATION@
};

struct CCVar { };

@WARSBOT_HELPER@

struct CPlayerMovement {
 SActorFrameMovementParams m_movement;
 SCharacterMoveRequest m_request;
 int m_actions=0;
 CPlayer& m_player;
 CPlayerMovement(CPlayer& p):m_player(p){}
 void AdjustPlayerPositionOnLadder(CPlayer&){}
 void OldClimb(CPlayer& player)
 {
  Vec3 move(0,0,1); float bottomDist=0.f;
  @OLD_BLOCK@
 }
 void ProposedClimb(CPlayer& player)
 {
  Vec3 move(0,0,1); float bottomDist=0.f;
  @NEW_BLOCK@
 }
};

static int fails=0;
static void check(bool ok,const char* what){if(!ok){std::printf("FAIL %s\n",what);++fails;}}
static bool flew(const SCharacterMoveRequest& r){return r.type==eCMT_Fly && r.velocity.len()>0.01f;}

int main(){
 // old/no FIFO: actual animation gate returns before eCMT_Fly.
 {CPlayer p; CPlayerMovement m(p); m.m_movement.desiredVelocity.y=1.f; p.e.ch=nullptr; m.OldClimb(p);
  check(!flew(m.m_request),"old/no-FIFO must not set native Fly (RED)");}
 // old/manual FIFO: control proves the gate can pass and the real block then sets Fly.
 {CPlayer p; CPlayerMovement m(p); m.m_movement.desiredVelocity.y=1.f; ICharacterInstance ci; ISkeletonAnim sa; sa.count=1; sa.anim.m_AnimParams.m_nFlags=CA_MANUAL_UPDATE; ci.sa=&sa; p.e.ch=&ci; m.OldClimb(p);
  check(flew(m.m_request),"old/manual-FIFO control must set Fly");}
 // proposed/no FIFO/owned: scoped fallback keeps native Fly input.
 {CPlayer p; CPlayerMovement m(p); m.m_movement.desiredVelocity.y=1.f; p.e.ch=nullptr; p.warsBot=true; gEnv->bServer=true; g_pGame->mgr.owns=true; WarsBots::BotConfig::Get().ladder=1; m.ProposedClimb(p);
  check(flew(m.m_request),"proposed/owned/no-FIFO must set Fly (GREEN)");}
 // proposed/no FIFO/owned/zero-input: ACTION_MOVE fallback supplies native up input.
 {CPlayer p; CPlayerMovement m(p); m.m_movement.desiredVelocity.y=0.f; m.m_actions=ACTION_MOVE; p.e.ch=nullptr; p.warsBot=true; gEnv->bServer=true; g_pGame->mgr.owns=true; WarsBots::BotConfig::Get().ladder=1; m.ProposedClimb(p);
  check(flew(m.m_request),"proposed/owned/zero-input fallback must set Fly (GREEN)");}
 // proposed/no FIFO/non-owned: human/flagOFF/non-owned unchanged.
 {CPlayer p; CPlayerMovement m(p); m.m_movement.desiredVelocity.y=1.f; p.e.ch=nullptr; p.warsBot=false; gEnv->bServer=true; g_pGame->mgr.owns=true; WarsBots::BotConfig::Get().ladder=1; m.ProposedClimb(p);
  check(!flew(m.m_request),"proposed/non-owned must stay no-Fly");}
 if(fails){std::printf("DS112 climb fixture FAILED (%d)\n",fails);return 1;}
 std::printf("DS112 actual climb fixture: old RED, manual-FIFO control, proposed GREEN, non-owned preserved\n");
 return 0;
}
'''


def build(repo: Path, rev: str, out: Path) -> Path:
    def show(path: str) -> str:
        return subprocess.check_output(['git', '-C', str(repo), 'show', f'{rev}:{path}']).decode('utf-8-sig')

    move = show('Code/CryGame/Actors/Player/PlayerMovement.cpp')
    player = show('Code/CryGame/Actors/Player/Player.cpp')
    block = actual_climb_block(move)
    update = method(player, 'bool CPlayer::UpdateLadderAnimation(')
    helper = method(move, 'static bool WarsBotLadderServer(')

    gate = ('\t\tif (!player.UpdateLadderAnimation(CPlayer::eLS_Climb, eLDir, animTime))\n'
            '\t\t\treturn;')
    proposed_gate = ('\t\tif (!player.UpdateLadderAnimation(CPlayer::eLS_Climb, eLDir, animTime))\n'
                     '\t\t{\n'
                     '\t\t\t// DS112: headless owned-server bots have no manual-update ladder FIFO.\n'
                     '\t\t\t// Keep the real native velocity for the gated bot; humans/clients still require animation.\n'
                     '\t\t\tif (!WarsBotLadderServer(player)) return;\n'
                     '\t\t}')
    if gate not in block:
        raise RuntimeError('actual animation gate marker missing')
    proposed = block.replace(gate, proposed_gate, 1)
    input_marker = 'move *= m_movement.desiredVelocity.y * 0.5f;'
    input_fallback = ('// DS112: headless owned bot may have no controller-projected ladder input.\n'
                      ' if (WarsBotLadderServer(player) && (m_actions & ACTION_MOVE) &&\n'
                      '  cry_fabsf(m_movement.desiredVelocity.y) < 0.01f)\n'
                      '  m_movement.desiredVelocity.y = 1.0f;\n'
                      + input_marker)
    if input_marker not in proposed:
        raise RuntimeError('actual input marker missing')
    proposed = proposed.replace(input_marker, input_fallback, 1)
    cpp = CPP.replace('@UPDATE_LADDER_ANIMATION@', update)
    cpp = cpp.replace('@WARSBOT_HELPER@', helper)
    cpp = cpp.replace('@OLD_BLOCK@', block)
    cpp = cpp.replace('@NEW_BLOCK@', proposed)
    out.mkdir(parents=True, exist_ok=True)
    unit = out / 'ds112_climb.cpp'
    unit.write_text(cpp, encoding='utf-8')
    return unit


def run(repo: Path, rev: str, out: Path) -> int:
    unit = build(repo, rev, out)
    exe = out / 'ds112_climb.exe'
    comp = subprocess.run(['cl', '/nologo', '/std:c++17', '/EHsc', str(unit), '/Fo:' + str(out) + '/', '/Fe:' + str(exe)], capture_output=True)
    (out / 'compile.log').write_bytes(comp.stdout + comp.stderr)
    if comp.returncode:
        print((comp.stdout + comp.stderr).decode(errors='replace'))
        raise RuntimeError('compile failure is not semantic RED')
    res = subprocess.run([str(exe)], capture_output=True)
    (out / 'run.log').write_bytes(res.stdout + res.stderr)
    print(res.stdout.decode(errors='replace'))
    return res.returncode


def main():
    ap = argparse.ArgumentParser()
    root = Path(__file__).resolve().parents[3]
    ap.add_argument('--repo', type=Path, default=root)
    ap.add_argument('--rev', default='3cfd859676d83bdecc1b091d3b9202323b039135')
    ap.add_argument('--out', type=Path, default=Path(__file__).resolve().parent / 'ds112-out')
    args = ap.parse_args()
    return run(args.repo, args.rev, args.out.resolve())


if __name__ == '__main__':
    raise SystemExit(main())
