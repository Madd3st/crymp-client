// WarsBots: one autonomous bot. Owns its actor identity, brain and lifecycle state.
// No state here is shared between instances; the registry lives in BotManager.
#pragma once

#include "CryCommon/CryMath/Cry_Math.h"
#include "CryCommon/CryEntitySystem/IEntity.h"
#include "CryGame/Bots/BotBrain.h"
#include "CryGame/Bots/BotProfile.h"
#include "CryGame/Bots/BotPhysicsProbe.h"
#include "CryGame/Bots/BotReviveGuard.h"

struct HitInfo;
class CActor;

namespace WarsBots
{
class BotManager;

class BotInstance final
{
public:
	BotInstance(BotManager& owner, int number, const BotProfile& profile);
	BotInstance(const BotInstance&) = delete;
	BotInstance& operator=(const BotInstance&) = delete;

	// Creates and registers the actor. False leaves no entity behind.
	bool Spawn(const Vec3& position);
	// Removes only the entity this instance created. Safe to call repeatedly.
	void Remove();
	bool Revive(EntityId forcedSpawn = 0, float forcedZOffset = 0.2f);
	bool DebugTeleport(const Vec3& position);
	void Update(float dt);
	// Entity system reported ENTITY_EVENT_DONE for our entity: forget it, never touch it again.
	void OnEntityDone();

	bool Owns(EntityId id) const;
	CActor* GetActor() const;
	bool CanRunLocalIA() const;
	bool AIEnabled() const { return m_brain.Running(); }
	bool Expired() const { return m_entityId == 0; }
	// True only while our own Remove() is deleting the entity.
	bool Removing() const { return m_removing; }
	bool RemovalPending() const { return m_removeRequested && m_entityId != 0; }   // DS-024 #3
	EntityId GetEntityId() const { return m_entityId; }
	int Number() const { return m_number; }
	const char* Name() const { return m_name; }
	const BotProfile& Profile() const { return m_profile; }
	BotBrain& Brain() { return m_brain; }
	const BotBrain& Brain() const { return m_brain; }

	bool IsOwnedProjectileHit(const HitInfo& hit) const;
	void TraceHit(const char* phase, const HitInfo& hit);
	void TraceKill(EntityId shooterId, bool killerEventEmitted);
	void Status() const;

	// Profiling: wall time spent in this instance's Update.
	float LastUpdateMs() const { return m_lastUpdateMs; }
	float MaxUpdateMs() const { return m_maxUpdateMs; }
	float TotalUpdateMs() const { return m_totalUpdateMs; }
	unsigned UpdateCount() const { return m_updateCount; }
	void ResetProfile() { m_maxUpdateMs = m_totalUpdateMs = 0; m_updateCount = 0; }

private:
    bool DoRevive(EntityId forcedSpawn,float forcedZOffset,bool guardRetry);
    void CheckReviveGuard(float now);
    BotReviveGuard m_reviveGuard;
    Vec3 m_guardOrigin=Vec3(0,0,0);
    std::vector<EntityId> m_guardFailedSpawns;
    bool PhysicsDiagnostics() const;
    void BeginPhysicsProbe(const char* origin);
    void TracePhysics(const char* phase, float elapsed) const;
    BotPhysicsProbe m_physicsProbe;
    const char* m_physicsOrigin="none";
    unsigned m_physicsEpoch=0;
	BotManager& m_owner;
	const int m_number;
	char m_name[32] = {};
	BotProfile m_profile;
	EntityId m_entityId = 0;
	// Identity guard only: never dereference this cached pointer.
	const IEntity* m_identity = nullptr;
	Vec3 m_spawnPosition = Vec3(0,0,0);
	int m_lastHealth = 0;
	int m_beforeHitHealth = 0;
	EntityId m_lastHitShooter = 0;
	EntityId m_lastSpawnLocation = 0;
	bool m_killPlayerObserved = false;
	bool m_removing = false;
	bool m_removeRequested = false;
	float m_lastUpdateMs = 0, m_maxUpdateMs = 0, m_totalUpdateMs = 0;
	unsigned m_updateCount = 0;
	BotBrain m_brain;
};
}
