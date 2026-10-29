#include "PortIncludes.h"
#include <algorithm>
#include "BotConfig.h"
#include "CryGame/Bots/BotDifficulty.h"
#include "CryGame/Bots/BotThreatScan.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotPace.h"
#include "CryGame/Bots/BotBrain.h"
#include "CryGame/Bots/BotCorners.h"
#include "CryGame/Bots/BotAimControl.h"
#include "CryCommon/CryAction/IMovementController.h"
#include "CryGame/Bots/BotInstance.h"
#include "CryGame/Bots/BotManager.h"
#include "CryGame/Bots/BotMatch.h"
#include "CryGame/Bots/BotWeapons.h"
#include "CryGame/Bots/BotWeaponRank.h"
#include "CryGame/Bots/BotUpgradeGate.h"
#include "CryGame/Bots/BotWeaponGoal.h"
#include "CryGame/Bots/BotAuditFixes.h"
#include "CryGame/Bots/BotNavMesh.h"
#include "CryGame/Bots/BotNavMeshCore.h"
#include "CryGame/Bots/BotRoamFallback.h"
#include "CryGame/Bots/BotPSCaptureRuntime.h"
#include "CryGame/Bots/BotPSBuy.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CryEntitySystem/IEntityProxy.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryCommon/CryAction/IWeapon.h"
#include <cmath>
// T-1 step 1 (DeepSeek review): every write to a control channel records its source line in BotIntent::why*, so the
// INTENT WHY log shows which rule decided suit/jump/gait/aim/move this tick. No behaviour change: same writes, same order.
#define WBI(field,value) (m_intent.field=(value), m_intent.why.field=short(__LINE__))
#define WBI_MOVETO(...) (m_intent.MoveTo(__VA_ARGS__), m_intent.why.move=short(__LINE__), m_intent.why.aim=short(__LINE__))
#include <cstdlib>

namespace WarsBots
{
namespace
{
const char* NameOf(BotBrain::State state)
{
	switch (state)
	{
	case BotBrain::State::SPAWNING: return "SPAWNING";
	case BotBrain::State::SEEKING_WEAPON: return "SEEKING_WEAPON";
	case BotBrain::State::MOVING_TO_WEAPON: return "MOVING_TO_WEAPON";
	case BotBrain::State::SEARCHING_ENEMY: return "SEARCHING_ENEMY";
	case BotBrain::State::CHASING_ENEMY: return "CHASING_ENEMY";
	case BotBrain::State::ATTACKING: return "ATTACKING";
	case BotBrain::State::DEAD: return "DEAD";
	case BotBrain::State::RESPAWNING: return "RESPAWNING";
	}
	return "UNKNOWN";
}

// DS-089: bounded water start for the path/reach/ray queries of a native swimmer. Sized from the measured
// feet->native-plane offset (C147: 1.18..2.31 m), so a fixed 2.0 m box no longer loses the deep-float rows; the box
// is still bounded (WaterQueryBounds clamps to 2.8 m) and the core accepts it only on a baked water polygon. Dry
// callers (`waterMovement` false, sentinel plane, feet at/above the plane) get no bounds and keep the old box.
bool WaterStartBounds(const BotBlackboard& bb, float feetZ, NavQueryBounds& out)
{
	if (!bb.waterMovement || !bb.waterValid) return false;
	const float lift = bb.waterZ - feetZ;
	if (!(lift > 0.05f)) return false;
	out = WaterQueryBounds(lift);
	return true;
}
}

DecisionState BotBrain::ToCommit(State s)
{
	switch (s)
	{
	case State::SPAWNING: return DecisionState::Spawning;
	case State::SEEKING_WEAPON: return DecisionState::SeekWeapon;
	case State::MOVING_TO_WEAPON: return DecisionState::MoveToWeapon;
	case State::SEARCHING_ENEMY: return DecisionState::Search;
	case State::CHASING_ENEMY: return DecisionState::Chase;
	case State::ATTACKING: return DecisionState::Attack;
	case State::DEAD: return DecisionState::Dead;
	case State::RESPAWNING: return DecisionState::Respawn;
	}
	return DecisionState::Search;
}

bool BotBrain::IsUrgentCommit(State s) const
{
	// Death/respawn must never wait for a dwell; a visible target may enter ATTACKING at once.
	return s == State::DEAD || s == State::RESPAWNING || s == State::SPAWNING ||
		(s == State::ATTACKING && m_combat.Target() != 0);
}

DecisionOwner BotBrain::OwnerOf(State s) const
{
	switch (s)
	{
	case State::SEEKING_WEAPON:
	case State::MOVING_TO_WEAPON: return DecisionOwner::Upgrade;
	case State::SEARCHING_ENEMY: return DecisionOwner::Roam;
	case State::CHASING_ENEMY:
	case State::ATTACKING: return DecisionOwner::Combat;
	default: return DecisionOwner::Arbiter;
	}
}

void BotBrain::Change(State state,bool initialize,DecisionOwner owner)
{
	// Preserve Start's former silent initialization; normal transitions keep their exact effects.
	if(initialize)
	{
		m_state=state;
		m_commit.Reset(); m_move.Reset(); m_moveOwner=DecisionOwner::Arbiter;
		m_commit.BeginTick(m_decisionSerial,m_now);
		return;
	}
	if (m_state == state) return;
	if (BotConfig::Get().stateCommit>=2)
	{
		const DecisionOwner who = owner==DecisionOwner::Arbiter ? OwnerOf(state) : owner;
		if (!m_commit.Offer(who,ToCommit(state),m_now,IsUrgentCommit(state)))
		{
			if (m_commit.ShouldLogConflict(m_now))
				WB_LOG(1,"[WarsBots] #%d STATE CONFLICT first=%s second=%s owner=%d actor=%u.",
					m_number,NameOf(m_state),NameOf(state),int(who),m_actorId);
			return;
		}
		m_move.Reset(); m_moveOwner=who;
	}
	m_state = state;
	m_stateTime = m_now;
	WB_LOG(2, "[WarsBots] #%d AI state=%s.", m_number, NameOf(state));
}

const char* BotBrain::StateName() const { return NameOf(m_state); }

bool BotBrain::IsBadItem(EntityId id) const
{
	for (const BadItem& b : m_badItems) if (b.id == id && m_now < b.until) return true;
	return false;
}

void BotBrain::MarkBadItem(EntityId id)
{
	BadItem* slot = &m_badItems[0];
	for (BadItem& b : m_badItems) if (b.until < slot->until) slot = &b;
	slot->id = id; slot->until = m_now + 90.0f;
}

float BotBrain::Rand01()
{
	m_rng = m_rng * 1664525u + 1013904223u;
	return float(m_rng >> 8) / float(1u << 24);
}

// Long moves use native Speed sprint like a player holding Shift, with energy hysteresis;
// occasionally a forward Strength jump once Speed has been held long enough to switch modes.
void BotBrain::TravelGait(float distance)
{
    if(BotPaceEnabled()){
        const bool allowed=m_profile.skill!=BotSkill::Beginner || m_profile.useSpeed;
        m_speedTravel=allowed && PaceSpeedBurst(m_speedTravel,distance,m_blackboard.energy);
        // Basic Armor sprint is available to every skill; complex Speed burst keeps the Beginner gate.
        WBI(gait,distance>=3.f?BotGait::Sprint:BotGait::Run);
        WBI(suitMode,m_speedTravel?NANOMODE_SPEED:NANOMODE_DEFENSE);
        if(!m_speedTravel)return;
    }else{
	if (!m_profile.useSpeed) { WBI(gait,BotGait::Run); return; }
	if (m_speedTravel) { if (m_blackboard.energy < 55.0f || distance < 8.0f) m_speedTravel = false; }
	else if (m_blackboard.energy > 120.0f && distance > 15.0f) m_speedTravel = true;
	WBI(gait,m_speedTravel ? BotGait::Sprint : BotGait::Run);
	if (!m_speedTravel) return;
	WBI(suitMode,NANOMODE_SPEED);
    }
	// Speed drains energy fast, so a ">160" threshold was almost never met: jump at >130 instead.
	// Owner playtest 25.10: 157 of 364 Strength jumps came from this random travel hop - "jumps in Strength for no
	// reason". wb_bot_jump_sanity drops it.
	if (BotConfig::Get().jumpSanity != 1 && distance > 15.0f && m_blackboard.grounded && m_blackboard.energy > 130.0f &&
		m_now >= m_blackboard.nextSuitChange && m_now >= m_nextTravelJump)
	{
		m_nextTravelJump = m_now + 5.0f + Rand01() * 5.0f;
		if (Rand01() < 0.7f) { WBI(jump,BotJump::Strength); WBI(suitMode,NANOMODE_STRENGTH); }
	}
}

Vec3 BotBrain::Steer(const Vec3& feet, const Vec3& goal)
{
	const BotNav& nav = g_pGame->GetBotManager().Nav();
	NavMeshCore* mesh = CurrentNavMesh(); // P0, wb_bot_navmesh: Detour corners instead of the sparse ray graph
	m_cornerSlow = false;
	if (!nav.Ready() && !mesh) return goal; // fallback: straight line + local avoidance
	// Failed paths also respect the two-second retry deadline. An empty failed path is
	// a cached result, not a reason to repeat A* at the 0.75-second global throttle.
	if (m_link.Active()) return Vec3(m_linkNow.b[0], m_linkNow.b[1], m_linkNow.b[2]); // C-036: the link executor owns movement
	if (((goal - m_pathGoal).GetLengthSquared() > 16.0f || m_now >= m_nextRepath) && m_now >= m_minRepath)
	{
		m_minRepath = m_now + 0.75f;
		BotNav::PathTrace trace;
		bool found = false;
		m_meshPathActive = false;
		if (mesh)
		{
			std::vector<float> xyz; const char* why = "navmesh";
			const float a[3] = {feet.x, feet.y, feet.z}, b[3] = {goal.x, goal.y, goal.z};
			// DS-089: a native swimmer floats 1.2-2.3 m below the water surface; use the plane-sized bounded water start
			// box only while the native water movement is active. The core accepts only the baked water polygon and
			// fails closed on a missing/non-water ref (no dry/seabed/stacked fallback).
			NavQueryBounds water;
			const bool waterStart = WaterStartBounds(m_blackboard, feet.z, water); // DS-089: sized from the native plane
			found = mesh->FindPathLinks(a, b, xyz, m_pathLinks, &why, waterStart ? &water : nullptr);
			if (!found) m_pathLinks.clear();
			m_meshPathActive = found;
			m_path.clear();
			for (size_t i = 0; i + 2 < xyz.size(); i += 3) m_path.push_back(Vec3(xyz[i], xyz[i + 1], xyz[i + 2]));
			trace.reason = why;
		}
		if (!found) m_pathLinks.clear();
		if (!found && nav.Ready()) found = nav.FindPath(feet, goal, m_path, &trace);
		m_pathIndex = 0; m_cornerFor = size_t(-1); m_pathStart = feet;
		m_pathGoal = goal;
		m_nextRepath = m_now + (found ? 5.0f : 2.0f);
		if (!found)
		{
			m_path.clear();
			WB_LOG(2, "[WarsBots] #%d NAV fallback start=(%.2f,%.2f,%.2f) goal=(%.2f,%.2f,%.2f) retry=2.0. actor=%u reason=%s from=%d to=%d expanded=%d",
				m_number, feet.x, feet.y, feet.z, goal.x, goal.y, goal.z,
                m_actorId,trace.reason,trace.from,trace.to,trace.expanded);
		}
	}
	if (m_path.empty()) { m_meshPathActive = false; return goal; } // Sparse graph can miss a physically reachable item.
	m_intent.meshPath = m_meshPathActive; // stairs on the corner path are walkable (BotMovement knee ray off)
	// C-074: BotMovement::Move stops 0.8 m short of its destination; corners v2 passes a corner only within 0.35 m, so
	// the bot stood 0.8 m short of every aim point (c074 stalls 343 -> 1704 s). Corner following owns arrival.
	m_intent.arrive = 0.8f;
	if (mesh && m_meshPathActive && BotConfig::Get().corners == 1) { m_intent.arrive = 0.2f; return SteerCorners(*mesh, feet); }
	// Advance past reached waypoints; skip ahead when the next-but-one is directly reachable.
	while (m_pathIndex + 1 < m_path.size())
	{
		const Vec3& p = m_path[m_pathIndex];
		if (m_meshPathActive && LinkAtCorner(m_pathIndex)) break; // a link take-off is passed only by the executor
		if ((p - feet).GetLengthSquared2D() < 2.25f && fabs_tpl(p.z - feet.z) < 2.0f) ++m_pathIndex;
		else break;
	}
	return m_path[m_pathIndex];
}

// wb_bot_corners v2 (BotCorners.h): aim at each corner moved 0.15 m to the outside of the turn, pass it only when
// reached or crossed, and ask for no sprint before sharp turns.
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
	if (m_pathIndex + 1 >= m_path.size() || LinkAtCorner(m_pathIndex) || m_cornerFor != m_pathIndex) return c;
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

void BotBrain::Configure(const BotProfile& profile, int number)
{
	m_profile = profile;
	m_number = number;
	// Golden-ratio phase: distinct, deterministic per slot. Drives aim noise, strafe and patrol
	// offsets so bots created in the same frame never share timers or "random" sequences.
	m_phase = float(number) * 1.6180339f;
	m_patrolStep = unsigned(number) * 7u;
	m_rng = 0x9E3779B9u * unsigned(number + 1);
	m_combat.Configure(profile);
	m_traits = MakeTraits(int(profile.skill), int(profile.style), unsigned(number)); // T-8
}

void BotBrain::ResetLife()
{
	m_trap.Reset(); m_trapStepUntil=0; m_trapJumpAttempted=false; m_escapeUntil=0; m_trackedTarget=0; m_trackedSelection=-1;
	m_locomotion.Reset();
	m_suit.Reset(); m_blackboard=BotBlackboard(); m_intent=BotIntent();
	m_testJump=BotJump::None; m_testGait=m_testMode=-1; m_testUntil=m_testModeUntil=m_testJumpAt=0;
	m_combat.Reset();
	m_weaponTarget = 0;
	m_patrolUntil = 0;
	m_detour = false; m_blockedSince = 0; m_chaseBlockedSince = 0; m_shotBlockedSince = 0;
	m_path.clear(); m_pathIndex = 0; m_cornerFor = size_t(-1); m_cornerSlow = false; m_nextRepath = 0; m_minRepath = 0; m_meshPathActive = false; m_pathLinks.clear(); m_link.Reset();
	if (m_ladder.Active() || m_ladderNativeLocked) m_ladderCancelPending = true;
	m_ladder.Reset(); m_ladderNativeLocked = false;
	m_vantage.Reset(CurrentVantage(),m_number);
	m_hold.Reset(); m_lastFightTarget = 0; m_lootUntil = 0;
	m_upgradeTracked = 0; m_upgradeBest = 1e9f; m_upgradeBestAt = m_now;   // DS-005
	m_withdrawUntil=m_nextWithdraw=0; m_withdrawCoverValid=false;
	m_psCapture.Reset(); m_psTarget=0; m_nextPsCapture=0; m_psPoints.clear(); m_psBought=false; m_psBuys=0; m_lifeStartedAt=m_now; m_nextAliveBuy=0; m_doorUsed.clear();
	m_speedTravel = false; m_nextTravelJump = m_now + 3.0f; m_nextDodge = 0; m_lastHitSeen = 0;
	m_upgradeTarget = 0; m_nextUpgradeSearch = 0; m_stuckReported = false; m_upgradeSince = 0;
	++m_life;
	m_testStrafe=0; m_strafeSide=((m_life+unsigned(m_number))&1)?1:-1; m_nextStrafe=m_now+0.37f*float(m_number%5);
	m_fallbackGiven = false;
	m_retreating = false;
	m_look.Reset(); m_lookValid = false; m_arbiter.Reset(); m_nextRangeSelect = 0; m_nextIdleSelect = 0; m_highUntil = 0; m_nextHighRoll = 0; m_highHolding = false; m_aimModelInit = false; m_aimModelTarget = 0; m_suitPolicy.Reset();
	m_weaponSearchStarted = m_now;
	m_nextSearch = m_now;
	m_deadTime = 0;
	m_psReviveLogged = false;
}

bool BotBrain::Start(BotInstance& bot)
{
	if (m_running || !bot.CanRunLocalIA() || !bot.GetActor())
	{
		WB_LOG(0, "[WarsBots] #%d AI start refused: already running, no owned actor or local IA gate closed.", m_number);
		return false;
	}
	WB_LOG(2, "[WarsBots] #%d AI state=SPAWNING (explicit server start) profile=%s/%s.", m_number,
		BotProfile::StyleName(m_profile.style), BotProfile::SkillName(m_profile.skill));
	m_running = true;
	m_now = 0;
	// Stagger 10 Hz decision frames across bots instead of running all on one frame.
	m_tick = 0.1f * float(m_number % 10) / 10.0f;
	m_nextTelemetry = 0;
	Change(State::SPAWNING,true);
	ResetLife();
	bot.GetActor()->GetGameObject()->EnablePrePhysicsUpdate(ePPU_Always);
	Change(State::SEEKING_WEAPON);
	return true;
}

void BotBrain::Stop(BotInstance& bot)
{
	if (!m_running) return;
	m_trap.Reset(); m_trapStepUntil=0;
	// DS-107: one owned cancellation path clears native ladder mode as a failure (never route success).
	if (CActor* actor = bot.GetActor()) CancelLadder(static_cast<CPlayer&>(*actor), "stop", false);
	else { m_ladder.Reset(); m_ladderNativeLocked = false; }
	m_running = false; // Cancel respawn before touching entity/weapon lifetime.
	m_combat.Reset();
	if (CActor* actor = bot.GetActor())
	{
		m_blackboard.Sample(static_cast<CPlayer&>(*actor),m_now);
		if (m_blackboard.canMove && m_blackboard.suitActive)
			m_suit.Apply(static_cast<CPlayer&>(*actor),NANOMODE_DEFENSE,m_blackboard,m_now);
		m_locomotion.Stop(static_cast<CPlayer&>(*actor));
		m_testJump=BotJump::None; m_testGait=m_testMode=-1; m_testUntil=m_testModeUntil=m_testJumpAt=0;
		m_testStrafe=0;
		actor->GetGameObject()->EnablePrePhysicsUpdate(ePPU_WhenAIActivated);
	}
	m_intent=BotIntent(); m_blackboard.nextJump=m_blackboard.nextSuitChange=0; m_suit.Reset();
	WB_LOG(2, "[WarsBots] #%d AI stopped; automatic respawn cancelled.", m_number);
}

void BotBrain::Status() const
{
	WB_LOG(0, "[WarsBots] #%d AI running=%d state=%s target=%u weaponTarget=%u verbose=%d.",
		m_number, m_running ? 1 : 0, NameOf(m_state), m_combat.Target(), m_weaponTarget, m_verbose ? 1 : 0);
}

void BotBrain::Update(BotInstance& bot, float dt)
{
	if (!m_running) return;
	if (!bot.CanRunLocalIA() || !bot.GetActor()) { Stop(bot); return; }
	dt = clamp_tpl(dt, 0.0f, 0.25f);
	m_now += dt;
	CPlayer& actor = static_cast<CPlayer&>(*bot.GetActor());
    m_actorId=actor.GetEntityId();
	m_blackboard.Sample(actor,m_now);
	m_locomotion.Observe(actor,m_blackboard,m_now);
    // DS-107: ResetLife cannot touch the actor; perform its native-ladder cleanup on the next owned update.
    if (m_ladderCancelPending) CancelLadder(actor,"reset",false);
    UpdateTelemetry(actor);
    // DS-107: death/stop must clear the native ladder mode through the single cancellation path.
    if (actor.GetHealth()<=0 && (m_ladder.Active() || m_ladderNativeLocked)) CancelLadder(actor,"death",false);
    if(UpdateDead(bot,actor,dt))return;
	if (BotConfig::Get().botTurnCap==1) UpdateTurnCap(actor,dt); // C-044: per-frame capped view toward the decided look point
	m_tick += dt;
	if (m_tick < 0.1f) return; // Bound entity searches, raycasts and decisions to 10 Hz.
	const float decisionTime = m_tick;
    ++m_decisionSerial;
	m_tick = 0;
	if (BotConfig::Get().stateCommit>=2) m_commit.BeginTick(m_decisionSerial,m_now);
	if (m_state == State::DEAD || m_state == State::RESPAWNING) { ResetLife(); Change(State::SEEKING_WEAPON); }

	m_intent=BotIntent();
	WBI(aim,actor.GetEntity()->GetWorldPos()+actor.GetBaseQuat().GetColumn1()*10.0f+Vec3(0,0,1.4f));
	// DS-107: a gate turned OFF mid-climb must still clean the native mode; when ON the ladder owns the tick.
	if (BotConfig::Get().ladder!=1 && (m_ladder.Active() || m_ladderNativeLocked)) CancelLadder(actor,"gate_off",false);
	if (BotConfig::Get().ladder==1 && m_ladder.Active()) { UpdateLadder(actor,decisionTime); return; }
    if(UpdateTest(actor,decisionTime))return;
    const int& qaWeapon=BotConfig::Get().qaSocomOnly;
    const bool socomQA=qaWeapon==1 && bot.CanRunLocalIA();
    if(UpdateWeaponSeek(actor,decisionTime,socomQA))return;
	IActor* enemy = m_combat.FindEnemy(actor, m_now, m_verbose);
	// DS-044 #1 (wb_bot_first_primary): a spawn pistol is not a plan. Go to a rifle before roam/fight unless an enemy
	// is within 10 m; the candidate is scored by navmesh walk length, not straight distance (UpdateUpgrade pathCost).
	{
		const float enemyD = enemy ? (enemy->GetEntity()->GetWorldPos()-actor.GetEntity()->GetWorldPos()).GetLength() : 1e9f;
		const float seenAge = enemy && m_combat.LastSeen()>0.0f ? m_now-m_combat.LastSeen() : 1e9f;
		const bool urgentFight = enemy && FirstPrimaryCombatUrgent(m_blackboard.damagedUntil-m_now>2.4f,
			m_combat.Visible(),seenAge);
		if (m_now>=m_escapeUntil && m_now>=m_withdrawUntil &&
			ShouldSeekFirstPrimary(BotConfig::Get().firstPrimary==1,BotWeapons::HasPrimary(actor),enemyD,socomQA,urgentFight) &&
			UpdateUpgrade(bot,actor,enemy,decisionTime,socomQA,true)) return;
	}
	if (m_lastFightTarget)
	{
		// the fight target died (or left): its dropped weapon / ammo is worth a look for a few seconds (tactics), and a
		// sniper counts the kill toward relocating (DS-005: BotVantage::OnKill was never called)
		IActor* last=g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_lastFightTarget);
		if (!last || last->GetHealth()<=0)
		{
			if (BotConfig::Get().tactics==1) { m_lootPos=m_lastFightPos; m_lootUntil=m_now+12.0f; }
			if (last && m_vantage.Phase()==VantagePhase::Hold) m_vantage.OnKill();
			m_lastFightTarget=0; m_hold.Reset();
		}
	}
	if (m_now<m_escapeUntil) enemy=nullptr; // A short, sensed-but-not-pursued local escape leg.
    if(BotConfig::Get().goalArbiter==1 && !socomQA && UpdateArbitrated(bot,actor,enemy,decisionTime,socomQA))return; // C-032
    if(UpdateUpgrade(bot,actor,enemy,decisionTime,socomQA))return;
    if(!enemy){
        // between fights a player tops the clip up (wb_bot_difficulty: N 40 %, V 50 %, E 60 %; Beginner waits for empty)
        if(m_profile.reloadAt>0.0f && m_now-m_combat.LastSeen()>2.0f) BotWeapons::TacticalReload(actor,m_profile.reloadAt);
        UpdateRoam(bot,actor,decisionTime);return;}
    UpdateCombat(bot,actor,decisionTime);
}

void BotBrain::UpdateTelemetry(CPlayer& actor)
{
	if (m_verbose && m_now >= m_nextTelemetry)
	{
		const Vec3 position = actor.GetEntity()->GetWorldPos();
		IWeapon* weapon = BotWeapons::Current(actor);
		IFireMode* mode = weapon ? weapon->GetFireMode(weapon->GetCurrentFireMode()) : nullptr;
		WB_LOG(2, "[WarsBots] AI telemetry EntityId=%u state=%s pos=(%.2f, %.2f, %.2f) health=%d weapon=%u ammo=%d target=%u.",
			actor.GetEntityId(), NameOf(m_state), position.x, position.y, position.z, actor.GetHealth(),
			actor.GetCurrentItem() ? actor.GetCurrentItem()->GetEntityId() : 0, mode ? mode->GetAmmoCount() : -1, m_combat.Target());
		WB_LOG(2,"[WarsBots] TACTIC SAMPLE actor=%u skill=%s style=%s weapon=%s suit=%d distance=%.2f action=%s target=%u visible=%d.",
			actor.GetEntityId(),BotProfile::SkillName(m_profile.skill),BotProfile::StyleName(m_profile.style),
			actor.GetCurrentItem()?actor.GetCurrentItem()->GetEntity()->GetClass()->GetName():"none",int(m_blackboard.mode),
			(m_combat.Target()?(m_combat.LastPosition()-position).GetLength():0.0f),m_now<m_escapeUntil?"escape":m_now<m_withdrawUntil?"withdraw":NameOf(m_state),m_combat.Target(),m_combat.Visible()?1:0);
		WB_LOG(2, "[WarsBots] AI decision target=%u visible=%d sample=%.2f age=%.2f (cached decision; muzzle checked separately).",
			m_combat.Target(), m_combat.Visible()?1:0, m_combat.SampleTime(), m_now-m_combat.SampleTime());
		const Vec3 heading = actor.GetEntity()->GetWorldRotation().GetColumn1();
		pe_status_dynamics dynamics;
		const bool physical = actor.GetEntity()->GetPhysics() && actor.GetEntity()->GetPhysics()->GetStatus(&dynamics);
		WB_LOG(2, "[WarsBots] Motion heading=(%.2f,%.2f) physicalSpeed=%.2f.",heading.x,heading.y,physical?dynamics.v.GetLength2D():-1.0f);
		int clients=0;
		IActorIteratorPtr connected=g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
		while (IActor* client=connected->Next())
			if (client->GetChannelId() && g_pGame->GetIGameFramework()->GetNetChannel(client->GetChannelId())) ++clients;
		WB_LOG(2, "[WarsBots] ABILITY PHYSICS: speed=%.2f vz=%.2f grounded=%d sprint=%d mode=%d energy=%.1f active=%d clients=%d.",
			m_blackboard.velocity.GetLength2D(),m_blackboard.velocity.z,m_blackboard.grounded?1:0,
			static_cast<const SPlayerStats*>(actor.GetActorStats())->bSprinting?1:0,int(m_blackboard.mode),m_blackboard.energy,m_blackboard.suitActive?1:0,clients);
		Vec3 sight=m_intent.aim-position; sight.z=0; sight.NormalizeSafe(FORWARD_DIRECTION);
        const Vec3 right(sight.y,-sight.x,0);
        WB_LOG(2, "[WarsBots] MOTION INTENT: gait=%d strafe=%d requestedBase=%.2f actual=%.2f lateral=%.2f facingDot=%.2f blocked=%d.",
            int(m_intent.gait),m_intent.strafe?1:0,m_intent.move?(m_intent.gait==BotGait::Walk?1.3f:2.5f):0.0f,
            m_blackboard.velocity.GetLength2D(),m_blackboard.velocity.Dot(right),heading.Dot(sight),m_blackboard.blocked?1:0);
        m_nextTelemetry = m_now + 2.0f;
	}

}

bool BotBrain::UpdateDead(BotInstance& bot,CPlayer& actor,float dt)
{
	if (actor.GetHealth() <= 0)
	{
		m_trap.Reset(); m_trapStepUntil=0;
		m_combat.StopFire();
		m_locomotion.Stop(actor);
		m_testJump=BotJump::None; m_testGait=m_testMode=-1; m_testUntil=m_testModeUntil=m_testJumpAt=0;
		if (m_state != State::DEAD)
		{
			Change(State::DEAD); m_deadTime=0; m_intent=BotIntent();
			m_blackboard.nextJump=m_blackboard.nextSuitChange=0; m_suit.Reset();
		}
		m_deadTime += dt;
		// DS-043: in PowerStruggle the PS revive wave owns the respawn; the 5 s self-revive double-revived a bot that
		// the wave then revived again. Keep a 30 s fallback so a broken queue cannot leave a bot dead for the match.
		BotGameModePolicy mode; if (g_pGame && g_pGame->GetGameRules()) mode = BotMatch::Mode(*g_pGame->GetGameRules()).Policy();
		const bool ps = mode.waveRevive;
		if (ps && mode.reviveBuy && BotConfig::Get().psBuy==1 && !m_psBought && m_deadTime>=2.0f) { m_psBought=true; PSTryBuy(bot,actor); }
		if (ps && !m_psReviveLogged && m_deadTime >= 5.0f)
		{
			m_psReviveLogged = true;
			WB_LOG(1, "[WarsBots] #%d PS revive delegated to the wave (5 s self-revive skipped, fallback 30 s) actor=%u.", m_number, m_actorId);
		}
		if (m_deadTime >= (ps ? 30.0f : 5.0f))
		{
			Change(State::RESPAWNING);
			const bool revived = bot.Revive();
			if (ps) WB_LOG(1, "[WarsBots] #%d PS fallback revive after %.1f s: %s.", m_number, m_deadTime, revived ? "ok" : "refused"); // C-121
			if (revived)
			{
				ResetLife();
				actor.GetGameObject()->EnablePrePhysicsUpdate(ePPU_Always);
				Change(State::SEEKING_WEAPON);
			}
			else { Change(State::DEAD); m_deadTime = 0; }
		}
		return true;
	}

    return false;
}

bool BotBrain::PSTryBuy(BotInstance& bot,CPlayer& actor)
{
	CGameRules* rules=g_pGame?g_pGame->GetGameRules():nullptr;
	IEntity* rulesEntity=rules?rules->GetEntity():nullptr;
	IScriptTable* table=rulesEntity?rulesEntity->GetScriptTable():nullptr;
	if(!table || !gEnv || !gEnv->pScriptSystem)return false;
	IScriptSystem* ss=gEnv->pScriptSystem;
	const EntityId id=actor.GetEntityId();
	int pp=0;
	if(ss->BeginCall(table,"GetPlayerPP")) { ss->PushFuncParam(table); ss->PushFuncParam(ScriptHandle(id)); ss->EndCall(pp); }
	PsBuyInput in;
	in.pp=pp; in.style=int(m_profile.style); in.skill=int(m_profile.skill);
	in.preferredRange=PickupRange(); in.hasPrimary=BotWeapons::HasPrimary(actor); in.alive=actor.GetHealth()>0;
	const PsBuyPlan plan=ChoosePsBuy(in);
	if(!plan.buyPrimary)
	{
		WB_LOG(1,"[WarsBots] #%d PS BUY skip pp=%d style=%s range=%.0f primary=%d.",m_number,pp,
			BotProfile::StyleName(m_profile.style),in.preferredRange,in.hasPrimary?1:0);
		return false;
	}
	auto buy=[&](const char* item)->bool {
		if(!item)return false;
		if(!ss->BeginCall(table,"WarsBotsBuy"))return false;
		ss->PushFuncParam(table); ss->PushFuncParam(ScriptHandle(id)); ss->PushFuncParam(item);
		// X-192: WarsBotsBuy returns a Lua boolean; EndCall(int) leaves 0 on success (PP 100->25/50 in the demo
		// while ok=0), so the laser branch never ran. Read it as bool, like CanEnterVehicle does.
		bool ok=false; ss->EndCall(ok); return ok;
	};
	const bool primary=buy(plan.primary);
	if (primary) SetUpgradeTarget(0); // Re-score against the new inventory, without banning the old item.
	const bool laser=primary&&plan.buyLaser?buy(plan.laser):false;
	const bool ammo=primary&&plan.buyAmmo?buy(plan.ammo):false;
	m_psBuys+=int(primary)+int(laser)+int(ammo);
	WB_LOG(1,"[WarsBots] #%d PS BUY pp=%d primary=%s price=%d ok=%d laser=%s ok=%d ammo=%s ok=%d actor=%u.",
		m_number,pp,plan.primary,plan.primaryPrice,primary?1:0,plan.buyLaser?plan.laser:"-",laser?1:0,
		plan.buyAmmo?plan.ammo:"-",ammo?1:0,id);
	(void)bot;
	return primary;
}


// DS-067d: doors are their own step so they can run before the capture goal without giving vehicles the same
// priority. Capture still runs before vehicles (unchanged); a door request never preempts capture movement.
bool BotBrain::UpdatePSDoors(BotInstance& bot,CPlayer& actor,float decisionTime)
{
	CGameRules* rules=g_pGame?g_pGame->GetGameRules():nullptr;
	if(!rules || BotMatch::Mode(*rules).Kind()!=BotGameModeKind::PowerStruggle)return false;
	if (BotConfig::Get().psDoors!=1) return false;
	if (m_now<m_nextDoorUse) return false;
	const EntityId id=actor.GetEntityId();
	const Vec3 feet=actor.GetEntity()->GetWorldPos();
	m_nextDoorUse=m_now+0.25f; // bound the 10 Hz proximity scan even when no door opens (C145 note)
	SEntityProximityQuery query; query.box=AABB(feet-Vec3(5,5,3),feet+Vec3(5,5,3)); gEnv->pEntitySystem->QueryProximity(query);
	for (int i=0;i<query.nCount;++i)
	{
		IEntity* e=query.pEntities[i]; if(!e || e->IsHidden())continue;
		const char* cls=e->GetClass()?e->GetClass()->GetName():""; if(std::strcmp(cls,"Door"))continue;
		IScriptTable* dt=e->GetScriptTable(); if(!dt)continue;
		int action=-1; dt->GetValue("action",action); if(action==1)continue; // DOOR_OPEN
		auto used=m_doorUsed.find(e->GetId()); if(used!=m_doorUsed.end() && m_now<used->second)continue;
		const Vec3 to=e->GetWorldPos()-feet; if(to.GetLengthSquared2D()>25.0f)continue;
		bool ahead=false;
		if (m_meshPathActive && m_pathIndex<m_path.size())
		{
			const Vec3 seg=m_path[m_pathIndex]-feet; const float len2=seg.GetLengthSquared2D();
			if (len2>0.01f) { const float t=clamp_tpl(to.Dot(seg)/len2,0.f,1.f); const Vec3 on=feet+seg*t; ahead=t>0.1f && (e->GetWorldPos()-on).GetLength2D()<=3.0f; }
		}
		else { const Vec3 fwd=actor.GetEntity()->GetWorldRotation().GetColumn1(); ahead=fwd.Dot(to.GetNormalizedSafe(fwd))>=0.2f; }
		if(!ahead) { WB_LOG(2,"[WarsBots] #%d PS DOOR skip entity=%u actor=%u reason=ahead dist=%.2f.",m_number,e->GetId(),id,to.GetLength()); continue; }
		// DS-054: Door:OnUsed is the client use path (self.server:SvRequestOpen RMI at Door.lua:356); from the server
		// it hits the client RMI dispatcher ("not a client"). Call the stock server handler Door:Open(user, DOOR_OPEN)
		// directly: Door.lua:523-524 "this is only safe to be called from server", :565/:572 allClients broadcast.
		Script::CallMethod(dt,"Open",actor.GetEntity()->GetScriptTable(),1);
		int post=-1; dt->GetValue("action",post);
		const Vec3 doorPos=e->GetWorldPos();
		m_doorUsed[e->GetId()]=m_now+2.0f;
		WB_LOG(1,"[WarsBots] #%d PS DOOR open entity=%u actor=%u pre=%d post=%d dist=%.2f actorPos=(%.2f,%.2f,%.2f) doorPos=(%.2f,%.2f,%.2f).",
			m_number,e->GetId(),id,action,post,(doorPos-feet).GetLength(),feet.x,feet.y,feet.z,doorPos.x,doorPos.y,doorPos.z);
		break;
	}
	return false; // never preempts capture in the same tick
}

bool BotBrain::UpdatePSInteract(BotInstance& bot,CPlayer& actor,float decisionTime)
{
	CGameRules* rules=g_pGame?g_pGame->GetGameRules():nullptr;
	if(!rules || BotMatch::Mode(*rules).Kind()!=BotGameModeKind::PowerStruggle)return false;
	if (BotConfig::Get().psVehicles!=1) return false;
	const EntityId id=actor.GetEntityId();
	const Vec3 feet=actor.GetEntity()->GetWorldPos();
	if (IVehicle* v=actor.GetLinkedVehicle())
	{
		if (m_now>=m_vehicleExitAt)
		{
			if (IVehicleSeat* s=v->GetSeatForPassenger(id)) s->Exit(false,false);
			WB_LOG(1,"[WarsBots] #%d PS VEHICLE exit vehicle=%u actor=%u.",m_number,v->GetEntityId(),id);
			m_nextVehicle=m_now+5.0f;
		}
		return true;
	}

	if (m_now>=m_nextVehicle)
	{
		SEntityProximityQuery query; query.box=AABB(feet-Vec3(10,10,3),feet+Vec3(10,10,3)); gEnv->pEntitySystem->QueryProximity(query);
		for (int i=0;i<query.nCount;++i)
		{
			IEntity* e=query.pEntities[i]; if(!e || e->IsHidden())continue;
			const char* cls=e->GetClass()?e->GetClass()->GetName():"";
			if (std::strcmp(cls,"Asian_ltv") && std::strcmp(cls,"Civ_car1")) continue;
			IVehicle* v=g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(e->GetId());
			if(!v)continue; IVehicleSeat* seat=v->GetSeatById(1); if(!seat || seat->GetPassenger())continue;
			bool can=false; IScriptTable* rt=rules->GetEntity()->GetScriptTable(); IScriptTable* vt=e->GetScriptTable();
			if(rt && vt && gEnv->pScriptSystem->BeginCall(rt,"CanEnterVehicle")) { gEnv->pScriptSystem->PushFuncParam(rt); gEnv->pScriptSystem->PushFuncParam(vt); gEnv->pScriptSystem->PushFuncParam(ScriptHandle(id)); gEnv->pScriptSystem->EndCall(can); }
			if(!can)continue;
			if(seat->Enter(id,false)){ m_vehicleExitAt=m_now+3.0f; WB_LOG(1,"[WarsBots] #%d PS VEHICLE enter vehicle=%u class=%s actor=%u.",m_number,e->GetId(),cls,id); return true; }
		}
		m_nextVehicle=m_now+2.0f;
	}
	return false;
}

bool BotBrain::UpdatePSCapture(BotInstance& bot,CPlayer& actor,float decisionTime)
{
	CGameRules* rules=g_pGame?g_pGame->GetGameRules():nullptr;
	if(!rules)return false;
	const int team=rules->GetTeam(actor.GetEntityId());
	if(!team)return false;
	const Vec3 feet=actor.GetEntity()->GetWorldPos();
	if(m_now>=m_nextPsCapture)
	{
		m_nextPsCapture=m_now+2.0f;
		const float from[3]={feet.x,feet.y,feet.z};
		// DS-087/089: keep the PS capture goal alive from the same bounded, plane-sized water start.
		NavQueryBounds water;
		const bool waterStart = WaterStartBounds(m_blackboard, feet.z, water);
		PSCaptureSnapshot(m_psPoints,*rules,from, waterStart?&water:nullptr);
	}
	// DS-072: hold on native area membership of the current target, not a 6 m sphere around the entity pivot.
	bool inside=false;
	if (m_psTarget)
	{
		for (const PsCapturePoint& p : m_psPoints)
			if (p.id==m_psTarget && p.areaId)
			{
				if (IEntity* area=gEnv->pEntitySystem->GetEntity(EntityId(p.areaId)))
					if (IEntityAreaProxy* proxy=static_cast<IEntityAreaProxy*>(area->GetProxy(ENTITY_PROXY_AREA)))
						inside=proxy->IsPointWithin(feet);
				break;
			}
	}
	const PsCapturePlan plan=m_psCapture.Update(m_psPoints.empty()?nullptr:m_psPoints.data(),int(m_psPoints.size()),team,
		feet.x,feet.y,feet.z,inside);
	if(!plan.valid)return false;
	if(m_psTarget!=plan.id)
	{
		m_psTarget=plan.id;
		WB_LOG(1,"[WarsBots] #%d PS CAPTURE target=%d pos=(%.1f,%.1f,%.1f) team=%d hold=%d actor=%u.",
			m_number,plan.id,plan.x,plan.y,plan.z,team,plan.hold?1:0,actor.GetEntityId());
	}
	Change(State::SEARCHING_ENEMY);
	if(plan.hold)
	{
		WBI(move,false);
		WBI(aim,Vec3(plan.x,plan.y,plan.z+1.5f));
	}
	else
	{
		const Vec3 goal(plan.x,plan.y,plan.z);
		const Vec3 step=Steer(feet,goal);
		WBI_MOVETO(step,step+Vec3(0,0,1.2f));
		TravelGait((goal-feet).GetLength());
	}
	ExecuteIntent(actor,decisionTime);
	return true;
}
bool BotBrain::UpdateTest(CPlayer& actor,float decisionTime)
{
	if (m_now<m_testUntil)
	{
		if (m_testGait>=0) { WBI_MOVETO(m_testGoal,m_testStrafe ? m_testLook : m_testGoal+Vec3(0,0,1.4f)); WBI(gait,static_cast<BotGait>(m_testGait)); m_intent.strafe=m_testStrafe!=0; }
		WBI(jump,m_now>=m_testJumpAt?m_testJump:BotJump::None);
		if (m_testJump!=BotJump::None) WBI(suitMode,m_testJump==BotJump::Strength?NANOMODE_STRENGTH:NANOMODE_DEFENSE);
		else if (m_testGait==int(BotGait::Sprint)) WBI(suitMode,NANOMODE_SPEED);
		if (m_now>=m_testJumpAt) m_testJump=BotJump::None; // Consume the command once, including a refused attempt.
		ExecuteIntent(actor,decisionTime); return true;
	}
	m_testGait=-1;

    return false;
}

bool BotBrain::UpdateWeaponSeek(CPlayer& actor,float decisionTime,bool socomQA)
{
    if (socomQA && (!actor.GetCurrentItem() ||
        strcmp(actor.GetCurrentItem()->GetEntity()->GetClass()->GetName(),"SOCOM") ||
        !BotWeapons::Current(actor) || !BotWeapons::HasAmmunition(actor,*BotWeapons::Current(actor)))) {
        m_combat.StopFire();WBI(move,false);ExecuteIntent(actor,decisionTime);return true;
    }
	if (!BotWeapons::SelectAvailable(actor))
	{
		m_combat.StopFire();
		if (m_now >= m_nextSearch && !m_weaponTarget)
		{
			Change(State::SEEKING_WEAPON);
			m_weaponTarget = BotWeapons::FindNearby(actor, 30.0f, m_previousWeaponClass, m_life, false, nullptr, 0, 20.f, BotConfig::Get().firstPrimary==1);
			m_nextSearch = m_now + 2.0f;
		}
		if (m_weaponTarget && !BotWeapons::AvailableForPickup(actor, m_weaponTarget))
		{
			WB_LOG(2, "[WarsBots] #%d weapon goal cancelled item=%u reason=unavailable.", m_number, m_weaponTarget);
			m_weaponTarget = 0;
		}
		IEntity* item = gEnv->pEntitySystem->GetEntity(m_weaponTarget);
		if (item && m_now - m_weaponSearchStarted < 6.0f)
		{
			Change(State::MOVING_TO_WEAPON);
			if ((item->GetWorldPos() - actor.GetEntity()->GetWorldPos()).GetLengthSquared2D() <= 4.0f && fabs_tpl(item->GetWorldPos().z - actor.GetEntity()->GetWorldPos().z) <= 2.2f)
			{
				WBI(move,false);
				if (BotWeapons::PickUp(actor, m_weaponTarget, PickupRange())) { m_previousWeaponClass = actor.GetCurrentItem()->GetEntity()->GetClass(); m_weaponTarget = 0; SetUpgradeTarget(0); Change(State::SEARCHING_ENEMY); }
				else m_weaponTarget = 0;
			}
			else { const Vec3 step=Steer(actor.GetEntity()->GetWorldPos(),item->GetWorldPos()); WBI_MOVETO(step,step+Vec3(0,0,1.2f)); TravelGait((item->GetWorldPos()-actor.GetEntity()->GetWorldPos()).GetLength()); }
		}
		else if (!m_fallbackGiven && (!item || m_now - m_weaponSearchStarted >= 6.0f))
		{
			WBI(move,false);
			m_fallbackGiven = true; // At most one test supply per life, never per frame.
			if (BotWeapons::GiveTestSCAR(actor)) { SetUpgradeTarget(0); Change(State::SEARCHING_ENEMY); }
		}
		else { m_weaponTarget = 0; WBI(move,false); Change(State::SEEKING_WEAPON); }
		ExecuteIntent(actor,decisionTime);
		return true;
	}


    return false;
}

float BotBrain::PickupRange() const
{ return WarsBots::IdleHoldRange(BotConfig::Get().vantage==1 && m_traits.camper>=0.7f); }

EntityId BotBrain::UpgradeCandidate(CPlayer& actor,bool anyHeld,bool pathCost,bool sniperOnly)
{
	(void)anyHeld; // with wb_bot_weapon_rank FindNearby keeps only real upgrades over what is held
	EntityId skip[8]; int skipCount = 0;
	for (const BadItem& b : m_badItems) if (b.id && m_now < b.until) skip[skipCount++] = b.id;
	// owner 25.10 (wb_bot_vantage): Marksmen judge pickups at sniper range, so DSG1/Gauss become real upgrades
	const float typical=PickupRange();
	// c066: no bot ever reached DSG1 (22 m up) / Gauss (tower) within 45 m -> snipers look across the map
	return BotWeapons::FindNearby(actor, typical>30.0f ? 200.0f : 45.0f, nullptr, m_life, true, skip, skipCount, typical, pathCost, sniperOnly);
}

// DS-097: reset the goal and its clocks together, so a stale progress/budget stamp can never outlive its target.
void BotBrain::SetUpgradeTarget(EntityId id)
{
	if (m_upgradeTarget!=id)
		WB_LOG(2,"[WarsBots] #%d WEAPON GOAL previous=%u target=%u event=%s.",m_number,m_upgradeTarget,id,id?"acquire":"clear");
	m_upgradeTarget = id;
	m_upgradeTracked = 0; m_upgradeBest = 1e9f; m_upgradeBestAt = 0; m_upgradePathSig = 0;
	m_upgradeSince = 0; // re-anchored on the first tick that actually pursues the goal
}

// DS-097: the only place that creates a weapon goal (was BotBrain.cpp:788-792 unconditional + :1761-1762 in the arbiter).
// Active pursuit is held; unchosen candidates refresh on the same throttle. Empty scans are throttled too.
bool BotBrain::TryAcquireUpgradeGoal(CPlayer& actor)
{
	if (!WarsBots::MayAcquireWeaponGoal(m_upgradeTarget, m_now, m_nextUpgradeSearch,
		WarsBots::WeaponGoalPursues(m_upgradeSince))) return false;
	const bool sniperOnly = BotConfig::Get().sniperReach==1 && PickupRange()>30.0f && !BotWeapons::HoldsSniper(actor);
	const EntityId id = UpgradeCandidate(actor, false, BotConfig::Get().firstPrimary==1 || sniperOnly, sniperOnly);
	m_nextUpgradeSearch = m_now + 3.0f;
	if (!id) { SetUpgradeTarget(0); return false; }
	SetUpgradeTarget(id);
	return true;
}

bool BotBrain::UpdateUpgrade(BotInstance& bot,CPlayer& actor,IActor* enemy,float decisionTime,bool socomQA,bool force)
{
	// Holding only the spawn pistol and nobody in sight: go for a rifle like a player would.
	// DS-038: a sniper-minded marksman with a rifle must keep searching; the old gate stopped after the FY71 at 1.3 s.
	// force: the goal arbiter (C-032) already decided this tick is a weapon tick.
	const bool hasPrimary=BotWeapons::HasPrimary(actor);
	const bool sniperMind=PickupRange()>30.0f;
	if (!socomQA && WarsBots::ShouldSearchUpgrade(hasPrimary,BotConfig::Get().weaponRank==1,sniperMind,force,enemy && m_combat.Visible()))
	{
		const bool sniperOnly = BotConfig::Get().sniperReach==1 && sniperMind && !BotWeapons::HoldsSniper(actor);
		TryAcquireUpgradeGoal(actor);
		// DS-097: a goal is created once (3 s throttle, only when none is tracked) and the total budget starts on the
		// first tick that actually walks to it, so a mere candidate look cannot age it or restart the timeout.
		if (m_upgradeTarget && !WarsBots::WeaponGoalPursues(m_upgradeSince))
		{
			m_upgradeSince=m_now; m_upgradeTracked=m_upgradeTarget; m_upgradeBest=1e9f; m_upgradeBestAt=m_now; m_upgradePathSig=0;
			WB_LOG(2,"[WarsBots] #%d WEAPON GOAL target=%u event=pursue since=%.3f.",m_number,m_upgradeTarget,m_upgradeSince);
		}
		else if (m_upgradeTarget && m_upgradeTarget != m_upgradeTracked)
		{ m_upgradeTracked=m_upgradeTarget; m_upgradeBest=1e9f; m_upgradeBestAt=m_now; m_upgradePathSig=0; }
		// Unreachable (on a ledge, behind glass, taken): give up when the walk stops getting closer for 8 s (c067: a
		// flat 10 s dropped every DSG1/Gauss trip - 100+ m with stairs and jump links), at most 60 s, skip it for 90 s.
		if (IEntity* goal = m_upgradeTarget ? gEnv->pEntitySystem->GetEntity(m_upgradeTarget) : nullptr)
		{
			const Vec3 feet=actor.GetEntity()->GetWorldPos();
			float d=(goal->GetWorldPos()-feet).GetLength();
			// wb_bot_path_progress: progress = remaining navmesh path, not straight distance. DSG1 on Steelmill is reached
			// by stairs that first lead away from it: straight distance grew, and every marksman marked it "unreachable"
			// after 8 s (owner playtest 25.10: bot #3 x4; C-080: 7 times, 0 bots ever held a sniper rifle).
			if (BotConfig::Get().pathProgress==1 && m_meshPathActive && !m_path.empty() && (m_pathGoal-goal->GetWorldPos()).GetLength2D()<2.0f)
			{
				float rem=(m_path[min(m_pathIndex,m_path.size()-1)]-feet).GetLength();
				for (size_t k=m_pathIndex+1;k<m_path.size();++k) rem+=(m_path[k]-m_path[k-1]).GetLength();
				d=rem;
				// DS-S1 #1: Steer repaths every 5 s; a longer new path must not age the old best into a false timeout.
				// New path -> restart progress from its own length (the 60 s cap still bounds endless repaths).
				const float sig=m_path.front().x*0.37f+m_path.front().y*1.13f+(float)m_path.size()*7.0f;
				if (fabsf(sig-m_upgradePathSig)>0.01f) { m_upgradePathSig=sig; if (d>m_upgradeBest) { m_upgradeBest=d; m_upgradeBestAt=m_now; } }
			}
			if (d < m_upgradeBest-1.0f) { m_upgradeBest=d; m_upgradeBestAt=m_now; }
		}
		// DS-048: while a jump link owns movement the bot cannot progress along the path; do not age the goal.
		if (BotConfig::Get().linkFailFast==1 && m_link.Active()) m_upgradeBestAt = m_now;
		const float progressTimeout = sniperOnly ? 20.0f : 8.0f;
		const float totalTimeout = sniperOnly ? 90.0f : 60.0f;
		if (m_upgradeTarget && WarsBots::WeaponGoalExpired(m_now, m_upgradeSince, m_upgradeBestAt, progressTimeout, totalTimeout))
		{
			WB_LOG(2, "[WarsBots] #%d weapon %u unreachable; skipping it.", m_number, m_upgradeTarget);
			MarkBadItem(m_upgradeTarget); SetUpgradeTarget(0);
		}
		// A selected item can disappear or gain an owner while we approach it.
		// Do not chase another player's gun until the ten-second timeout.
		if (m_upgradeTarget && !BotWeapons::AvailableForPickup(actor, m_upgradeTarget))
		{
			WB_LOG(2, "[WarsBots] #%d weapon goal cancelled item=%u reason=unavailable.", m_number, m_upgradeTarget);
			SetUpgradeTarget(0);
		}
		IEntity* item = m_upgradeTarget ? gEnv->pEntitySystem->GetEntity(m_upgradeTarget) : nullptr;
		if (item)
		{
			Change(State::MOVING_TO_WEAPON);
			const Vec3 feet = actor.GetEntity()->GetWorldPos();
			const Vec3 delta = item->GetWorldPos() - feet;
			const float d = delta.GetLength();
			if (delta.GetLengthSquared2D() <= 4.0f && fabs_tpl(delta.z) <= 2.2f)
			{
				WBI(move,false);
				if (BotWeapons::PickUp(actor, m_upgradeTarget, PickupRange())) m_previousWeaponClass = actor.GetCurrentItem()->GetEntity()->GetClass();
				else MarkBadItem(m_upgradeTarget);
				SetUpgradeTarget(0);
			}
			else { const Vec3 step=Steer(feet,item->GetWorldPos()); WBI_MOVETO(step,step+Vec3(0,0,1.2f)); TravelGait(d); }
			ExecuteIntent(actor,decisionTime);
			if (bot.GetActor()!=&actor) return true;
			return true;
		}
	}

    return false;
}

void BotBrain::UpdateRoam(BotInstance& bot,CPlayer& actor,float decisionTime)
{
		// DS-034: SEARCHING_ENEMY used to be set here and MOVING_TO_WEAPON by UpdateUpgrade below in the same tick
		// (13,004 + 12,999 flips in playtest 3), and every Change() restarted m_stateTime (reaction, stuck checks).
		// The state is written once, after deciding whether this tick delegates to the weapon goal.
		const bool commit = BotConfig::Get().stateCommit == 1;
		if (!commit) Change(State::SEARCHING_ENEMY);
		m_blackboard.enemy=0; m_blackboard.visible=false; m_blackboard.distance=0;
		const Vec3 feet = actor.GetEntity()->GetWorldPos();
		if (UpdateVantage(actor,feet,decisionTime)) { Change(State::SEARCHING_ENEMY); return; } // owner 25.10: snipers hold roofs
		if (BotConfig::Get().tactics==1 && m_now<m_lootUntil && !m_upgradeTarget)
		{
			// fragmovies: take the dead enemy's gun/ammo before moving on
			const EntityId drop=BotWeapons::FindDrop(actor,m_lootPos,6.0f);
			m_lootUntil=0;
			if (drop)
			{
				SetUpgradeTarget(drop); // DS-097: the budget starts in UpdateUpgrade when the bot actually walks to it
				WB_LOG(2,"[WarsBots] #%d LOOT item=%u at (%.1f,%.1f,%.1f) actor=%u.",m_number,drop,m_lootPos.x,m_lootPos.y,m_lootPos.z,actor.GetEntityId());
			}
		}
		if (BotConfig::Get().tactics==1 && m_upgradeTarget && UpdateUpgrade(bot,actor,nullptr,decisionTime,false,true)) return;
		if (commit) Change(State::SEARCHING_ENEMY);
		// DS-048 #3: PS-1 capture goal; roam only runs with no visible enemy, so combat still wins.
		// C-139 (owner: first money goes to a weapon at once; C-137: 0 buys because nobody died): also buy alive,
		// every 2 s for the first 20 s of a life; stock WarsBotsBuy refuses outside a buy zone.
		if (BotConfig::Get().psBuy==1 && !m_psBought && m_now>=m_nextAliveBuy && g_pGame && g_pGame->GetGameRules() &&
			BotMatch::Mode(*g_pGame->GetGameRules()).Policy().reviveBuy && m_now-m_lifeStartedAt<20.f)
		{ m_nextAliveBuy=m_now+2.f; if (PSTryBuy(bot,actor)) m_psBought=true; }
		// DS-067d: doors before capture (a closed spawn door starved the scan), capture before vehicles (unchanged).
		if (BotConfig::Get().psDoors==1 && UpdatePSDoors(bot,actor,decisionTime)) return;
		// DS-067f: a bot already in a vehicle must leave through the vehicle path before a capture goal runs from the
		// driver seat; the opportunistic enter search still runs after capture, so only the exit branch preempts.
		if (BotConfig::Get().psVehicles==1 && actor.GetLinkedVehicle() && UpdatePSInteract(bot,actor,decisionTime)) return;
		if (BotConfig::Get().psCapture==1 && g_pGame && g_pGame->GetGameRules() && BotMatch::Mode(*g_pGame->GetGameRules()).Policy().captureGoals && UpdatePSCapture(bot,actor,decisionTime)) return;
		if (BotConfig::Get().psVehicles==1 && UpdatePSInteract(bot,actor,decisionTime)) return;
		// C-037 hearing: a gunshot 6..50 m away that nobody can see is worth a look - roam toward it for a while.
		Vec3 heard;
		if (BotConfig::Get().botPerception==1 && m_combat.Heard(m_now,4.0f,heard) &&
			(heard-feet).GetLengthSquared2D()>36.0f && (heard-feet).GetLengthSquared2D()<2500.0f &&
			(heard-m_patrolGoal).GetLengthSquared2D()>25.0f)
		{
			NavMeshCore* mesh = CurrentNavMesh();
			const float from[3] = {feet.x, feet.y, feet.z}, to[3] = {heard.x, heard.y, heard.z};
			float snap[3];
			NavQueryBounds water;
			const bool waterStart = WaterStartBounds(m_blackboard, feet.z, water);
			const NavQueryBounds* startBounds = waterStart ? &water : nullptr;
			const bool projected = !mesh || (startBounds
				? mesh->Project(from, snap, startBounds->side, startBounds->up, startBounds->down, true)
				: mesh->Project(from, snap));
			if (!mesh || !projected || mesh->Reachable(from, to, 2.6f, startBounds))
			{
				m_patrolGoal = heard; m_patrolUntil = m_now + 8.0f; m_detour = false;
				WB_LOG(2, "[WarsBots] #%d HEARD gunshot at (%.1f,%.1f,%.1f), investigating.", m_number, heard.x, heard.y, heard.z);
			}
		}
		if (m_now >= m_patrolUntil || (m_patrolGoal-feet).GetLengthSquared2D() < 4.0f)
		{
			// Roam between the map's IA spawn points (public map knowledge, like a human's),
			// in a per-bot order. After a blocked leg, one short local detour leg; SafeStep gates every move.
			CGameRules* rules = g_pGame->GetGameRules();
			const int count = rules ? rules->GetSpawnLocationCount() : 0;
			++m_patrolStep;
			IEntity* waypoint = nullptr;
			// Every other leg, go where experience on this map says fights happen.
			Vec3 hotspot(0,0,0);
			bool picked = false;
			const bool safe = m_profile.style == BotStyle::Marksman || m_profile.style == BotStyle::Defender;
			NavMeshCore* mesh = CurrentNavMesh();
			const float from[3] = {feet.x, feet.y, feet.z};
			float snap[3];
			// DS-087: project the swimming start with the bounded water box; dry floors keep the default box.
			NavQueryBounds water;
			const bool waterStart = WaterStartBounds(m_blackboard, feet.z, water);
			const NavQueryBounds* startBounds = waterStart ? &water : nullptr;
			const bool projected = !mesh || (startBounds
				? mesh->Project(from, snap, startBounds->side, startBounds->up, startBounds->down, true)
				: mesh->Project(from, snap));
			if (mesh && !projected) mesh = nullptr; // off the mesh: keep the old goal choice
			auto reachable = [&](const Vec3& p) { const float q[3] = {p.x, p.y, p.z}; return !mesh || mesh->Reachable(from, q, 2.6f, startBounds); };
			if (!m_detour && (m_patrolStep & 1) && g_pGame->GetBotManager().Nav().PickHotspot(m_rng + m_patrolStep, safe, hotspot, &feet, actor.GetEntityId()) &&
				(hotspot - feet).GetLengthSquared2D() > 64.0f && reachable(hotspot))
			{ m_patrolGoal = hotspot; m_patrolUntil = m_now + 30.0f; picked = true; }
			else if (count && !m_detour)
				for (int tries = 0; tries < count && !waypoint; ++tries, ++m_patrolStep)
				{
					IEntity* spawn = gEnv->pEntitySystem->GetEntity(rules->GetSpawnLocation(int((m_patrolStep * 7u + unsigned(m_number) * 3u) % unsigned(count))));
					if (spawn && (spawn->GetWorldPos() - feet).GetLengthSquared2D() > 64.0f && reachable(spawn->GetWorldPos())) waypoint = spawn;
				}
			if (picked) {}
			else if (waypoint)
			{
				m_patrolGoal = waypoint->GetWorldPos();
				m_patrolUntil = m_now + 30.0f;
			}
			else
			{
				const float angle = (m_patrolStep * 2.399963f) + m_phase;
				const Vec3 local = feet + Vec3(cosf(angle),sinf(angle),0) * (6.0f + (m_patrolStep%3)*2.0f);
				// DS-082 B1: the local fallback is not a path request. Walk only the reachable fraction of the leg
				// (C145 US t173..179 steered through the hull and logged NAV fallback/stuck); without a mesh keep the
				// old full-leg goal.
				float fraction = 1.0f;
				// DS-092: the local water leg is sampled and committed at the gated native water plane. The path goal is
				// projected with the dry box (up 1.0), so a goal at the swimmer's feet z is unreachable even in open
				// water; dry callers keep feet.z exactly (waterStart is false).
				const float goalZ = RoamGoalZ(mesh && waterStart, feet.z, m_blackboard.waterZ);
				if (mesh)
				{
					bool sampled[4];
					for (int i = 0; i < 4; ++i)
					{
						Vec3 p = feet + (local - feet) * (1.0f - 0.25f * float(i));
						p.z = goalZ;
						sampled[i] = reachable(p);
					}
					fraction = RoamFallbackFraction(true, sampled);
				}
				if (fraction > 0.f)
				{
					Vec3 goal = feet + (local - feet) * fraction;
					goal.z = goalZ;
					m_patrolGoal = goal;
				}
				else m_patrolGoal = feet;   // fraction 0: hold this leg at the feet, as before
				m_patrolUntil = m_now + 4.0f;
			}
			m_detour = false;
		}
		{ const Vec3 step=Steer(feet,m_patrolGoal); WBI_MOVETO(step,step+Vec3(0,0,1.3f)); }
		TravelGait((m_patrolGoal-feet).GetLength());
		ExecuteIntent(actor,decisionTime);
		if (bot.GetActor()!=&actor) return;
		if (m_blackboard.blocked && m_now - m_blockedSince > 1.5f && m_blockedSince > 0)
		{
			g_pGame->GetBotManager().Nav().ReportStuck(feet,actor.GetEntityId()); // learned: route around this spot next time
			m_detour=true; m_patrolUntil=m_now; m_blockedSince=0; m_nextRepath=0;
		}
		else if (m_blackboard.blocked && m_blockedSince <= 0) m_blockedSince=m_now;
		else if (!m_blackboard.blocked) m_blockedSince=0;

		return;

}

// owner 25.10 (wb_bot_vantage): a Marksman/Defender holding a DSG1/Gauss takes a baked sniper spot (BotVantage.h),
// holds it looking along its best view and rotates. Returns true when this tick was handled (holding).
bool BotBrain::UpdateVantage(CPlayer& actor,const Vec3& feet,float decisionTime)
{
	VantageMap* map=CurrentVantage();
	// C-104 (C-100 probes, C-098 "no sniper ever held"): vantage below starts only with a DSG1/Gauss CURRENT, but every
	// pickup and fight re-selected the SCAR/FY71 at 20 m -> deadlock. Idle sniper-minded bots re-judge at 60 m.
	// DS-026: Defenders (camper 0.6) hold too; own timer so a fight right after it re-selects at once.
	if (BotConfig::Get().weaponRank==1 && BotConfig::Get().vantage==1 && m_traits.camper>=0.5f && m_now>=m_nextIdleSelect)
	{ m_nextIdleSelect=m_now+2.0f; BotWeapons::SelectForRange(actor,WarsBots::IdleHoldRange(true)); }
	IItem* held=actor.GetCurrentItem();
	const char* cls=held ? held->GetEntity()->GetClass()->GetName() : "";
	// wb_bot_high_ground: Veteran/Elite sometimes take the height with any rifle (owner fights from roofs)
	if (map && BotConfig::Get().highGround==1 && m_profile.skill>=BotSkill::Veteran && m_now>=m_nextHighRoll && m_now>=m_highUntil)
	{
		m_nextHighRoll=m_now+20.0f;
		if (Rand01()<(m_profile.skill==BotSkill::Elite?0.45f:0.25f))
		{ m_highUntil=m_now+70.0f; m_highHolding=false; WB_LOG(2,"[WarsBots] #%d HIGH GROUND start actor=%u.",m_number,actor.GetEntityId()); }
	}
	const bool high=map && BotConfig::Get().highGround==1 && m_now<m_highUntil;
	const bool sniper=high || (map && m_traits.camper>=0.5f &&   // T-8: Marksman ~0.9, Defender ~0.6
		(!strcmp(cls,"DSG1") || !strcmp(cls,"GaussRifle")));
	if (!sniper) { if (m_vantage.Phase()!=VantagePhase::None) m_vantage.Reset(map,m_number); return false; }
	if (m_now<m_blackboard.damagedUntil) m_vantage.OnHit(m_now);
	NavMeshCore* mesh=CurrentNavMesh();
	const float from[3]={feet.x,feet.y,feet.z};
	float snap[3];
	const bool onMesh=mesh && mesh->Project(from,snap);
	const VantageOutput o=m_vantage.Update(*map,m_number,m_now,feet.x,feet.y,feet.z,m_rng+unsigned(m_life),
		[&](const VSpot& s){ const float q[3]={s.x,s.y,s.z}; return !onMesh || mesh->Reachable(from,q); });
	if (o.relocated) WB_LOG(2,"[WarsBots] #%d VANTAGE relocate actor=%u.",m_number,actor.GetEntityId());
	// DS-041: the owner holds a high spot 8-15 s (median 10.8), not a sniper's 25-45 s
	if (high && o.phase==VantagePhase::Hold && !m_highHolding)
	{ m_highHolding=true; m_highUntil=std::min(m_highUntil,m_now+8.0f+7.0f*Rand01()); }
	if (o.phase==VantagePhase::None) return false;
	if (o.phase==VantagePhase::Travel)
	{
		if ((m_patrolGoal-Vec3(o.x,o.y,o.z)).GetLengthSquared()>1.0f)
			WB_LOG(2,"[WarsBots] #%d VANTAGE travel to (%.1f,%.1f,%.1f) actor=%u.",m_number,o.x,o.y,o.z,actor.GetEntityId());
		m_patrolGoal=Vec3(o.x,o.y,o.z); m_patrolUntil=m_now+5.0f; m_detour=false;
		return false;   // the normal roam path walks there
	}
	// hold: stand on the spot and watch the best view direction (the look policy sees no path -> keeps this view)
	const Vec3 eye=feet+Vec3(0,0,1.6f);
	const float yaw=DEG2RAD(o.lookYaw);
	if (o.move) { const Vec3 step=Steer(feet,Vec3(o.x,o.y,o.z)); WBI_MOVETO(step,eye+Vec3(sinf(yaw),cosf(yaw),0)*30.0f); WBI(gait,BotGait::Walk); }
	else { WBI(move,false); WBI(aim,eye+Vec3(sinf(yaw),cosf(yaw),-0.05f)*30.0f); if (m_traits.prone>0.5f) m_intent.stance=STANCE_PRONE; }
	ExecuteIntent(actor,decisionTime);
	return true;
}

void BotBrain::UpdateCombat(BotInstance& bot,CPlayer& actor,float decisionTime)
{
	const bool visible = m_combat.Visible();
	const Vec3 destination = m_combat.LastPosition();
	m_blackboard.enemy=m_combat.Target(); m_blackboard.visible=visible;
	m_blackboard.lastEnemyPosition=destination;
	const float distance = (destination - actor.GetEntity()->GetWorldPos()).GetLength();
	m_blackboard.distance=distance;
	// C-022: hold the best primary for this fight distance; not mid-burst, at most every 2 s (switch costs time).
	if (BotConfig::Get().weaponRank==1 && m_now>=m_nextRangeSelect && m_now-m_combat.LastBurstRequest()>0.5f)
	{
		// wb_bot_pro_fire: a switch costs 0.6-1 s without shooting -> mid-fight only for a clear gain, at most every 4 s
		const bool pro=BotConfig::Get().proFire==1 && visible;
		m_nextRangeSelect=m_now+(pro?4.0f:2.0f); BotWeapons::SelectForRange(actor,distance,pro?0.3f:0.12f);
	}
    Vec3 aim=m_combat.LastAim();
    if(m_trackedTarget!=m_combat.Target() || m_trackedSelection!=m_combat.SelectedAt()) {
        SMovementState movement;actor.GetMovementController()->GetMovementState(movement);
        m_trackedAim=movement.eyePosition+movement.fireDirection.GetNormalizedSafe(FORWARD_DIRECTION)*max(1.f,distance);
        m_trackedTarget=m_combat.Target();m_trackedSelection=m_combat.SelectedAt();
    }
    if (BotConfig::Get().aimModel==1) aim=AimModelPoint(actor,aim,distance,decisionTime); // C-038
    else
    {
    // Track only the last observed/cached point. No velocity extrapolation or hidden query.
    m_trackedAim+=(aim-m_trackedAim)*BotAimControl::Alpha(m_profile,decisionTime);
    aim=m_trackedAim;
    const float error=BotAimControl::ErrorRadius(m_profile,distance);
	const float t = m_now + m_phase;
	aim += Vec3(sinf(t * 2.7f), cosf(t * 1.9f), sinf(t * 1.3f) * 0.3f) * error;
    }
	IItem* held=actor.GetCurrentItem();
	const float desiredRange=m_profile.WeaponRange(held?held->GetEntity()->GetClass()->GetName():nullptr,m_life+unsigned(m_number));
	const bool disadvantage=m_profile.retreatHealth>0 && (m_blackboard.health<=m_profile.retreatHealth ||
		(m_profile.skill==BotSkill::Elite && m_combat.VisibleEnemies()>=2 && m_blackboard.health<60));
	if (disadvantage && m_now>=m_nextWithdraw)
	{
		m_withdrawUntil=m_now+2.5f; m_nextWithdraw=m_now+5.0f;
		m_withdrawCoverValid=false;
		if (BotConfig::Get().withdrawCover==1)
		{
			// DS-044 #2: back off to a reachable cover spot that breaks LOS to the threat, not 4 m into the open.
			int n=0; const CoverCandidate* spots=CurrentCover(n);
			const Vec3 feet=actor.GetEntity()->GetWorldPos();
			const Vec3 chest=destination+Vec3(0,0,1.3f);
			NavMeshCore* mesh=CurrentNavMesh();
			const float from[3]={feet.x,feet.y,feet.z};
			const int i=spots ? PickCover(spots,n,TVec{feet.x,feet.y,feet.z},TVec{destination.x,destination.y,destination.z},desiredRange,
				[&](const TVec& p){
					const float q[3]={p.x,p.y,p.z};
					return (!mesh || mesh->Reachable(from,q)) && !EyeRayClear(actor,Vec3(p.x,p.y,p.z+1.6f),chest);
				}, 8.0f, 6, BotConfig::Get().withdrawFull==1?1u:0u) : -1;
			if (i>=0) { m_withdrawCover=Vec3(spots[i].p.x,spots[i].p.y,spots[i].p.z); m_withdrawCoverValid=true; m_nextWithdrawPick=m_now+0.5f; }
			else { m_withdrawUntil=0; }
		}
		const bool aborted=BotConfig::Get().withdrawCover==1 && !m_withdrawCoverValid;
		WB_LOG(2,"[WarsBots] TACTIC actor=%u skill=%s style=%s action=%s health=%d energy=%.1f distance=%.1f visible=%d cover=%d.",
			actor.GetEntityId(),BotProfile::SkillName(m_profile.skill),BotProfile::StyleName(m_profile.style),
			aborted?"withdraw_abort":"withdraw",m_blackboard.health,m_blackboard.energy,distance,visible?1:0,m_withdrawCoverValid?1:0);
	}
	if (m_now<m_withdrawUntil)
	{
		// DS-049: the threat moves during the 1.4 s approach; re-pick full cover on the live threat, abort if none.
		if (BotConfig::Get().withdrawFull==1 && m_now>=m_nextWithdrawPick)
		{
			m_nextWithdrawPick=m_now+0.5f;
			int n2=0; const CoverCandidate* s2=CurrentCover(n2);
			const Vec3 b2=actor.GetEntity()->GetWorldPos(); const Vec3 chest2=destination+Vec3(0,0,1.3f);
			NavMeshCore* mesh2=CurrentNavMesh(); const float from2[3]={b2.x,b2.y,b2.z};
			const int j=s2 ? PickCover(s2,n2,TVec{b2.x,b2.y,b2.z},TVec{destination.x,destination.y,destination.z},desiredRange,
				[&](const TVec& p){ const float q[3]={p.x,p.y,p.z}; return (!mesh2 || mesh2->Reachable(from2,q)) && !EyeRayClear(actor,Vec3(p.x,p.y,p.z+1.6f),chest2); }, 8.0f, 6, 1u) : -1;
			if (j>=0)
			{
				const Vec3 np(s2[j].p.x,s2[j].p.y,s2[j].p.z);
				if (!m_withdrawCoverValid || (np-m_withdrawCover).GetLength()>1.5f) m_withdrawCover=np;
				m_withdrawCoverValid=true;
			}
			else { m_withdrawCoverValid=false; m_withdrawUntil=0; return; }
		}
		if (BotConfig::Get().withdrawCover==1 && m_withdrawCoverValid)
		{
			const Vec3 feet=actor.GetEntity()->GetWorldPos();
			WBI_MOVETO(Steer(feet,m_withdrawCover),aim);
		}
		else
		{
			const Vec3 away=(actor.GetEntity()->GetWorldPos()-destination).GetNormalizedSafe(FORWARD_DIRECTION);
			WBI_MOVETO(actor.GetEntity()->GetWorldPos()+away*4.0f,aim);
		}
		m_intent.strafe=visible; m_intent.combat=BotCombatIntent::Aim;
		// wb_bot_pro_fire: a player backs off shooting, not just aiming
		if (BotConfig::Get().proFire==1 && visible && distance<=m_profile.fireRange && m_now>=std::max(m_stateTime,m_combat.SelectedAt())+m_profile.reaction)
			m_intent.combat=BotCombatIntent::Fire;
		WBI(suitMode,NANOMODE_DEFENSE);
		if (!BotPaceEnabled() && !visible && m_profile.useSpeed && m_blackboard.energy>120 && m_now>=m_blackboard.damagedUntil)
		{ WBI(gait,BotGait::Sprint); WBI(suitMode,NANOMODE_SPEED); m_intent.combat=BotCombatIntent::None; }
		ExecuteIntent(actor,decisionTime); return;
	}
	if ((BotConfig::Get().vantage==1 || BotConfig::Get().highGround==1) && m_vantage.Phase()==VantagePhase::Hold && visible && distance>12.0f && !m_combat.ShotBlocked())
	{
		// owner 25.10: a sniper on its spot fights from there (elite marksmen lying down: spread x0.33) instead of
		// running off the roof toward the target
		Change(State::ATTACKING);
		WBI(move,false); WBI(aim,aim);
		if (m_traits.prone>0.5f) m_intent.stance=STANCE_PRONE;
		m_intent.combat=m_now>=std::max(m_stateTime,m_combat.SelectedAt())+m_profile.reaction?BotCombatIntent::Fire:BotCombatIntent::Aim; // DS-035: one clock, not 2x reaction
		UpdateCombatPost(bot,actor,decisionTime);
		return;
	}
	m_lastFightTarget=m_combat.Target(); m_lastFightPos=destination;
	if (BotConfig::Get().tactics==1)
	{
		if (!visible && UpdateHoldAngle(bot,actor,destination,distance,desiredRange,decisionTime)) return;
		if (visible && m_hold.Phase()!=HoldPhase::None) m_hold.Update(m_now,TVec{0,0,0},true);
	}
	// DS-034: a target moving around holdRange flipped ATTACKING/CHASING every decision (2,768 times in playtest 3);
	// once attacking, keep attacking a visible target for at least 0.5 s.
	// DS-043 P1: stateCommit 2 widens the band to holdRange+4 (or preferredRange+6) and holds it for 1.5 s.
	const int commitMode=BotConfig::Get().stateCommit;
	const float attackHold=commitMode>=2?1.5f:0.5f;
	const bool attackDwell = commitMode>=1 && m_state == State::ATTACKING && m_now - m_stateTime < attackHold;
	const float leaveRange = commitMode>=2 ? max(m_profile.holdRange+4.0f,m_profile.preferredRange+6.0f) : m_profile.holdRange;
	if (visible && (attackDwell || distance <= (m_state == State::ATTACKING ? leaveRange : m_profile.engageRange)))
	{
        UpdateAttacking(actor,destination,aim,distance,desiredRange);
    }else{
        UpdateChasing(actor,destination,aim,distance,visible);
    }
    UpdateCombatPost(bot,actor,decisionTime);
}

// wb_bot_tactics: the target just broke line of sight -> hold the angle from a nearby cover spot that still sees its
// last position (BotTactics.h). Returns true when this tick was handled.
bool BotBrain::UpdateHoldAngle(BotInstance& bot,CPlayer& actor,const Vec3& threat,float distance,float desiredRange,float decisionTime)
{
	const float sinceSeen=m_combat.LastSeen()>0 ? m_now-m_combat.LastSeen() : 1e9f;
	if (!m_hold.Begin(unsigned(m_combat.Target()),m_now,sinceSeen,distance,m_traits.holder,m_rng+unsigned(m_decisionSerial)*40503u))
		return false;
	const Vec3 feet=actor.GetEntity()->GetWorldPos();
	if (m_hold.Phase()==HoldPhase::None)
	{
		int n=0; const CoverCandidate* spots=CurrentCover(n);
		const Vec3 chest=threat+Vec3(0,0,1.3f);
		const int i=spots ? PickCover(spots,n,TVec{feet.x,feet.y,feet.z},TVec{threat.x,threat.y,threat.z},desiredRange,
			[&](const TVec& p){ return EyeRayClear(actor,Vec3(p.x,p.y,p.z+1.6f),chest); }) : -1;
		if (i<0) { m_hold.Fail(); return false; }
		m_hold.Assign(spots[i].p,m_now,3.0f+2.0f*Rand01());
		WB_LOG(2,"[WarsBots] #%d HOLD angle from (%.1f,%.1f,%.1f) on (%.1f,%.1f,%.1f) actor=%u.",m_number,spots[i].p.x,spots[i].p.y,spots[i].p.z,threat.x,threat.y,threat.z,actor.GetEntityId());
	}
	const HoldPhase ph=m_hold.Update(m_now,TVec{feet.x,feet.y,feet.z},false);
	if (ph!=HoldPhase::Move && ph!=HoldPhase::Hold) return false;
	const Vec3 aim=threat+Vec3(0,0,1.3f);   // pre-aim the spot where it disappeared, chest height
	Change(State::CHASING_ENEMY);
	if (ph==HoldPhase::Move)
	{
		const TVec& s=m_hold.Spot();
		WBI_MOVETO(Steer(feet,Vec3(s.x,s.y,s.z)),aim); m_intent.strafe=true; WBI(gait,BotGait::Run);
	}
	else { WBI(move,false); WBI(aim,aim); }
	m_intent.combat=BotCombatIntent::Aim;
	UpdateCombatPost(bot,actor,decisionTime);
	return true;
}

void BotBrain::UpdateAttacking(CPlayer& actor,const Vec3& destination,const Vec3& aim,float distance,float desiredRange)
{

		// Retreat locally when crowded; obstruction checks still forbid unsafe steps.
		if (distance < (m_retreating ? m_profile.retreatExit : m_profile.retreatEnter))
		{
			m_retreating = true;
			Vec3 away = (actor.GetEntity()->GetWorldPos()-destination).GetNormalizedSafe(FORWARD_DIRECTION);
			WBI_MOVETO(actor.GetEntity()->GetWorldPos()+away*3.0f,aim);
			m_intent.combat=BotCombatIntent::Aim;
			// wb_bot_pro_fire: point blank is the moment to shoot, not to only aim while stepping back
			if (BotConfig::Get().proFire==1 && m_now>=std::max(m_stateTime,m_combat.SelectedAt())+m_profile.reaction)
				m_intent.combat=BotCombatIntent::Fire;
			m_intent.strafe=true;
		}
		else
        {
            m_retreating=false;
            if (m_now>=m_nextStrafe)
            {
                m_strafeSide=-m_strafeSide;
                m_nextStrafe=m_now+m_profile.strafePeriod+m_profile.strafeJitter*float((m_life+unsigned(m_number))%3);
            }
            Vec3 toward=destination-actor.GetEntity()->GetWorldPos(); toward.z=0;
            toward.NormalizeSafe(FORWARD_DIRECTION);
            Vec3 lateral(-toward.y,toward.x,0);
            // wb_bot_difficulty: a Beginner does not strafe - only closes / opens the distance
            Vec3 travel=lateral*float(m_profile.fightStrafe?m_strafeSide:0)+toward*clamp_tpl((distance-desiredRange)*0.1f,-0.5f,0.5f);
            // DS-024 #1: no-strafe used to walk 4 m toward the target even at the right distance (close -> retreat ->
            // close loop). Inside a deadband around desiredRange a non-strafing bot plants its feet and shoots.
            const bool plant=!m_profile.fightStrafe && fabsf(distance-desiredRange)<=2.0f+0.15f*desiredRange;
            travel.NormalizeSafe(lateral);
            // Eyes see the target but the barrel is blocked by cover: step out (forward and to
            // the current strafe side) instead of standing and staring; flip side if it persists.
            if (m_combat.ShotBlocked())
            {
                if (m_shotBlockedSince<=0) m_shotBlockedSince=m_now;
                if (m_now-m_shotBlockedSince>0.4f)
                {
                    travel=(toward*0.8f+lateral*float(m_strafeSide)*0.6f).GetNormalizedSafe(toward);
                    if (m_now-m_shotBlockedSince>2.5f) { m_strafeSide=-m_strafeSide; m_shotBlockedSince=m_now; }
                }
            }
            else m_shotBlockedSince=0;
            if (plant && m_shotBlockedSince<=0) { WBI(move,false); WBI(aim,aim); }
            else WBI_MOVETO(actor.GetEntity()->GetWorldPos()+travel*4.0f,aim);
            m_intent.strafe=true;
            // fragmovies: duels are strafe + hop, never standing in the open (normal jump: Armor only)
            if (BotConfig::Get().tactics==1 && m_blackboard.mode==NANOMODE_DEFENSE && m_intent.jump==BotJump::None &&
                m_jumpStrafe.Want(m_now,m_blackboard.grounded,distance,m_profile.fightJump>=0.0f?m_profile.fightJump:m_traits.jumper,m_rng+unsigned(m_decisionSerial)*2654435761u))
                WBI(jump,BotJump::Normal);
        }
		Change(State::ATTACKING);
		m_intent.combat=m_now>=std::max(m_stateTime,m_combat.SelectedAt())+m_profile.reaction?BotCombatIntent::Fire:BotCombatIntent::Aim; // DS-035: one clock, not 2x reaction
		// Just got hit: sometimes a Strength jump to break the enemy's aim (native jump, energy cost).
		const bool freshHit = m_blackboard.damagedUntil - m_now > 2.4f && m_blackboard.damagedUntil != m_lastHitSeen;
		if (freshHit)
		{
			m_lastHitSeen = m_blackboard.damagedUntil;
			if (m_blackboard.grounded && m_blackboard.energy >= 110.0f && m_now >= m_nextDodge && Rand01() < m_profile.dodgeChance)
			{ WBI(jump,BotJump::Strength); WBI(suitMode,NANOMODE_STRENGTH); m_nextDodge=m_now+4.0f; }
		}

}

void BotBrain::UpdateChasing(CPlayer& actor,const Vec3& destination,const Vec3& aim,float distance,bool visible)
{

		Change(State::CHASING_ENEMY);
		WBI_MOVETO(Steer(actor.GetEntity()->GetWorldPos(),destination),aim);
		// A visible target beyond engage range is still shot at while closing in, like a player
		// with a rifle would; outside fireRange or without LOS the bot only moves.
		if (visible && distance <= m_profile.fireRange)
		{
			m_intent.strafe=true;
			m_intent.combat=m_now>=std::max(m_stateTime,m_combat.SelectedAt())+m_profile.reaction?BotCombatIntent::Fire:BotCombatIntent::Aim; // DS-035: one clock, not 2x reaction
		}
		else m_combat.StopFire();
		// Running into geometry toward a target we cannot see: give it up and roam instead.
		if (!visible && m_blackboard.blocked) { if (m_chaseBlockedSince<=0) m_chaseBlockedSince=m_now; }
		else m_chaseBlockedSince=0;
		if (m_chaseBlockedSince>0 && m_now-m_chaseBlockedSince>m_profile.blockedChasePatience)
		{
			if (m_verbose) WB_LOG(1, "[WarsBots] #%d chase abandoned target=%u (blocked, not visible).", m_number, m_combat.Target());
			g_pGame->GetBotManager().Nav().ReportStuck(actor.GetEntity()->GetWorldPos(),actor.GetEntityId());
			m_combat.Reset(); m_chaseBlockedSince=0; m_patrolUntil=0; m_detour=true;
		}
		if (!visible || BotPaceEnabled()) TravelGait(distance); // hunting a hidden target: Speed travel like a player
		else
		{
			WBI(gait,m_profile.useSpeed && distance>(m_blackboard.mode==NANOMODE_SPEED?m_profile.sprintChaseRange-2.0f:m_profile.sprintChaseRange) && m_now-m_stateTime>0.4f && m_blackboard.energy>(m_blackboard.mode==NANOMODE_SPEED?50.0f:100.0f)?BotGait::Sprint:BotGait::Run);
			if (m_intent.gait==BotGait::Sprint) WBI(suitMode,NANOMODE_SPEED);
		}
		if (!visible && distance>12.0f && distance<30.0f && BotSuit::CloakEnabled() && m_blackboard.energy>120.0f)
		{ WBI(suitMode,NANOMODE_CLOAK); WBI(gait,BotGait::Walk); }


}

void BotBrain::UpdateCombatPost(BotInstance& bot,CPlayer& actor,float decisionTime)
{

	ExecuteIntent(actor,decisionTime);
	if (bot.GetActor()!=&actor) return;
	IWeapon* activeWeapon=BotWeapons::Current(actor);
	const bool protect=m_now-m_combat.LastBurstRequest()<4.0f ||
		m_now-m_combat.SelectedAt()<0.75f || (activeWeapon && activeWeapon->IsReloading());

}


bool BotBrain::RecoverTrap(CPlayer& actor)
{
    const int& enabled=BotConfig::Get().trapRecovery;
    if (enabled!=1 || !gEnv->bServer ||
        !g_pGame->GetBotManager().Owns(actor.GetEntityId()) || !g_pGame->GetBotManager().CanRunLocalIA())
    { m_trap.Reset(); m_trapStepUntil=0; return false; }
    const Vec3 p=actor.GetEntity()->GetWorldPos();
    // DS-070: a gated swimmer making no progress is not a land trap; the same policy disables the watchdog here.
    const int level=m_trap.Update(p.x,p.y,p.z,m_now,m_intent.move,m_blackboard.canMove && !m_blackboard.waterMovement,false);
    if (level)
    {
        WB_LOG(1,"[WarsBots] TRAP RECOVERY level=%d actor=%u age=%.2f pos=(%.2f,%.2f,%.2f) blocked=%d speed=%.2f grounded=%d sink=%d.",
            level,actor.GetEntityId(),m_trap.Age(m_now),p.x,p.y,p.z,m_blackboard.blocked?1:0,m_blackboard.velocity.GetLength2D(),m_blackboard.grounded?1:0,0);
        if (level==1)
        {
            m_path.clear();m_pathIndex=0;m_cornerFor=size_t(-1);m_nextRepath=m_minRepath=0;m_pathLinks.clear();m_link.Reset();
        }
        else if (level==2 || level==3)
        {
            Vec3 forward=m_intent.destination-p;forward.z=0;forward.NormalizeSafe(actor.GetBaseQuat().GetColumn1());
            m_trapDirection=level==2 ? Vec3(-forward.y,forward.x,0)*float(m_number%2?1:-1) : -forward;
            m_trapStepUntil=m_now+3.f;m_trapJumpAttempted=false;
        }
        else if (level==4)
        {
            // Native IA suicide -> ProcessDeath/OnPlayerKilled -> GameRules::KillPlayer.
            // Keeps score penalties, native death broadcasts and the existing same-actor respawn.
            m_combat.StopFire();m_locomotion.Stop(actor);
            CGameRules* rules=g_pGame->GetGameRules();
            const bool called=rules && Script::CallMethod(rules->GetEntity()->GetScriptTable(),"KillPlayer",actor.GetEntity()->GetScriptTable());
            WB_LOG(1,"[WarsBots] TRAP RECOVERY level=4 actor=%u nativeSelfKill=%d health=%d.",actor.GetEntityId(),called?1:0,actor.GetHealth());
            return true;
        }
    }
    if (!m_trap.Level()) m_trapStepUntil=0;
    if (m_trap.Level()>=2 && m_trap.Level()<4 && m_now<m_trapStepUntil)
    {
        m_combat.StopFire();m_intent.combat=BotCombatIntent::None;
        WBI_MOVETO(p+m_trapDirection*3.f,m_intent.aim);
        m_intent.strafe=false;WBI(gait,BotGait::Run);WBI(suitMode,NANOMODE_DEFENSE);
        WBI(jump,BotJump::None);
        if (m_trap.Level()==3 && !m_trapJumpAttempted && m_blackboard.grounded &&
            m_blackboard.mode==NANOMODE_DEFENSE && m_now>=m_blackboard.nextJump)
        { WBI(jump,BotJump::Normal);m_trapJumpAttempted=true; } // Executor still requires JumpSpace.
    }
    return false;
}

void BotBrain::ExecuteIntent(CPlayer& actor, float dt)
{
    if (RecoverTrap(actor)) return;
	// DS-043 P1: hold a destination inside one state. A new state resets the commit in Change(); trap/link override it.
	if (BotConfig::Get().stateCommit>=2)
	{
		if (m_link.Active() || m_trap.Level()>=2) m_move.Reset();
		else if (m_intent.move)
		{
			const Vec3 d=m_intent.destination;
			if (!m_move.Valid(d.x,d.y,d.z,m_now))
			{
				const DecisionOwner owner=OwnerOf(m_state);
				if (!m_move.Has() || m_moveOwner==owner) { m_move.Set(d.x,d.y,d.z,m_now); m_moveOwner=owner; }
				else
				{
					float x=0,y=0,z=0; m_move.Get(x,y,z);
					m_intent.MoveTo(Vec3(x,y,z),m_intent.aim);
					m_intent.why.move=short(__LINE__);
					if (m_verbose) WB_LOG(2,"[WarsBots] #%d MOVE COMMIT hold owner=%d actor=%u.",m_number,int(m_moveOwner),actor.GetEntityId());
				}
			}
		}
	}
    ApplyLookPolicy(actor); // C-033, wb_bot_look
	if (m_now<m_testModeUntil && m_testMode>=0 && m_intent.combat==BotCombatIntent::None && m_intent.jump==BotJump::None)
		WBI(suitMode,static_cast<ENanoMode>(m_testMode));
	m_blackboard.obstacleJump=BotJump::None;
	m_intent.linkJump=false;
	m_link.SetBlockedFailFast(BotConfig::Get().linkFailFast==1); // DS-048, default OFF
	if (BotConfig::Get().ladder==1 && ApplyLadderExecutor(actor)) return; // DS-107: immediate/exclusive ownership
	if (BotConfig::Get().jumpLinks==1) ApplyLinkExecutor(actor); else m_link.Reset(); // C-036
	// wb_bot_jump_sanity: on a navmesh path the mesh already proved the way (jumps come from baked links); the low
	// obstacle rays read stair steps as a wall and asked for Strength jumps at every staircase (149 of 364, owner
	// playtest 25.10). Trap recovery still jumps (level 3) when stuck.
	const bool obstacleJumps=!(BotConfig::Get().jumpSanity==1 && m_intent.meshPath && m_trap.Level()<2);
	if (obstacleJumps && !m_link.Active() && m_intent.move && m_blackboard.grounded && m_now>=m_blackboard.nextJump && m_now>=m_testUntil &&
        !(m_trap.Level()>=2 && m_now<m_trapStepUntil))
	{
		const Vec3 direction=(m_intent.destination-m_blackboard.position).GetNormalizedSafe(FORWARD_DIRECTION);
		m_blackboard.obstacleJump=m_locomotion.Obstacle(actor,direction);
		if (m_blackboard.obstacleJump!=BotJump::None && m_intent.combat==BotCombatIntent::None)
		{
			WBI(jump,m_blackboard.obstacleJump);
			WBI(suitMode,m_intent.jump==BotJump::Strength?NANOMODE_STRENGTH:NANOMODE_DEFENSE);
	}
}
	if (!m_blackboard.grounded && m_blackboard.velocity.z < -3.0f)
		WBI(suitMode,NANOMODE_DEFENSE); // Armor before landing: native suit absorbs fall damage.
	else if (!m_blackboard.grounded && static_cast<const SPlayerStats*>(actor.GetActorStats())->jumped && m_blackboard.mode==NANOMODE_STRENGTH && m_intent.jump==BotJump::None)
		WBI(suitMode,NANOMODE_STRENGTH);
	if (!m_link.Active() && !BotPaceEnabled() && (m_now<m_blackboard.damagedUntil || m_intent.combat!=BotCombatIntent::None || m_blackboard.energy<45.0f) && m_intent.jump!=BotJump::Strength)
	{ WBI(suitMode,NANOMODE_DEFENSE); if (m_intent.gait==BotGait::Sprint) WBI(gait,BotGait::Run); }
	if(BotPaceEnabled() && m_intent.jump==BotJump::Strength)m_speedTravel=false;
    if(!m_link.Active() && BotPaceEnabled() && m_intent.jump!=BotJump::Strength && (m_now<m_blackboard.damagedUntil || m_intent.combat==BotCombatIntent::Fire)){WBI(suitMode,NANOMODE_DEFENSE);m_speedTravel=false;}
    if (BotConfig::Get().suitPolicy==1 && !m_link.Active()) ApplySuitPolicy(actor); // roadmap 12: the last word on the suit mode
    // DS-040 (wb_bot_pro_fire): a visible enemy inside the hold range must not sprint. Native fire is blocked while
    // sprinting; C-112 had 12 visible combat gaps / 116.5 s at 60-89 % sprint. After the suit policy on purpose.
    if (BotConfig::Get().proFire==1 && m_blackboard.visible && m_blackboard.distance<=m_profile.holdRange && m_intent.jump==BotJump::None && m_intent.gait==BotGait::Sprint)
    { WBI(gait,BotGait::Run); m_speedTravel=false; if (m_intent.suitMode==NANOMODE_SPEED) WBI(suitMode,NANOMODE_DEFENSE); } // DS-043 (c): no Speed suit in the fight either
    if (m_cornerSlow && !m_link.Active() && m_intent.jump==BotJump::None)
    { // wb_bot_corners: no sprint / Speed into a sharp turn; after the suit policy on purpose (c071: 6.3 m/s down the 1 m Steelmill stair, overshoot)
        if (m_intent.gait==BotGait::Sprint) WBI(gait,BotGait::Run);
        if (m_intent.suitMode==NANOMODE_SPEED) { WBI(suitMode,NANOMODE_DEFENSE); m_speedTravel=false; }
    }
    const bool suitReady=m_suit.Apply(actor,m_intent.suitMode,m_blackboard,m_now,m_profile.cloakApproach,m_intent.linkJump && m_intent.suitMode==NANOMODE_STRENGTH);
	if(!suitReady && m_intent.gait==BotGait::Sprint){
        if(BotPaceEnabled() && m_intent.jump==BotJump::None){m_speedTravel=false;WBI(suitMode,NANOMODE_DEFENSE);m_suit.Apply(actor,NANOMODE_DEFENSE,m_blackboard,m_now);}
        else WBI(gait,BotGait::Run);
    }
	if (BotConfig::Get().botTurnCap==1)
	{
		BotIntent capped=m_intent; capped.aim=CappedLook(actor); // C-044: the decision tick never snaps the view
		m_locomotion.Execute(actor,capped,m_blackboard,m_now,dt);
	}
	else m_locomotion.Execute(actor,m_intent,m_blackboard,m_now,dt);
	// T-1 step 1: which rule decided this tick (source lines of BotBrain.cpp), logged when the suit request changes or
	// a jump is requested - the before/after evidence for the control-frame refactor.
	if (m_intent.suitMode!=m_lastSuitRequest || m_intent.jump!=BotJump::None)
		WB_LOG(2,"[WarsBots] #%d INTENT WHY suit=%d@%d jump=%d@%d gait=%d@%d move=%d@%d aim@%d state=%s actor=%u.",m_number,
			int(m_intent.suitMode),m_intent.why.suitMode,int(m_intent.jump),m_intent.why.jump,int(m_intent.gait),m_intent.why.gait,
			m_intent.move?1:0,m_intent.why.move,m_intent.why.aim,StateName(),actor.GetEntityId());
	m_lastSuitRequest=m_intent.suitMode;
	// wb_bot_pro_fire: a cloaked bot that wants to fire drops cloak now (Armor) instead of waiting on the suit policy
	if (BotConfig::Get().proFire==1 && m_intent.combat==BotCombatIntent::Fire && m_blackboard.mode==NANOMODE_CLOAK)
	{ WBI(suitMode,NANOMODE_DEFENSE); m_suit.Apply(actor,NANOMODE_DEFENSE,m_blackboard,m_now); }
	if (m_intent.combat==BotCombatIntent::Fire && m_blackboard.mode!=NANOMODE_CLOAK)
	{
		IActor* target=g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_combat.Target());
		if (target) m_combat.Attack(actor,*target,m_now,m_verbose); else m_combat.StopFire();
}
	else m_combat.StopFire();
	if (BotConfig::Get().weaponHandling==1)
	{
		// DS-035: zoom a visible target at >12 m even while chasing (20-30 m hits were fired with hip spread).
		const bool visibleTarget=m_combat.Target() && m_combat.Visible();
		const float dist=visibleTarget ? (m_combat.LastPosition()-m_blackboard.position).GetLength() : -1.0f;
		const bool fighting=m_intent.combat!=BotCombatIntent::None && visibleTarget;
		const bool zoomFar=visibleTarget && dist>12.0f;
		if (fighting || zoomFar)
			BotWeapons::Handle(actor,dist/max(0.2f,m_profile.zoomScale),m_blackboard.suitActive && m_blackboard.mode==NANOMODE_CLOAK);
		else
			BotWeapons::Handle(actor,dist,m_blackboard.suitActive && m_blackboard.mode==NANOMODE_CLOAK);
	}
}
// ---------------------------------------------------------------------------------------------- C154/DS-107 ladder
// Starts at a real NavLinkLadder corner. The pure policy owns approach/grab/forward-climb/native-exit/settle; the
// native CPlayerMovement top ray and animation/OnLadder completion initiate and complete the exit. Bot logic never
// requests eLAT_ExitTop, never steers sideways while attached, never snaps and never treats height alone as success.
bool BotBrain::ApplyLadderExecutor(CPlayer& actor)
{
	if (m_ladder.Active()) return true;
	if (m_link.Active()) return false; // an existing jump/drop keeps ownership until its own completion
	NavMeshCore* mesh=CurrentNavMesh();
	if (!mesh || !m_meshPathActive || !m_intent.move || m_intent.combat==BotCombatIntent::Fire) return false;
	const NavPathLink* l=LinkAtCorner(m_pathIndex);
	if (!l || l->kind!=NavLinkLadder) return false;
	const Vec3 a(l->a[0],l->a[1],l->a[2]);
	const Vec3 feet=m_blackboard.position;
	if ((a-feet).GetLengthSquared2D()>4.0f || fabs_tpl(a.z-feet.z)>2.0f) return false;
	m_ladderNow=*l;
	m_ladder.Start(m_now,LVec{l->a[0],l->a[1],l->a[2]},LVec{l->b[0],l->b[1],l->b[2]});
	m_ladderNativeLocked=false; m_ladderCancelPending=false;
	WB_LOG(2,"[WarsBots] #%d LADDER start a=(%.2f,%.2f,%.2f) b=(%.2f,%.2f,%.2f) actor=%u.",m_number,
		l->a[0],l->a[1],l->a[2],l->b[0],l->b[1],l->b[2],actor.GetEntityId());
	return true;
}

// One cancellation path for fail / flag-OFF / death / stop / reset. It clears native ladder mode as a FAILURE
// (never success), stops bot input/fire, and optionally bans the link and re-paths.
void BotBrain::CancelLadder(CPlayer& actor, const char* why, bool ban)
{
	SPlayerStats* stats=actor.GetPlayerStats();
	const bool nativeAttached=stats && (stats->isOnLadder.Value() || stats->isExitingLadder);
	const bool hadRoute=m_ladder.Active() || m_ladderNativeLocked || m_ladderNow.ref || m_ladderCancelPending;
	m_ladderCancelPending=false; // also consume a reset whose native revive already cleared ladder stats
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
		stats->inFreefall.Value()>0 || stats->inZeroG || stats->isGrabbed || stats->spectatorMode || stats->flyMode)
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
	if (o.phase==LadderPhase::Done)
	{
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

// ---------------------------------------------------------------------------------------------- C-036 links
// Takes over at a link take-off corner of a Detour path and runs BotJumpLink until landing; a failed link is banned.
void BotBrain::ApplyLinkExecutor(CPlayer& actor)
{
	if (m_ladder.Active()) return; // DS-107: the native ladder owns this leg
	NavMeshCore* mesh=CurrentNavMesh();
	const Vec3 feet=m_blackboard.position;
	if (!mesh) { m_link.Reset(); return; }
	if (!m_link.Active())
	{
		if (!m_meshPathActive || !m_intent.move || m_intent.combat==BotCombatIntent::Fire) return;
		const NavPathLink* l=LinkAtCorner(m_pathIndex);
		if (!l) return;
		if (l->kind!=NavLinkDrop && l->kind!=NavLinkJump && l->kind!=NavLinkStrength) return;
		const Vec3 a(l->a[0],l->a[1],l->a[2]);
		if ((a-feet).GetLengthSquared2D()>4.0f || fabs_tpl(a.z-feet.z)>2.0f) return;
		m_linkNow=*l;
		const LinkMove move=l->kind==NavLinkStrength?LinkMove::Strength:(l->kind==NavLinkJump?LinkMove::Jump:LinkMove::Drop);
		m_link.Start(m_now,move,JVec{l->a[0],l->a[1],l->a[2]},JVec{l->b[0],l->b[1],l->b[2]});
		WB_LOG(2,"[WarsBots] #%d LINK start kind=%s a=(%.2f,%.2f,%.2f) b=(%.2f,%.2f,%.2f) energy=%.0f actor=%u.",m_number,NavLinkName(l->kind),
			l->a[0],l->a[1],l->a[2],l->b[0],l->b[1],l->b[2],m_blackboard.energy,actor.GetEntityId());
	}
	LinkInput in;
	in.now=m_now; in.feet=JVec{feet.x,feet.y,feet.z}; in.grounded=m_blackboard.grounded;
	in.strengthActive=m_blackboard.suitActive && m_blackboard.mode==NANOMODE_STRENGTH;
	in.armorActive=!m_blackboard.suitActive || m_blackboard.mode==NANOMODE_DEFENSE;
	in.energy=m_blackboard.energy;
	in.speed=m_blackboard.velocity.GetLength2D();
	const LinkOutput o=m_link.Update(in);
	if (o.phase==LinkPhase::Done)
	{
		WB_LOG(2,"[WarsBots] #%d LINK done kind=%s at=(%.2f,%.2f,%.2f) actor=%u.",m_number,NavLinkName(m_linkNow.kind),feet.x,feet.y,feet.z,actor.GetEntityId());
		{ const size_t next=size_t(m_linkNow.corner)+1; m_cornerFor=size_t(-1); m_pathIndex=m_path.empty()?0:(next<m_path.size()?next:m_path.size()-1); }
		m_link.Reset();
		return;
	}
	if (o.phase==LinkPhase::Failed)
	{
		static const char* const why[]={"none","timeout","no_takeoff","missed_landing","no_energy","too_high"};
		WB_LOG(1,"[WarsBots] #%d LINK failed kind=%s reason=%s at=(%.2f,%.2f,%.2f)%s actor=%u.",m_number,NavLinkName(m_linkNow.kind),
			why[int(o.fail)],feet.x,feet.y,feet.z,o.fail==LinkFail::NoEnergy?"":" -> fail noted",actor.GetEntityId());
		// O-027: one failure can be timing (hit, cooldown, short run-up) -> ban a link only on its second failure.
		// DS-046: a link above the jump rise is impossible; ban it at once, do not waste two probes.
		if (o.fail==LinkFail::TooHigh) mesh->BanLink(m_linkNow.ref);
		else if (o.fail!=LinkFail::NoEnergy && mesh->NoteLinkFailure(m_linkNow.ref)) mesh->BanLink(m_linkNow.ref);
		m_link.Reset(); m_path.clear(); m_pathLinks.clear(); m_pathIndex=0; m_cornerFor=size_t(-1); m_nextRepath=m_minRepath=0;
		return;
	}
	if (!m_link.Active()) return;
	WBI(move,true);
	m_intent.destination=Vec3(o.moveTo.x,o.moveTo.y,o.moveTo.z);
	WBI(gait,o.slow?BotGait::Walk:BotGait::Run);
	if (m_link.Phase()==LinkPhase::Act && m_link.Move()!=LinkMove::Drop) WBI(gait,BotGait::Sprint); // c066: Strength links still land short
	WBI(jump,o.jump==LinkJumpRequest::Strength?BotJump::Strength:(o.jump==LinkJumpRequest::Normal?BotJump::Normal:BotJump::None));
	m_intent.linkJump=m_intent.jump!=BotJump::None;
	if (o.wantStrength) WBI(suitMode,NANOMODE_STRENGTH);
	else if (o.wantArmor) WBI(suitMode,NANOMODE_DEFENSE);
	m_speedTravel=false;
}

// ---------------------------------------------------------------------------------------------- sprint C-047
// C-033 look policy (BotLookPolicy.h): without a visible target look ~3 m ahead along the path, at a point of interest
// (hidden enemy's last position, the weapon being fetched) only with a clear eye ray or when it is very close.
// Fixes C-042: bots looked exactly at their aim point through walls (no_route fallback aims at the far goal).
void BotBrain::ApplyLookPolicy(CPlayer& actor)
{
	if (BotConfig::Get().botLook!=1 || m_now<m_testUntil || m_vantage.Phase()==VantagePhase::Hold) return; // a held sniper spot owns the view
	if (m_combat.Target() && m_combat.Visible()) { m_look.Reset(); return; } // a seen target owns the view
	const Vec3 feet=actor.GetEntity()->GetWorldPos();
	const Vec3 eye=feet+Vec3(0,0,1.6f);
	LookInput in;
	in.now=m_now; in.eye=LookVec{eye.x,eye.y,eye.z};
	const Vec3 view=actor.GetViewRotation().GetColumn1();
	in.viewYaw=RAD2DEG(atan2f(view.x,view.y));
    in.viewPitch=RAD2DEG(atan2f(view.z,sqrtf(view.x*view.x+view.y*view.y)));
    in.pathActive=m_intent.move;
	in.speed=m_blackboard.velocity.GetLength2D();
	Vec3 interest(0,0,0), heard(0,0,0); bool hasInterest=false;
	if (m_combat.Target() && !m_combat.Visible()) { interest=m_combat.LastPosition()+Vec3(0,0,1.2f); hasInterest=true; }
	else if (BotConfig::Get().botPerception==1 && m_combat.Heard(m_now,2.5f,heard))
	{ interest=heard+Vec3(0,0,1.2f); hasInterest=true; } // C-037 hearing: glance at a gunshot (clear-ray rule still applies)
	else if (IEntity* item=gEnv->pEntitySystem->GetEntity(m_upgradeTarget?m_upgradeTarget:m_weaponTarget))
	{ interest=item->GetWorldPos(); hasInterest=true; }
	LookVec pts[8]; int n=0;
	for (size_t i=m_pathIndex; i<m_path.size() && n<8; ++i) pts[n++]=LookVec{m_path[i].x,m_path[i].y,m_path[i].z};
	if (n==0 && m_intent.move)
	{
		// No route (BotNav fallback): look where the body actually goes, not at the far goal behind the wall.
		Vec3 v=m_blackboard.velocity; v.z=0;
		Vec3 dir=v.GetLength2D()>1.0f ? v.GetNormalized() : Vec3(m_intent.destination.x-feet.x,m_intent.destination.y-feet.y,0).GetNormalizedSafe(actor.GetBaseQuat().GetColumn1());
		const Vec3 ahead=feet+dir*4.0f;
		pts[n++]=LookVec{ahead.x,ahead.y,ahead.z};
	}
	unsigned lookRays=0;
	if (n)
	{
		in.hasCorner=true;
		in.corner=ClearPathLookAhead(LookVec{feet.x,feet.y,feet.z},pts,n,lookRays,
            [&](const LookVec& p){return EyeRayClear(actor,eye,Vec3(p.x,p.y,p.z));});
	}
    if(hasInterest && lookRays<2){
        ++lookRays;in.hasInterest=true;in.interest=LookVec{interest.x,interest.y,interest.z};
        in.interestRayClear=EyeRayClear(actor,eye,interest);
    }
	if (BotConfig::Get().threatScan==1)
	{
		// wb_bot_threat_scan: now and then a 0.6 s glance at a high sniper spot ahead with a clear eye ray
		const int skill=m_profile.skill==BotSkill::Beginner?0:m_profile.skill==BotSkill::Veteran?2:m_profile.skill==BotSkill::Elite?3:1;
		const float period=ThreatScanPeriod(skill);
		if (period>0.0f && m_now<m_glanceUntil) { in.hasGlance=true; in.glance=LookVec{m_glancePt.x,m_glancePt.y,m_glancePt.z}; }
		else if (period>0.0f && m_now>=m_nextGlance)
		{
			m_nextGlance=m_now+period*(0.7f+0.6f*Rand01());
			if (VantageMap* map=CurrentVantage())
			{
				std::vector<float> xyz; xyz.reserve(size_t(map->Count())*3);
				for (int i=0;i<map->Count();++i) { const VSpot& s=map->Spot(i); xyz.push_back(s.x); xyz.push_back(s.y); xyz.push_back(s.z); }
				const float travel=in.hasCorner ? BotLookPolicy::YawTo(in.eye,in.corner) : in.viewYaw;
				int pick[3]; const int n=ThreatScanCandidates(xyz.data(),map->Count(),eye.x,eye.y,eye.z,travel,pick,3);
				// own budget: the path look-ahead may already have spent its rays (C-098: 0 glances logged)
				for (int k=0;k<n;++k)
				{
					const VSpot& s=map->Spot(pick[k]); const Vec3 p(s.x,s.y,s.z+1.4f);
					if (EyeRayClear(actor,eye,p)) { m_glancePt=p; m_glanceUntil=m_now+0.6f; in.hasGlance=true; in.glance=LookVec{p.x,p.y,p.z};
						WB_LOG(2,"[WarsBots] #%d GLANCE spot=(%.1f,%.1f,%.1f) actor=%u.",m_number,p.x,p.y,p.z,actor.GetEntityId()); break; }
				}
			}
		}
	}
	const LookOutput o=m_look.Update(in);
	const float yaw=DEG2RAD(o.yaw), pitch=DEG2RAD(o.pitch);
	WBI(aim,eye+Vec3(sinf(yaw)*cosf(pitch),cosf(yaw)*cosf(pitch),sinf(pitch))*8.0f);
    if(m_lookDiagnostics<128 && m_now>=m_nextLookDiagnostic && BotConfig::Get().moveTrace!=0){
        static const char* const names[]={"keep","target","attacker","interest","path","next","glance"};
        const unsigned reason=static_cast<unsigned>(o.reason);
        WB_LOG(2,"[WarsBots] LOOK TRACE actor=%u reason=%s state=%s active=%d speed=%.2f interest=%d clear=%d rays=%u eye=(%.3f,%.3f,%.3f) corner=(%.3f,%.3f,%.3f) aim=(%.3f,%.3f,%.3f)",
            actor.GetEntityId(),reason<6?names[reason]:"invalid",StateName(),in.pathActive?1:0,in.speed,in.hasInterest?1:0,in.interestRayClear?1:0,lookRays,
            eye.x,eye.y,eye.z,in.corner.x,in.corner.y,in.corner.z,m_intent.aim.x,m_intent.aim.y,m_intent.aim.z);
        ++m_lookDiagnostics;m_nextLookDiagnostic=m_now+1.f;
    }
}

bool BotBrain::EyeRayClear(CPlayer& actor,const Vec3& from,const Vec3& to) const
{
	if (!gEnv->pPhysicalWorld) return false;
	ray_hit hit;
	IItem* held=actor.GetCurrentItem();
	IPhysicalEntity* heldPhysics=held ? held->GetEntity()->GetPhysics() : nullptr;
	return gEnv->pPhysicalWorld->RayWorldIntersection(from,to-from,ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid,
		rwi_stop_at_pierceable|rwi_colltype_any,&hit,1,actor.GetEntity()->GetPhysics(),heldPhysics)==0;
}

// C-044: bots turned the view up to ~2000 deg/s in one 31 ms server tick (owner demo 23:33: >720 deg/s in 9.5 % of samples,
// humans 0.6 %), which reads as lag. The view now rotates toward the decided look point at a per-skill rate every frame.
float BotBrain::TurnRate() const
{
	if (m_profile.turnRate>0.0f) return m_profile.turnRate; // wb_bot_difficulty
	switch (m_profile.skill)
	{
	case BotSkill::Beginner: return 400.0f;
	case BotSkill::Veteran: return 800.0f;
	case BotSkill::Elite: return 900.0f;
	default: return 720.0f;
	}
}

void BotBrain::UpdateTurnCap(CPlayer& actor,float dt)
{
	if (m_now<m_testUntil) { m_lookValid=false; return; }
	if (m_intent.aim.IsZero()) return; // No decided look point yet after start/revive.
	const Vec3 eye=actor.GetEntity()->GetWorldPos()+Vec3(0,0,1.6f);
	Vec3 want=m_intent.aim-eye;
	const float dist=want.GetLength();
	if (dist<0.2f) return;
	want/=dist;
	if (!m_lookValid) { m_lookDir=actor.GetViewRotation().GetColumn1().GetNormalizedSafe(want); m_lookValid=true; }
	const float maxStep=DEG2RAD(TurnRate())*dt;
	const float angle=acos_tpl(clamp_tpl(m_lookDir.Dot(want),-1.0f,1.0f));
	if (angle<=maxStep) m_lookDir=want;
	else
	{
		Vec3 axis=m_lookDir.Cross(want);
		if (axis.GetLengthSquared()<1e-8f) axis=Vec3(0,0,1); // exactly behind: turn around the vertical
		axis.Normalize();
		m_lookDir=(Quat::CreateRotationAA(maxStep,axis)*m_lookDir).GetNormalizedSafe(want);
	}
	m_locomotion.Look(actor,eye+m_lookDir*max(2.0f,dist));
}

Vec3 BotBrain::CappedLook(CPlayer& actor) const
{
	if (!m_lookValid) {
        const Vec3 eye=actor.GetEntity()->GetWorldPos()+Vec3(0,0,1.6f);
        return eye+actor.GetViewRotation().GetColumn1()*max(2.0f,(m_intent.aim-eye).GetLength());
    }
	const Vec3 eye=actor.GetEntity()->GetWorldPos()+Vec3(0,0,1.6f);
	return eye+m_lookDir*max(2.0f,(m_intent.aim-eye).GetLength());
}

// C-038 aim model (BotAimModel.h): flick to a new target with a Fitts-like duration and an end error, then track with
// lag + partial lead, plus Ornstein-Uhlenbeck hand noise - instead of a low-pass filter plus fixed 2.7/1.9/1.3 Hz sines
// (the "ellipse" the owner saw as aim wandering). Uses only the observed/cached aim point (no hidden query).
Vec3 BotBrain::AimModelPoint(CPlayer& actor,const Vec3& observed,float distance,float dt)
{
	SMovementState movement; actor.GetMovementController()->GetMovementState(movement);
	const Vec3 eye=movement.eyePosition;
	const Vec3 d=observed-eye;
	const float h=sqrtf(d.x*d.x+d.y*d.y);
	const float yaw=RAD2DEG(atan2f(d.x,d.y)), pitch=RAD2DEG(atan2f(d.z,max(0.01f,h)));
	if (!m_aimModelInit)
	{
		const int skill=m_profile.skill==BotSkill::Beginner?0:m_profile.skill==BotSkill::Veteran?2:m_profile.skill==BotSkill::Elite?3:1;
		m_aimModel=BotAimModel(m_profile.turnRate>0.0f?DifficultyAim(m_profile.skill):AimSkillPreset(skill),0x9E3779B9u*unsigned(m_number+1)+m_life); // turnRate>0 = wb_bot_difficulty profile
		const Vec3 v=movement.eyeDirection.GetNormalizedSafe(actor.GetEntity()->GetWorldRotation().GetColumn1());
		m_aimModel.SetView(RAD2DEG(atan2f(v.x,v.y)),RAD2DEG(asin_tpl(clamp_tpl(v.z,-1.0f,1.0f))));
		m_aimModelInit=true; m_aimModelTarget=0;
	}
	if (m_aimModelTarget!=m_combat.Target())
	{
		// New target: the flick starts from where the bot really looks now, not from a stale model view.
		const Vec3 v=movement.eyeDirection.GetNormalizedSafe(actor.GetEntity()->GetWorldRotation().GetColumn1());
		m_aimModel.SetView(RAD2DEG(atan2f(v.x,v.y)),RAD2DEG(asin_tpl(clamp_tpl(v.z,-1.0f,1.0f))));
	}
	AimInput in;
	in.dt=max(0.02f,dt);
	in.hasTarget=m_combat.Target()!=0;
	in.targetId=int(m_combat.Target());
	in.targetYaw=yaw; in.targetPitch=pitch;
	if (m_aimModelTarget==m_combat.Target())
	{
		in.targetYawRate=BotAimModel::Wrap(yaw-m_prevTargetYaw)/in.dt;
		in.targetPitchRate=(pitch-m_prevTargetPitch)/in.dt;
	}
	in.targetSizeDeg=RAD2DEG(atan2f(0.45f,max(1.0f,distance)));
	m_aimModelTarget=m_combat.Target(); m_prevTargetYaw=yaw; m_prevTargetPitch=pitch;
	const AimOutput o=m_aimModel.Update(in);
	const float ry=DEG2RAD(o.yaw), rp=DEG2RAD(o.pitch);
	return eye+Vec3(sinf(ry)*cosf(rp),cosf(ry)*cosf(rp),sinf(rp))*max(1.0f,d.GetLength());
}

// Roadmap 12 suit policy (BotSuitPolicy.h): one decision with priorities, energy hysteresis and a 1.5 s hold instead of
// the last of ~25 scattered suitMode writes. BotSuit::Apply still enforces the native switch cooldown.
void BotBrain::ApplySuitPolicy(CPlayer& actor)
{
	SuitInput in;
	in.now=m_now;
	in.energy=m_blackboard.energy;
	in.health=m_blackboard.health;
	in.airborne=!m_blackboard.grounded;
	in.verticalSpeed=m_blackboard.velocity.z;
	in.damagedRecently=m_blackboard.damagedUntil-m_now>2.4f;
	in.enemyVisible=m_combat.Target() && m_combat.Visible();
	in.enemyKnownHidden=m_combat.Target() && !m_combat.Visible();
	in.enemyDistance=m_combat.Target() ? (m_combat.LastPosition()-actor.GetEntity()->GetWorldPos()).GetLength() : 1e9f;
	in.wantStrengthJump=m_intent.jump==BotJump::Strength;
	in.moving=m_intent.move;
	float travel=(m_intent.destination-actor.GetEntity()->GetWorldPos()).GetLength();
	for (size_t i=m_pathIndex; i+1<m_path.size(); ++i) travel+=(m_path[i+1]-m_path[i]).GetLength();
	in.travelDistance=travel;
	in.firing=m_intent.combat==BotCombatIntent::Fire;
	in.stealthStyle=m_profile.style==BotStyle::Flanker || m_profile.style==BotStyle::Hunter || m_profile.cloakApproach;
	in.cloakAllowed=BotSuit::CloakEnabled() || m_profile.cloakApproach;
	in.canUseSpeed=m_profile.skill!=BotSkill::Beginner || m_profile.useSpeed;
	const SuitDecision d=m_suitPolicy.Update(in);
	switch (d.mode)
	{
	case SuitMode::Speed: WBI(suitMode,NANOMODE_SPEED); break;
	case SuitMode::Strength: WBI(suitMode,NANOMODE_STRENGTH); break;
	case SuitMode::Cloak: WBI(suitMode,NANOMODE_CLOAK); break;
	default: WBI(suitMode,NANOMODE_DEFENSE); break;
	}
	// DS-033: never downgrade a link's Strength jump - a Normal jump cannot cover a +4 m link (27/49 link failures).
	if (d.mode!=SuitMode::Strength && m_intent.jump==BotJump::Strength && !m_intent.linkJump) WBI(jump,BotJump::Normal); // no energy for it
	m_speedTravel=d.mode==SuitMode::Speed;
}

// C-032 goal arbiter (BotGoalArbiter.h): one decision per tick with commitment windows, so CHASING_ENEMY and
// MOVING_TO_WEAPON no longer flip on instantaneous visibility (14.9 decision flips per bot-minute in the baseline demo).
bool BotBrain::UpdateArbitrated(BotInstance& bot,CPlayer& actor,IActor* enemy,float decisionTime,bool socomQA)
{
	const Vec3 feet=actor.GetEntity()->GetWorldPos();
	const bool hasPrimary=BotWeapons::HasPrimary(actor);
	const bool ranked=BotConfig::Get().weaponRank==1;
	// DS-097: same single writer and 3 s throttle as UpdateUpgrade. This call only keeps the arbiter's candidate view;
	// it does not start the budget, so a goal the arbiter scores but does not chase cannot time out into a false ban.
	if (!hasPrimary || ranked) TryAcquireUpgradeGoal(actor);
	if (m_upgradeTarget && !BotWeapons::AvailableForPickup(actor,m_upgradeTarget)) SetUpgradeTarget(0);
	IEntity* item=m_upgradeTarget ? gEnv->pEntitySystem->GetEntity(m_upgradeTarget) : nullptr;
	ArbiterInput in;
	in.now=m_now;
	in.hasPrimary=hasPrimary;
	in.enemyVisible=enemy && m_combat.Visible();
	in.enemyDistance=enemy ? (m_combat.LastPosition()-feet).GetLength() : 1e9f;
	in.lastSeenAge=(enemy && m_combat.LastSeen()>0) ? m_now-m_combat.LastSeen() : 1e9f;
	in.engageRange=m_profile.engageRange;
	in.damagedRecently=m_blackboard.damagedUntil-m_now>2.4f;
	in.weaponGoalAvailable=item!=nullptr;
	in.weaponGoalDistance=item ? (item->GetWorldPos()-feet).GetLength() : 1e9f;
	in.weaponGoalValue=item ? FightValue(item->GetClass()->GetName(),PickupRange()) : 0.0f;
	in.heldWeaponValue=BotWeapons::HeldValue(actor,PickupRange());
	// Inventory may improve outside this pursuit (PS buy, script grant, another pickup). A worthless goal is gone,
	// not merely a low score that commitment can keep alive. Temporary combat suppression below is not cancellation.
	if (m_upgradeTarget && m_arbiter.Current()==ArbiterGoal::Weapon && !m_arbiter.WeaponGoalWorthwhile(in))
	{ SetUpgradeTarget(0); in.weaponGoalAvailable=false; in.weaponGoalValue=0.0f; }
	// DS-040: never shop for a gun while a visible enemy is inside the hold range; that was the 19.6 s sidearm gap.
	if (BotConfig::Get().proFire==1 && in.enemyVisible && in.enemyDistance<=m_profile.holdRange) { in.weaponGoalAvailable=false; in.weaponGoalValue=0.0f; }
	const ArbiterDecision d=m_arbiter.Update(in);
	if (d.switched)
		WB_LOG(2,"[WarsBots] #%d ARBITER goal=%s reason=%s score=%.2f actor=%u.",m_number,GoalArbiter::GoalName(d.goal),
			GoalArbiter::ReasonName(d.reason),d.score,actor.GetEntityId());
	if (d.goal==ArbiterGoal::Weapon && item && UpdateUpgrade(bot,actor,enemy,decisionTime,socomQA,true)) return true;
	if ((d.goal==ArbiterGoal::Fight || d.goal==ArbiterGoal::Hunt) && enemy) { UpdateCombat(bot,actor,decisionTime); return true; }
	// Goal without an executable target this tick (item gone, target forgotten): the old order decides.
	if (enemy) { UpdateCombat(bot,actor,decisionTime); return true; }
	UpdateRoam(bot,actor,decisionTime);
	return true;
}

void BotBrain::AbilityCommand(BotInstance& bot, IConsoleCmdArgs* args)
{
	const char* command=args->GetArg(0);
	const char* value=args->GetArgCount()>=2?args->GetArg(1):"";
	CPlayer* actor=static_cast<CPlayer*>(bot.GetActor());
	if (!actor) return;
	if (!strcmp(command,"wb_bot_abilities"))
	{
		m_blackboard.Sample(*actor,m_now);
		WB_LOG(2, "[WarsBots] ABILITIES health=%d grounded=%d speed=%.2f vz=%.2f mode=%d energy=%.1f active=%d jumpCooldown=%.2f suitCooldown=%.2f.",
			m_blackboard.health,m_blackboard.grounded?1:0,m_blackboard.velocity.GetLength2D(),m_blackboard.velocity.z,
			int(m_blackboard.mode),m_blackboard.energy,m_blackboard.suitActive?1:0,
			max(0.0f,m_blackboard.nextJump-m_now),max(0.0f,m_blackboard.nextSuitChange-m_now));
		return;
	}
	if (actor->GetHealth()<=0) { WB_LOG(2, "[WarsBots] ACTION REFUSED: dead actor."); return; }
	if (!strcmp(command,"wb_bot_jump") && args->GetArgCount()<=3 &&
		(args->GetArgCount()==1 || !strcmp(value,"normal") || !strcmp(value,"strength")))
	{
		if (m_testJump!=BotJump::None) { WB_LOG(2, "[WarsBots] ACTION REFUSED: jump command already queued."); return; }
		float delay=0;
		if (args->GetArgCount()==3)
		{
			char* end=nullptr; delay=strtof(args->GetArg(2),&end);
			if (!end || end==args->GetArg(2) || *end || !std::isfinite(delay) || delay<0 || delay>30)
			{ WB_LOG(2, "[WarsBots] ACTION REFUSED: jump delay must be 0..30 seconds."); return; }
		}
		m_testJump=!strcmp(value,"strength")?BotJump::Strength:BotJump::Normal;
		m_testJumpAt=m_now+delay; m_testGait=-1; m_testStrafe=0; m_testUntil=m_testJumpAt+6.0f;
		return;
	}
	if (!strcmp(command,"wb_bot_strafe"))
    {
        if (!strcmp(value,"auto")) { m_testStrafe=0; m_testUntil=0; m_testGait=-1; return; }
        if (!strcmp(value,"left")) m_testStrafe=-1;
        else if (!strcmp(value,"right")) m_testStrafe=1;
        else { WB_LOG(0, "[WarsBots] Usage: wb_bot_strafe left|right|auto."); return; }
        m_testGait=int(BotGait::Run);
        m_testJump=BotJump::None;
        m_testGoal=actor->GetEntity()->GetWorldPos()+actor->GetBaseQuat().GetColumn0()*float(m_testStrafe)*12.0f;
        m_testLook=actor->GetEntity()->GetWorldPos()+actor->GetBaseQuat().GetColumn1()*20.0f+Vec3(0,0,1.4f);
        m_testUntil=m_now+6.0f; return;
    }
    if (!strcmp(command,"wb_bot_gait"))
	{
		m_testStrafe=0; m_testJump=BotJump::None;
		if (!strcmp(value,"auto")) { m_testUntil=0; m_testGait=-1; return; }
		if (!strcmp(value,"walk")) m_testGait=int(BotGait::Walk);
		else if (!strcmp(value,"run")) m_testGait=int(BotGait::Run);
		else if (!strcmp(value,"sprint")) m_testGait=int(BotGait::Sprint);
		else { WB_LOG(0, "[WarsBots] Usage: wb_bot_gait walk|run|sprint|auto."); return; }
		m_testGoal=actor->GetEntity()->GetWorldPos()+actor->GetBaseQuat().GetColumn1()*25.0f;
		m_testUntil=m_now+6.0f; return;
	}
	if (!strcmp(command,"wb_bot_suit"))
	{
		if (!strcmp(value,"auto")) { m_testMode=-1; m_testModeUntil=0; return; }
		if (!strcmp(value,"defense")) m_testMode=NANOMODE_DEFENSE;
		else if (!strcmp(value,"speed")) m_testMode=NANOMODE_SPEED;
		else if (!strcmp(value,"strength")) m_testMode=NANOMODE_STRENGTH;
		else if (!strcmp(value,"cloak")) m_testMode=NANOMODE_CLOAK;
		else { WB_LOG(0, "[WarsBots] Usage: wb_bot_suit defense|speed|strength|cloak|auto."); return; }
		m_testModeUntil=m_now+8.0f; return;
	}
	WB_LOG(0, "[WarsBots] Usage: wb_bot_jump [normal|strength] [delaySeconds].");
}

}
