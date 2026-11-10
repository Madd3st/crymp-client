
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
    float GetLengthSquared() const { return x * x + y * y + z * z; }
    void Normalize() { const float l = len(); if (l > 0) { x /= l; y /= l; z /= l; } }
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
    bool linkJump = false, meshPath = false, unstickStrength = false, unstickClimb = false;
    void MoveTo(const Vec3& d, const Vec3& l) { move = true; destination = d; aim = l; }
    float arrive = 0.8f;
    struct Why { short suitMode = 0, jump = 0, gait = 0, aim = 0, move = 0; } why;
};
#define WBI_MOVETO(...) (m_intent.MoveTo(__VA_ARGS__), m_intent.why.move = (short)__LINE__)
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

struct MockSensor { int resets = 0; void Reset() { ++resets; } };
struct MockSteps { int step = 0, resets = 0; void Reset() { step = 0; ++resets; } };
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
    int LadderQueueAhead(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const;
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
    unsigned long long m_subQueueRef = 0; float m_subQueueSince = 0, m_subQueueCooldown = 0, m_subQueueAt = -1e9f;
    int m_subQueueSlot = -1; bool m_subQueueStanding = false;
    MockSensor m_unstickSensor; MockSteps m_unstickSteps; bool m_unstickStuck = false, m_unstickHasStall = false, m_unstickBackTried = false;
    bool UpdateLadderQueue(CPlayer& actor, const NavPathLink& l, const Vec3& a, const Vec3& feet, float dist2);
    bool LadderQueuedAhead(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const;
    bool LadderQueued() const;
    void CancelLadderQueue(const char* why);
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
bool BotBrain::UpdateLadderQueue(CPlayer& actor,const NavPathLink& l,const Vec3& a,const Vec3& feet,float dist2)
{
	if (!SlowLadderLink(true,l.a,l.b) || dist2>7.0f*7.0f || fabs_tpl(a.z-feet.z)>2.0f || m_now<m_subQueueCooldown || !g_pGame)
	{ CancelLadderQueue("out_of_queue"); return false; }
	const int ahead=g_pGame->GetBotManager().LadderQueueAhead(l.ref,a,dist2,m_number);
	if (ahead<=0) { CancelLadderQueue("front"); return false; }   // nobody ahead: the normal gate/yield path decides
	if (m_subQueueRef!=l.ref) { CancelLadderQueue("other_ladder"); m_subQueueRef=l.ref; m_subQueueSince=m_now; m_subQueueSlot=-1; }
	m_subQueueAt=m_now;
	// Codex #38: the queue owns the bot - an UNSTICK chain already running (jump/strength/back-off) ends here, its intent too.
	m_unstickSteps.Reset(); m_unstickStuck=false; m_unstickHasStall=false; m_unstickBackTried=false;
	m_intent.jump=BotJump::None; m_intent.unstickStrength=false; m_intent.unstickClimb=false;
	if (LadderQueueTimedOut(m_subQueueSince,m_now))
	{
		WB_LOG(1,"[WarsBots] #%d SUB QUEUE timeout ladder=%llu waited=%.1f actor=%u.",m_number,l.ref,m_now-m_subQueueSince,actor.GetEntityId());
		m_subQueueRef=0; m_subQueueAt=-1e9f; m_subQueueCooldown=m_now+10.0f; m_nextRepath=m_minRepath=0;
		return false;
	}
	Vec3 dir=feet-a; dir.z=0;
	if (dir.GetLengthSquared()<0.01f) dir=Vec3(1,0,0);
	dir.Normalize();
	const Vec3 slot=a+dir*LadderQueueOffset(ahead);
	if (m_subQueueSlot!=ahead)
	{
		m_subQueueSlot=ahead;
		WB_LOG(1,"[WarsBots] #%d SUB QUEUE slot=%.1f ladder=%llu ahead=%d waited=%.1f actor=%u.",m_number,LadderQueueOffset(ahead),l.ref,ahead,m_now-m_subQueueSince,actor.GetEntityId());
	}
	m_unstickSensor.Reset(); m_trap.Reset(); m_trapStepUntil=0;   // waiting in a queue is not a stall
	m_subQueueStanding=(slot-feet).GetLengthSquared2D()<=0.6f*0.6f;
	if (!m_subQueueStanding) { WBI_MOVETO(slot,Vec3(a.x,a.y,a.z+1.2f)); WBI(gait,BotGait::Walk); return true; }
	m_combat.StopFire();
	m_intent=BotIntent();
	WBI(aim,Vec3(a.x,a.y,a.z+1.2f));
	m_locomotion.Stop(actor);
	actor.SetWarsBotActions(false,false,STANCE_STAND);
	return true;
}
bool BotBrain::LadderQueued() const
{ return m_subQueueRef != 0 && BotConfig::Get().subExit == 1 && m_now - m_subQueueAt <= 0.5f; }
void BotBrain::CancelLadderQueue(const char* why)
{
	if (!m_subQueueRef) return;
	WB_LOG(2,"[WarsBots] #%d SUB QUEUE cancel reason=%s ladder=%llu waited=%.1f.",m_number,why,m_subQueueRef,m_now-m_subQueueSince);
	m_subQueueRef=0; m_subQueueAt=-1e9f; m_subQueueSlot=-1; m_subQueueStanding=false;
}
bool BotBrain::LadderQueuedAhead(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const
{
	if (!m_running || m_number == myNumber || m_blackboard.health <= 0 || m_blackboard.swimming) return false;
	if (m_ladder.Active()) return m_ladderNow.ref == ref;
	const bool queued = LadderQueued() && m_subQueueRef == ref;
	if (!queued && !YieldLadderTo(ref, a, 1e9f, myNumber)) return false;   // climbing or waiting at the 2 m gate
	const float d2 = (a - m_blackboard.position).GetLengthSquared2D();
	return d2 + 0.01f < myDist2 || (std::fabs(d2 - myDist2) <= 0.01f && m_number < myNumber);
}
int BotManager::LadderQueueAhead(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const
{
	int ahead = 0;
	for (const auto& entry : m_bots)
		if (entry.second && entry.second->AIEnabled() && entry.second->Brain().LadderQueuedAhead(ref, a, myDist2, myNumber)) ++ahead;
	return ahead;
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


static void Reg(EntityId id, int number, BotBrain& b, CPlayer& a, const Vec3& pos, const NavPathLink& link) {
    a.id = id; a.e.p = pos;
    b.m_number = number; b.m_running = true; b.m_meshPathActive = true; b.m_intent.move = true;
    b.m_blackboard.health = 100; b.m_blackboard.position = pos; b.m_blackboard.grounded = true;
    b.m_path = { Vec3(link.a[0], link.a[1], link.a[2]), Vec3(link.b[0], link.b[1], link.b[2]) };
    b.m_pathLinks = { link }; b.m_pathIndex = 0;
    auto inst = std::make_unique<BotInstance>(); inst->ai = true; inst->brain = &b;
    g_game.mgr.m_bots[id] = std::move(inst);
}

int main() {
    int fails = 0;
    auto check = [&](bool c, const char* m) { if (!c) { std::printf("FAIL: %s\n", m); ++fails; } };
    const NavPathLink ladder{ 0, NavLinkLadder, {2116.12f, 1880.62f, 90.16f}, {2117.50f, 1880.60f, 99.76f}, 4693115ull };
    WarsBots::BotConfig::Get().ladder = 1;
    WarsBots::BotConfig::Get().subExit = 1;
    g_game.mgr.m_bots.clear();
    BotBrain climber, q; CPlayer ac, aq;
    Reg(2, 2, climber, ac, Vec3(2116.2f, 1880.6f, 90.2f), ladder);
    Reg(6, 6, q, aq, Vec3(2111.0f, 1880.6f, 90.2f), ladder);           // 5.1 m from the base, inside the 7 m queue area
    climber.m_now = q.m_now = 1.0f;
    check(climber.ApplyLadderExecutor(ac) && climber.m_ladder.Active(), "#2 starts the ladder");

    // 1. queue entry with a running UNSTICK chain: walking to the 2.5 m slot
    q.m_unstickSteps.step = 2; q.m_unstickStuck = true; q.m_intent.jump = BotJump::Strength; q.m_intent.unstickStrength = true;
    const bool owned = q.ApplyLadderExecutor(aq);
    check(!owned, "walking to the slot: the executor leaves this frame's intent to the normal pipeline");
    check(q.LadderQueued() && q.m_subQueueRef == ladder.ref, "#6 is queued");
    check(q.m_unstickSteps.step == 0 && q.m_unstickSteps.resets > 0 && !q.m_unstickStuck, "running UNSTICK chain reset on queue entry");
    check(q.m_intent.jump == BotJump::None && !q.m_intent.unstickStrength, "unstick jump intent cleared");
    check(q.m_intent.move && std::fabs(q.m_intent.destination.x - (2116.12f - 2.5f)) < 0.2f, "moves to the 2.5 m slot on its approach line");
    check(q.m_unstickSensor.resets > 0, "SPEC-004 sensor muted in the queue");
    // standing in the slot
    q.m_now = 2.0f; q.m_blackboard.position = Vec3(2116.12f - 2.5f, 1880.6f, 90.2f); q.m_intent.move = true;
    const int stops = q.m_locomotion.stops;
    check(q.ApplyLadderExecutor(aq) && q.m_subQueueStanding && q.m_locomotion.stops > stops, "standing in the slot stops the bot");
    check(q.LadderQueued(), "still queued while standing");

    // 2. ON -> OFF with a live queue
    WarsBots::BotConfig::Get().subExit = 0;
    check(!q.LadderQueued(), "flag OFF: readers no longer see a queue");
    q.m_now = 2.2f; q.m_intent.move = true;
    q.ApplyLadderExecutor(aq);
    check(q.m_subQueueRef == 0, "flag OFF: the next executor tick cancels the queue");
    check(g_game.mgr.LadderQueueAhead(ladder.ref, Vec3(2116.12f, 1880.62f, 90.16f), 49.0f, 9) == 1, "flag OFF: only the climber is ahead");
    WarsBots::BotConfig::Get().subExit = 1;

    // 3a. interruption by combat
    q.m_now = 3.0f; q.m_blackboard.position = Vec3(2111.0f, 1880.6f, 90.2f); q.m_intent = BotIntent(); q.m_intent.move = true;
    q.ApplyLadderExecutor(aq); check(q.LadderQueued(), "re-queued");
    q.m_now = 3.1f; q.m_intent.combat = BotCombatIntent::Fire;
    q.ApplyLadderExecutor(aq);
    check(q.m_subQueueRef == 0 && !q.LadderQueued(), "combat cancels the queue");
    // 3b. path change
    q.m_now = 3.2f; q.m_intent = BotIntent(); q.m_intent.move = true;
    q.ApplyLadderExecutor(aq); check(q.LadderQueued(), "re-queued after combat");
    q.m_pathLinks.clear(); q.m_now = 3.3f;
    q.ApplyLadderExecutor(aq);
    check(q.m_subQueueRef == 0, "path without the ladder cancels the queue");
    // 3c. stale: queued, then the executor is not called for a while
    q.m_pathLinks = { ladder }; q.m_now = 4.0f; q.m_intent = BotIntent(); q.m_intent.move = true;
    q.ApplyLadderExecutor(aq); check(q.LadderQueued(), "re-queued after the path change");
    q.m_now = 4.6f;
    check(!q.LadderQueued(), "a queue not refreshed for > 0.5 s is not seen by spread/ahead");

    // 4a. timeout: 25 s in the queue -> cancel + replan + 10 s cooldown
    q.m_now = 4.7f; q.ApplyLadderExecutor(aq);
    q.m_now = q.m_subQueueSince + 25.1f; q.m_intent = BotIntent(); q.m_intent.move = true;
    check(!q.ApplyLadderExecutor(aq) && q.m_subQueueRef == 0 && q.m_subQueueCooldown > q.m_now, "timeout cancels and cools down");
    // 4b. cold OFF never queues
    WarsBots::BotConfig::Get().subExit = 0;
    BotBrain c; CPlayer acold; Reg(7, 7, c, acold, Vec3(2111.0f, 1880.6f, 90.2f), ladder); c.m_now = 1.0f;
    c.ApplyLadderExecutor(acold);
    check(c.m_subQueueRef == 0 && !c.LadderQueued(), "cold OFF: no queue");

    if (fails) { std::printf("SPEC-006a queue caller FAILED (%d)\n", fails); return 1; }
    std::printf("SPEC-006a queue caller PASS\n");
    return 0;
}
