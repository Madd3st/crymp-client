
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
 bool NeedsWarsBotLadderPhysicsExit() const {return false;}
 void SetWarsBotLadderExitPhysics(bool,bool) {}
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
	bool m_warsBotLadderExitTargetValid = false;
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
bool TryWarsBotLadderExitRetry(IAnimationGraphState* graph) {
	if (!graph || !m_warsBotLadderGraphOverride || graph!=m_warsBotLadderGraph) return false;
	if (!gEnv || !gEnv->bServer || !g_pGame || !IsWarsBotActor() || !g_pGame->GetBotManager().Owns(GetEntityId()))
		return false;
	if (WarsBots::BotConfig::Get().ladder!=1) return false;
	if (!m_stats.isOnLadder.Value() || !m_stats.isExitingLadder) return false;
	if (GetHealth()<=0 || IsFrozen() || m_stats.isRagDoll) return false;
	// Only after the frame's AC mode handoff: PrePhysicsUpdate computes AlwaysPhysics on the trigger frame and
	// PerAnimGraph from the next frame, so a retry is meaningful only once graph motion is actually selected.
	if (!m_pAnimatedCharacter || !(m_pAnimatedCharacter->GetParams().flags & eACF_PerAnimGraph)) return false;
	if (m_warsBotLadderExitRetryUsed) return false;
	const IAnimationGraphState::InputID signal=graph->GetInputId("Signal");
	const char* signalName=graph->GetInputName(signal);
	if (!signalName || std::strcmp(signalName,"Signal")!=0) return false;
	char current[256]={};
	graph->GetInput(signal,current);
	// The stock signal may still be pending: never overwrite/resend it. Only retry from the default/consumed value.
	if (!graph->IsDefaultInputValue(signal)) return false;
	char state[256]={};
	if (const char* currentState=graph->GetCurrentStateName())
		std::strncpy(state,currentState,sizeof(state)-1);
	// Preserve an already playing/pending exit clip.
	if (std::strstr(state,"ladderOffTop")) return false;
	m_warsBotLadderExitRetryUsed=true; // consume even a rejected call: never retry every frame
	if (!graph->SetInput(signal,"exit_ladder_top")) return false;
	m_warsBotLadderExitSignalPending=true;
	WB_LOG(2,"[WarsBots] LADDER EXIT RETRY actor=%u state=%s previousSignal=%s one_shot=1.",
		GetEntityId(),state,current[0]?current:"<default>");
	return true;
}
void ResetWarsBotLadderExitRetry() {
	m_warsBotLadderExitRetryUsed=false;
	m_warsBotLadderExitSignalPending=false;
}
void ClearWarsBotLadderExitRetry() {
	const bool pending=m_warsBotLadderExitSignalPending;
	m_warsBotLadderExitRetryUsed=false;
	m_warsBotLadderExitSignalPending=false;
	if (!pending || !gEnv || !gEnv->bServer || !g_pGame || !IsWarsBotActor() ||
		!g_pGame->GetBotManager().Owns(GetEntityId())) return;
	IAnimationGraphState* graph=m_pAnimatedCharacter ? m_pAnimatedCharacter->GetAnimationGraphState() : nullptr;
	if (!graph || !m_warsBotLadderGraphOverride || graph!=m_warsBotLadderGraph) return;
	const IAnimationGraphState::InputID signal=graph->GetInputId("Signal");
	const char* signalName=graph->GetInputName(signal);
	if (!signalName || std::strcmp(signalName,"Signal")!=0) return;
	char current[256]={};
	graph->GetInput(signal,current);
	if (std::strcmp(current,"exit_ladder_top")!=0) return; // consumed already or a foreign signal: never clear it
	graph->ClearInput(signal); // restore the default, not a guessed default string
	WB_LOG(2,"[WarsBots] LADDER EXIT RETRY cleared actor=%u.",GetEntityId());
}
void RestoreWarsBotLadderGraph() {
	m_warsBotLadderExitTargetValid=false;
	SetWarsBotLadderExitPhysics(false,false); // release our collider layer even if the graph was replaced
	ClearWarsBotLadderExitRetry(); // also consume orphan latches before the early return
	if (!m_warsBotLadderGraphOverride) return;
	m_warsBotLadderGraphOverride=false; // consume even if teardown has replaced the graph or entity ownership
	IAnimationGraphState* graph=m_pAnimatedCharacter ? m_pAnimatedCharacter->GetAnimationGraphState() : nullptr;
	if (gEnv && gEnv->bServer && g_pGame && IsWarsBotActor() &&
		g_pGame->GetBotManager().Owns(GetEntityId()) && graph && graph==m_warsBotLadderGraph)
	{
		if (m_warsBotLadderGraphStanceDefault || m_warsBotLadderGraphStance.empty())
			graph->ClearInput(m_warsBotLadderGraphStanceId);
		else graph->SetInput(m_warsBotLadderGraphStanceId,m_warsBotLadderGraphStance.c_str());
		graph->SetAnimationActivation(m_warsBotLadderGraphWasActive);
		WB_LOG(2,"[WarsBots] LADDER GRAPH restored actor=%u active=%d.",GetEntityId(),m_warsBotLadderGraphWasActive?1:0);
	}
	m_warsBotLadderGraph=nullptr; m_warsBotLadderGraphStance.clear();
}
float CalculatePseudoSpeed(bool wantSprint) const {
	CNanoSuit* pSuit = GetNanoSuit();
	bool inSpeedMode = ((pSuit != NULL) && (pSuit->GetMode() == NANOMODE_SPEED));
	bool hasEnergy = (pSuit == NULL) || (pSuit->GetSuitEnergy() > (0.2f * NANOSUIT_ENERGY)); // AI has no suit and can sprint endlessly.

	if (!wantSprint)
		return 1.0f;
	else if (!inSpeedMode || !hasEnergy)
		return 2.0f;
	else
		return 3.0f;
}
	void LeaveExitTopBlock() { const int reason = eLAT_ExitTop; (void)reason; {
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
		ResetWarsBotLadderExitRetry(); // native signal below; one recovery attempt starts unused
#endif
		m_stats.isExitingLadder = true;
		UpdateLadderAnimation(CPlayer::eLS_ExitTop, eLDIR_Up);
	} }
};

static bool WarsBotLadderServer(const CPlayer& player)
{
	return gEnv && gEnv->bServer && g_pGame && player.IsWarsBotActor() && WarsBots::BotConfig::Get().ladder == 1 &&
		g_pGame->GetBotManager().Owns(player.GetEntityId());
}

struct MockMovement
{
 void ProcessWarsBotLadderPhysicsExit(CPlayer&,bool& landed){landed=false;}
	SPlayerStats& m_stats;
	CPlayer& m_player;
	MockMovement(CPlayer& p) : m_stats(p.st), m_player(p) {}
	void ExitBlock(CPlayer& player) 
	{
{
		const bool warsBot = WarsBotLadderServer(player);
		if (gEnv->bClient || warsBot)
		{
			IAnimatedCharacter* pAC = m_player.GetAnimatedCharacter();
			IAnimationGraphState* pAGState = pAC ? pAC->GetAnimationGraphState() : NULL;
			const char* agOutputOnLadder = pAGState ? pAGState->QueryOutput("OnLadder") : NULL;
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
			// DS-124: copy the scalar output and the state text BEFORE any graph write. SetInput may replace the
			// queried state and invalidate the pointers above; nothing below may read them after a mutation.
			const bool warsBotHasOutput = warsBot && agOutputOnLadder;
			const char warsBotOutput = warsBotHasOutput ? agOutputOnLadder[0] : '\0';
			char warsBotState[256] = {};
			if (warsBot && pAGState)
			{
				if (const char* currentState = pAGState->GetCurrentStateName())
					std::strncpy(warsBotState, currentState, sizeof(warsBotState)-1);
			}
			if (warsBot)
			{
				const bool exitHeld = std::strstr(warsBotState, "ladderOffTop") != NULL;
				char currentSignal[256] = {};
				if (pAGState)
				{
					const IAnimationGraphState::InputID signalId = pAGState->GetInputId("Signal");
					const char* signalName = pAGState->GetInputName(signalId);
					if (signalName && std::strcmp(signalName, "Signal")==0) pAGState->GetInput(signalId, currentSignal);
				}
				float graphPseudo=-1.0f;
				if (pAGState)
				{
					const auto id=pAGState->GetInputId("PseudoSpeed");
					const char* name=pAGState->GetInputName(id);
					if (name && std::strcmp(name,"PseudoSpeed")==0) graphPseudo=pAGState->GetInputAsFloat(id);
				}
				ICharacterInstance* character = player.GetEntity()->GetCharacter(0);

				// C154: an accepted Signal is not proof that the installed actor can select/play its clip.
				// Read the live inputs and FIFO without forcing a state, loading an animation or changing motion.
				char action[256]={}, stance[256]={}, item[256]={};
				if (pAGState)
				{
					const auto readInput=[&](const char* key,char* value) {
						const auto id=pAGState->GetInputId(key);
						const char* name=pAGState->GetInputName(id);
						if (name && std::strcmp(name,key)==0) pAGState->GetInput(id,value);
					};
					readInput("Action",action); readInput("Stance",stance); readInput("Item",item);
				}
				IAnimationSet* animationSet=character?character->GetIAnimationSet():nullptr;
				ISkeletonAnim* skeleton=character?character->GetISkeletonAnim():nullptr;
				const int exitClip=animationSet?animationSet->GetAnimIDByName("relaxed_ladderOffTop_nw_01"):-1;
				const int fifo=skeleton?skeleton->GetNumAnimsInFIFO(0):0;
				int fifoClip=-1; float fifoTime=-1.f;
				if (fifo>0)
				{
					const CAnimation& animation=skeleton->GetAnimFromFIFO(0,fifo-1);
					fifoClip=animation.m_LMG0.m_nAnimID[0]; fifoTime=animation.m_fAnimTime;
				}
				const char* fifoName=animationSet&&fifoClip>=0?animationSet->GetNameByAnimID(fifoClip):nullptr;
				WB_LOG(2,"[WarsBots] LADDER EXIT INPUTS actor=%u action=%s stance=%s item=%s animationSet=%d exitClip=%d fifo=%d fifoClip=%d fifoName=%s fifoTime=%.3f model=%s.",
					player.GetEntityId(),action,stance,item,animationSet?1:0,exitClip,fifo,fifoClip,fifoName?fifoName:"<none>",fifoTime,
					character?character->GetFilePath():"<none>");

				WB_LOG(2,"[WarsBots] LADDER EXIT actor=%u graph=%d output=%d active=%d state=%s held=%d signal=%s pseudo=%.2f upd=%d motionFlags=%u pos=(%.3f,%.3f,%.3f).",
					player.GetEntityId(),pAGState?1:0,warsBotHasOutput?int(static_cast<unsigned char>(warsBotOutput)):-1,
					pAGState&&pAGState->GetAnimationActivation()?1:0,
					warsBotState[0]?warsBotState:"<none>",exitHeld?1:0,
					currentSignal[0]?currentSignal:"<default>",
					graphPseudo,
					character&&(character->GetFlags()&CS_FLAG_UPDATE_ALWAYS)?1:0,
					pAC?unsigned(pAC->GetParams().flags):0u,player.GetEntity()->GetWorldPos().x,
					player.GetEntity()->GetWorldPos().y,player.GetEntity()->GetWorldPos().z);
			}
#endif
			// DS-107: an owned bot must not turn a missing graph/output into a premature ReachedEnd. Keep the native
			// exiting state and let the policy fail closed (BlockedTop) unless the graph gives a real completion.
			if (warsBot && (!pAC || !pAGState || !agOutputOnLadder)) return;
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
			if (warsBot)
			{
				// The stock graph has OnLadder=1 only in ladder states; completion may return its empty default.
				// Empty is never completion by itself: require the same real physical egress as literal 0.
				if (warsBotOutput && warsBotOutput!='0' && warsBotOutput!='1') return;
				if (player.NeedsWarsBotLadderPhysicsExit())
				{
					bool landed=false;
					ProcessWarsBotLadderPhysicsExit(player,landed);
					if (landed) player.RequestLeaveLadder(CPlayer::eLAT_ReachedEnd);
					return;
				}
				if (warsBotOutput!='1')
				{
					const Vec3 feet=player.GetEntity()->GetWorldPos();
					const bool physicalReady=WarsBots::NativeLadderExitPhysicalReady(
						WarsBots::LVec{feet.x,feet.y,feet.z},
						WarsBots::LVec{m_stats.ladderBottom.x,m_stats.ladderBottom.y,m_stats.ladderBottom.z},
						WarsBots::LVec{m_stats.ladderTop.x,m_stats.ladderTop.y,m_stats.ladderTop.z});
					// Completion is checked first and returns immediately: a real exit must never be re-signalled.
					if (physicalReady)
					{
						player.RequestLeaveLadder(CPlayer::eLAT_ReachedEnd);
						return;
					}
					// Not complete: at most one bounded recovery signal for this owned native exit. It does not
					// move, teleport or complete the actor; the policy still fails closed after the exit budget.
					player.TryWarsBotLadderExitRetry(pAGState);
					return;
				}
			}
#endif
			bool onLadder = agOutputOnLadder && agOutputOnLadder[0] == '1';
			if (!onLadder)
				player.RequestLeaveLadder(CPlayer::eLAT_ReachedEnd);
		}
		return;
	}
	}

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
	bool ExecuteLadder(CPlayer& actor, const Vec3& moveTo, const Vec3& aim, float dt, bool nativeAttached) {
	const SPlayerStats* stats=actor.GetPlayerStats();
	if (!nativeAttached || !stats || !stats->isOnLadder.Value() || stats->isExitingLadder) return false;
	// The SafeStep exception exists only while the native ladder is attached. Approach and post-exit settle call
	// the ordinary safe executor instead.
	// PlayerMovementController divides desired speed by stance speed. Supply normal full forward input, not 1 m/s
	// (which would become a ~0.25 m/s climb and exhaust the route deadline before reaching the 9.5 m shaft top).
	// DS-124: the native non-sprint input value is CPlayer::CalculatePseudoSpeed(false) == 1.0
	// (Player.cpp:2503-2515), not BotMovement's 0.3/0.5 fallback. Match this graph input to the native value;
	// the bot still uses a move target rather than human delta input. This does not prove the exit root cause.
	const bool accepted = m_movement.Move(actor, moveTo, aim, dt, actor.GetStanceMaxSpeed(STANCE_STAND), STANCE_STAND, 1.5f,
		nativeAttached, false, actor.CalculatePseudoSpeed(false), false, 0.5f, false);
	actor.SetWarsBotActions(false, accepted, STANCE_STAND);
	if (!accepted) m_movement.Aim(actor, aim);
	return accepted;
}
};

static int fails = 0;
static bool expectNew = true;
static void check(bool ok, const char* what) { if (!ok) { std::printf("FAIL %s\n", what); ++fails; } }
static void expect(bool newSemantics, const char* what) { check(expectNew ? newSemantics : !newSemantics, what); }
static float effectivePseudo(float pseudo, float speed) {
 struct Request { float value=0;void SetPseudoSpeed(float v){value=v;} } request;
 request.SetPseudoSpeed(pseudo>=0.f?pseudo:(speed<2.0f?0.3f:0.5f));
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
