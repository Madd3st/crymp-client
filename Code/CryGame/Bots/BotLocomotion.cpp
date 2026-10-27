#include "PortIncludes.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotPace.h"
#include "CryGame/Bots/BotLocomotion.h"
#include "CryGame/Game.h"
#include "CryGame/Bots/BotManager.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryCommon/CryAction/IMovementController.h"

namespace WarsBots
{
namespace
{
bool Ray(CPlayer& actor, const Vec3& from, const Vec3& delta, ray_hit& hit)
{
	IItem* held = actor.GetCurrentItem();
	return gEnv->pPhysicalWorld->RayWorldIntersection(from,delta,ent_all,
		rwi_stop_at_pierceable|rwi_colltype_any,&hit,1,actor.GetEntity()->GetPhysics(),
		held ? held->GetEntity()->GetPhysics() : nullptr) != 0;
}
}
void BotLocomotion::Reset()
{
	m_movement.Reset(); m_pending=m_airborne=m_strength=m_sprinting=false;
	m_requested=m_startZ=m_apex=m_nextLog=0;
}
void BotLocomotion::Stop(CPlayer& actor)
{
	m_movement.Stop(actor);
	actor.SetWarsBotActions(false,false,STANCE_STAND);
	const float nextLog=m_nextLog; Reset(); m_nextLog=nextLog;
}
void BotLocomotion::Refused(CPlayer& actor, const char* reason, float now)
{
	if (now >= m_nextLog) { WB_LOG(2, "[WarsBots] ACTION REFUSED: %s actor=%u.",reason,actor.GetEntityId()); m_nextLog=now+2.0f; }
}
void BotLocomotion::Observe(CPlayer& actor, BotBlackboard& state, float now)
{
	if (state.health<=0) { if (m_pending || m_airborne) Stop(actor); return; }
	// Water is a native movement state, not a grounded landing. Clear the land-jump carry without publishing a fake landing.
	if (state.waterMovement) { m_pending=m_airborne=m_strength=false; return; }
	if (m_pending && !state.grounded && state.velocity.z>0.2f)
	{
		m_pending=false; m_airborne=true;
		actor.PublishWarsBotJump(m_strength);
        g_pGame->GetBotManager().Demo().Jump(actor.GetEntityId(),m_strength);
		WB_LOG(2, "[WarsBots] JUMP TAKEOFF: strength=%d vz=%.2f energy=%.1f actor=%u.",m_strength?1:0,state.velocity.z,state.energy,actor.GetEntityId());
	}
	if (m_pending && now-m_requested>1.0f) { m_pending=false; Refused(actor,"jump_not_accepted_by_native_movement",now); }
	if (m_airborne)
	{
		m_apex=max(m_apex,state.position.z);
		if (state.grounded)
		{
			WB_LOG(2, "[WarsBots] JUMP LANDING: strength=%d rise=%.2f displacementZ=%.2f energy=%.1f actor=%u.",m_strength?1:0,m_apex-m_startZ,state.position.z-m_startZ,state.energy,actor.GetEntityId());
            g_pGame->GetBotManager().Demo().Land(actor.GetEntityId(),m_apex-state.position.z);
			m_airborne=false;
            if(BotPaceEnabled())state.nextJump=now+.5f;
		}
	}
}
BotJump BotLocomotion::Obstacle(CPlayer& actor, const Vec3& direction) const
{
	ray_hit hit;
	const Vec3 feet=actor.GetEntity()->GetWorldPos();
	if (!Ray(actor,feet+Vec3(0,0,0.4f),direction*1.5f,hit)) return BotJump::None;
	if (Ray(actor,feet+Vec3(0,0,1.8f),direction*2.0f,hit)) return BotJump::None;
	const bool strength=Ray(actor,feet+Vec3(0,0,1.0f),direction*2.0f,hit);
	if (!JumpSpace(actor,direction,true,strength)) return BotJump::None;
	return strength?BotJump::Strength:BotJump::Normal;
}
bool BotLocomotion::JumpSpace(CPlayer& actor, const Vec3& direction, bool moving, bool strength) const
{
	const Vec3 feet=actor.GetEntity()->GetWorldPos();
	ray_hit hit;
	// Conservative standing clearance; never infer a safe jump just from the low obstacle ray.
	for (float side : {-0.3f,0.0f,0.3f})
		if (Ray(actor,feet+Vec3(-direction.y*side,direction.x*side,1.65f),Vec3(0,0,strength?7.0f:2.5f),hit)) return false;
	Vec3 landing=feet+direction*(moving?(strength?3.0f:2.0f):0.0f);
	if (!Ray(actor,landing+Vec3(0,0,strength?3.0f:1.0f),Vec3(0,0,strength?-4.0f:-2.0f),hit) || hit.n.z<0.75f) return false;
	landing=hit.pt;
	if (fabs_tpl(landing.z-feet.z)>(strength?1.5f:0.6f)) return false;
	if (moving && Ray(actor,feet+Vec3(0,0,strength?2.5f:1.3f),landing-feet,hit)) return false;
	// A moving Strength jump carries ~5-6 m: the far touchdown must not be a ledge drop either.
	if (moving && strength)
	{
		ray_hit far;
		const Vec3 farPoint=feet+direction*5.5f;
		if (!Ray(actor,farPoint+Vec3(0,0,3.0f),Vec3(0,0,-6.0f),far) || far.n.z<0.6f || far.pt.z<feet.z-2.5f) return false;
	}
	primitives::sphere body; body.r=0.35f; body.center=landing+Vec3(0,0,0.4f);
	IPhysicalEntity* skip[2]; int skipCount=0;
	if (IPhysicalEntity* bodyPhysics=actor.GetEntity()->GetPhysics()) skip[skipCount++]=bodyPhysics;
	IItem* held=actor.GetCurrentItem();
	if (held && held->GetEntity()->GetPhysics()) skip[skipCount++]=held->GetEntity()->GetPhysics();
	geom_contact* contact=nullptr;
	const float collision=gEnv->pPhysicalWorld->PrimitiveWorldIntersection(body.type,&body,Vec3(0,0,1.3f),
		ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid|ent_living,&contact,0,
		(geom_colltype_player<<rwi_colltype_bit)|rwi_stop_at_pierceable,0,0,0,skip,skipCount);
	return collision<=0.001f;
}
bool BotLocomotion::Execute(CPlayer& actor, const BotIntent& intent, BotBlackboard& state, float now, float dt)
{
	const bool pace=BotPaceEnabled();
    if (!state.canMove) { Stop(actor); Refused(actor,"movement_state",now); return false; }
	const Vec3 direction=(intent.destination-state.position).GetNormalizedSafe(actor.GetBaseQuat().GetColumn1());
	const bool strength=intent.jump==BotJump::Strength;
	bool jump=false;
	if (intent.jump!=BotJump::None)
	{
		if (state.waterMovement) Refused(actor,"jump_swimming",now);
		else if (!state.grounded || m_pending || m_airborne) Refused(actor,"jump_not_grounded_or_pending",now);
		else if (intent.stance==STANCE_PRONE || actor.GetStance()==STANCE_PRONE) Refused(actor,"jump_prone",now);
		else if (now<state.nextJump) Refused(actor,"jump_cooldown",now);
		else if (strength && (!state.suitActive || state.mode!=NANOMODE_STRENGTH || state.energy<(pace?70.f:90.f))) Refused(actor,"jump_strength_mode_or_energy",now);
		else if (!strength && state.mode==NANOMODE_STRENGTH) Refused(actor,"normal_jump_waiting_for_defense",now);
		else if (!intent.linkJump && !JumpSpace(actor,direction,intent.move,strength)) Refused(actor,"jump_clearance_or_landing",now);
		else
		{
			jump=true; m_pending=true; m_strength=strength; m_requested=now;
			m_startZ=m_apex=state.position.z; state.nextJump=now+(pace?.5f:3.f);
			WB_LOG(2, "[WarsBots] BOT ACTION: JUMP strength=%d energy=%.1f actor=%u.",strength?1:0,state.energy,actor.GetEntityId());
		}
	}
	// Let a validated jump carry forward through the low obstruction; never a position change.
    const bool eligible=intent.move && !state.waterMovement && intent.gait==BotGait::Sprint &&
        (pace?intent.combat!=BotCombatIntent::Fire:intent.combat==BotCombatIntent::None) &&
        state.grounded && state.suitActive && !jump && !m_pending && !m_airborne && !actor.WarsBotWeaponFiring();
    const bool sprint=eligible && (pace?PaceSprintEnergy(m_sprinting,state.energy):state.energy>45.f);
    m_sprinting=sprint;
    const EStance stance=state.waterMovement?STANCE_SWIM:intent.stance;
    bool accepted=false;
    if(intent.move){
        const float base=intent.gait==BotGait::Walk?1.3f:(pace?actor.GetStanceMaxSpeed(stance):2.5f);
        const float pseudo=pace?actor.CalculatePseudoSpeed(sprint):-1.f;
        // DS-070: ProcessSwimming applies the request in view space (PlayerMovement.cpp:835-836) and the WarsBot view
        // follows the aim while strafing (Player.cpp:5085-5090), so strafing in water would swim toward the aim.
        const bool strafe=intent.strafe && !state.waterMovement;
        accepted=m_movement.Move(actor,intent.destination,intent.aim,dt,base,stance,sprint?2.7f:1.5f,(jump||m_pending||m_airborne)&&!state.waterMovement,strafe,pseudo,intent.meshPath,intent.arrive,state.waterMovement);
    }else {m_movement.Stop(actor);m_movement.Aim(actor,intent.aim);}
	state.blocked=intent.move && !accepted;
	actor.SetWarsBotActions(sprint && accepted,intent.move && accepted,stance);
	CMovementRequest request;
	request.SetStance(jump?STANCE_STAND:stance);
	if (jump) request.SetJump();
	actor.GetMovementController()->RequestMovement(request);
	return accepted;
}
void BotLocomotion::Look(CPlayer& actor, const Vec3& target) { m_movement.Look(actor, target); }
}
