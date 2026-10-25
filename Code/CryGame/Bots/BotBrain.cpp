#include "PortIncludes.h"
#include "BotConfig.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotPace.h"
#include "CryGame/Bots/BotBrain.h"
#include "CryGame/Bots/BotAimControl.h"
#include "CryCommon/CryAction/IMovementController.h"
#include "CryGame/Bots/BotInstance.h"
#include "CryGame/Bots/BotManager.h"
#include "CryGame/Bots/BotWeapons.h"
#include "CryGame/Bots/BotWeaponRank.h"
#include "CryGame/Bots/BotNavMesh.h"
#include "CryGame/Bots/BotNavMeshCore.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryCommon/CryAction/IWeapon.h"
#include <cmath>
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
}

void BotBrain::Change(State state,bool initialize)
{
    // Preserve Start's former silent initialization; normal transitions keep their exact effects.
    if(initialize){m_state=state;return;}
	if (m_state == state) return;
	m_progress.Reset();
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
        m_intent.gait=distance>=3.f?BotGait::Sprint:BotGait::Run;
        m_intent.suitMode=m_speedTravel?NANOMODE_SPEED:NANOMODE_DEFENSE;
        if(!m_speedTravel)return;
    }else{
	if (!m_profile.useSpeed) { m_intent.gait=BotGait::Run; return; }
	if (m_speedTravel) { if (m_blackboard.energy < 55.0f || distance < 8.0f) m_speedTravel = false; }
	else if (m_blackboard.energy > 120.0f && distance > 15.0f) m_speedTravel = true;
	m_intent.gait = m_speedTravel ? BotGait::Sprint : BotGait::Run;
	if (!m_speedTravel) return;
	m_intent.suitMode = NANOMODE_SPEED;
    }
	// Speed drains energy fast, so a ">160" threshold was almost never met: jump at >130 instead.
	if (distance > 15.0f && m_blackboard.grounded && m_blackboard.energy > 130.0f &&
		m_now >= m_blackboard.nextSuitChange && m_now >= m_nextTravelJump)
	{
		m_nextTravelJump = m_now + 5.0f + Rand01() * 5.0f;
		if (Rand01() < 0.7f) { m_intent.jump = BotJump::Strength; m_intent.suitMode = NANOMODE_STRENGTH; }
	}
}

Vec3 BotBrain::Steer(const Vec3& feet, const Vec3& goal)
{
	const BotNav& nav = g_pGame->GetBotManager().Nav();
	NavMeshCore* mesh = CurrentNavMesh(); // P0, wb_bot_navmesh: Detour corners instead of the sparse ray graph
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
			found = mesh->FindPathLinks(a, b, xyz, m_pathLinks, &why);
			if (!found) m_pathLinks.clear();
			m_meshPathActive = found;
			m_path.clear();
			for (size_t i = 0; i + 2 < xyz.size(); i += 3) m_path.push_back(Vec3(xyz[i], xyz[i + 1], xyz[i + 2]));
			trace.reason = why;
		}
		if (!found) m_pathLinks.clear();
		if (!found && nav.Ready()) found = nav.FindPath(feet, goal, m_path, &trace);
		m_pathIndex = 0;
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
}

void BotBrain::ResetLife()
{
	m_progress.Reset(); m_trap.Reset(); m_trapStepUntil=0; m_trapJumpAttempted=false; m_escapeUntil=0; m_trackedTarget=0; m_trackedSelection=-1;
	m_locomotion.Reset();
	m_suit.Reset(); m_blackboard=BotBlackboard(); m_intent=BotIntent();
	m_testJump=BotJump::None; m_testGait=m_testMode=-1; m_testUntil=m_testModeUntil=m_testJumpAt=0;
	m_combat.Reset();
	m_weaponTarget = 0;
	m_patrolUntil = 0;
	m_detour = false; m_blockedSince = 0; m_chaseBlockedSince = 0; m_shotBlockedSince = 0;
	m_path.clear(); m_pathIndex = 0; m_nextRepath = 0; m_minRepath = 0; m_meshPathActive = false; m_pathLinks.clear(); m_link.Reset();
	m_vantage.Reset(CurrentVantage(),m_number);
	m_withdrawUntil=m_nextWithdraw=0;
	m_speedTravel = false; m_nextTravelJump = m_now + 3.0f; m_nextDodge = 0; m_lastHitSeen = 0;
	m_upgradeTarget = 0; m_nextUpgradeSearch = 0; m_stuckReported = false; m_upgradeSince = 0;
	++m_life;
	m_testStrafe=0; m_strafeSide=((m_life+unsigned(m_number))&1)?1:-1; m_nextStrafe=m_now+0.37f*float(m_number%5);
	m_fallbackGiven = false;
	m_retreating = false;
	m_look.Reset(); m_lookValid = false; m_arbiter.Reset(); m_nextRangeSelect = 0; m_aimModelInit = false; m_aimModelTarget = 0; m_suitPolicy.Reset();
	m_weaponSearchStarted = m_now;
	m_nextSearch = m_now;
	m_deadTime = 0;
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
    UpdateTelemetry(actor);
    if(UpdateDead(bot,actor,dt))return;
	if (BotConfig::Get().botTurnCap==1) UpdateTurnCap(actor,dt); // C-044: per-frame capped view toward the decided look point
	m_tick += dt;
	if (m_tick < 0.1f) return; // Bound entity searches, raycasts and decisions to 10 Hz.
	const float decisionTime = m_tick;
    ++m_decisionSerial;
	m_tick = 0;
	if (m_state == State::DEAD || m_state == State::RESPAWNING) { ResetLife(); Change(State::SEEKING_WEAPON); }

	m_intent=BotIntent();
	m_intent.aim=actor.GetEntity()->GetWorldPos()+actor.GetBaseQuat().GetColumn1()*10.0f+Vec3(0,0,1.4f);
    if(UpdateTest(actor,decisionTime))return;
    const int& qaWeapon=BotConfig::Get().qaSocomOnly;
    const bool socomQA=qaWeapon==1 && bot.CanRunLocalIA();
    if(UpdateWeaponSeek(actor,decisionTime,socomQA))return;
	IActor* enemy = m_combat.FindEnemy(actor, m_now, m_verbose);
	if (m_now<m_escapeUntil) enemy=nullptr; // A short, sensed-but-not-pursued local escape leg.
    if(BotConfig::Get().goalArbiter==1 && !socomQA && UpdateArbitrated(bot,actor,enemy,decisionTime,socomQA))return; // C-032
    if(UpdateUpgrade(bot,actor,enemy,decisionTime,socomQA))return;
    if(!enemy){UpdateRoam(bot,actor,decisionTime);return;}
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
		if (m_deadTime >= 5.0f)
		{
			Change(State::RESPAWNING);
			if (bot.Revive())
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

bool BotBrain::UpdateTest(CPlayer& actor,float decisionTime)
{
	if (m_now<m_testUntil)
	{
		if (m_testGait>=0) { m_intent.MoveTo(m_testGoal,m_testStrafe ? m_testLook : m_testGoal+Vec3(0,0,1.4f)); m_intent.gait=static_cast<BotGait>(m_testGait); m_intent.strafe=m_testStrafe!=0; }
		m_intent.jump=m_now>=m_testJumpAt?m_testJump:BotJump::None;
		if (m_testJump!=BotJump::None) m_intent.suitMode=m_testJump==BotJump::Strength?NANOMODE_STRENGTH:NANOMODE_DEFENSE;
		else if (m_testGait==int(BotGait::Sprint)) m_intent.suitMode=NANOMODE_SPEED;
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
        m_combat.StopFire();m_intent.move=false;ExecuteIntent(actor,decisionTime);return true;
    }
	if (!BotWeapons::SelectAvailable(actor))
	{
		m_combat.StopFire();
		if (m_now >= m_nextSearch && !m_weaponTarget)
		{
			Change(State::SEEKING_WEAPON);
			m_weaponTarget = BotWeapons::FindNearby(actor, 30.0f, m_previousWeaponClass, m_life);
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
				m_intent.move=false;
				if (BotWeapons::PickUp(actor, m_weaponTarget)) { m_previousWeaponClass = actor.GetCurrentItem()->GetEntity()->GetClass(); m_weaponTarget = 0; Change(State::SEARCHING_ENEMY); }
				else m_weaponTarget = 0;
			}
			else { const Vec3 step=Steer(actor.GetEntity()->GetWorldPos(),item->GetWorldPos()); m_intent.MoveTo(step,step+Vec3(0,0,1.2f)); TravelGait((item->GetWorldPos()-actor.GetEntity()->GetWorldPos()).GetLength()); }
		}
		else if (!m_fallbackGiven && (!item || m_now - m_weaponSearchStarted >= 6.0f))
		{
			m_intent.move=false;
			m_fallbackGiven = true; // At most one test supply per life, never per frame.
			if (BotWeapons::GiveTestSCAR(actor)) Change(State::SEARCHING_ENEMY);
		}
		else { m_weaponTarget = 0; m_intent.move=false; Change(State::SEEKING_WEAPON); }
		ExecuteIntent(actor,decisionTime);
		return true;
	}


    return false;
}

EntityId BotBrain::UpgradeCandidate(CPlayer& actor,bool anyHeld)
{
	(void)anyHeld; // with wb_bot_weapon_rank FindNearby keeps only real upgrades over what is held
	EntityId skip[8]; int skipCount = 0;
	for (const BadItem& b : m_badItems) if (b.id && m_now < b.until) skip[skipCount++] = b.id;
	// owner 25.10 (wb_bot_vantage): Marksmen judge pickups at sniper range, so DSG1/Gauss become real upgrades
	const float typical=BotConfig::Get().vantage==1 && m_profile.style==BotStyle::Marksman ? 60.0f : 20.0f;
	return BotWeapons::FindNearby(actor, 45.0f, nullptr, m_life, true, skip, skipCount, typical);
}

bool BotBrain::UpdateUpgrade(BotInstance& bot,CPlayer& actor,IActor* enemy,float decisionTime,bool socomQA,bool force)
{
	// Holding only the spawn pistol and nobody in sight: go for a rifle like a player would.
	// force: the goal arbiter (C-032) already decided this tick is a weapon tick.
	if (!socomQA && (force || ((!enemy || !m_combat.Visible()) && !BotWeapons::HasPrimary(actor))))
	{
		if (m_now >= m_nextUpgradeSearch && !m_upgradeTarget)
		{
			m_upgradeTarget = UpgradeCandidate(actor, false);
			m_upgradeSince = m_now;
			m_nextUpgradeSearch = m_now + 3.0f;
		}
		// Unreachable (on a ledge, behind glass, taken): give up after 10 s and skip it for 90 s.
		if (m_upgradeTarget && m_now - m_upgradeSince > 10.0f)
		{
			WB_LOG(2, "[WarsBots] #%d weapon %u unreachable; skipping it.", m_number, m_upgradeTarget);
			MarkBadItem(m_upgradeTarget); m_upgradeTarget = 0;
		}
		// A selected item can disappear or gain an owner while we approach it.
		// Do not chase another player's gun until the ten-second timeout.
		if (m_upgradeTarget && !BotWeapons::AvailableForPickup(actor, m_upgradeTarget))
		{
			WB_LOG(2, "[WarsBots] #%d weapon goal cancelled item=%u reason=unavailable.", m_number, m_upgradeTarget);
			m_upgradeTarget = 0;
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
				m_intent.move=false;
				if (BotWeapons::PickUp(actor, m_upgradeTarget)) m_previousWeaponClass = actor.GetCurrentItem()->GetEntity()->GetClass();
				else MarkBadItem(m_upgradeTarget);
				m_upgradeTarget = 0;
			}
			else { const Vec3 step=Steer(feet,item->GetWorldPos()); m_intent.MoveTo(step,step+Vec3(0,0,1.2f)); TravelGait(d); }
			ExecuteIntent(actor,decisionTime);
			if (bot.GetActor()!=&actor) return true;
			return true;
		}
	}

    return false;
}

void BotBrain::UpdateRoam(BotInstance& bot,CPlayer& actor,float decisionTime)
{

		Change(State::SEARCHING_ENEMY);
		m_blackboard.enemy=0; m_blackboard.visible=false; m_blackboard.distance=0;
		const Vec3 feet = actor.GetEntity()->GetWorldPos();
		if (UpdateVantage(actor,feet,decisionTime)) return; // owner 25.10: snipers hold roofs
		// C-037 hearing: a gunshot 6..50 m away that nobody can see is worth a look - roam toward it for a while.
		Vec3 heard;
		if (BotConfig::Get().botPerception==1 && m_combat.Heard(m_now,4.0f,heard) &&
			(heard-feet).GetLengthSquared2D()>36.0f && (heard-feet).GetLengthSquared2D()<2500.0f &&
			(heard-m_patrolGoal).GetLengthSquared2D()>25.0f)
		{
			NavMeshCore* mesh = CurrentNavMesh();
			const float from[3] = {feet.x, feet.y, feet.z}, to[3] = {heard.x, heard.y, heard.z};
			float snap[3];
			if (!mesh || !mesh->Project(from, snap) || mesh->Reachable(from, to))
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
			if (mesh && !mesh->Project(from, snap)) mesh = nullptr; // off the mesh: keep the old goal choice
			auto reachable = [&](const Vec3& p) { const float q[3] = {p.x, p.y, p.z}; return !mesh || mesh->Reachable(from, q); };
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
				m_patrolGoal = feet + Vec3(cosf(angle),sinf(angle),0) * (6.0f + (m_patrolStep%3)*2.0f);
				m_patrolUntil = m_now + 4.0f;
			}
			m_detour = false;
		}
		{ const Vec3 step=Steer(feet,m_patrolGoal); m_intent.MoveTo(step,step+Vec3(0,0,1.3f)); }
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
	IItem* held=actor.GetCurrentItem();
	const char* cls=held ? held->GetEntity()->GetClass()->GetName() : "";
	const bool sniper=map && (m_profile.style==BotStyle::Marksman || m_profile.style==BotStyle::Defender) &&
		(!strcmp(cls,"DSG1") || !strcmp(cls,"GaussRifle"));
	if (!sniper) { if (m_vantage.Phase()!=VantagePhase::None) m_vantage.Reset(map,m_number); return false; }
	if (m_now<m_blackboard.damagedUntil) m_vantage.OnHit(m_now);
	NavMeshCore* mesh=CurrentNavMesh();
	const float from[3]={feet.x,feet.y,feet.z};
	float snap[3];
	const bool onMesh=mesh && mesh->Project(from,snap);
	const VantageOutput o=m_vantage.Update(*map,m_number,m_now,feet.x,feet.y,feet.z,m_rng+unsigned(m_life),
		[&](const VSpot& s){ const float q[3]={s.x,s.y,s.z}; return !onMesh || mesh->Reachable(from,q); });
	if (o.relocated) WB_LOG(2,"[WarsBots] #%d VANTAGE relocate actor=%u.",m_number,actor.GetEntityId());
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
	if (o.move) { const Vec3 step=Steer(feet,Vec3(o.x,o.y,o.z)); m_intent.MoveTo(step,eye+Vec3(sinf(yaw),cosf(yaw),0)*30.0f); m_intent.gait=BotGait::Walk; }
	else { m_intent.move=false; m_intent.aim=eye+Vec3(sinf(yaw),cosf(yaw),-0.05f)*30.0f; }
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
	{ m_nextRangeSelect=m_now+2.0f; BotWeapons::SelectForRange(actor,distance); }
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
		WB_LOG(2,"[WarsBots] TACTIC actor=%u skill=%s style=%s action=withdraw health=%d energy=%.1f distance=%.1f visible=%d.",
			actor.GetEntityId(),BotProfile::SkillName(m_profile.skill),BotProfile::StyleName(m_profile.style),
			m_blackboard.health,m_blackboard.energy,distance,visible?1:0);
	}
	if (m_now<m_withdrawUntil)
	{
		const Vec3 away=(actor.GetEntity()->GetWorldPos()-destination).GetNormalizedSafe(FORWARD_DIRECTION);
		m_intent.MoveTo(actor.GetEntity()->GetWorldPos()+away*4.0f,aim);
		m_intent.strafe=visible; m_intent.combat=BotCombatIntent::Aim;
		m_intent.suitMode=NANOMODE_DEFENSE;
		if (!BotPaceEnabled() && !visible && m_profile.useSpeed && m_blackboard.energy>120 && m_now>=m_blackboard.damagedUntil)
		{ m_intent.gait=BotGait::Sprint; m_intent.suitMode=NANOMODE_SPEED; m_intent.combat=BotCombatIntent::None; }
		ExecuteIntent(actor,decisionTime); return;
	}
	if (visible && distance <= (m_state == State::ATTACKING ? m_profile.holdRange : m_profile.engageRange))
	{
        UpdateAttacking(actor,destination,aim,distance,desiredRange);
    }else{
        UpdateChasing(actor,destination,aim,distance,visible);
    }
    UpdateCombatPost(bot,actor,decisionTime);
}

void BotBrain::UpdateAttacking(CPlayer& actor,const Vec3& destination,const Vec3& aim,float distance,float desiredRange)
{

		// Retreat locally when crowded; obstruction checks still forbid unsafe steps.
		if (distance < (m_retreating ? m_profile.retreatExit : m_profile.retreatEnter))
		{
			m_retreating = true;
			Vec3 away = (actor.GetEntity()->GetWorldPos()-destination).GetNormalizedSafe(FORWARD_DIRECTION);
			m_intent.MoveTo(actor.GetEntity()->GetWorldPos()+away*3.0f,aim);
			m_intent.combat=BotCombatIntent::Aim;
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
            Vec3 travel=lateral*float(m_strafeSide)+toward*clamp_tpl((distance-desiredRange)*0.1f,-0.5f,0.5f);
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
            m_intent.MoveTo(actor.GetEntity()->GetWorldPos()+travel*4.0f,aim);
            m_intent.strafe=true;
        }
		Change(State::ATTACKING);
		m_intent.combat=m_now-m_stateTime>=m_profile.reaction && m_now-m_combat.SelectedAt()>=m_profile.reaction?BotCombatIntent::Fire:BotCombatIntent::Aim;
		// Just got hit: sometimes a Strength jump to break the enemy's aim (native jump, energy cost).
		const bool freshHit = m_blackboard.damagedUntil - m_now > 2.4f && m_blackboard.damagedUntil != m_lastHitSeen;
		if (freshHit)
		{
			m_lastHitSeen = m_blackboard.damagedUntil;
			if (m_blackboard.grounded && m_blackboard.energy >= 110.0f && m_now >= m_nextDodge && Rand01() < m_profile.dodgeChance)
			{ m_intent.jump=BotJump::Strength; m_intent.suitMode=NANOMODE_STRENGTH; m_nextDodge=m_now+4.0f; }
		}

}

void BotBrain::UpdateChasing(CPlayer& actor,const Vec3& destination,const Vec3& aim,float distance,bool visible)
{

		Change(State::CHASING_ENEMY);
		m_intent.MoveTo(Steer(actor.GetEntity()->GetWorldPos(),destination),aim);
		// A visible target beyond engage range is still shot at while closing in, like a player
		// with a rifle would; outside fireRange or without LOS the bot only moves.
		if (visible && distance <= m_profile.fireRange)
		{
			m_intent.strafe=true;
			m_intent.combat=m_now-m_stateTime>=m_profile.reaction && m_now-m_combat.SelectedAt()>=m_profile.reaction?BotCombatIntent::Fire:BotCombatIntent::Aim;
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
			m_intent.gait=m_profile.useSpeed && distance>(m_blackboard.mode==NANOMODE_SPEED?m_profile.sprintChaseRange-2.0f:m_profile.sprintChaseRange) && m_now-m_stateTime>0.4f && m_blackboard.energy>(m_blackboard.mode==NANOMODE_SPEED?50.0f:100.0f)?BotGait::Sprint:BotGait::Run;
			if (m_intent.gait==BotGait::Sprint) m_intent.suitMode=NANOMODE_SPEED;
		}
		if (!visible && distance>12.0f && distance<30.0f && BotSuit::CloakEnabled() && m_blackboard.energy>120.0f)
		{ m_intent.suitMode=NANOMODE_CLOAK; m_intent.gait=BotGait::Walk; }


}

void BotBrain::UpdateCombatPost(BotInstance& bot,CPlayer& actor,float decisionTime)
{

	ExecuteIntent(actor,decisionTime);
	if (bot.GetActor()!=&actor) return;
	IWeapon* activeWeapon=BotWeapons::Current(actor);
	const bool protect=m_now-m_combat.LastBurstRequest()<4.0f ||
		m_now-m_combat.SelectedAt()<0.75f || (activeWeapon && activeWeapon->IsReloading());
	if (m_intent.move && NoProgress(actor,"combat",m_combat.Target(),protect))
	{
		g_pGame->GetBotManager().Nav().ReportStuck(actor.GetEntity()->GetWorldPos(),actor.GetEntityId());
		m_combat.AvoidCurrent(m_now,8.0f); m_escapeUntil=m_now+1.5f;
		m_detour=true; m_patrolUntil=0; m_nextRepath=0;
	}

}

bool BotBrain::NoProgress(CPlayer& actor,const char* kind,EntityId goal,bool protect)
{
	const int& experiment=BotConfig::Get().progressExperiment;
	if (experiment!=1) { m_progress.Reset(); return false; }
	if (!m_blackboard.canMove) { m_progress.Reset(); return false; }
	const Vec3 p=actor.GetEntity()->GetWorldPos();
	const float timeout=6.0f-2.0f*m_profile.targetJudgement;
	if (!m_progress.Stalled(p.x,p.y,p.z,m_now,timeout) || protect) return false;
	WB_LOG(2,"[WarsBots] PROGRESS ESCAPE actor=%u kind=%s goal=%u pos=(%.2f,%.2f,%.2f) age=%.2f.",
		actor.GetEntityId(),kind,goal,p.x,p.y,p.z,m_progress.Age(m_now));
	m_progress.Reset();
	return true;
}

bool BotBrain::RecoverTrap(CPlayer& actor)
{
    const int& enabled=BotConfig::Get().trapRecovery;
    if (enabled!=1 || !gEnv->bServer ||
        !g_pGame->GetBotManager().Owns(actor.GetEntityId()) || !g_pGame->GetBotManager().CanRunLocalIA())
    { m_trap.Reset(); m_trapStepUntil=0; return false; }
    const Vec3 p=actor.GetEntity()->GetWorldPos();
    const int level=m_trap.Update(p.x,p.y,p.z,m_now,m_intent.move,m_blackboard.canMove,false);
    if (level)
    {
        WB_LOG(1,"[WarsBots] TRAP RECOVERY level=%d actor=%u age=%.2f pos=(%.2f,%.2f,%.2f) blocked=%d speed=%.2f grounded=%d sink=%d.",
            level,actor.GetEntityId(),m_trap.Age(m_now),p.x,p.y,p.z,m_blackboard.blocked?1:0,m_blackboard.velocity.GetLength2D(),m_blackboard.grounded?1:0,0);
        if (level==1)
        {
            m_path.clear();m_pathIndex=0;m_nextRepath=m_minRepath=0;m_pathLinks.clear();m_link.Reset();
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
        m_intent.MoveTo(p+m_trapDirection*3.f,m_intent.aim);
        m_intent.strafe=false;m_intent.gait=BotGait::Run;m_intent.suitMode=NANOMODE_DEFENSE;
        m_intent.jump=BotJump::None;
        if (m_trap.Level()==3 && !m_trapJumpAttempted && m_blackboard.grounded &&
            m_blackboard.mode==NANOMODE_DEFENSE && m_now>=m_blackboard.nextJump)
        { m_intent.jump=BotJump::Normal;m_trapJumpAttempted=true; } // Executor still requires JumpSpace.
    }
    return false;
}

void BotBrain::ExecuteIntent(CPlayer& actor, float dt)
{
    if (RecoverTrap(actor)) return;
    ApplyLookPolicy(actor); // C-033, wb_bot_look
	if (m_now<m_testModeUntil && m_testMode>=0 && m_intent.combat==BotCombatIntent::None && m_intent.jump==BotJump::None)
		m_intent.suitMode=static_cast<ENanoMode>(m_testMode);
	m_blackboard.obstacleJump=BotJump::None;
	m_intent.linkJump=false;
	if (BotConfig::Get().jumpLinks==1) ApplyLinkExecutor(actor); else m_link.Reset(); // C-036
	if (!m_link.Active() && m_intent.move && m_blackboard.grounded && m_now>=m_blackboard.nextJump && m_now>=m_testUntil &&
        !(m_trap.Level()>=2 && m_now<m_trapStepUntil))
	{
		const Vec3 direction=(m_intent.destination-m_blackboard.position).GetNormalizedSafe(FORWARD_DIRECTION);
		m_blackboard.obstacleJump=m_locomotion.Obstacle(actor,direction);
		if (m_blackboard.obstacleJump!=BotJump::None && m_intent.combat==BotCombatIntent::None)
		{
			m_intent.jump=m_blackboard.obstacleJump;
			m_intent.suitMode=m_intent.jump==BotJump::Strength?NANOMODE_STRENGTH:NANOMODE_DEFENSE;
	}
}
	if (!m_blackboard.grounded && m_blackboard.velocity.z < -3.0f)
		m_intent.suitMode=NANOMODE_DEFENSE; // Armor before landing: native suit absorbs fall damage.
	else if (!m_blackboard.grounded && static_cast<const SPlayerStats*>(actor.GetActorStats())->jumped && m_blackboard.mode==NANOMODE_STRENGTH && m_intent.jump==BotJump::None)
		m_intent.suitMode=NANOMODE_STRENGTH;
	if (!BotPaceEnabled() && (m_now<m_blackboard.damagedUntil || m_intent.combat!=BotCombatIntent::None || m_blackboard.energy<45.0f) && m_intent.jump!=BotJump::Strength)
	{ m_intent.suitMode=NANOMODE_DEFENSE; if (m_intent.gait==BotGait::Sprint) m_intent.gait=BotGait::Run; }
	if(BotPaceEnabled() && m_intent.jump==BotJump::Strength)m_speedTravel=false;
    if(BotPaceEnabled() && m_intent.jump!=BotJump::Strength && (m_now<m_blackboard.damagedUntil || m_intent.combat==BotCombatIntent::Fire)){m_intent.suitMode=NANOMODE_DEFENSE;m_speedTravel=false;}
    if (BotConfig::Get().suitPolicy==1 && !m_link.Active()) ApplySuitPolicy(actor); // roadmap 12: the last word on the suit mode
    const bool suitReady=m_suit.Apply(actor,m_intent.suitMode,m_blackboard,m_now);
	if(!suitReady && m_intent.gait==BotGait::Sprint){
        if(BotPaceEnabled() && m_intent.jump==BotJump::None){m_speedTravel=false;m_intent.suitMode=NANOMODE_DEFENSE;m_suit.Apply(actor,NANOMODE_DEFENSE,m_blackboard,m_now);}
        else m_intent.gait=BotGait::Run;
    }
	if (BotConfig::Get().botTurnCap==1)
	{
		BotIntent capped=m_intent; capped.aim=CappedLook(actor); // C-044: the decision tick never snaps the view
		m_locomotion.Execute(actor,capped,m_blackboard,m_now,dt);
	}
	else m_locomotion.Execute(actor,m_intent,m_blackboard,m_now,dt);
	if (m_intent.combat==BotCombatIntent::Fire && m_blackboard.mode!=NANOMODE_CLOAK)
	{
		IActor* target=g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_combat.Target());
		if (target) m_combat.Attack(actor,*target,m_now,m_verbose); else m_combat.StopFire();
}
	else m_combat.StopFire();
	if (BotConfig::Get().weaponHandling==1)
	{
		// owner 25.10: laser on (off in cloak), zoom only while fighting a seen target far away
		const bool fighting=m_intent.combat!=BotCombatIntent::None && m_combat.Target() && m_combat.Visible();
		const float dist=fighting ? (m_combat.LastPosition()-m_blackboard.position).GetLength() : -1.0f;
		BotWeapons::Handle(actor,dist,m_blackboard.suitActive && m_blackboard.mode==NANOMODE_CLOAK);
	}
}
// ---------------------------------------------------------------------------------------------- C-036 links
// Takes over at a link take-off corner of a Detour path and runs BotJumpLink until landing; a failed link is banned.
void BotBrain::ApplyLinkExecutor(CPlayer& actor)
{
	NavMeshCore* mesh=CurrentNavMesh();
	const Vec3 feet=m_blackboard.position;
	if (!mesh) { m_link.Reset(); return; }
	if (!m_link.Active())
	{
		if (!m_meshPathActive || !m_intent.move || m_intent.combat==BotCombatIntent::Fire) return;
		const NavPathLink* l=LinkAtCorner(m_pathIndex);
		if (!l) return;
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
		{ const size_t next=size_t(m_linkNow.corner)+1; m_pathIndex=m_path.empty()?0:(next<m_path.size()?next:m_path.size()-1); }
		m_link.Reset();
		return;
	}
	if (o.phase==LinkPhase::Failed)
	{
		static const char* const why[]={"none","timeout","no_takeoff","missed_landing","no_energy"};
		WB_LOG(1,"[WarsBots] #%d LINK failed kind=%s reason=%s at=(%.2f,%.2f,%.2f)%s actor=%u.",m_number,NavLinkName(m_linkNow.kind),
			why[int(o.fail)],feet.x,feet.y,feet.z,o.fail==LinkFail::NoEnergy?"":" -> banned",actor.GetEntityId());
		if (o.fail!=LinkFail::NoEnergy) mesh->BanLink(m_linkNow.ref);
		m_link.Reset(); m_path.clear(); m_pathLinks.clear(); m_pathIndex=0; m_nextRepath=m_minRepath=0;
		return;
	}
	if (!m_link.Active()) return;
	m_intent.move=true;
	m_intent.destination=Vec3(o.moveTo.x,o.moveTo.y,o.moveTo.z);
	m_intent.gait=o.slow?BotGait::Walk:BotGait::Run;
	m_intent.jump=o.jump==LinkJumpRequest::Strength?BotJump::Strength:(o.jump==LinkJumpRequest::Normal?BotJump::Normal:BotJump::None);
	m_intent.linkJump=m_intent.jump!=BotJump::None;
	if (o.wantStrength) m_intent.suitMode=NANOMODE_STRENGTH;
	else if (o.wantArmor) m_intent.suitMode=NANOMODE_DEFENSE;
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
	const LookOutput o=m_look.Update(in);
	const float yaw=DEG2RAD(o.yaw), pitch=DEG2RAD(o.pitch);
	m_intent.aim=eye+Vec3(sinf(yaw)*cosf(pitch),cosf(yaw)*cosf(pitch),sinf(pitch))*8.0f;
    if(m_lookDiagnostics<128 && m_now>=m_nextLookDiagnostic && BotConfig::Get().moveTrace!=0){
        static const char* const names[]={"keep","target","attacker","interest","path","next"};
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
		m_aimModel=BotAimModel(AimSkillPreset(skill),0x9E3779B9u*unsigned(m_number+1)+m_life);
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
	in.stealthStyle=m_profile.style==BotStyle::Flanker || m_profile.style==BotStyle::Hunter;
	in.cloakAllowed=BotSuit::CloakEnabled();
	in.canUseSpeed=m_profile.skill!=BotSkill::Beginner || m_profile.useSpeed;
	const SuitDecision d=m_suitPolicy.Update(in);
	switch (d.mode)
	{
	case SuitMode::Speed: m_intent.suitMode=NANOMODE_SPEED; break;
	case SuitMode::Strength: m_intent.suitMode=NANOMODE_STRENGTH; break;
	case SuitMode::Cloak: m_intent.suitMode=NANOMODE_CLOAK; break;
	default: m_intent.suitMode=NANOMODE_DEFENSE; break;
	}
	if (d.mode!=SuitMode::Strength && m_intent.jump==BotJump::Strength) m_intent.jump=BotJump::Normal; // no energy for it
	m_speedTravel=d.mode==SuitMode::Speed;
}

// C-032 goal arbiter (BotGoalArbiter.h): one decision per tick with commitment windows, so CHASING_ENEMY and
// MOVING_TO_WEAPON no longer flip on instantaneous visibility (14.9 decision flips per bot-minute in the baseline demo).
bool BotBrain::UpdateArbitrated(BotInstance& bot,CPlayer& actor,IActor* enemy,float decisionTime,bool socomQA)
{
	const Vec3 feet=actor.GetEntity()->GetWorldPos();
	const bool hasPrimary=BotWeapons::HasPrimary(actor);
	const bool ranked=BotConfig::Get().weaponRank==1;
	if ((!hasPrimary || ranked) && m_now>=m_nextUpgradeSearch && !m_upgradeTarget)
	{ m_upgradeTarget=UpgradeCandidate(actor,hasPrimary); m_upgradeSince=m_now; m_nextUpgradeSearch=m_now+3.0f; }
	if (m_upgradeTarget && !BotWeapons::AvailableForPickup(actor,m_upgradeTarget)) m_upgradeTarget=0;
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
	in.weaponGoalValue=item ? FightValue(item->GetClass()->GetName(),20.0f) : 0.0f;
	in.heldWeaponValue=BotWeapons::HeldValue(actor,20.0f);
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
