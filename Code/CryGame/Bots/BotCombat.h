#pragma once
#include "CryCommon/CryMath/Cry_Math.h"
#include "CryCommon/CryEntitySystem/IEntity.h"
#include "CryGame/Bots/BotProfile.h"
class CActor;
struct IActor;
namespace WarsBots
{
class BotCombat
{
public:
	void Configure(const BotProfile& profile) { m_burst = profile.burst; m_burstGap = profile.burstGap; }
	void Reset();
	void StopFire();
	IActor* FindEnemy(CActor& actor, float now, bool verbose = false);
	static Vec3 AimPoint(IActor& target);
	static bool CanSee(CActor& actor, IActor& target);
	bool Attack(CActor& actor, IActor& target, float now, bool verbose);
	EntityId Target() const { return m_target; }
	bool Visible() const { return m_visible; }
	const Vec3& LastPosition() const { return m_lastPosition; }
	const Vec3& LastAim() const { return m_lastAim; }
	float SampleTime() const { return m_sampleTime; }
	// Last Attack had LOS from the eyes but the barrel was blocked by cover.
	bool ShotBlocked() const { return m_shotBlocked; }
	// Profiling counters across all bots (main thread only), reset on read.
	static unsigned TakeRayCount();
	static unsigned TakeScanCount();
private:
	bool Observe(CActor& actor, IActor& target, float now, bool verbose);
	bool m_visible = false;
	bool m_shotBlocked = false;
	Vec3 m_lastPosition = Vec3(0,0,0), m_lastAim = Vec3(0,0,0);
	float m_sampleTime = 0, m_nextDiagnostic = 0;
	EntityId m_target = 0;
	EntityId m_firingWeapon = 0;
	float m_lastSeen = 0;
	float m_nextBurst = 0;
	float m_burstEnd = 0;
	float m_burst = 0.2f, m_burstGap = 0.65f;
};
}
