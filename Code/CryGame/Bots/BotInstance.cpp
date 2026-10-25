// WarsBots: per-bot lifecycle. Uses the existing actor network binding; no fabricated channel.
#include "PortIncludes.h"
#include "BotConfig.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotInstance.h"
#include "CryGame/Bots/BotManager.h"
#include "CryGame/Bots/BotMatch.h"
#include "CryGame/Bots/CryMPEngineAdapter.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Items/Weapons/WeaponSystem.h"
#include "CryGame/Items/Weapons/Projectile.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CryAction/IActorSystem.h"
#include <cstdio>
#include <cstring>
#include <cmath>

namespace WarsBots
{
BotInstance::BotInstance(BotManager& owner, int number, const BotProfile& profile)
	: m_owner(owner), m_number(number), m_profile(profile)
{
	if (BotConfig::Get().nameProfile == 1)
	{
		// owner 25.10: the level of play shows at a glance, e.g. "WarsBot_02 [Normal/Assault]"
		char skill[16], style[16];
		_snprintf(skill, sizeof(skill) - 1, "%s", BotProfile::SkillName(profile.skill)); skill[sizeof(skill) - 1] = 0;
		_snprintf(style, sizeof(style) - 1, "%s", BotProfile::StyleName(profile.style)); style[sizeof(style) - 1] = 0;
		if (skill[0] >= 'a' && skill[0] <= 'z') skill[0] = char(skill[0] - 32);
		if (style[0] >= 'a' && style[0] <= 'z') style[0] = char(style[0] - 32);
		_snprintf(m_name, sizeof(m_name) - 1, "WarsBot_%02d [%s/%s]", number, skill, style);
	}
	else _snprintf(m_name, sizeof(m_name) - 1, "WarsBot_%02d", number);
	m_brain.Configure(profile, number);
}

bool BotInstance::Spawn(const Vec3& position)
{
	IGameFramework* framework = m_owner.Framework();
	if (m_entityId || !framework || !CanRunLocalIA())
	{
		WB_LOG(0, "[WarsBots] #%d spawn refused: already spawned or local IA gate closed.", m_number);
		return false;
	}
	if (gEnv->pEntitySystem->FindEntityByName(m_name))
	{
		WB_LOG(0, "[WarsBots] #%d spawn refused: entity name %s is occupied.", m_number, m_name);
		return false;
	}
	// Direct creation avoids SpawnPlayer's lookup by the shared channel ID zero.
	// CActor::Init performs BindToNetwork; do not bind twice or fabricate a channel.
	IActor* actor = nullptr;
	{
		BotManager::CreationScope scope(m_owner, m_name);
		actor = CryMPEngineAdapter::CreateServerActor(*framework, position, m_name);
	}
	if (!actor)
	{
		WB_LOG(0, "[WarsBots] #%d CreateActor failed; no match registration attempted.", m_number);
		return false;
	}
	m_entityId = actor->GetEntityId();
	m_identity = actor->GetEntity();
	// Ownership must be visible to the registry before IA registration queries it.
	m_owner.Adopt(*this);
	if (!BotMatch::Register(*g_pGame->GetGameRules(), *actor))
	{
		WB_LOG(0, "[WarsBots] #%d IA registration failed; removing only this new owned actor.", m_number);
		Remove();
		return false;
	}
	BotMatch::EquipLikePlayer(*g_pGame->GetGameRules(), *actor);
	const Vec3 actualPosition = actor->GetEntity()->GetWorldPos();
	m_lastSpawnLocation = 0;
	if (CGameRules* rules = g_pGame->GetGameRules())
		for (int i = 0; i < rules->GetSpawnLocationCount(); ++i)
			if (IEntity* spawn = gEnv->pEntitySystem->GetEntity(rules->GetSpawnLocation(i)))
				if ((spawn->GetWorldPos() - actualPosition).GetLengthSquared() < 2.25f)
				{ m_lastSpawnLocation = spawn->GetId(); break; }
	m_spawnPosition = actualPosition;
	m_lastHealth = actor->GetHealth();
	m_lastHitShooter = 0;
	m_killPlayerObserved = false;
	WB_LOG(0, "[WarsBots] #%d created EntityId=%u name=%s profile=%s/%s position=(%.3f, %.3f, %.3f).",
		m_number, m_entityId, m_name, BotProfile::StyleName(m_profile.style), BotProfile::SkillName(m_profile.skill),
		actualPosition.x, actualPosition.y, actualPosition.z);
    BeginPhysicsProbe("create");
	return true;
}

void BotInstance::Remove()
{
    m_reviveGuard.Cancel();
    m_physicsProbe.Cancel();
	m_brain.Stop(*this);
	if (!m_entityId)
		return;
	const EntityId id = m_entityId;
	IEntity* entity = gEnv->pEntitySystem->GetEntity(id);
	if (!entity || entity != m_identity)
	{
		WB_LOG(0, "[WarsBots] #%d identity expired EntityId=%u; no entity removed.", m_number, id);
		m_entityId = 0;
		m_identity = nullptr;
		return;
	}
	// Only the entity created by this instance; never delete by name or channel.
	BotMatch::Unregister(id);
	m_removing = true;
	gEnv->pEntitySystem->RemoveEntity(id, true);
	m_removing = false;
	if (!gEnv->pEntitySystem->GetEntity(id))
	{
		m_entityId = 0;
		m_identity = nullptr;
		WB_LOG(0, "[WarsBots] #%d removed EntityId=%u.", m_number, id);
	}
	else
		WB_LOG(0, "[WarsBots] #%d removal pending EntityId=%u; registry keeps the slot until ENTITY_EVENT_DONE.", m_number, id);
}

void BotInstance::OnEntityDone()
{
    m_reviveGuard.Cancel();
    m_physicsProbe.Cancel();
	if (!m_entityId)
		return;
	m_brain.Stop(*this);
	BotMatch::Unregister(m_entityId);
	WB_LOG(2, "[WarsBots] #%d ENTITY_EVENT_DONE EntityId=%u.", m_number, m_entityId);
	m_entityId = 0;
	m_identity = nullptr;
}

bool BotInstance::Owns(EntityId id) const
{
	return id != 0 && id == m_entityId && m_identity && gEnv && gEnv->bServer &&
		gEnv->pEntitySystem->GetEntity(id) == m_identity;
}

bool BotInstance::CanRunLocalIA() const
{
	return m_owner.CanRunLocalIA();
}

CActor* BotInstance::GetActor() const
{
	IGameFramework* framework = m_owner.Framework();
	if (!framework || !Owns(m_entityId)) return nullptr;
	IActor* actor = framework->GetIActorSystem()->GetActor(m_entityId);
	return actor && !strcmp(actor->GetEntity()->GetClass()->GetName(), "Player") ? static_cast<CActor*>(actor) : nullptr;
}

bool BotInstance::IsOwnedProjectileHit(const HitInfo& hit) const
{
	// In-flight server bullets remain valid after AI shutdown; no new shots are enabled here.
	IGameFramework* framework = m_owner.Framework();
	if (!framework || hit.remote || !Owns(hit.shooterId) || !hit.projectileId) return false;
	CProjectile* projectile = g_pGame->GetWeaponSystem()->GetProjectile(hit.projectileId);
	IItem* weapon = framework->GetIItemSystem()->GetItem(hit.weaponId);
	return projectile && !projectile->IsRemote() && projectile->GetOwnerId() == hit.shooterId &&
		weapon && weapon->GetOwnerId() == hit.shooterId;
}

void BotInstance::TraceHit(const char* phase, const HitInfo& hit)
{
	if (!Owns(hit.targetId)) return;
	CActor* actor=GetActor();
	if (!actor) return;
	if (strcmp(phase,"before-Lua")==0)
	{ m_lastHitShooter=hit.shooterId; m_beforeHitHealth=actor->GetHealth(); }
	else if (strcmp(phase,"after-Lua")==0 && actor->GetHealth()<m_beforeHitHealth)
		m_brain.NotifyDamage(hit.shooterId);
}

void BotInstance::TraceKill(EntityId shooterId, bool killerEventEmitted)
{
	m_killPlayerObserved = true;
	IEntity* shooter = gEnv->pEntitySystem->GetEntity(shooterId);
	WB_LOG(1, "[WarsBots] #%d GameRules::KillPlayer target=%u killer=%u name=%s; eGE_Kill emitted=%d.",
		m_number, m_entityId, shooterId, shooter ? shooter->GetName() : "<none>", killerEventEmitted ? 1 : 0);
}

void BotInstance::Status() const
{
	IGameFramework* framework = m_owner.Framework();
	IActor* actor = framework && m_entityId ? framework->GetIActorSystem()->GetActor(m_entityId) : nullptr;
	if (!actor || actor->GetEntity() != m_identity)
	{
		WB_LOG(0, "[WarsBots] #%d %s: no live owned actor.", m_number, m_name);
		return;
	}
	CGameRules* rules = g_pGame->GetGameRules();
	WB_LOG(0, "[WarsBots] #%d %s EntityId=%u channelId=%u IsPlayer=%d health=%d participant=%s profile=%s/%s ai=%d.",
		m_number, m_name, m_entityId, static_cast<unsigned>(actor->GetChannelId()), actor->IsPlayer() ? 1 : 0,
		actor->GetHealth(), rules && rules->IsMatchParticipant(m_entityId) ? "YES" : "NO",
		BotProfile::StyleName(m_profile.style), BotProfile::SkillName(m_profile.skill), m_brain.Running() ? 1 : 0);
	WB_LOG(0, "[WarsBots] #%d lifecycle physicsProfile=%u lastHitShooter=%u KillPlayerObserved=%d update avg=%.3fms max=%.3fms.",
		m_number, static_cast<unsigned>(actor->GetGameObject()->GetAspectProfile(eEA_Physics)), m_lastHitShooter,
		m_killPlayerObserved ? 1 : 0, m_updateCount ? m_totalUpdateMs / float(m_updateCount) : 0.0f, m_maxUpdateMs);
}

bool BotInstance::PhysicsDiagnostics() const
{
    const int& flag=BotConfig::Get().physicsDiagnostics;
    return flag==1 && Owns(m_entityId) && CanRunLocalIA();
}

void BotInstance::BeginPhysicsProbe(const char* origin)
{
    m_physicsProbe.Cancel();
    if (!PhysicsDiagnostics()) return;
    m_physicsOrigin=origin;++m_physicsEpoch;
    m_physicsProbe.Start(gEnv->pTimer->GetFrameStartTime().GetSeconds());
    TracePhysics("now",0.f);
}

void BotInstance::TracePhysics(const char* phase,float elapsed) const
{
    CActor* actor=GetActor();
    if (!actor || !PhysicsDiagnostics()) return;
    IPhysicalEntity* body=actor->GetEntity()->GetPhysics();
    pe_status_pos pose;pe_status_awake awake;pe_status_living living;pe_player_dynamics dynamics;
    const bool hasPose=body && body->GetStatus(&pose)>0;
    const bool hasLiving=body && body->GetStatus(&living)>0;
    const bool hasDynamics=body && body->GetParams(&dynamics)>0;
    const Vec3 entity=actor->GetEntity()->GetWorldPos(),physical=hasPose?pose.pos:Vec3(0,0,0);
    WB_LOG(1,"[WarsBots] BODY STATE actor=%u epoch=%u spawn=%u origin=%s phase=%s elapsed=%.3f health=%d profile=%u type=%d awake=%d active=%d enabled=unknown_api poseValid=%d entity=(%.3f,%.3f,%.3f) physics=(%.3f,%.3f,%.3f) delta=%.4f.",
        m_entityId,m_physicsEpoch,m_lastSpawnLocation,m_physicsOrigin,phase,elapsed,actor->GetHealth(),
        unsigned(actor->GetGameObject()->GetAspectProfile(eEA_Physics)),body?body->GetType():-1,
        body?body->GetStatus(&awake):-1,hasDynamics && !is_unused(dynamics.bActive)?dynamics.bActive:-1,hasPose?1:0,
        entity.x,entity.y,entity.z,physical.x,physical.y,physical.z,hasPose?(entity-physical).GetLength():-1.f);
    if (hasLiving)
        WB_LOG(1,"[WarsBots] BODY LIVING actor=%u epoch=%u spawn=%u phase=%s flying=%d stuck=%d squashed=%d stairs=%d groundHeight=%.3f groundCollider=%d vel=(%.3f,%.3f,%.3f) requested=(%.3f,%.3f,%.3f) airControl=%.3f.",
            m_entityId,m_physicsEpoch,m_lastSpawnLocation,phase,living.bFlying,living.bStuck,living.bSquashed,living.bOnStairs,
            living.groundHeight,living.pGroundCollider?1:0,living.vel.x,living.vel.y,living.vel.z,
            living.velRequested.x,living.velRequested.y,living.velRequested.z,hasDynamics && !is_unused(dynamics.kAirControl)?dynamics.kAirControl:-1.f);
    else WB_LOG(1,"[WarsBots] BODY LIVING actor=%u epoch=%u spawn=%u phase=%s supported=0.",m_entityId,m_physicsEpoch,m_lastSpawnLocation,phase);
}

void BotInstance::Update(float dt)
{
	const CTimeValue started = gEnv->pTimer->GetAsyncTime();
	m_brain.Update(*this, dt);
    CheckReviveGuard(gEnv->pTimer->GetFrameStartTime().GetSeconds());
    CActor* sampled=GetActor();
    if (!PhysicsDiagnostics() || !sampled || sampled->GetHealth()<=0) m_physicsProbe.Cancel();
    else
    {
        const float now=gEnv->pTimer->GetFrameStartTime().GetSeconds();
        const unsigned due=m_physicsProbe.Take(now);
        if (due&1u) TracePhysics("+1",m_physicsProbe.Elapsed(now));
        if (due&2u) TracePhysics("+5",m_physicsProbe.Elapsed(now));
    }
	if (Owns(m_entityId))
		if (IActor* actor = m_owner.Framework()->GetIActorSystem()->GetActor(m_entityId))
		{
			const int health = actor->GetHealth();
			if (m_lastHealth > 0 && health <= 0)
				WB_LOG(1, "[WarsBots] #%d death observed EntityId=%u health=%d lastHitShooter=%u KillPlayerObserved=%d.",
					m_number, m_entityId, health, m_lastHitShooter, m_killPlayerObserved ? 1 : 0);
			else if (m_lastHealth <= 0 && health > 0)
				WB_LOG(2, "[WarsBots] #%d alive transition EntityId=%u health=%d.", m_number, m_entityId, health);
			m_lastHealth = health;
		}
	m_lastUpdateMs = (gEnv->pTimer->GetAsyncTime() - started).GetMilliSeconds();
	m_totalUpdateMs += m_lastUpdateMs;
	m_maxUpdateMs = max(m_maxUpdateMs, m_lastUpdateMs);
	++m_updateCount;
}

bool BotInstance::Revive(EntityId forcedSpawn, float forcedZOffset)
{
    return DoRevive(forcedSpawn,forcedZOffset,false);
}

bool BotInstance::DoRevive(EntityId forcedSpawn,float forcedZOffset,bool guardRetry)
{
	if (!Owns(m_entityId) || !CanRunLocalIA())
	{
		WB_LOG(1, "[WarsBots] #%d revive refused: requires owned actor and open local IA gate.", m_number);
		return false;
	}
	CGameRules* rules = g_pGame->GetGameRules();
	CActor* actor = GetActor();
	if (!rules || !actor || (!guardRetry && actor->GetHealth()>0) || (guardRetry && actor->GetHealth()<=0) || actor->GetChannelId() != 0 || !actor->GetInventory() ||
		actor->GetLinkedVehicle() || rules->IsFrozen(m_entityId))
	{
		WB_LOG(1, "[WarsBots] #%d revive refused: owned actor needs correct life state, inventory, no freeze/vehicle.", m_number);
		return false;
	}
	const EntityId id = m_entityId;
	const EntityId chosen = forcedSpawn ? forcedSpawn : m_owner.ChooseSpawnLocation(id, m_lastSpawnLocation,guardRetry?&m_guardFailedSpawns:nullptr);
	if (!chosen)
	{
		WB_LOG(1, "[WarsBots] #%d revive deferred: no safe IA spawn; retry in 5 seconds.", m_number);
		return false; // Safer than reviving inside geometry or another player.
	}
    if (forcedSpawn) {
        bool registered=false;
        for(int i=0;i<rules->GetSpawnLocationCount();++i) registered |= rules->GetSpawnLocation(i)==forcedSpawn;
        if(!registered)return false;
    }
	IEntity* spawn = gEnv->pEntitySystem->GetEntity(chosen);
	if (!spawn) return false;
    if(!Owns(id))return false;
    actor=GetActor();if(!actor)return false;
    const float z=forcedSpawn?forcedZOffset:0.2f;
    const Vec3 position = spawn->GetWorldPos() + Vec3(0,0,z);
	const Ang3 angles(spawn->GetWorldRotation());
	WB_LOG(2, "[WarsBots] #%d revive requested EntityId=%u spawnId=%u previousSpawn=%u position=(%.3f,%.3f,%.3f).",
		m_number, id, chosen, m_lastSpawnLocation, position.x, position.y, position.z);
	rules->RevivePlayer(actor, position, angles, 0, !guardRetry);

	IActor* revived = m_owner.Framework()->GetIActorSystem()->GetActor(id);
	const bool alive = Owns(id) && revived && revived->GetHealth() > 0 &&
		revived->GetGameObject()->GetAspectProfile(eEA_Physics) == eAP_Alive;
	WB_LOG(1, "[WarsBots] #%d revive result EntityId=%u sameEntity=%d alive=%d health=%d.",
		m_number, id, Owns(id) ? 1 : 0, alive ? 1 : 0, revived ? revived->GetHealth() : -1);
	if (alive)
	{
        m_owner.Demo().Revive(id,chosen);
		if(!guardRetry)BotMatch::EquipLikePlayer(*rules, *revived);
		m_lastSpawnLocation = chosen;
		m_lastHealth = revived->GetHealth();
		m_lastHitShooter = 0;
		m_killPlayerObserved = false;
        BeginPhysicsProbe("revive");
        const int& guard=BotConfig::Get().reviveGuard;
        if(guard==1){
            if(!guardRetry)m_guardFailedSpawns.clear();
            m_guardOrigin=revived->GetEntity()->GetWorldPos();
            m_reviveGuard.Begin(gEnv->pTimer->GetFrameStartTime().GetSeconds(),!guardRetry);
        }else m_reviveGuard.Cancel();
	}
	return alive;
}
void BotInstance::CheckReviveGuard(float now)
{
    if(!m_reviveGuard.Pending())return;
    const int& flag=BotConfig::Get().reviveGuard;
    CActor* actor=GetActor();
    if(flag!=1 || !CanRunLocalIA() || !Owns(m_entityId) || !m_brain.Running() ||
        !actor || actor->GetHealth()<=0 || actor->GetLinkedVehicle() || g_pGame->GetGameRules()->IsFrozen(m_entityId))
    {m_reviveGuard.Cancel();return;}
    const float moved=(actor->GetEntity()->GetWorldPos()-m_guardOrigin).GetLength();
    pe_status_living living;bool bad=false;
    if(IPhysicalEntity* physics=actor->GetEntity()->GetPhysics())
        if(physics->GetStatus(&living))bad=living.bStuck || !std::isfinite(living.groundHeight) || living.groundHeight< -1e8f;
    const bool requesting=m_brain.RequestsMovement();
    const auto result=m_reviveGuard.Check(now,moved,requesting,bad);
    if(result!=BotReviveGuard::Result::Retry && result!=BotReviveGuard::Result::Exhausted)return;
    WB_LOG(0,"[WarsBots] REVIVE GUARD actor=%u spawn=%u moved=%.3f retry=%u requesting=%d badBody=%d exhausted=%d",m_entityId,m_lastSpawnLocation,moved,m_reviveGuard.Retries(),requesting?1:0,bad?1:0,result==BotReviveGuard::Result::Exhausted?1:0);
    if(result==BotReviveGuard::Result::Exhausted)return;
    m_guardFailedSpawns.push_back(m_lastSpawnLocation);
    m_brain.Stop(*this);
    const bool revived=DoRevive(0,.2f,true);
    if(Owns(m_entityId) && GetActor() && GetActor()->GetHealth()>0){
        m_brain.Start(*this);
        if(!revived)m_reviveGuard.Begin(now,false); // At most2 attempts even when no safe point is free.
    }else m_reviveGuard.Cancel();
    WB_LOG(0,"[WarsBots] REVIVE GUARD result actor=%u spawn=%u retry=%u revived=%d nativeKill=0",m_entityId,m_lastSpawnLocation,m_reviveGuard.Retries(),revived?1:0);
}

// Explicit diagnostic relocation, never called by automatic trap recovery.
bool BotInstance::DebugTeleport(const Vec3& position)
{
    CActor* actor=GetActor();
    if(!CanRunLocalIA() || !Owns(m_entityId) || !actor || actor->GetChannelId()!=0 ||
        actor->GetHealth()<=0 || actor->GetLinkedVehicle() || !position.IsValid() ||
        g_pGame->GetGameRules()->IsFrozen(m_entityId)) return false;
    const bool running=AIEnabled();
    if(running)m_brain.Stop(*this);
    const CActor::MoveParams params(position,actor->GetEntity()->GetWorldRotation());
    // MovePlayer addresses a real client channel; channel-zero bots need broadcast.
    actor->GetGameObject()->InvokeRMI(CActor::ClMoveTo(),params,eRMI_ToAllClients|eRMI_NoLocalCalls);
    actor->GetEntity()->SetWorldTM(Matrix34::Create(Vec3(1,1,1),params.rot,params.pos));
    WB_LOG(0,"[WarsBots] DEBUG TELEPORT actor=%u bot=%d position=(%.3f,%.3f,%.3f) manual=1",m_entityId,m_number,position.x,position.y,position.z);
    if(running)m_brain.Start(*this);
    return true;
}

}
