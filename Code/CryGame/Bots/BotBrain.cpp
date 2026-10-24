#include "PortIncludes.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotBrain.h"
#include "CryGame/Bots/BotAimControl.h"
#include "CryCommon/CryAction/IMovementController.h"
#include "CryGame/Bots/BotInstance.h"
#include "CryGame/Bots/BotManager.h"
#include "CryGame/Bots/BotWeapons.h"
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

void BotBrain::Change(State state)
{
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
	if (!m_profile.useSpeed) { m_intent.gait=BotGait::Run; return; }
	if (m_speedTravel) { if (m_blackboard.energy < 55.0f || distance < 8.0f) m_speedTravel = false; }
	else if (m_blackboard.energy > 120.0f && distance > 15.0f) m_speedTravel = true;
	m_intent.gait = m_speedTravel ? BotGait::Sprint : BotGait::Run;
	if (!m_speedTravel) return;
	m_intent.suitMode = NANOMODE_SPEED;
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
	if (!nav.Ready()) return goal; // fallback: straight line + local avoidance
	// Failed paths also respect the two-second retry deadline. An empty failed path is
	// a cached result, not a reason to repeat A* at the 0.75-second global throttle.
	if (((goal - m_pathGoal).GetLengthSquared() > 16.0f || m_now >= m_nextRepath) && m_now >= m_minRepath)
	{
		m_minRepath = m_now + 0.75f;
		const bool found = nav.FindPath(feet, goal, m_path);
		m_pathIndex = 0;
		m_pathGoal = goal;
		m_nextRepath = m_now + (found ? 5.0f : 2.0f);
		if (!found)
		{
			m_path.clear();
			WB_LOG(2, "[WarsBots] #%d NAV fallback start=(%.2f,%.2f,%.2f) goal=(%.2f,%.2f,%.2f) retry=2.0.",
				m_number, feet.x, feet.y, feet.z, goal.x, goal.y, goal.z);
		}
	}
	if (m_path.empty()) return goal; // Sparse graph can miss a physically reachable item.
	// Advance past reached waypoints; skip ahead when the next-but-one is directly reachable.
	while (m_pathIndex + 1 < m_path.size())
	{
		const Vec3& p = m_path[m_pathIndex];
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
	m_progress.Reset(); m_escapeUntil=0; m_trackedTarget=0; m_trackedSelection=-1;
	m_locomotion.Reset();
	m_suit.Reset(); m_blackboard=BotBlackboard(); m_intent=BotIntent();
	m_testJump=BotJump::None; m_testGait=m_testMode=-1; m_testUntil=m_testModeUntil=m_testJumpAt=0;
	m_combat.Reset();
	m_weaponTarget = 0;
	m_patrolUntil = 0;
	m_detour = false; m_blockedSince = 0; m_chaseBlockedSince = 0; m_shotBlockedSince = 0;
	m_path.clear(); m_pathIndex = 0; m_nextRepath = 0; m_minRepath = 0;
	m_withdrawUntil=m_nextWithdraw=0;
	m_speedTravel = false; m_nextTravelJump = m_now + 3.0f; m_nextDodge = 0; m_lastHitSeen = 0;
	m_upgradeTarget = 0; m_nextUpgradeSearch = 0; m_stuckReported = false; m_upgradeSince = 0;
	++m_life;
	m_testStrafe=0; m_strafeSide=((m_life+unsigned(m_number))&1)?1:-1; m_nextStrafe=m_now+0.37f*float(m_number%5);
	m_fallbackGiven = false;
	m_retreating = false;
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
	m_state = State::SPAWNING;
	ResetLife();
	bot.GetActor()->GetGameObject()->EnablePrePhysicsUpdate(ePPU_Always);
	Change(State::SEEKING_WEAPON);
	return true;
}

void BotBrain::Stop(BotInstance& bot)
{
	if (!m_running) return;
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
	m_blackboard.Sample(actor,m_now);
	m_locomotion.Observe(actor,m_blackboard,m_now);
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
	if (actor.GetHealth() <= 0)
	{
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
		return;
	}
	m_tick += dt;
	if (m_tick < 0.1f) return; // Bound entity searches, raycasts and decisions to 10 Hz.
	const float decisionTime = m_tick;
	m_tick = 0;
	if (m_state == State::DEAD || m_state == State::RESPAWNING) { ResetLife(); Change(State::SEEKING_WEAPON); }

	m_intent=BotIntent();
	m_intent.aim=actor.GetEntity()->GetWorldPos()+actor.GetBaseQuat().GetColumn1()*10.0f+Vec3(0,0,1.4f);
	if (m_now<m_testUntil)
	{
		if (m_testGait>=0) { m_intent.MoveTo(m_testGoal,m_testStrafe ? m_testLook : m_testGoal+Vec3(0,0,1.4f)); m_intent.gait=static_cast<BotGait>(m_testGait); m_intent.strafe=m_testStrafe!=0; }
		m_intent.jump=m_now>=m_testJumpAt?m_testJump:BotJump::None;
		if (m_testJump!=BotJump::None) m_intent.suitMode=m_testJump==BotJump::Strength?NANOMODE_STRENGTH:NANOMODE_DEFENSE;
		else if (m_testGait==int(BotGait::Sprint)) m_intent.suitMode=NANOMODE_SPEED;
		if (m_now>=m_testJumpAt) m_testJump=BotJump::None; // Consume the command once, including a refused attempt.
		ExecuteIntent(actor,decisionTime); return;
	}
	m_testGait=-1;
    ICVar* qaWeapon=gEnv->pConsole->GetCVar("wb_qa_socom_only");
    const bool socomQA=qaWeapon && qaWeapon->GetIVal()==1 && bot.CanRunLocalIA();
    if (socomQA && (!actor.GetCurrentItem() ||
        strcmp(actor.GetCurrentItem()->GetEntity()->GetClass()->GetName(),"SOCOM") ||
        !BotWeapons::Current(actor) || !BotWeapons::HasAmmunition(actor,*BotWeapons::Current(actor)))) {
        m_combat.StopFire();m_intent.move=false;ExecuteIntent(actor,decisionTime);return;
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
		return;
	}

	IActor* enemy = m_combat.FindEnemy(actor, m_now, m_verbose);
	if (m_now<m_escapeUntil) enemy=nullptr; // A short, sensed-but-not-pursued local escape leg.
	// Holding only the spawn pistol and nobody in sight: go for a rifle like a player would.
	if (!socomQA && (!enemy || !m_combat.Visible()) && !BotWeapons::HasPrimary(actor))
	{
		if (m_now >= m_nextUpgradeSearch && !m_upgradeTarget)
		{
			EntityId skip[8]; int skipCount = 0;
			for (const BadItem& b : m_badItems) if (b.id && m_now < b.until) skip[skipCount++] = b.id;
			m_upgradeTarget = BotWeapons::FindNearby(actor, 45.0f, nullptr, m_life, true, skip, skipCount);
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
			if (bot.GetActor()!=&actor) return;
			return;
		}
	}
	if (!enemy)
	{
		Change(State::SEARCHING_ENEMY);
		m_blackboard.enemy=0; m_blackboard.visible=false; m_blackboard.distance=0;
		const Vec3 feet = actor.GetEntity()->GetWorldPos();
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
			if (!m_detour && (m_patrolStep & 1) && g_pGame->GetBotManager().Nav().PickHotspot(m_rng + m_patrolStep, safe, hotspot) &&
				(hotspot - feet).GetLengthSquared2D() > 64.0f)
			{ m_patrolGoal = hotspot; m_patrolUntil = m_now + 30.0f; picked = true; }
			else if (count && !m_detour)
				for (int tries = 0; tries < count && !waypoint; ++tries, ++m_patrolStep)
				{
					IEntity* spawn = gEnv->pEntitySystem->GetEntity(rules->GetSpawnLocation(int((m_patrolStep * 7u + unsigned(m_number) * 3u) % unsigned(count))));
					if (spawn && (spawn->GetWorldPos() - feet).GetLengthSquared2D() > 64.0f) waypoint = spawn;
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
			g_pGame->GetBotManager().Nav().ReportStuck(feet); // learned: route around this spot next time
			m_detour=true; m_patrolUntil=m_now; m_blockedSince=0; m_nextRepath=0;
		}
		else if (m_blackboard.blocked && m_blockedSince <= 0) m_blockedSince=m_now;
		else if (!m_blackboard.blocked) m_blockedSince=0;

		return;
	}
	const bool visible = m_combat.Visible();
	const Vec3 destination = m_combat.LastPosition();
	m_blackboard.enemy=m_combat.Target(); m_blackboard.visible=visible;
	m_blackboard.lastEnemyPosition=destination;
	const float distance = (destination - actor.GetEntity()->GetWorldPos()).GetLength();
	m_blackboard.distance=distance;
    Vec3 aim=m_combat.LastAim();
    if(m_trackedTarget!=m_combat.Target() || m_trackedSelection!=m_combat.SelectedAt()) {
        SMovementState movement;actor.GetMovementController()->GetMovementState(movement);
        m_trackedAim=movement.eyePosition+movement.fireDirection.GetNormalizedSafe(FORWARD_DIRECTION)*max(1.f,distance);
        m_trackedTarget=m_combat.Target();m_trackedSelection=m_combat.SelectedAt();
    }
    // Track only the last observed/cached point. No velocity extrapolation or hidden query.
    m_trackedAim+=(aim-m_trackedAim)*BotAimControl::Alpha(m_profile,decisionTime);
    aim=m_trackedAim;
    const float error=BotAimControl::ErrorRadius(m_profile,distance);
	const float t = m_now + m_phase;
	aim += Vec3(sinf(t * 2.7f), cosf(t * 1.9f), sinf(t * 1.3f) * 0.3f) * error;
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
		if (!visible && m_profile.useSpeed && m_blackboard.energy>120 && m_now>=m_blackboard.damagedUntil)
		{ m_intent.gait=BotGait::Sprint; m_intent.suitMode=NANOMODE_SPEED; m_intent.combat=BotCombatIntent::None; }
		ExecuteIntent(actor,decisionTime); return;
	}
	if (visible && distance <= (m_state == State::ATTACKING ? m_profile.holdRange : m_profile.engageRange))
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
	else
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
			g_pGame->GetBotManager().Nav().ReportStuck(actor.GetEntity()->GetWorldPos());
			m_combat.Reset(); m_chaseBlockedSince=0; m_patrolUntil=0; m_detour=true;
		}
		if (!visible) TravelGait(distance); // hunting a hidden target: Speed travel like a player
		else
		{
			m_intent.gait=m_profile.useSpeed && distance>(m_blackboard.mode==NANOMODE_SPEED?m_profile.sprintChaseRange-2.0f:m_profile.sprintChaseRange) && m_now-m_stateTime>0.4f && m_blackboard.energy>(m_blackboard.mode==NANOMODE_SPEED?50.0f:100.0f)?BotGait::Sprint:BotGait::Run;
			if (m_intent.gait==BotGait::Sprint) m_intent.suitMode=NANOMODE_SPEED;
		}
		if (!visible && distance>12.0f && distance<30.0f && BotSuit::CloakEnabled() && m_blackboard.energy>120.0f)
		{ m_intent.suitMode=NANOMODE_CLOAK; m_intent.gait=BotGait::Walk; }

	}
	ExecuteIntent(actor,decisionTime);
	if (bot.GetActor()!=&actor) return;
	IWeapon* activeWeapon=BotWeapons::Current(actor);
	const bool protect=m_now-m_combat.LastBurstRequest()<4.0f ||
		m_now-m_combat.SelectedAt()<0.75f || (activeWeapon && activeWeapon->IsReloading());
	if (m_intent.move && NoProgress(actor,"combat",m_combat.Target(),protect))
	{
		g_pGame->GetBotManager().Nav().ReportStuck(actor.GetEntity()->GetWorldPos());
		m_combat.AvoidCurrent(m_now,8.0f); m_escapeUntil=m_now+1.5f;
		m_detour=true; m_patrolUntil=0; m_nextRepath=0;
	}
}
bool BotBrain::NoProgress(CPlayer& actor,const char* kind,EntityId goal,bool protect)
{
	ICVar* experiment=gEnv->pConsole->GetCVar("wb_progress_experiment");
	if (!experiment || experiment->GetIVal()!=1) { m_progress.Reset(); return false; }
	if (!m_blackboard.canMove) { m_progress.Reset(); return false; }
	const Vec3 p=actor.GetEntity()->GetWorldPos();
	const float timeout=6.0f-2.0f*m_profile.targetJudgement;
	if (!m_progress.Stalled(p.x,p.y,p.z,m_now,timeout) || protect) return false;
	WB_LOG(2,"[WarsBots] PROGRESS ESCAPE actor=%u kind=%s goal=%u pos=(%.2f,%.2f,%.2f) age=%.2f.",
		actor.GetEntityId(),kind,goal,p.x,p.y,p.z,m_progress.Age(m_now));
	m_progress.Reset();
	return true;
}

void BotBrain::ExecuteIntent(CPlayer& actor, float dt)
{
	if (m_now<m_testModeUntil && m_testMode>=0 && m_intent.combat==BotCombatIntent::None && m_intent.jump==BotJump::None)
		m_intent.suitMode=static_cast<ENanoMode>(m_testMode);
	m_blackboard.obstacleJump=BotJump::None;
	if (m_intent.move && m_blackboard.grounded && m_now>=m_blackboard.nextJump && m_now>=m_testUntil)
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
	if ((m_now<m_blackboard.damagedUntil || m_intent.combat!=BotCombatIntent::None || m_blackboard.energy<45.0f) && m_intent.jump!=BotJump::Strength)
	{ m_intent.suitMode=NANOMODE_DEFENSE; if (m_intent.gait==BotGait::Sprint) m_intent.gait=BotGait::Run; }
	const bool suitReady=m_suit.Apply(actor,m_intent.suitMode,m_blackboard,m_now);
	if (!suitReady && m_intent.gait==BotGait::Sprint) m_intent.gait=BotGait::Run;
	m_locomotion.Execute(actor,m_intent,m_blackboard,m_now,dt);
	if (m_intent.combat==BotCombatIntent::Fire && m_blackboard.mode!=NANOMODE_CLOAK)
	{
		IActor* target=g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_combat.Target());
		if (target) m_combat.Attack(actor,*target,m_now,m_verbose); else m_combat.StopFire();
}
	else m_combat.StopFire();
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
