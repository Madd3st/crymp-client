#include "PortIncludes.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotCombat.h"
#include "CryGame/Bots/BotWeapons.h"
#include "CryGame/Bots/BotFireControl.h"
#include "CryGame/Bots/BotAimControl.h"
#include "CryGame/Bots/BotDifficulty.h"
#include "CryGame/Bots/BotTargeting.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryGame/Bots/BotConfig.h"
#include "CryGame/Game.h"
#include "CryCommon/CryAction/IActorSystem.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CryAction/IWeapon.h"
#include "CryCommon/CryAction/IMovementController.h"

namespace WarsBots
{
namespace
{
unsigned g_rayCount = 0;
unsigned g_scanCount = 0;
// Hostility comes from the shared participant model: humans with a live channel AND
// channel-0 bots from our registry are both valid targets; teams decide alliances.
bool Enemy(IActor* target, CActor& self)
{
	return BotTargeting::IsEnemy(self, target);
}
}

unsigned BotCombat::TakeRayCount() { const unsigned n = g_rayCount; g_rayCount = 0; return n; }
unsigned BotCombat::TakeScanCount() { const unsigned n = g_scanCount; g_scanCount = 0; return n; }

void BotCombat::Reset() { StopFire(); m_nextAimChoice=0; m_preferHead=false; m_avoid=0; m_avoidUntil=0; m_lastBurstRequest=-100.0f; m_retaliator=0; m_retaliatorUntil=m_selectedAt=m_nextScan=0; m_visibleEnemies=0; m_shotBlocked = false; m_target = 0; m_visible = false; m_sampleTime = m_nextDiagnostic = 0; m_lastPosition.zero(); m_lastAim.zero(); m_lastSeen = 0; m_nextBurst = 0; m_heardAt = 0; m_heardPos.zero(); m_perception.Reset(); m_dmgTarget = 0; m_dmgDealt = m_dmgLastHp = 0; m_dmgAt = m_finishStart = -100.0f; }

void BotCombat::StopFire()
{
	if (m_firingWeapon && g_pGame)
		if (IItem* item = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_firingWeapon))
			if (IWeapon* weapon = item->GetIWeapon()) weapon->StopFire();
	m_firingWeapon = 0;
}

namespace
{
IPhysicalEntity* HeldPhysics(IActor& actor)
{
	IItem* item = actor.GetCurrentItem();
	return item ? item->GetEntity()->GetPhysics() : nullptr;
}
bool ClearRay(CActor& actor, IActor& target, const Vec3& origin, const Vec3& end, EntityId& obstruction)
{
	ray_hit hit;
	++g_rayCount;
	int count = gEnv->pPhysicalWorld->RayWorldIntersection(origin, end-origin,
		ent_all, rwi_stop_at_pierceable | rwi_colltype_any, &hit, 1,
		actor.GetEntity()->GetPhysics(), HeldPhysics(actor));
	if (!count || hit.pCollider == target.GetEntity()->GetPhysics()) return true;
	IEntity* entity = gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider);
	// Living-body and character/part physics can differ while belonging to one Player.
	if (entity && entity->GetId() == target.GetEntityId()) return true;
	obstruction = entity ? entity->GetId() : 0; // 0 includes static level geometry.
	IItem* item = entity ? g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(entity->GetId()) : nullptr;
	return item && item->GetOwnerId() == target.GetEntityId() && target.GetCurrentItem() == item;
}
Vec3 Eyes(IActor& actor)
{
	// Stance height ignores weapon animation and the victim's view direction.
	return actor.GetEntity()->GetWorldPos() + Vec3(0,0,
		clamp_tpl(static_cast<CActor&>(actor).GetStanceInfo(static_cast<CActor&>(actor).GetStance())->viewOffset.z, 0.35f, 1.8f));
}
bool Sight(CActor& actor, IActor& target, EntityId& obstruction)
{
	if (!gEnv->pPhysicalWorld) return false;
	return ClearRay(actor, target, Eyes(actor), BotCombat::AimPoint(target), obstruction) ||
		ClearRay(actor, target, Eyes(actor), Eyes(target), obstruction);
}
}
Vec3 BotCombat::AimPoint(IActor& target)
{
	const Vec3 feet = target.GetEntity()->GetWorldPos();
	return feet + (Eyes(target)-feet) * 0.75f;
}
bool BotCombat::CanSee(CActor& actor, IActor& target)
	{
	EntityId obstruction = 0;
	return Sight(actor, target, obstruction);
	}
bool BotCombat::Observe(CActor& actor, IActor& target, float now, bool verbose)
{
	EntityId obstruction = 0;
	m_sampleTime = now;
	const bool wasVisible=m_visible;
	// Chest first; if only the head shows over cover, aim there (what a player does).
	const bool chest = gEnv->pPhysicalWorld && ClearRay(actor, target, Eyes(actor), AimPoint(target), obstruction);
	const bool head = !chest && gEnv->pPhysicalWorld && ClearRay(actor, target, Eyes(actor), Eyes(target), obstruction);
	m_visible = chest || head;
	if (m_visible)
	{
		// A meaningful visual loss needs a fresh reaction, but brief cover flicker can be
		// anticipated. Do not restart the independent target-hold clock on reacquisition.
		if (!wasVisible && target.GetEntityId()==m_target && now>m_selectedAt && now-m_lastSeen>0.5f)
		{
			StopFire();
			m_nextBurst=m_profile.NextTargetBurst(m_nextBurst,now);
			WB_LOG(2,"[WarsBots] TARGET REACQUIRE actor=%u target=%u delay=%.2f.",
				actor.GetEntityId(),m_target,m_profile.reaction);
		}
        if(now>=m_nextAimChoice) {
            IItem* held=actor.GetCurrentItem();
            const char* name=held?held->GetEntity()->GetClass()->GetName():nullptr;
            const unsigned sample=actor.GetEntityId()*16777619u ^ target.GetEntityId()*33u ^ unsigned(now*10)*2654435761u;
            m_preferHead=BotAimControl::PreferHead(m_profile,name,
                (target.GetEntity()->GetWorldPos()-actor.GetEntity()->GetWorldPos()).GetLength(),sample);
            m_nextAimChoice=now+1.5f;
            if(verbose) WB_LOG(2,"[WarsBots] AIM ZONE actor=%u target=%u preferred=%s interval=1.5.",
                actor.GetEntityId(),target.GetEntityId(),m_preferHead?"head":"body");
        }
		m_lastSeen = now;
		m_lastPosition = target.GetEntity()->GetWorldPos();
        const Vec3 headPoint=Eyes(target)-Vec3(0,0,.08f);
        const bool preferredHeadClear=m_preferHead && ClearRay(actor,target,Eyes(actor),headPoint,obstruction);
        m_lastAim=preferredHeadClear?headPoint:(chest?AimPoint(target):Eyes(target));
	}
	else if (verbose && now >= m_nextDiagnostic)
	{
		const Vec3 from = Eyes(actor), to = AimPoint(target);
		WB_LOG(2, "[WarsBots] LOS blocked target=%u obstruction=%u eyes=(%.2f,%.2f,%.2f) chest=(%.2f,%.2f,%.2f) sample=%.2f.",
			target.GetEntityId(), obstruction, from.x,from.y,from.z,to.x,to.y,to.z,now);
		m_nextDiagnostic = now + 2.0f;
	}
	return m_visible;
}

IActor* BotCombat::FindEnemy(CActor& actor, float now, bool verbose)
{
	auto* actors = g_pGame->GetIGameFramework()->GetIActorSystem();
	IActor* old = actors->GetActor(m_target);
	const Vec3 origin=actor.GetEntity()->GetWorldPos();
	const bool oldValid = Enemy(old, actor);
	const float sight2=m_profile.sightRange*m_profile.sightRange; // 80 m unless wb_bot_difficulty
	if (oldValid && (old->GetEntity()->GetWorldPos()-origin).GetLengthSquared()<sight2)
		Observe(actor,*old,now,verbose);
	else m_visible=false;
	// wb_bot_finish: health drop of the target seen while this bot was firing at it = own damage (a player sees the hits)
	if (oldValid && m_visible)
	{
		const float hp=float(old->GetHealth());
		if (m_dmgTarget!=m_target) { m_dmgTarget=m_target; m_dmgDealt=0; m_dmgAt=m_finishStart=-100.0f; }
		else if (hp<m_dmgLastHp-0.5f && now-m_lastBurstRequest<1.0f)
		{
			const bool was=m_dmgDealt>=30.0f; m_dmgDealt+=m_dmgLastHp-hp; m_dmgAt=now;
			if (!was && m_dmgDealt>=30.0f) m_finishStart=now; // DS-018 #2: the finish window has a fixed start
		}
		m_dmgLastHp=hp;
	}
	const bool remembered=oldValid && now-m_lastSeen<=3.0f && (m_lastPosition-origin).GetLengthSquared()<sight2;
	// Same scan cadence/range and LOS rules at every skill. Hidden targets only use cached memory.
	if (remembered && now<m_nextScan) return old;
	m_nextScan=now+0.6f;
	++g_scanCount;
	IItem* held=actor.GetCurrentItem();
	IWeapon* weapon=BotWeapons::Current(actor);
	const bool hasAmmo=weapon && BotWeapons::HasAmmunition(actor,*weapon);
	const float desired=m_profile.WeaponRange(held?held->GetEntity()->GetClass()->GetName():nullptr,actor.GetEntityId());
	SMovementState movement;
	actor.GetMovementController()->GetMovementState(movement);
	IActor* best=nullptr;
	float bestScore=-1e9f, oldScore=-1e9f;
	m_visibleEnemies=0;
	// C-037: a new target must be noticed (view cone + time to notice + cloak); the LOS ray alone is not enough.
	const bool perception=BotConfig::Get().botPerception==1;
	if (perception)
	{
		ObserverState o; o.now=now;
		const Vec3 eye=Eyes(actor), fwd=movement.eyeDirection.GetNormalizedSafe(actor.GetEntity()->GetWorldRotation().GetColumn1());
		o.eye=PVec{eye.x,eye.y,eye.z}; o.forward=PVec{fwd.x,fwd.y,fwd.z};
		m_perception.BeginTick(o);
	}
	float heardDistance=1e9f; // closest heard-but-unseen enemy this scan
	IActorIteratorPtr iterator=actors->CreateActorIterator();
	while (IActor* candidate=iterator->Next())
	{
		if (!Enemy(candidate,actor) || (candidate->GetEntityId()==m_avoid && now<m_avoidUntil)) continue;
		const Vec3 delta=candidate->GetEntity()->GetWorldPos()-origin;
		const float distance=delta.GetLength();
		if (!(distance<m_profile.sightRange)) continue; // Also reject non-finite candidate positions.
		EntityId obstruction=0;
		// Unlike Observe, this does not overwrite selected-target memory while scanning.
		const bool los=Sight(actor,*candidate,obstruction);
		if (perception)
		{
			TargetCue cue; cue.id=int(candidate->GetEntityId());
			const Vec3 chest=AimPoint(*candidate); cue.pos=PVec{chest.x,chest.y,chest.z};
			cue.losClear=los;
			CPlayer* player=CPlayer::FromIActor(candidate);
			CNanoSuit* suit=player ? player->GetNanoSuit() : nullptr;
			cue.cloaked=suit && suit->IsActive() && suit->GetMode()==NANOMODE_CLOAK;
			// O-026 P1: Speed-mode sprint is loud (footsteps 12 m) and shows through cloak; was never set
			if (IPhysicalEntity* phys=candidate->GetEntity()->GetPhysics())
			{
				pe_status_dynamics dyn;
				if (phys->GetStatus(&dyn)) { cue.speed=Vec3(dyn.v.x,dyn.v.y,0).GetLength(); cue.sprintSpeedMode=suit && suit->IsActive() && suit->GetMode()==NANOMODE_SPEED && cue.speed>6.0f; }
			}
			IItem* theirs=candidate->GetCurrentItem();
			IWeapon* theirWeapon=theirs ? theirs->GetIWeapon() : nullptr;
			IFireMode* theirMode=theirWeapon ? theirWeapon->GetFireMode(theirWeapon->GetCurrentFireMode()) : nullptr;
			if (theirMode && theirMode->IsFiring()) cue.sinceShot=0.f; // muzzle flash / gunshot reveals
			cue.damagedMe=candidate->GetEntityId()==m_retaliator && now<m_retaliatorUntil;
			const TargetPercept seen=m_perception.Observe(cue);
			const bool tracked=candidate==old && m_visible; // an acquired, still visible target is kept by LOS
			if (seen.heard && !seen.visible && distance<heardDistance)
			{ heardDistance=distance; m_heardPos=candidate->GetEntity()->GetWorldPos(); m_heardAt=now; }
			if (!seen.visible && !tracked) continue;
		}
		else if (!los) continue;
		++m_visibleEnemies;
		const bool muzzle=movement.weaponPosition.IsValid() &&
			(ClearRay(actor,*candidate,movement.weaponPosition,AimPoint(*candidate),obstruction) ||
			 ClearRay(actor,*candidate,movement.weaponPosition,Eyes(*candidate),obstruction));
		const Vec3 forward=candidate->GetEntity()->GetWorldTM().GetColumn1().GetNormalizedSafe(FORWARD_DIRECTION);
		const bool facing=forward.Dot((-delta).GetNormalizedSafe(FORWARD_DIRECTION))>0.5f;
		const float score=m_profile.TargetUtility(distance,desired,muzzle,facing,
			candidate->GetEntityId()==m_retaliator && now<m_retaliatorUntil,float(actor.GetHealth()),hasAmmo);
		if (candidate==old) oldScore=score;
		if (score>bestScore || (score==bestScore && best && candidate->GetEntityId()<best->GetEntityId()))
		{ best=candidate; bestScore=score; }
	}
	if (remembered && m_visible && best && best!=old &&
		!m_profile.CanSwitchTarget(now-m_selectedAt,oldScore,bestScore)) best=old;
	if (BotConfig::Get().finish==1 && remembered && best && best!=old)
	{
		// DS-015: the hysteresis above only runs while the old target is visible - one 0.6 s scan behind a corner
		// switched instantly. Keep it for 1 s of lost sight; keep a target this bot has hurt (>= 30 HP in 5 s) for 2 s,
		// unless the bot itself is low and the newcomer is the one shooting it.
		// hard cap 6 s from the moment the bot had dealt 30 HP: someone else's damage cannot pin the target forever
		const bool finishing=m_dmgTarget==m_target && m_dmgDealt>=30.0f && now-m_dmgAt<5.0f && now-m_finishStart<6.0f && now-m_lastSeen<2.0f;
		const bool blink=!m_visible && now-m_lastSeen<1.0f;
		const bool threatened=best->GetEntityId()==m_retaliator && now<m_retaliatorUntil && actor.GetHealth()<40;
		if ((finishing || blink) && !threatened) best=old;
	}
	if (!best && remembered) return old;
	if (!best) { StopFire(); m_target=0; m_visible=false; return nullptr; }
	const EntityId previous=m_target;
	if (best->GetEntityId()!=previous)
	{
		StopFire(); m_target=best->GetEntityId(); m_selectedAt=now; m_nextAimChoice=0;
		m_nextBurst=m_profile.NextTargetBurst(m_nextBurst,now); m_shotBlocked=false;
		WB_LOG(2,"[WarsBots] TARGET CHOICE actor=%u old=%u target=%u skill=%s style=%s score=%.3f visible=%d health=%d ammo=%d.",
			actor.GetEntityId(),previous,m_target,BotProfile::SkillName(m_profile.skill),BotProfile::StyleName(m_profile.style),
			bestScore,m_visibleEnemies,actor.GetHealth(),hasAmmo?1:0);
		WB_LOG(2,"[WarsBots] Bot %u acquired enemy id=%u name=%s kind=%s.",actor.GetEntityId(),m_target,
			best->GetEntity()->GetName(),BotTargeting::Name(BotTargeting::Kind(best)));
	}
	Observe(actor,*best,now,false);
	return best;
}

bool BotCombat::Attack(CActor& actor, IActor& target, float now, bool verbose)
{
	IWeapon* weapon = BotWeapons::Current(actor);
	if (!weapon || !Enemy(&target, actor) || !CanSee(actor, target)) { StopFire(); return false; }
	// Detection can use eyes; shooting must also have a clear muzzle-to-chest ray.
	SMovementState muzzleState;
	actor.GetMovementController()->GetMovementState(muzzleState);
	EntityId obstruction = 0;
	// Muzzle ray to the chest, else to the head: the weapon sits lower/side of the eyes, so cover
	// that the eyes clear can still block the barrel. Report it so the brain can reposition.
	Vec3 shotPoint = m_target==target.GetEntityId() && m_visible ? m_lastAim : AimPoint(target);
	bool clear = muzzleState.weaponPosition.IsValid() &&
		ClearRay(actor, target, muzzleState.weaponPosition, shotPoint, obstruction);
	if (!clear && muzzleState.weaponPosition.IsValid())
	{
		shotPoint = (shotPoint-AimPoint(target)).GetLengthSquared()>.01f ? AimPoint(target) : Eyes(target)-Vec3(0,0,.08f);
		clear = ClearRay(actor, target, muzzleState.weaponPosition, shotPoint, obstruction);
	}
	m_shotBlocked = !clear;
	if (!clear) { StopFire(); return false; }
	if (BotWeapons::ReloadIfNeeded(actor, *weapon) || !BotWeapons::HasAmmunition(actor, *weapon))
	{ StopFire(); return false; }
	SMovementState state;
	actor.GetMovementController()->GetMovementState(state);
	Vec3 direction = (shotPoint - state.weaponPosition).GetNormalizedSafe(FORWARD_DIRECTION);
	// wb_bot_difficulty: per-skill fire gate (Elite 2 deg, Beginner 20); old gate 0.96 dot (~16 deg) for everyone
	const float gate=m_profile.turnRate>0.0f ? cosf(DEG2RAD(FireToleranceDeg(m_profile,(shotPoint-state.weaponPosition).GetLength()))) : 0.96f;
	if (direction.Dot(state.fireDirection.GetNormalizedSafe(FORWARD_DIRECTION)) < gate)
	{ StopFire(); return false; }
	IFireMode* mode=weapon->GetFireMode(weapon->GetCurrentFireMode());
    auto fire=BotFireControl::Plan(mode?mode->GetType():nullptr,mode?mode->GetFireRate():0,
        m_profile.reaction,m_burst,m_burstGap,mode?mode->GetSpinUpTime():0);
    if (m_firingWeapon && (now >= m_burstEnd || m_firingWeapon!=actor.GetCurrentItem()->GetEntityId())) StopFire();
	if (!m_firingWeapon && now >= m_nextBurst && weapon->CanFire())
	{
		m_firingWeapon = actor.GetCurrentItem()->GetEntityId();
		m_burstEnd = now + fire.holdSeconds;
		m_nextBurst = now + fire.startInterval;
		m_lastBurstRequest=now;
		weapon->StartFire();
        if (fire.pulse) StopFire(); // release the native trigger; no repeated single-frame spam
		if (verbose) WB_LOG(2, "[WarsBots] Stock weapon burst requested weapon=%u target=%u.", actor.GetCurrentItem()->GetEntityId(), target.GetEntityId());
	}
	return m_firingWeapon != 0;
}
}
