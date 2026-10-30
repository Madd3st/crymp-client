"""DS-124 actual-caller controls for the bounded native ladder-exit retry.

Extracts REAL source blocks (no copied decision logic) from Player.cpp, PlayerMovement.cpp, BotLocomotion.cpp and
BotLadder.h at a Git revision, compiles them against a small mock world, and checks the DS124 semantics:
  * at most one recovery signal per owned exit, only after the stock signal is consumed and offTop is not current;
  * completion output + physical egress are checked first and return without any retry;
  * no reset of a playing/pending exit clip, no unrelated input cleared, no replaced graph touched, no
    IsSignalledInput use as pending status;
  * ownership/life/frozen/ragdoll/OFF/graph/InputID/mode-handoff guards;
  * lifecycle reset on native ExitTop and cleanup on restore (cancel/done/OFF/death/revive/graph replacement);
  * WaitExit failure budget from the real BotLadder.h (failure only; the physical/grounded Done path still tested);
  * ExecuteLadder's real callee reaches the native CPlayer::CalculatePseudoSpeed(false) value 1.0.

Usage:
  python ds124_exit_retry_caller_test.py --repo <checkout> --rev abac72a9... --expect old --out <tmp>
  python ds124_exit_retry_caller_test.py --repo <patched tree>                    --expect new --out <tmp>

A compiler failure is never semantic RED: the driver raises on it. Exit 0 only when the selected expectation holds.
"""
from pathlib import Path
import argparse
import subprocess
import sys
import re


def read(repo: Path, rev: str, path: str) -> str:
    if rev == 'WORKTREE':
        return (repo / path).read_text(encoding='utf-8-sig')
    out = subprocess.run(['git', '-C', str(repo), 'show', f'{rev}:{path}'], capture_output=True, check=True)
    return out.stdout.decode('utf-8-sig', errors='replace')


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
    left = text.index('{', text.index(marker))
    depth = 0
    for i in range(left, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return text[left:i + 1]
    raise ValueError('unclosed block: ' + marker)


CPP_HEAD = r'''
#define WARSBOTS_EXPERIMENTAL_SPAWNER 1
#include "CryGame/Bots/BotLadder.h"
#include <cstdio>
#include <cstring>
#include <string>
#define WB_LOG(...) ((void)0)
#define CS_FLAG_UPDATE_ALWAYS (1u << 3)
#define eACF_PerAnimGraph 4u
using namespace WarsBots;

struct Vec3 { float x=0,y=0,z=0; Vec3(){} Vec3(float a,float b,float c):x(a),y(b),z(c){} };
struct Coherent { bool v=false; bool Value() const {return v;} void operator=(bool b){v=b;} };
struct SPlayerStats { Coherent isOnLadder; bool isExitingLadder=false, isRagDoll=false; Vec3 ladderTop, ladderBottom; };
struct CNanoSuit { int mode=0; float energy=100.f; int GetMode() const {return mode;} float GetSuitEnergy() const {return energy;} };
enum { NANOMODE_SPEED = 1 };
static const float NANOSUIT_ENERGY = 100.f;
enum EStance { STANCE_STAND = 0 };

struct IAnimationGraphState
{
	using InputID = unsigned;
	std::string out = "0", state = "RelaxedIdleNW+NothingNW", signal = "none";
	bool validName = true, failSet = false, active = true, signalDefault = true;
	int signalSets = 0, signalAttempts = 0, otherSets = 0, signalledQueries = 0;
	const char* QueryOutput(const char*) { return out.c_str(); }
	const char* GetCurrentStateName() { return state.c_str(); }
	InputID GetInputId(const char* n) { return std::strcmp(n, "Signal") == 0 ? 1u : 2u; }
	const char* GetInputName(InputID id) const { if (id == 1u) return validName ? "Signal" : "Wrong"; return "PseudoSpeed"; }
	void GetInput(InputID id, char* buf) const { std::strcpy(buf, id == 1u ? signal.c_str() : "0.5"); }
	bool IsDefaultInputValue(InputID id) const { return id==1u && signalDefault && (signal.empty() || signal=="none"); }
	float GetInputAsFloat(InputID) { return 0.5f; }
	bool SetInput(InputID id, const char* v)
	{
		if (id != 1u) { ++otherSets; return true; }
		++signalAttempts; if (failSet) return false;
		++signalSets; signal = v;
		// Simulate the AG replacing the queried state: pointers returned before this call may become stale.
		out = "9"; state = "RelaxedRunNW+NothingNW";
		return true;
	}
	bool SetInput(const char* n, const char* v) { return SetInput(GetInputId(n), v); }
	void ClearInput(InputID id) { if (id == 1u) signal = "none"; }
	void SetAnimationActivation(bool a) { active = a; }
	bool GetAnimationActivation() { return active; }
	bool IsSignalledInput(InputID) const { ++const_cast<IAnimationGraphState*>(this)->signalledQueries; return true; }
};

struct CAnimation { struct { int m_nAnimID[1]={0}; } m_LMG0; float m_fAnimTime=0.f; };
struct IAnimationSet { int GetAnimIDByName(const char*) {return 1;} const char* GetNameByAnimID(int) {return "mock";} };
struct ISkeletonAnim { int GetNumAnimsInFIFO(int) {return 0;} CAnimation& GetAnimFromFIFO(int,int) {static CAnimation a;return a;} };
struct ICharacterInstance { virtual int GetFlags() = 0; IAnimationSet* GetIAnimationSet() {return nullptr;} ISkeletonAnim* GetISkeletonAnim() {return nullptr;} const char* GetFilePath() {return "mock";} };
struct MockChar : ICharacterInstance { int flags = CS_FLAG_UPDATE_ALWAYS; int GetFlags() override { return flags; } };
struct SAnimatedCharacterParams { unsigned flags = eACF_PerAnimGraph; };
struct Entity { Vec3 p; MockChar ch; Vec3 GetWorldPos() const { return p; } MockChar* GetCharacter(int) { return &ch; } };
struct MockGraph;
struct IAnimatedCharacter
{
	MockGraph* g = nullptr;
	SAnimatedCharacterParams params;
	SAnimatedCharacterParams& GetParams() { return params; }
	IAnimationGraphState* GetAnimationGraphState();
};
struct MockGraph : IAnimationGraphState {};
IAnimationGraphState* IAnimatedCharacter::GetAnimationGraphState() { return g; }

struct MockEnv { bool bServer = true, bClient = false; } env;
static MockEnv* gEnv = &env;
struct MockManager { bool owns = true; bool Owns(unsigned) { return owns; } };
struct MockGame { MockManager mgr; MockManager& GetBotManager() { return mgr; } } game;
static MockGame* g_pGame = &game;
namespace WarsBots { struct BotConfig { int ladder=1; static BotConfig& Get(){ static BotConfig c; return c; } }; }

struct CPlayer
{
	enum ELadderActionType { eLAT_None, eLAT_ReachedEnd, eLAT_ExitTop, eLAT_Jump, eLAT_StrafeLeft, eLAT_StrafeRight, eLAT_Use, eLAT_Die };
	enum ELadderState { eLS_Exit = 0, eLS_ExitTop, eLS_Climb };
	enum ELadderDirection { eLDIR_Stationary = 0, eLDIR_Up, eLDIR_Down };
	SPlayerStats st;
	SPlayerStats& m_stats = st;
	MockGraph graph, other;
	IAnimatedCharacter* m_pAnimatedCharacter = nullptr;
	IAnimatedCharacter ac;
	Entity ent;
	CNanoSuit* suit = nullptr;
	IAnimationGraphState* m_warsBotLadderGraph = nullptr;
	bool m_warsBotLadderGraphOverride = false, m_warsBotLadderGraphWasActive = false;
	bool m_warsBotLadderGraphStanceDefault = true, m_warsBotLadderExitRetryUsed = false, m_warsBotLadderExitSignalPending = false;
	unsigned char m_warsBotLadderGraphStanceId = 0;
	std::string m_warsBotLadderGraphStance;
	bool warsBot = true, frozen = false;
	int health = 100, leaves = 0;
	CPlayer() { m_pAnimatedCharacter = &ac; ac.g = &graph; m_warsBotLadderGraph = &graph; }
	bool IsWarsBotActor() const { return warsBot; }
	unsigned GetEntityId() const { return 7; }
	int GetHealth() const { return health; }
	bool IsFrozen() const { return frozen; }
	SPlayerStats* GetPlayerStats() { return &st; }
	Entity* GetEntity() { return &ent; }
	IAnimatedCharacter* GetAnimatedCharacter() { return m_pAnimatedCharacter; }
	CNanoSuit* GetNanoSuit() const { return suit; }
	float GetStanceMaxSpeed(EStance) const { return 4.0f; }
	void RequestLeaveLadder(ELadderActionType) { ++leaves; st.isOnLadder = false; st.isExitingLadder = false; }
	void SetWarsBotActions(bool, bool, int) {}
	bool UpdateLadderAnimation(ELadderState, ELadderDirection) { return false; }
@RETRY@
@RESET@
@CLEAR@
@RESTORE@
@PSEUDO@
	void LeaveExitTopBlock() { const int reason = eLAT_ExitTop; (void)reason; @EXIT_TOP@ }
};

@HELPER@

struct MockMovement
{
	SPlayerStats& m_stats;
	CPlayer& m_player;
	MockMovement(CPlayer& p) : m_stats(p.st), m_player(p) {}
	void ExitBlock(CPlayer& player) @EXIT_BLOCK@
};

struct MockBotMovement
{
	int moves = 0; float lastSpeed = 0, lastProbe = 0, lastPseudo = 0, lastArrive = 0; bool lastJump = false;
	bool Move(CPlayer&, const Vec3&, const Vec3&, float, float speed, EStance, float probe, bool jumpTraverse,
		bool, float pseudo, bool, float arrive, bool)
	{ ++moves; lastSpeed=speed; lastProbe=probe; lastPseudo=pseudo; lastArrive=arrive; lastJump=jumpTraverse; return true; }
	void Aim(CPlayer&, const Vec3&) {}
	void Look(CPlayer&, const Vec3&) {}
	void Stop(CPlayer&) {}
};
struct MockLoco
{
	MockBotMovement m_movement;
	void Look(CPlayer&, const Vec3&) {}
	@EXECUTE_LADDER@
};

static int fails = 0;
static bool expectNew = true;
static void check(bool ok, const char* what) { if (!ok) { std::printf("FAIL %s\n", what); ++fails; } }
static void expect(bool newSemantics, const char* what) { check(expectNew ? newSemantics : !newSemantics, what); }
static float effectivePseudo(float pseudo, float speed) {
 struct Request { float value=0;void SetPseudoSpeed(float v){value=v;} } request;
 @ACTUAL_MOVE_PSEUDO@
 return request.value;
}

static void Arm(CPlayer& p)
{
	p.ac.params.flags = eACF_PerAnimGraph;
	p.m_warsBotLadderGraph = &p.graph;
	p.m_warsBotLadderGraphOverride = true;
	p.graph.out = "0"; p.graph.state = "RelaxedIdleNW+NothingNW"; p.graph.signal = "none";
	p.graph.signalSets = 0; p.graph.signalAttempts = 0; p.graph.signalDefault = true; p.graph.otherSets = 0; p.graph.failSet = false; p.graph.validName = true;
	p.st.isOnLadder = true; p.st.isExitingLadder = true; p.st.isRagDoll = false;
	p.ent.p = Vec3(0, 0, 10); p.st.ladderBottom = Vec3(0, 0, 10); p.st.ladderTop = Vec3(0, 0, 20);
	p.warsBot = true; p.frozen = false; p.health = 100; p.leaves = 0;
	p.m_warsBotLadderExitRetryUsed = false; p.m_warsBotLadderExitSignalPending = false;
	WarsBots::BotConfig::Get().ladder = 1; gEnv->bServer = true; gEnv->bClient = false; g_pGame->mgr.owns = true;
}

static void LadderWaitExit(BotLadder& L, float at, LadderInput& in)
{
	in = LadderInput();
	in.now = 0.f; in.grounded = true; in.feet = LVec{0, 0, 10}; in.onLadder = true;
	L.Start(0.f, LVec{0, 0, 10}, LVec{0, 0, 20});
	L.SetNative(LVec{0, 0, 10}, LVec{0, 0, 20}, LVec{0, 1, 0});
	L.Update(in);                 // Approach -> Grab -> Climb
	in.now = 0.5f; in.feet = LVec{0, 0, 19.5f}; in.exiting = true;
	L.Update(in);                 // Climb -> WaitExit at 0.5 s
	in.onLadder = true; in.exiting = true; in.now = at;
}

int main(int argc, char** argv)
{
	expectNew = argc > 1 && std::strcmp(argv[1], "new") == 0;

	// 1. ExecuteLadder real callee: native CalculatePseudoSpeed(false) == 1.0 reaches the Move() pseudo arg.
	{
		MockLoco loco; CPlayer p; p.st.isOnLadder = true; p.st.isExitingLadder = false;
		loco.ExecuteLadder(p, Vec3(0,0,10), Vec3(0,0,11), 0.1f, true);
		check(loco.m_movement.moves == 1, "ExecuteLadder real callee runs");
		check(loco.m_movement.lastProbe == 1.5f && loco.m_movement.lastArrive == 0.5f, "ExecuteLadder arg positions unchanged");
		check(effectivePseudo(loco.m_movement.lastPseudo, loco.m_movement.lastSpeed) == (expectNew ? 1.0f : 0.5f),
			"ExecuteLadder pseudo is native non-sprint parity");
		check(p.CalculatePseudoSpeed(false) == 1.0f, "native CalculatePseudoSpeed(false) contract");
	}

	// 2. One bounded retry, never two, only after the stock signal is default/consumed.
	{
		CPlayer p; Arm(p);
		p.TryWarsBotLadderExitRetry(&p.graph); p.TryWarsBotLadderExitRetry(&p.graph);
		expect(p.graph.signalSets == 1, "at most one bounded recovery signal per exit");
		expect(p.graph.signal == "exit_ladder_top", "recovery uses the stock exit_ladder_top value");
		check(p.graph.signalledQueries == 0, "IsSignalledInput is never used as pending status");
	}

	// 2b. The real exit block itself is bounded: repeated frames send at most one recovery signal, no completion.
	{
		CPlayer p; Arm(p); MockMovement m(p);
		m.ExitBlock(p); m.ExitBlock(p); m.ExitBlock(p);
		expect(p.graph.signalSets == 1, "exit block sends at most one recovery signal");
		check(p.leaves == 0, "no completion without physical egress");
	}

	// 3. Completion first: real physical egress completes and never re-signals.
	{
		CPlayer p; Arm(p); MockMovement m(p);
		p.ent.p = Vec3(0.6f, 0.6f, 19.5f);
		m.ExitBlock(p);
		check(p.leaves == 1, "physical egress completion leaves the ladder");
		check(p.graph.signalSets == 0, "no retry after real completion");
	}

	// 4. A playing/pending off-top clip is preserved; output 1 is never retried.
	{
		CPlayer p; Arm(p); MockMovement m(p);
		p.graph.out = "1"; p.graph.state = "relaxed_ladderOffTop_nw+NothingNW";
		m.ExitBlock(p); p.TryWarsBotLadderExitRetry(&p.graph);
		check(p.graph.signalSets == 0 && p.leaves == 0, "playing exit clip is preserved");
		Arm(p); p.graph.state = "relaxed_ladderOffTop_nw+NothingNW";
		p.TryWarsBotLadderExitRetry(&p.graph);
		check(p.graph.signalSets == 0, "off-top state blocks the retry");
	}

	// 5. Stock signal still pending: never resend/overwrite it.
	{
		CPlayer p; Arm(p); p.graph.signal = "exit_ladder_top";
		p.TryWarsBotLadderExitRetry(&p.graph);
		check(p.graph.signalSets == 0, "pending stock signal is not overwritten");
	}

	// 6. Guards: null/invalid ID/replaced graph/OFF/non-owned/dead/frozen/ragdoll/mode handoff.
	{
		CPlayer p; Arm(p);
		check(!p.TryWarsBotLadderExitRetry(nullptr), "null graph refuses");
		p.graph.validName = false; check(!p.TryWarsBotLadderExitRetry(&p.graph), "invalid Signal input name refuses");
		Arm(p); p.m_warsBotLadderGraph = &p.other; check(!p.TryWarsBotLadderExitRetry(&p.graph), "replaced graph refuses");
		Arm(p); WarsBots::BotConfig::Get().ladder = 0; check(!p.TryWarsBotLadderExitRetry(&p.graph), "OFF gate refuses");
		Arm(p); g_pGame->mgr.owns = false; check(!p.TryWarsBotLadderExitRetry(&p.graph), "non-owned refuses");
		Arm(p); p.health = 0; check(!p.TryWarsBotLadderExitRetry(&p.graph), "dead refuses");
		Arm(p); p.frozen = true; check(!p.TryWarsBotLadderExitRetry(&p.graph), "frozen refuses");
		Arm(p); p.st.isRagDoll = true; check(!p.TryWarsBotLadderExitRetry(&p.graph), "ragdoll refuses");
		Arm(p); p.ac.params.flags = 2u; check(!p.TryWarsBotLadderExitRetry(&p.graph), "pre-handoff AlwaysPhysics refuses");
	}

	// Integration controls: a rejected SetInput is still a consumed attempt, and default status uses the API.
	{
	 CPlayer p; Arm(p); p.graph.failSet=true;
	 p.TryWarsBotLadderExitRetry(&p.graph); p.TryWarsBotLadderExitRetry(&p.graph);
	 check(p.graph.signalAttempts==(expectNew?1:0),"rejected signal attempt never repeats");
	 check(!p.m_warsBotLadderExitSignalPending,"rejected signal is never owned pending");
	 Arm(p);p.graph.signal="none";p.graph.signalDefault=false;
	 p.TryWarsBotLadderExitRetry(&p.graph);
	 check(p.graph.signalSets==0,"nondefault API input preserved even when string resembles default");
	 Arm(p);p.m_warsBotLadderGraphOverride=false;
	 p.m_warsBotLadderExitRetryUsed=true;p.m_warsBotLadderExitSignalPending=true;
	 p.RestoreWarsBotLadderGraph();
	 expect(!p.m_warsBotLadderExitRetryUsed&&!p.m_warsBotLadderExitSignalPending,"orphan latches consumed before restore early return");
	}

	// 7. Human/client legacy path unchanged and never retried.
	{
		CPlayer p; Arm(p); MockMovement m(p); p.warsBot = false; gEnv->bClient = true;
		m.ExitBlock(p);
		check(p.leaves == 1 && p.graph.signalSets == 0, "human/client legacy exit is unchanged");
		gEnv->bClient = false;
	}

	// 8. Native ExitTop resets the retry latch; cleanup clears only our own still-current signal.
	{
		CPlayer p; Arm(p);
		p.m_warsBotLadderExitRetryUsed = true; p.m_warsBotLadderExitSignalPending = true;
		p.LeaveExitTopBlock();
		expect(!p.m_warsBotLadderExitRetryUsed && !p.m_warsBotLadderExitSignalPending, "ExitTop resets the retry latch");

		Arm(p); p.m_warsBotLadderExitRetryUsed = true; p.m_warsBotLadderExitSignalPending = true;
		p.graph.signal = "exit_ladder_top";
		p.RestoreWarsBotLadderGraph();
		expect(!p.m_warsBotLadderExitSignalPending && p.graph.signal == "none", "restore clears our pending signal");

		Arm(p); p.m_warsBotLadderExitSignalPending = true; p.graph.signal = "fall";
		p.m_warsBotLadderGraphOverride = true;
		p.RestoreWarsBotLadderGraph();
		check(p.graph.signal == "fall", "cleanup never clears an unrelated signal");

		Arm(p); p.m_warsBotLadderExitSignalPending = true; p.graph.signal = "exit_ladder_top";
		p.m_warsBotLadderGraph = &p.other; p.m_warsBotLadderGraphOverride = true;
		p.RestoreWarsBotLadderGraph();
		check(p.graph.signal == "exit_ladder_top" && p.other.signal == "none", "cleanup never touches a replaced graph");

		// OFF/revive cleanup still funnels through Restore (BotBrain gate_off -> CancelLadder, Player::Revive).
		Arm(p); p.m_warsBotLadderExitSignalPending = true; p.graph.signal = "exit_ladder_top";
		WarsBots::BotConfig::Get().ladder = 0;
		p.RestoreWarsBotLadderGraph();
		expect(p.graph.signal == "none", "OFF/revive cleanup still clears our pending signal");
	}

	// 9. WaitExit budget: failure-only bound, then the unchanged physical/grounded Done path.
	{
		BotLadder L; LadderInput in; LadderWaitExit(L, 0.5f + (expectNew ? 2.9f : 2.05f), in);
		LadderOutput o = L.Update(in);
		if (expectNew)
		{
			check(o.phase == LadderPhase::WaitExit, "exit budget does not fail before its bound");
			in.now = 3.6f; o = L.Update(in);
			check(o.phase == LadderPhase::Failed && o.fail == LadderFail::BlockedTop, "budget failure is BlockedTop");
		}
		else
		{
			check(o.phase == LadderPhase::Failed && o.fail == LadderFail::BlockedTop, "old 2.0s budget fails at 2.05s");
		}

		BotLadder D; LadderInput di; LadderWaitExit(D, 1.0f, di);
		di.onLadder = false; di.exiting = false; di.grounded = false; di.feet = LVec{0, 1.5f, 19.5f};
		o = D.Update(di);
		check(o.phase != LadderPhase::Done, "no timer/height-only success");
		di.grounded = true; di.feet = LVec{0, 0, 20};
		o = D.Update(di);
		check(o.phase == LadderPhase::Done, "grounded physical settle still completes");
	}

	if (fails) { std::printf("DS124 caller %s FAILED (%d)\n", expectNew ? "GREEN" : "RED", fails); return 1; }
	std::printf("DS124 caller %s PASS\n", expectNew ? "GREEN" : "RED");
	return 0;
}
'''


def build(repo: Path, rev: str, out: Path) -> None:
    player = read(repo, rev, 'Code/CryGame/Actors/Player/Player.cpp')
    move = read(repo, rev, 'Code/CryGame/Actors/Player/PlayerMovement.cpp')
    loco = read(repo, rev, 'Code/CryGame/Bots/BotLocomotion.cpp')
    ladder_h = read(repo, rev, 'Code/CryGame/Bots/BotLadder.h')

    def opt_method(text, sig, new_sig):
        try:
            body = method(text, sig)
        except ValueError:
            return None
        return new_sig + ' ' + body[body.index('{'):]

    retry = opt_method(player, 'bool CPlayer::TryWarsBotLadderExitRetry(',
                       'bool TryWarsBotLadderExitRetry(IAnimationGraphState* graph)')
    if retry is None:
        retry = 'bool TryWarsBotLadderExitRetry(IAnimationGraphState*) { return false; }'
    reset = opt_method(player, 'void CPlayer::ResetWarsBotLadderExitRetry(',
                       'void ResetWarsBotLadderExitRetry()')
    if reset is None:
        reset = 'void ResetWarsBotLadderExitRetry() {}'
    clear = opt_method(player, 'void CPlayer::ClearWarsBotLadderExitRetry(',
                       'void ClearWarsBotLadderExitRetry()')
    if clear is None:
        clear = 'void ClearWarsBotLadderExitRetry() {}'
    restore = opt_method(player, 'void CPlayer::RestoreWarsBotLadderGraph(',
                         'void RestoreWarsBotLadderGraph()')
    if restore is None:
        raise RuntimeError('RestoreWarsBotLadderGraph not found')
    pseudo = opt_method(player, 'float CPlayer::CalculatePseudoSpeed(',
                        'float CalculatePseudoSpeed(bool wantSprint) const')
    if pseudo is None:
        raise RuntimeError('CalculatePseudoSpeed not found')
    execute = method(loco, 'bool BotLocomotion::ExecuteLadder(')
    execute = ('bool ExecuteLadder(CPlayer& actor, const Vec3& moveTo, const Vec3& aim, float dt, bool nativeAttached) '
               + execute[execute.index('{'):])
    helper = method(move, 'static bool WarsBotLadderServer(')

    exit_block = block(move, 'if (m_stats.isExitingLadder)')
    exit_top = block(player, 'if (reason == eLAT_ExitTop)')

    move_body=read(repo, rev, 'Code/CryGame/Bots/BotMovement.cpp')
    actual_pseudo=re.search(r'request\.SetPseudoSpeed\([^;]+;',method(move_body,'bool BotMovement::Move(')).group(0)
    cpp = CPP_HEAD.replace('@ACTUAL_MOVE_PSEUDO@',actual_pseudo)
    cpp = cpp.replace('@RETRY@', retry)
    cpp = cpp.replace('@RESET@', reset)
    cpp = cpp.replace('@CLEAR@', clear)
    cpp = cpp.replace('@RESTORE@', restore)
    cpp = cpp.replace('@PSEUDO@', pseudo)
    cpp = cpp.replace('@HELPER@', helper)
    cpp = cpp.replace('@EXIT_BLOCK@', '\n\t{\n' + exit_block + '\n\t}\n')
    cpp = cpp.replace('@EXIT_TOP@', exit_top)
    cpp = cpp.replace('@EXECUTE_LADDER@', execute)

    out.mkdir(parents=True, exist_ok=True)
    (out / 'ds124_exit_retry.cpp').write_text(cpp, encoding='utf-8')
    inc = out / 'include/CryGame/Bots/BotLadder.h'
    inc.parent.mkdir(parents=True, exist_ok=True)
    inc.write_text(ladder_h, encoding='utf-8')


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[1])
    ap.add_argument('--rev', default='WORKTREE')
    ap.add_argument('--expect', choices=['old', 'new'], default='new')
    ap.add_argument('--out', type=Path, default=Path(__file__).resolve().parents[1]/'units/ladder-exit-retry-caller')
    ap.add_argument('--vcvars', default=None, help='optional vcvars batch to make cl available')
    args = ap.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / 'run.log').unlink(missing_ok=True)  # stale results cannot mask a compiler failure
    build(args.repo, args.rev, args.out)
    unit = args.out / 'ds124_exit_retry.cpp'
    exe = args.out / 'ds124_exit_retry.exe'
    include = args.out / 'include'
    cl = (f'cl /nologo /std:c++17 /EHsc /I "{include}" /I "{args.repo / "Code"}" '
          f'"{unit}" /Fo:"{args.out}\\\\" /Fe:"{exe}"')
    if args.vcvars:
        cmd = f'call "{args.vcvars}" >nul && {cl}'
        comp = subprocess.run(cmd, capture_output=True, shell=True)
    else:
        comp = subprocess.run(cl, capture_output=True, shell=True)
    (args.out / 'compile.log').write_bytes(comp.stdout + comp.stderr)
    if comp.returncode:
        sys.stdout.buffer.write(comp.stdout + comp.stderr)
        raise RuntimeError('fixture must compile; compiler failure is not semantic RED')
    run = subprocess.run([str(exe), args.expect], capture_output=True)
    (args.out / 'run.log').write_bytes(run.stdout + run.stderr)
    sys.stdout.buffer.write(run.stdout + run.stderr)
    return run.returncode


if __name__ == '__main__':
    raise SystemExit(main())
