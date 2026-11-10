
#define WARSBOTS_EXPERIMENTAL_SPAWNER 1
#include "CryGame/Bots/BotLadder.h"
#include "CryGame/Bots/BotCorners.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace WarsBots;
using EntityId = unsigned;
using std::max;
using std::min;

#define WB_LOG(...) ((void)0)
template<class T> T fabs_tpl(T v) { return (T)std::fabs((double)v); }

enum EStance { STANCE_STAND = 0 };
enum ENanoMode { NANOMODE_DEFENSE = 3 };
enum class BotGait { Walk, Run, Sprint };
enum class BotJump { None, Normal, Strength };
enum class BotCombatIntent { None, Aim, Fire };

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3() = default;
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    float len() const { return std::sqrt(x * x + y * y + z * z); }
    float GetLength2D() const { return std::sqrt(x * x + y * y); }
    float GetLengthSquared2D() const { return x * x + y * y; }
};

struct CoherentBool {
    bool v = false;
    bool Value() const { return v; }
    void operator=(bool b) { v = b; }
};
struct CoherentInt {
    int v = 0;
    int Value() const { return v; }
    void operator=(int x) { v = x; }
};

struct SPlayerStats {
    CoherentBool isOnLadder, isFrozen;
    CoherentInt inFreefall;
    bool isExitingLadder = false, isRagDoll = false, inZeroG = false, isGrabbed = false;
    int spectatorMode = 0, flyMode = 0;
    Vec3 ladderTop, ladderBottom, ladderOrientation;
};

struct Entity {
    Vec3 p;
    Vec3 GetWorldPos() const { return p; }
};

struct CPlayer {
    enum ELadderActionType { eLAT_None, eLAT_ReachedEnd, eLAT_ExitTop, eLAT_Jump, eLAT_StrafeLeft, eLAT_StrafeRight, eLAT_Use, eLAT_Die };
    EntityId id = 1;
    Entity e;
    SPlayerStats st;
    bool ladderUsable = false;
    bool swimming = false;
    int grabs = 0, leaves = 0, stopActions = 0;
    Vec3 exitTarget;
    SPlayerStats* GetPlayerStats() { return &st; }
    Entity* GetEntity() { return &e; }
    EntityId GetEntityId() const { return id; }
    bool IsFrozen() const { return st.isFrozen.Value(); }
    void* GetLinkedEntity() { return nullptr; }
    bool ShouldSwim() const { return swimming; }
    bool IsLadderUsable() { return ladderUsable; }
    void RequestGrabOnLadder(ELadderActionType) { ++grabs; st.isOnLadder = true; }
    void RequestLeaveLadder(ELadderActionType) { ++leaves; st.isOnLadder = false; st.isExitingLadder = false; }
    void RestoreWarsBotLadderGraph() {}
    void SetWarsBotLadderExitTarget(const Vec3& t) { exitTarget = t; }
    void SetWarsBotActions(bool, bool move, int) { if (!move) ++stopActions; }
};

struct NavQueryBounds { float side = 1.5f, up = 1.0f, down = 3.0f; };
struct NavPathLink { int corner = 0, kind = 0; float a[3] = {}, b[3] = {}; unsigned long long ref = 0; };
enum NavLinkKind { NavLinkDrop = 0, NavLinkJump, NavLinkStrength, NavLinkLift, NavLinkLadder };

struct NavMeshCore {
    int bans = 0;
    unsigned long long lastBannedRef = 0;
    void BanLink(unsigned long long ref) { ++bans; lastBannedRef = ref; }
    bool Project(const float* q, float* s, float = 0.2f, float = 0.6f, float = 0.6f) const {
        s[0] = q[0]; s[1] = q[1]; s[2] = q[2];
        return true;
    }
    bool RayClear(const float*, const float*, const NavQueryBounds* = nullptr) const { return true; }
};

namespace WarsBots {
struct BotConfig {
    int ladder = 1;
    int subExit = 0;   // SPEC-006a queue OFF here: this regression covers the existing gate/yield path
    static BotConfig& Get() { static BotConfig c; return c; }
};
}

struct BotBlackboard {
    int health = 100;
    Vec3 position, velocity;
    bool grounded = true, canMove = true, suitActive = true, swimming = false, waterMovement = false;
    int mode = NANOMODE_DEFENSE;
    float energy = 100.f, waterZ = -1e6f;
    bool waterValid = false;
};

static bool WaterStartBounds(const BotBlackboard&, float, NavQueryBounds&) { return false; }

struct BotCombat {
    int stopFires = 0;
    void StopFire() { ++stopFires; }
};

struct BotIntent {
    bool move = false;
    Vec3 destination, aim;
    BotGait gait = BotGait::Run;
    BotJump jump = BotJump::None;
    int suitMode = NANOMODE_DEFENSE;
    BotCombatIntent combat = BotCombatIntent::None;
    bool linkJump = false, meshPath = false;
    float arrive = 0.8f;
    struct Why { short suitMode = 0, jump = 0, gait = 0, aim = 0, move = 0; } why;
};
#define WBI(field, value) (m_intent.field = (value), m_intent.why.field = (short)__LINE__)

struct MockLoco {
    int stops = 0, looks = 0, executes = 0, ladderExecutes = 0;
    void Stop(CPlayer&) { ++stops; }
    void Look(CPlayer&, const Vec3&) { ++looks; }
    bool Execute(CPlayer&, const BotIntent&, BotBlackboard&, float, float) { ++executes; return true; }
    bool ExecuteLadder(CPlayer&, const Vec3&, const Vec3&, float, bool) { ++ladderExecutes; return true; }
};

struct MockTrap {
    int resets = 0;
    void Reset() { ++resets; }
};

struct MockLink {
    bool active = false;
    bool Active() const { return active; }
};

struct BotBrain;
struct BotInstance {
    bool ai = true;
    BotBrain* brain = nullptr;
    bool AIEnabled() const { return ai; }
    BotBrain& Brain() const { return *brain; }
};

struct BotManager {
    std::unordered_map<EntityId, std::unique_ptr<BotInstance>> m_bots;
    const BotInstance* Find(EntityId id) const {
        auto it = m_bots.find(id);
        return it != m_bots.end() ? it->second.get() : nullptr;
    }
    bool AIEnabled(EntityId id) const;
    bool ShouldYieldLadder(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const;
};

struct MockGame {
    BotManager mgr;
    BotManager& GetBotManager() { return mgr; }
};
static MockGame g_game;
static MockGame* g_pGame = &g_game;
static NavMeshCore g_mesh;
static NavMeshCore* CurrentNavMesh() { return &g_mesh; }

struct BotBrain {
    BotLadder m_ladder;
    NavPathLink m_ladderNow;
    bool m_ladderNativeLocked = false;
    bool m_ladderCancelPending = false;
    bool m_ladderWaiting = false;
    MockLink m_link;
    std::vector<Vec3> m_path;
    std::vector<NavPathLink> m_pathLinks;
    size_t m_pathIndex = 0, m_cornerFor = size_t(-1);
    CornerPlan m_cornerPlan;
    Vec3 m_pathStart = Vec3(0, 0, 0);
    bool m_cornerSlow = false;
    bool m_meshPathActive = true;
    float m_nextRepath = 0.f, m_minRepath = 0.f, m_now = 0.f, m_trapStepUntil = 0.f;
    int m_number = 1;
    bool m_running = true, m_verbose = false;
    BotIntent m_intent;
    BotBlackboard m_blackboard;
    MockLoco m_locomotion;
    BotCombat m_combat;
    MockTrap m_trap;
    std::string lastCancelReason;
    bool lastCancelBan = false;
    int cancelCount = 0;

    const NavPathLink* LinkAtCorner(size_t corner) const {
        for (const NavPathLink& l : m_pathLinks)
            if (size_t(l.corner) == corner) return &l;
        return nullptr;
    }

    bool YieldLadderTo(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const;
    unsigned long long m_subQueueRef = 0; bool m_subQueueStanding = false;   // SPEC-006a (flag OFF in this test)
    bool UpdateLadderQueue(CPlayer&, const NavPathLink&, const Vec3&, const Vec3&, float) { return false; }
    void CancelLadderQueue(const char*) { m_subQueueRef = 0; }
    bool ApplyLadderExecutor(CPlayer& actor);
    void CancelLadderBody(CPlayer& actor, const char* why, bool ban);
    void CancelLadder(CPlayer& actor, const char* why, bool ban) {
        lastCancelReason = why ? why : "";
        lastCancelBan = ban;
        ++cancelCount;
        CancelLadderBody(actor, why, ban);
    }
    void UpdateLadder(CPlayer& actor, float dt);
    Vec3 SteerCorners(NavMeshCore& mesh, const Vec3& feet);
};

bool BotBrain::YieldLadderTo(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const
{
	if (!m_running || m_number == myNumber || m_blackboard.health <= 0 || m_blackboard.swimming) return false;
	if (m_ladder.Active())
		return ShouldYieldLadderStart(ref, myDist2, myNumber, m_ladderNow.ref, true, false, 0.f, m_number);
	if (m_link.Active() || !m_meshPathActive || m_intent.combat == BotCombatIntent::Fire)
		return false;
	const NavPathLink* l = LinkAtCorner(m_pathIndex);
	if (!l || l->kind != NavLinkLadder) return false;
	const Vec3 feet = m_blackboard.position;
	const float d2 = (a - feet).GetLengthSquared2D();
	if (d2 > 4.0f || fabs_tpl(a.z - feet.z) > 2.0f) return false;
	return ShouldYieldLadderStart(ref, myDist2, myNumber, l->ref, false, true, d2, m_number);
}
bool BotManager::AIEnabled(EntityId id) const
{
	const BotInstance* bot = Find(id);
	return bot && bot->AIEnabled();
}
bool BotManager::ShouldYieldLadder(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const
{
	for (const auto& entry : m_bots)
		if (entry.second && entry.second->AIEnabled() && entry.second->Brain().YieldLadderTo(ref, a, myDist2, myNumber))
			return true;
	return false;
}
bool BotBrain::ApplyLadderExecutor(CPlayer& actor)
{
	if (BotConfig::Get().subExit!=1) CancelLadderQueue("flag_off");
	if (m_ladder.Active()) { CancelLadderQueue("climbing"); return true; }
	if (m_link.Active()) { CancelLadderQueue("link"); return false; } // an existing jump/drop keeps ownership until its own completion
	NavMeshCore* mesh=CurrentNavMesh();
	if (!mesh || !m_meshPathActive || !m_intent.move || m_intent.combat==BotCombatIntent::Fire) { CancelLadderQueue(m_intent.combat==BotCombatIntent::Fire?"combat":"path"); return false; }
	const NavPathLink* l=LinkAtCorner(m_pathIndex);
	if (!l || l->kind!=NavLinkLadder) { CancelLadderQueue("path"); return false; }
	const Vec3 a(l->a[0],l->a[1],l->a[2]);
	const Vec3 feet=m_blackboard.position;
	const float dist2=(a-feet).GetLengthSquared2D();
	if (BotConfig::Get().subExit==1 && UpdateLadderQueue(actor,*l,a,feet,dist2)) return m_subQueueStanding; // walking to the slot: the normal intent runs
	if (dist2>4.0f || fabs_tpl(a.z-feet.z)>2.0f) return false;
	if (l->ref && g_pGame && g_pGame->GetBotManager().ShouldYieldLadder(l->ref,a,dist2,m_number))
	{
		m_nextRepath=max(m_nextRepath,m_now+1.0f);
		m_trap.Reset(); m_trapStepUntil=0;
		m_combat.StopFire();
		m_intent=BotIntent();
		WBI(aim,Vec3(a.x,a.y,a.z+1.2f));
		m_locomotion.Stop(actor);
		actor.SetWarsBotActions(false,false,STANCE_STAND);
		return true;
	}
	m_ladderNow=*l;
	m_ladder.Start(m_now,LVec{l->a[0],l->a[1],l->a[2]},LVec{l->b[0],l->b[1],l->b[2]});
	m_ladderNativeLocked=false; m_ladderCancelPending=false;
	WB_LOG(2,"[WarsBots] #%d LADDER start a=(%.2f,%.2f,%.2f) b=(%.2f,%.2f,%.2f) actor=%u.",m_number,
		l->a[0],l->a[1],l->a[2],l->b[0],l->b[1],l->b[2],actor.GetEntityId());
	return true;
}
void BotBrain::CancelLadderBody(CPlayer& actor, const char* why, bool ban)
{
	SPlayerStats* stats=actor.GetPlayerStats();
	const bool nativeAttached=stats && (stats->isOnLadder.Value() || stats->isExitingLadder);
	const bool hadRoute=m_ladder.Active() || m_ladderNativeLocked || m_ladderNow.ref || m_ladderCancelPending;
	m_ladderCancelPending=false; // also consume a reset whose native revive already cleared ladder stats
	actor.RestoreWarsBotLadderGraph();
	if (!hadRoute && !nativeAttached) return;
	WB_LOG(1,"[WarsBots] #%d LADDER cancel reason=%s nativeOn=%d exiting=%d pos=(%.2f,%.2f,%.2f) actor=%u.",m_number,
		why,nativeAttached?1:0,(stats&&stats->isExitingLadder)?1:0,
		actor.GetEntity()->GetWorldPos().x,actor.GetEntity()->GetWorldPos().y,actor.GetEntity()->GetWorldPos().z,
		actor.GetEntityId());
	m_combat.StopFire();
	m_intent=BotIntent();
	if (nativeAttached) actor.RequestLeaveLadder(CPlayer::eLAT_ReachedEnd); // cleanup only, never route success
	m_locomotion.Stop(actor);
	actor.SetWarsBotActions(false,false,STANCE_STAND);
	if (ban)
	{
		NavMeshCore* mesh=CurrentNavMesh();
		if (mesh && m_ladderNow.ref) mesh->BanLink(m_ladderNow.ref);
	}
	m_path.clear(); m_pathLinks.clear(); m_pathIndex=0; m_cornerFor=size_t(-1); m_nextRepath=m_minRepath=0; m_meshPathActive=false;
	m_ladder.Reset(); m_ladderNativeLocked=false; m_ladderNow=NavPathLink();
}
void BotBrain::UpdateLadder(CPlayer& actor, float dt)
{
	SPlayerStats* stats=actor.GetPlayerStats();
	const Vec3 feet=actor.GetEntity()->GetWorldPos();
	if (!stats || actor.IsFrozen() || stats->isRagDoll || actor.GetLinkedEntity() ||
		stats->inFreefall.Value()>0 || stats->inZeroG || stats->isGrabbed || stats->spectatorMode || stats->flyMode ||
		LadderApproachInvalid(m_ladder.Phase(),stats->isOnLadder.Value(),stats->isExitingLadder,
			m_blackboard.swimming || actor.ShouldSwim(),feet.z,m_ladderNow.a[2]))
	{
		CancelLadder(actor,"movement_state",false);
		return;
	}
	const Vec3 center(m_ladderNow.a[0],m_ladderNow.a[1],m_ladderNow.a[2]+1.2f);
	LadderInput in;
	in.now=m_now; in.feet=LVec{feet.x,feet.y,feet.z}; in.grounded=m_blackboard.grounded;
	in.onLadder=stats && stats->isOnLadder.Value();
	in.exiting=stats && stats->isExitingLadder;
	in.atTop=stats && (stats->ladderTop - feet).len() < 2.5f;
	// Probe in the actual grab phase. Once attached, the native frame is locked: no ray may mutate it mid-climb.
	const LadderPhase phase=m_ladder.Phase();
	if ((phase==LadderPhase::Approach || phase==LadderPhase::Grab) && !in.onLadder && !m_ladderNativeLocked)
	{
		m_locomotion.Look(actor,center);
		// Validate in the actual grab phase, immediately before the native call. Lock only after attachment.
		in.canGrab=phase==LadderPhase::Grab && actor.IsLadderUsable();
		if (in.canGrab)
		{
			if (!stats || !m_ladder.SetNative(
				LVec{stats->ladderBottom.x,stats->ladderBottom.y,stats->ladderBottom.z},
				LVec{stats->ladderTop.x,stats->ladderTop.y,stats->ladderTop.z},
				LVec{stats->ladderOrientation.x,stats->ladderOrientation.y,stats->ladderOrientation.z}))
				m_ladder.RejectNative();
		}
	}
	const LadderOutput o=m_ladder.Update(in);
	if (o.grab)
	{
		actor.RequestGrabOnLadder(CPlayer::eLAT_Use);
		m_ladderNativeLocked=stats && stats->isOnLadder.Value();
		in.onLadder=m_ladderNativeLocked;
	}
	// The executor needs the actual validated landing, not an assumed distance in front of the ladder.
	if (m_ladderNativeLocked && in.onLadder)
	{
		actor.SetWarsBotLadderExitTarget(Vec3(m_ladderNow.b[0],m_ladderNow.b[1],m_ladderNow.b[2]));
	}
	if (o.phase==LadderPhase::Done)
	{
		actor.RestoreWarsBotLadderGraph();
		WB_LOG(2,"[WarsBots] #%d LADDER done nativeOn=%d exiting=%d grounded=%d pos=(%.2f,%.2f,%.2f) actor=%u.",m_number,
			in.onLadder?1:0,in.exiting?1:0,in.grounded?1:0,feet.x,feet.y,feet.z,actor.GetEntityId());
		const size_t next=size_t(m_ladderNow.corner)+1;
		m_cornerFor=size_t(-1);
		m_pathIndex=m_path.empty()?0:(next<m_path.size()?next:m_path.size()-1);
		m_ladder.Reset(); m_ladderNativeLocked=false; m_ladderCancelPending=false; m_ladderNow=NavPathLink();
		return;
	}
	if (o.phase==LadderPhase::Failed)
	{
		static const char* const why[]={"none","timeout","no_grab","lost","blocked_top","wrong_floor","wrong_ladder"};
		WB_LOG(1,"[WarsBots] #%d LADDER failed reason=%s nativeOn=%d exiting=%d grounded=%d pos=(%.2f,%.2f,%.2f) actor=%u.",
			m_number,why[int(o.fail)],in.onLadder?1:0,in.exiting?1:0,in.grounded?1:0,feet.x,feet.y,feet.z,actor.GetEntityId());
		CancelLadder(actor,why[int(o.fail)],true);
		return;
	}
	m_intent=BotIntent();
	m_combat.StopFire();
	const LVec& into=m_ladder.Into();
	WBI(aim,feet+Vec3(into.x*2.f,into.y*2.f,1.6f)); // retain a real view target while native exit owns motion
	if (o.phase==LadderPhase::WaitExit)
	{
		// Native root motion owns the exit. Do not send Stop or a sideways target: wait for native completion.
	}
	else if (in.onLadder)
	{
		WBI(move,true);
		const Vec3 current=actor.GetEntity()->GetWorldPos();
		m_intent.destination=current+Vec3(into.x*2.f,into.y*2.f,0);
		WBI(gait,BotGait::Walk);
		WBI(suitMode,NANOMODE_DEFENSE);
		m_intent.combat=BotCombatIntent::None; m_intent.jump=BotJump::None; m_intent.linkJump=false;
		m_intent.meshPath=false; m_intent.arrive=0.5f;
		m_locomotion.ExecuteLadder(actor,m_intent.destination,m_intent.aim,dt,true);
	}
	else
	{
		// Approach, grab approach and post-exit settle use the ordinary safe executor and its canMove/gates.
		WBI(move,true);
		m_intent.destination=Vec3(o.moveTo.x,o.moveTo.y,o.moveTo.z);
		WBI(aim,center);
		WBI(gait,BotGait::Walk);
		WBI(suitMode,NANOMODE_DEFENSE);
		m_intent.combat=BotCombatIntent::None; m_intent.jump=BotJump::None; m_intent.linkJump=false;
		m_intent.meshPath=false; m_intent.arrive=0.5f;
		m_locomotion.Execute(actor,m_intent,m_blackboard,m_now,dt);
	}
	if (m_verbose) WB_LOG(2,"[WarsBots] #%d LADDER phase=%d grab=%d climb=%d settle=%d nativeOn=%d exiting=%d grounded=%d vz=%.2f pos=(%.2f,%.2f,%.2f) actor=%u.",
		m_number,int(o.phase),o.grab?1:0,o.climbing?1:0,o.settling?1:0,in.onLadder?1:0,in.exiting?1:0,in.grounded?1:0,
		m_blackboard.velocity.z,feet.x,feet.y,feet.z,actor.GetEntityId());
}
Vec3 BotBrain::SteerCorners(NavMeshCore& mesh, const Vec3& feet)
{
	for (;;)
	{
		if (m_pathIndex + 1 >= m_path.size() || LinkAtCorner(m_pathIndex)) break; // final goal / link take-off: exact
		const Vec3& c = m_path[m_pathIndex];
		const Vec3& n = m_path[m_pathIndex + 1];
		if (m_cornerFor != m_pathIndex)
		{
			const Vec3& a = m_pathIndex ? m_path[m_pathIndex - 1] : m_pathStart;
			const float pa[2] = {a.x, a.y}, pc[2] = {c.x, c.y}, pn[2] = {n.x, n.y};
			m_cornerFor = m_pathIndex; m_cornerPlan = OffsetCorner(pa, pc, pn);
			if (m_cornerPlan.moved)
			{
				// keep the moved point on the mesh (it moves away from the obstacle, so normally it is)
				const float q[3] = {m_cornerPlan.x, m_cornerPlan.y, c.z}; float s[3];
				if (!mesh.Project(q, s, 0.2f, 0.6f, 0.6f) || (s[0] - q[0]) * (s[0] - q[0]) + (s[1] - q[1]) * (s[1] - q[1]) > 0.0025f)
				{ m_cornerPlan.x = c.x; m_cornerPlan.y = c.y; m_cornerPlan.moved = false; }
			}
		}
		if (fabs_tpl(c.z - feet.z) >= 2.0f) break;
		const float pf[2] = {feet.x, feet.y}, pc[2] = {c.x, c.y}, pn[2] = {n.x, n.y};
		bool byPlane = false;
		if (!CornerReached(pf, pc, pn, m_cornerPlan, 0.35f, 0.05f, &byPlane)) break;
		if (byPlane)
		{
			const float a3[3] = {feet.x, feet.y, feet.z}, b3[3] = {n.x, n.y, n.z};
			// DS-089: a swimmer's raw feet are below the water poly, so the ray start is snapped with the same bounded
			// water box; without it RayClear failed on every water corner (0/1085) and blocked corner advancement.
			NavQueryBounds water;
			const bool waterStart = WaterStartBounds(m_blackboard, feet.z, water);
			if (!mesh.RayClear(a3, b3, waterStart ? &water : nullptr)) break; // crossed the line but the next leg is not in straight reach yet
		}
		++m_pathIndex;
	}
	const Vec3& c = m_path[m_pathIndex];
	if (const NavPathLink* l = LinkAtCorner(m_pathIndex))
	{
		if (l->kind == NavLinkLadder && (c - feet).GetLength2D() < 4.0f) m_cornerSlow = true;
		return c;
	}
	if (m_pathIndex + 1 >= m_path.size() || m_cornerFor != m_pathIndex) return c;
	// A sharp turn is often split into two close corners (Steelmill stair mouth: 42 + 48 deg): count both.
	CornerPlan turn = m_cornerPlan;
	if (m_pathIndex + 2 < m_path.size() && (m_path[m_pathIndex + 1] - c).GetLength2D() < 1.5f)
	{
		const Vec3& n = m_path[m_pathIndex + 1]; const Vec3& nn = m_path[m_pathIndex + 2];
		const float pc[2] = {c.x, c.y}, pn[2] = {n.x, n.y}, pnn[2] = {nn.x, nn.y};
		turn.turnDeg += OffsetCorner(pc, pn, pnn).turnDeg;
	}
	m_cornerSlow = CornerSlow(turn, (c - feet).GetLength2D());
	return Vec3(m_cornerPlan.x, m_cornerPlan.y, c.z);
}

static void RegisterBot(EntityId id, int number, BotBrain& b, CPlayer& a, const Vec3& pos, const NavPathLink& link) {
    a.id = id;
    a.e.p = pos;
    a.st.ladderBottom = Vec3(link.a[0], link.a[1], link.a[2]);
    a.st.ladderTop = Vec3(link.b[0], link.b[1], link.b[2]);
    a.st.ladderOrientation = Vec3(0.f, 1.f, 0.f);
    b.m_number = number;
    b.m_running = true;
    b.m_meshPathActive = true;
    b.m_intent.move = true;
    b.m_blackboard.health = 100;
    b.m_blackboard.position = pos;
    b.m_blackboard.grounded = true;
    b.m_path = { Vec3(link.a[0], link.a[1], link.a[2]), Vec3(link.b[0], link.b[1], link.b[2]) };
    b.m_pathLinks = { link };
    b.m_pathIndex = 0;
    auto inst = std::make_unique<BotInstance>();
    inst->ai = true;
    inst->brain = &b;
    g_game.mgr.m_bots[id] = std::move(inst);
}

int main(int argc, char** argv) {
    const bool expectNew = argc > 1 && std::strcmp(argv[1], "new") == 0;
    int fails = 0;
    auto check = [&](bool cond, const char* msg) {
        if (!cond) { std::printf("FAIL: %s\n", msg); ++fails; }
    };
    auto expect = [&](bool newCond, const char* msg) {
        check(expectNew ? newCond : !newCond, msg);
    };

    const NavPathLink shoreLadder{ 0, NavLinkLadder, {2116.12f, 1880.62f, 90.16f}, {2117.50f, 1880.60f, 99.76f}, 4693115ull };

    // 1. Shore lower-bay rush: #2 climbs at 0.31s..12.58s while #6 arrives at 1.92s (blocked by #2 until 12.58s).
    {
        g_game.mgr.m_bots.clear();
        g_mesh = NavMeshCore();
        BotBrain b2, b6;
        CPlayer a2, a6;
        RegisterBot(65481, 2, b2, a2, Vec3(2117.25f, 1880.62f, 90.16f), shoreLadder);
        RegisterBot(65461, 6, b6, a6, Vec3(2114.50f, 1887.88f, 90.58f), shoreLadder);

        b2.m_now = 0.31f;
        check(b2.ApplyLadderExecutor(a2) && b2.m_ladder.Active(), "#2 starts Shore ship ladder at 0.31s");

        // #6 reaches the 2 m gate at 1.92s while #2 is on the ladder (a6.ladderUsable=false while #2 occupies rungs).
        a6.e.p = Vec3(2115.64f, 1881.45f, 89.55f);
        b6.m_blackboard.position = a6.e.p;
        a6.ladderUsable = false;
        b6.m_now = 1.92f;
        const bool handled = b6.ApplyLadderExecutor(a6);
        check(handled, "#6 is handled by ApplyLadderExecutor at 1.92s");
        expect(!b6.m_ladder.Active() && !b6.m_intent.move && b6.m_locomotion.stops == 1 && b6.m_trap.resets == 1,
            "#6 waits at gate while #2 owns ref=4693115");

        // Advance to 5.57s while #2 is still climbing.
        for (float t : {2.55f, 3.50f, 4.50f, 5.57f}) {
            b6.m_now = t;
            b6.m_intent.move = true;
            if (b6.m_ladder.Active()) b6.UpdateLadder(a6, 0.1f);
            else b6.ApplyLadderExecutor(a6);
        }
        expect(g_mesh.bans == 0, "ref=4693115 is NOT banned at 5.57s while #6 waits for #2");

        if (expectNew) {
            // #2 finishes LADDER done at 12.58s at the upper landing -> #6 starts with a fresh timer at 12.60s and grabs.
            b2.m_ladder.Reset();
            b2.m_ladderNow = NavPathLink();
            b2.m_pathIndex = 1;
            b2.m_blackboard.position = Vec3(2117.53f, 1880.60f, 99.81f);
            b6.m_now = 12.60f;
            b6.m_intent.move = true;
            check(b6.ApplyLadderExecutor(a6) && b6.m_ladder.Active(),
                "#6 starts ladder immediately after #2 finishes at 12.58s");
            a6.e.p = Vec3(2116.12f, 1880.62f, 90.16f);
            b6.m_blackboard.position = a6.e.p;
            a6.ladderUsable = true;
            b6.m_now = 12.70f;
            b6.UpdateLadder(a6, 0.1f);
            b6.m_now = 12.80f;
            b6.UpdateLadder(a6, 0.1f);
            check(a6.grabs == 1 && g_mesh.bans == 0, "#6 grabs free ladder with fresh timer and 0 bans");
        }
    }

    // 2. Distance-ordered queue handoff: closer #12 (d2=1.44) goes before further #4 (d2=3.24) even if #4 ticks first.
    if (expectNew) {
        g_game.mgr.m_bots.clear();
        g_mesh = NavMeshCore();
        BotBrain b2, b4, b12;
        CPlayer a2, a4, a12;
        RegisterBot(65481, 2, b2, a2, Vec3(2116.12f, 1880.62f, 90.16f), shoreLadder);
        RegisterBot(65471, 4, b4, a4, Vec3(2114.32f, 1880.62f, 90.16f), shoreLadder);  // d = 1.80m (d2 = 3.24)
        RegisterBot(65431, 12, b12, a12, Vec3(2117.32f, 1880.62f, 90.16f), shoreLadder); // d = 1.20m (d2 = 1.44)

        b2.m_now = 0.31f;
        b2.ApplyLadderExecutor(a2);
        b4.m_now = 1.15f; b4.ApplyLadderExecutor(a4);
        b12.m_now = 1.60f; b12.ApplyLadderExecutor(a12);
        check(!b4.m_ladder.Active() && !b12.m_ladder.Active(), "both #4 and #12 wait while #2 climbs");

        // #2 finishes at the upper landing; #4's 10 Hz tick fires 20ms before #12's tick.
        b2.m_ladder.Reset();
        b2.m_ladderNow = NavPathLink();
        b2.m_pathIndex = 1;
        b2.m_blackboard.position = Vec3(2117.53f, 1880.60f, 99.81f);
        b4.m_now = 12.60f; b4.m_intent.move = true;
        b4.ApplyLadderExecutor(a4);
        check(!b4.m_ladder.Active() && !b4.m_intent.move, "further #4 yields to closer waiting #12");

        b12.m_now = 12.62f; b12.m_intent.move = true;
        b12.ApplyLadderExecutor(a12);
        check(b12.m_ladder.Active(), "closer #12 starts ladder on its tick");
    }

    // 3. Shore #4 bilge water fall (pos=(2116.99,1883.08,87.84), swim=1): immediate movement_state cancel, 0 bans.
    {
        g_game.mgr.m_bots.clear();
        g_mesh = NavMeshCore();
        BotBrain b4;
        CPlayer a4;
        RegisterBot(65471, 4, b4, a4, Vec3(2114.20f, 1880.42f, 90.16f), shoreLadder);
        b4.m_now = 1.15f;
        check(b4.ApplyLadderExecutor(a4) && b4.m_ladder.Active(), "#4 starts ladder at 1.15s");

        // At 1.85s, #4 falls off the platform into the ship bilge water (z=87.84, swimming=true).
        a4.e.p = Vec3(2116.99f, 1883.08f, 87.84f);
        a4.swimming = true;
        b4.m_blackboard.position = a4.e.p;
        b4.m_blackboard.swimming = true;
        b4.m_blackboard.grounded = false;
        b4.m_now = 1.85f;
        b4.UpdateLadder(a4, 0.1f);

        if (!expectNew) {
            // On old code, #4 stays active in Approach at 1.85s and times out at 26.24s, banning the link.
            b4.m_now = 26.24f;
            b4.UpdateLadder(a4, 0.1f);
        }
        expect(b4.lastCancelReason == "movement_state" && !b4.lastCancelBan && g_mesh.bans == 0,
            "#4 bilge water fall cancels as movement_state at 1.85s without banning ref=4693115");
    }

    // 4. SteerCorners pre-ladder slowdown: NavLinkLadder within 4 m sets m_cornerSlow=true; Strength link stays false.
    {
        BotBrain b;
        b.m_path = { Vec3(2116.12f, 1880.62f, 90.16f), Vec3(2117.50f, 1880.60f, 99.76f) };
        b.m_pathLinks = { shoreLadder };
        b.m_pathIndex = 0;
        b.m_cornerSlow = false;
        const Vec3 target = b.SteerCorners(g_mesh, Vec3(2114.06f, 1880.42f, 90.12f));
        check(std::fabs(target.x - 2116.12f) < 1e-3f, "SteerCorners returns exact ladder take-off corner");
        expect(b.m_cornerSlow, "SteerCorners sets m_cornerSlow=true within 4 m of NavLinkLadder");

        if (expectNew) {
            b.m_cornerSlow = false;
            b.m_pathLinks[0].kind = NavLinkStrength;
            b.SteerCorners(g_mesh, Vec3(2114.06f, 1880.42f, 90.12f));
            check(!b.m_cornerSlow, "SteerCorners keeps m_cornerSlow=false for NavLinkStrength run-up");
        }
    }

    if (fails) {
        std::printf("Shore ladder queue caller controls FAILED (%d)\n", fails);
        return 1;
    }
    std::printf("Shore ladder queue caller controls PASS (%s)\n", expectNew ? "new" : "old");
    return 0;
}
