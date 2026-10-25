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

void BotMovement::Look(CActor& actor, const Vec3& target)
{
	static_cast<CPlayer&>(actor).SetWarsBotAim(target);
	if (auto* controller = actor.GetMovementController())
	{
		CMovementRequest request;
		request.SetLookTarget(target);
		request.SetAimTarget(target);
		request.SetFireTarget(target);
		controller->RequestMovement(request);
	}
}

bool BotMovement::SafeStep(CActor& actor, const Vec3& direction, float probe, bool meshGuided, float slope) const
{
	if (!gEnv->pPhysicalWorld || !actor.GetEntity()->GetPhysics())
		return false;
	const Vec3 feet = actor.GetEntity()->GetWorldPos();
	IItem* held = actor.GetCurrentItem();
	IPhysicalEntity* heldPhysics = held ? held->GetEntity()->GetPhysics() : nullptr;
	ray_hit hit;
	const unsigned flags = rwi_stop_at_pierceable | rwi_colltype_any;
	// Three parallel body-width rays prevent a shoulder clipping a corner.
	// P0 (C-055 stalls, X-049): on a Detour corner path the knee ray at 0.4 m hits stair risers 1-1.5 m ahead and
	// declared the stairs a wall (level-4 traps on Steelmill stairs at z 142-144). The mesh already proved the slope/step.
	for (float side : {-0.3f,0.0f,0.3f})
	for (float height : {0.4f,1.1f,1.6f})
		if (!(meshGuided && height < 1.0f) && gEnv->pPhysicalWorld->RayWorldIntersection(feet + Vec3(-direction.y*side,direction.x*side,height), (direction + Vec3(0,0,slope)) * probe,
			ent_all, flags, &hit, 1, actor.GetEntity()->GetPhysics(), heldPhysics))
			return false;
	// Stop at real cliffs only: drops up to ~3 m are taken (Armor absorbs the landing, see BotBrain).
	return gEnv->pPhysicalWorld->RayWorldIntersection(feet + direction * (probe-0.3f) + Vec3(0,0,0.6f),
		Vec3(0,0,-3.6f), ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid,
		flags, &hit, 1, actor.GetEntity()->GetPhysics()) != 0;
}

bool BotMovement::Move(CActor& actor, const Vec3& destination, const Vec3& lookAt, float dt, float speed, EStance stance, float probe, bool jumpTraverse, bool strafe, float pseudo, bool meshGuided, float arrive)
{
	auto* controller = actor.GetMovementController();
	if (!controller) return false;
	const Vec3 feet = actor.GetEntity()->GetWorldPos();
	Vec3 direction = destination - feet;
	direction.z = 0;
	// Height differences are not a reason to stop: walking horizontally toward a target on another
	// level is how ramps and stairs get found without a pathfinder. Only arrival stops here.
	(void)jumpTraverse;
	if (direction.GetLengthSquared() < arrive * arrive)
	{
		Stop(actor);
		Aim(actor, lookAt);
		return false;
	}
	const float distance = direction.GetLength();
	direction.NormalizeSafe();
	// C-074: when corner following owns arrival (wb_bot_corners) the destination is a corner the mesh already cleared;
	// obstacles beyond it are the next leg's business. A 1.5/2.7 m probe near a corner hits the wall behind it.
	float slope = 0;
	if (meshGuided && arrive < 0.5f)
	{
		probe = min(probe, distance + 0.3f);
		// Body rays follow the leg's slope: on stairs the steps rise ~1 m per 1.5 m and level rays at 1.1/1.6 m hit
		// them - the bot declared the stairs a wall and dithered at the bottom (owner playtest 25.10 hotspot 1290,1350).
		slope = clamp_tpl((destination.z - feet.z) / max(distance, 0.5f), -1.0f, 1.0f);
	}
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
	bool safe = jumpTraverse || (m_avoidTime <= 0 && SafeStep(actor, direction, probe, meshGuided, slope));
	m_turned = false;
	if (!safe)
	{
		// Wider fan, including backing out of corners.
		for (float angle : {m_side * 0.8f, -m_side * 0.8f, m_side * 1.57f, -m_side * 1.57f, m_side * 2.36f, -m_side * 2.36f, 3.14159f})
		{
			Vec3 candidate = Quat::CreateRotationZ(angle) * direction;
			if (SafeStep(actor, candidate, probe, meshGuided)) { direction = candidate; safe = true; m_turned = fabs_tpl(angle) > 1.6f; break; }
		}
	}
	if (!safe) { Stop(actor); Aim(actor, lookAt); return false; }
	static_cast<CPlayer&>(actor).SetWarsBotMotion(direction,speed,lookAt,strafe);
	CMovementRequest request;
	request.SetMoveTarget(feet + direction * 2.0f);
	request.SetDesiredSpeed(speed);
	request.SetPseudoSpeed(pseudo>=0.f?pseudo:(speed<2.0f?0.3f:0.5f));
	request.SetStance(stance);
	request.SetLookTarget(lookAt);
	request.SetAimTarget(lookAt);
	request.SetFireTarget(lookAt);
	request.SetBodyTarget(strafe ? lookAt : feet + direction * 4.0f);
	request.SetAllowStrafing(strafe);
	return controller->RequestMovement(request);
}
}
