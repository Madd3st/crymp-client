#include "PortIncludes.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotCombat.h"
#include "CryGame/Bots/BotWeapons.h"
#include "CryGame/Bots/BotTargeting.h"
#include "CryGame/Actors/Actor.h"
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

void BotCombat::Reset() { StopFire(); m_retaliator=0; m_retaliatorUntil=m_selectedAt=m_nextScan=0; m_visibleEnemies=0; m_shotBlocked = false; m_target = 0; m_visible = false; m_sampleTime = m_nextDiagnostic = 0; m_lastPosition.zero(); m_lastAim.zero(); m_lastSeen = 0; m_nextBurst = 0; }

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
	// Chest first; if only the head shows over cover, aim there (what a player does).
	const bool chest = gEnv->pPhysicalWorld && ClearRay(actor, target, Eyes(actor), AimPoint(target), obstruction);
	const bool head = !chest && gEnv->pPhysicalWorld && ClearRay(actor, target, Eyes(actor), Eyes(target), obstruction);
	m_visible = chest || head;
	if (m_visible)
	{
		m_lastSeen = now;
		m_lastPosition = target.GetEntity()->GetWorldPos();
		m_lastAim = chest ? AimPoint(target) : Eyes(target) - Vec3(0,0,0.08f);
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
	if (oldValid && (old->GetEntity()->GetWorldPos()-origin).GetLengthSquared()<6400.0f)
		Observe(actor,*old,now,verbose);
	else m_visible=false;
	const bool remembered=oldValid && now-m_lastSeen<=3.0f && (m_lastPosition-origin).GetLengthSquared()<6400.0f;
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
	IActorIteratorPtr iterator=actors->CreateActorIterator();
	while (IActor* candidate=iterator->Next())
	{
		if (!Enemy(candidate,actor)) continue;
		const Vec3 delta=candidate->GetEntity()->GetWorldPos()-origin;
		const float distance=delta.GetLength();
		if (distance>=80.0f) continue;
		EntityId obstruction=0;
		// Unlike Observe, this does not overwrite selected-target memory while scanning.
		if (!Sight(actor,*candidate,obstruction)) continue;
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
	if (!best && remembered) return old;
	if (!best) { StopFire(); m_target=0; m_visible=false; return nullptr; }
	const EntityId previous=m_target;
	if (best->GetEntityId()!=previous)
	{
		StopFire(); m_target=best->GetEntityId(); m_selectedAt=now;
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
	Vec3 shotPoint = AimPoint(target);
	bool clear = muzzleState.weaponPosition.IsValid() &&
		ClearRay(actor, target, muzzleState.weaponPosition, shotPoint, obstruction);
	if (!clear && muzzleState.weaponPosition.IsValid())
	{
		shotPoint = Eyes(target) - Vec3(0,0,0.08f);
		clear = ClearRay(actor, target, muzzleState.weaponPosition, shotPoint, obstruction);
	}
	m_shotBlocked = !clear;
	if (!clear) { StopFire(); return false; }
	if (BotWeapons::ReloadIfNeeded(actor, *weapon) || !BotWeapons::HasAmmunition(actor, *weapon))
	{ StopFire(); return false; }
	SMovementState state;
	actor.GetMovementController()->GetMovementState(state);
	Vec3 direction = (shotPoint - state.weaponPosition).GetNormalizedSafe(FORWARD_DIRECTION);
	if (direction.Dot(state.fireDirection.GetNormalizedSafe(FORWARD_DIRECTION)) < 0.96f)
	{ StopFire(); return false; }
	if (m_firingWeapon && now >= m_burstEnd) StopFire();
	if (!m_firingWeapon && now >= m_nextBurst && weapon->CanFire())
	{
		m_firingWeapon = actor.GetCurrentItem()->GetEntityId();
		m_burstEnd = now + m_burst;
		m_nextBurst = now + m_burstGap;
		weapon->StartFire();
		if (verbose) WB_LOG(2, "[WarsBots] Stock weapon burst requested weapon=%u target=%u.", m_firingWeapon, target.GetEntityId());
	}
	return m_firingWeapon != 0;
}
}
