// WarsBots: per-bot lifecycle. Uses the existing actor network binding; no fabricated channel.
#include "PortIncludes.h"
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

namespace WarsBots
{
BotInstance::BotInstance(BotManager& owner, int number, const BotProfile& profile)
	: m_owner(owner), m_number(number), m_profile(profile)
{
	_snprintf(m_name, sizeof(m_name) - 1, "WarsBot_%02d", number);
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
	return true;
}

void BotInstance::Remove()
{
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

void BotInstance::Update(float dt)
{
	const CTimeValue started = gEnv->pTimer->GetAsyncTime();
	m_brain.Update(*this, dt);
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

bool BotInstance::Revive()
{
	if (!Owns(m_entityId) || !CanRunLocalIA())
	{
		WB_LOG(1, "[WarsBots] #%d revive refused: requires owned actor and open local IA gate.", m_number);
		return false;
	}
	CGameRules* rules = g_pGame->GetGameRules();
	CActor* actor = GetActor();
	if (!rules || !actor || actor->GetHealth() > 0 || actor->GetChannelId() != 0 || !actor->GetInventory() ||
		actor->GetLinkedVehicle() || rules->IsFrozen(m_entityId))
	{
		WB_LOG(1, "[WarsBots] #%d revive refused: actor must be a dead, unfrozen Player with inventory and no vehicle.", m_number);
		return false;
	}
	const EntityId id = m_entityId;
	const EntityId chosen = m_owner.ChooseSpawnLocation(id, m_lastSpawnLocation);
	if (!chosen)
	{
		WB_LOG(1, "[WarsBots] #%d revive deferred: no safe IA spawn; retry in 5 seconds.", m_number);
		return false; // Safer than reviving inside geometry or another player.
	}
	IEntity* spawn = gEnv->pEntitySystem->GetEntity(chosen);
	if (!spawn) return false;
	const Vec3 position = spawn->GetWorldPos() + Vec3(0,0,0.2f);
	const Ang3 angles(spawn->GetWorldRotation());
	WB_LOG(2, "[WarsBots] #%d revive requested EntityId=%u spawnId=%u previousSpawn=%u position=(%.3f,%.3f,%.3f).",
		m_number, id, chosen, m_lastSpawnLocation, position.x, position.y, position.z);
	rules->RevivePlayer(actor, position, angles, 0, true);

	IActor* revived = m_owner.Framework()->GetIActorSystem()->GetActor(id);
	const bool alive = Owns(id) && revived && revived->GetHealth() > 0 &&
		revived->GetGameObject()->GetAspectProfile(eEA_Physics) == eAP_Alive;
	WB_LOG(1, "[WarsBots] #%d revive result EntityId=%u sameEntity=%d alive=%d health=%d.",
		m_number, id, Owns(id) ? 1 : 0, alive ? 1 : 0, revived ? revived->GetHealth() : -1);
	if (alive)
	{
		BotMatch::EquipLikePlayer(*rules, *revived);
		m_lastSpawnLocation = chosen;
		m_lastHealth = revived->GetHealth();
		m_lastHitShooter = 0;
		m_killPlayerObserved = false;
	}
	return alive;
}
}
