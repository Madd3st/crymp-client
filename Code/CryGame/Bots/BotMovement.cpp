#include "PortIncludes.h"
#include "CryGame/Bots/BotMovement.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CryAction/IMovementController.h"

namespace WarsBots
{
void BotMovement::Reset() { m_sampleTime = 0; m_avoidTime = 0; m_side = 1; }

void BotMovement::Stop(CActor& actor)
{
	static_cast<CPlayer&>(actor).SetWarsBotMotion(actor.GetEntity()->GetWorldRotation().GetColumn1(),0,
		actor.GetEntity()->GetWorldPos()+actor.GetEntity()->GetWorldRotation().GetColumn1()*10.0f+Vec3(0,0,1.4f));
	if (auto* controller = actor.GetMovementController())
	{
		CMovementRequest request;
		request.ClearMoveTarget();
		request.SetDesiredSpeed(0);
		request.SetPseudoSpeed(0);
		controller->RequestMovement(request);
	}
}

void BotMovement::Aim(CActor& actor, const Vec3& target)
{
	static_cast<CPlayer&>(actor).SetWarsBotMotion(target-actor.GetEntity()->GetWorldPos(),0,target);
	if (auto* controller = actor.GetMovementController())
	{
		CMovementRequest request;
		request.SetLookTarget(target);
		request.SetAimTarget(target);
		request.SetFireTarget(target);
		request.SetBodyTarget(target);
		request.SetStance(STANCE_STAND);
		request.SetAllowStrafing(true);
		controller->RequestMovement(request);
	}
}

bool BotMovement::SafeStep(CActor& actor, const Vec3& direction, float probe) const
{
	if (!gEnv->pPhysicalWorld || !actor.GetEntity()->GetPhysics())
		return false;
	const Vec3 feet = actor.GetEntity()->GetWorldPos();
	IItem* held = actor.GetCurrentItem();
	IPhysicalEntity* heldPhysics = held ? held->GetEntity()->GetPhysics() : nullptr;
	ray_hit hit;
	const unsigned flags = rwi_stop_at_pierceable | rwi_colltype_any;
	// Three parallel body-width rays prevent a shoulder clipping a corner.
	for (float side : {-0.3f,0.0f,0.3f})
	for (float height : {0.4f,1.1f,1.6f})
		if (gEnv->pPhysicalWorld->RayWorldIntersection(feet + Vec3(-direction.y*side,direction.x*side,height), direction * probe,
			ent_all, flags, &hit, 1, actor.GetEntity()->GetPhysics(), heldPhysics))
			return false;
	// Stop at real cliffs only: drops up to ~3 m are taken (Armor absorbs the landing, see BotBrain).
	return gEnv->pPhysicalWorld->RayWorldIntersection(feet + direction * (probe-0.3f) + Vec3(0,0,0.6f),
		Vec3(0,0,-3.6f), ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid,
		flags, &hit, 1, actor.GetEntity()->GetPhysics()) != 0;
}

bool BotMovement::Move(CActor& actor, const Vec3& destination, const Vec3& lookAt, float dt, float speed, EStance stance, float probe, bool jumpTraverse, bool strafe)
{
	auto* controller = actor.GetMovementController();
	if (!controller) return false;
	const Vec3 feet = actor.GetEntity()->GetWorldPos();
	Vec3 direction = destination - feet;
	direction.z = 0;
	// Height differences are not a reason to stop: walking horizontally toward a target on another
	// level is how ramps and stairs get found without a pathfinder. Only arrival stops here.
	(void)jumpTraverse;
	if (direction.GetLengthSquared() < 0.8f * 0.8f)
	{
		Stop(actor);
		Aim(actor, lookAt);
		return false;
	}
	direction.NormalizeSafe();
	if (m_sampleTime == 0) m_sample = feet;
	m_sampleTime += dt;
	if (m_sampleTime >= 2.0f)
	{
		if ((feet - m_sample).GetLengthSquared2D() < 0.09f)
		{
			m_avoidTime = 1.2f;
			m_side = -m_side;
		}
		m_sampleTime = 0;
	}
	m_avoidTime = max(0.0f, m_avoidTime - dt);
	bool safe = jumpTraverse || (m_avoidTime <= 0 && SafeStep(actor, direction, probe));
	m_turned = false;
	if (!safe)
	{
		// Wider fan, including backing out of corners.
		for (float angle : {m_side * 0.8f, -m_side * 0.8f, m_side * 1.57f, -m_side * 1.57f, m_side * 2.36f, -m_side * 2.36f, 3.14159f})
		{
			Vec3 candidate = Quat::CreateRotationZ(angle) * direction;
			if (SafeStep(actor, candidate, probe)) { direction = candidate; safe = true; m_turned = fabs_tpl(angle) > 1.6f; break; }
		}
	}
	if (!safe) { Stop(actor); Aim(actor, lookAt); return false; }
	static_cast<CPlayer&>(actor).SetWarsBotMotion(direction,speed,lookAt,strafe);
	CMovementRequest request;
	request.SetMoveTarget(feet + direction * 2.0f);
	request.SetDesiredSpeed(speed);
	request.SetPseudoSpeed(speed<2.0f?0.3f:0.5f);
	request.SetStance(stance);
	request.SetLookTarget(lookAt);
	request.SetAimTarget(lookAt);
	request.SetFireTarget(lookAt);
	request.SetBodyTarget(strafe ? lookAt : feet + direction * 4.0f);
	request.SetAllowStrafing(strafe);
	return controller->RequestMovement(request);
}
}
