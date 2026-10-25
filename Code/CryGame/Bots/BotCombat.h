#pragma once
#include "CryCommon/CryMath/Cry_Math.h"
#include "CryCommon/CryEntitySystem/IEntity.h"
#include "CryGame/Bots/BotProfile.h"
#include "CryGame/Bots/BotPerception.h"
class CActor;
struct IActor;
namespace WarsBots
{
class BotCombat
{
public:
	void Configure(const BotProfile& profile) { m_profile = profile; m_burst = profile.burst; m_burstGap = profile.burstGap; m_perception.SetReactionTime(profile.reaction); m_perception.SetSenses(profile.coneDeg,profile.sightRange,profile.hearScale); }
	void Reset();
	void StopFire();
	void NotifyDamage(EntityId shooter, float now) { m_retaliator=shooter; m_retaliatorUntil=now+3.0f; }
	void AvoidCurrent(float now, float seconds)
	{ StopFire(); m_avoid=m_target; m_avoidUntil=now+seconds; m_target=0; m_visible=false; }
	float LastBurstRequest() const { return m_lastBurstRequest; }
	float SelectedAt() const { return m_selectedAt; }
	int VisibleEnemies() const { return m_visibleEnemies; }
	IActor* FindEnemy(CActor& actor, float now, bool verbose = false);
	static Vec3 AimPoint(IActor& target);
	static bool CanSee(CActor& actor, IActor& target);
	bool Attack(CActor& actor, IActor& target, float now, bool verbose);
	EntityId Target() const { return m_target; }
	bool Visible() const { return m_visible; }
    EntityId LastAttacker()const{return m_retaliator;}
    bool DamageMemory(float now)const{return m_retaliator!=0 && now<m_retaliatorUntil;}
	const Vec3& LastPosition() const { return m_lastPosition; }
	const Vec3& LastAim() const { return m_lastAim; }
	float SampleTime() const { return m_sampleTime; }
	float LastSeen() const { return m_lastSeen; } // last time the target was visible (0 = never)
	// C-037 hearing (wb_bot_perception=1): closest enemy heard (gunshot <= 60 m, Speed sprint <= 12 m) but not seen.
	bool Heard(float now, float maxAge, Vec3& pos) const
	{ if (m_heardAt>0 && now-m_heardAt<=maxAge) { pos=m_heardPos; return true; } return false; }
	// Last Attack had LOS from the eyes but the barrel was blocked by cover.
	bool ShotBlocked() const { return m_shotBlocked; }
	// Profiling counters across all bots (main thread only), reset on read.
	static unsigned TakeRayCount();
	static unsigned TakeScanCount();
private:
	BotProfile m_profile;
	BotPerception m_perception; // C-037, used only with wb_bot_perception=1
    bool m_preferHead=false;
    float m_nextAimChoice=0;
	EntityId m_retaliator=0, m_avoid=0;
	float m_avoidUntil=0, m_lastBurstRequest=-100.0f;
	float m_retaliatorUntil=0, m_selectedAt=0, m_nextScan=0;
	int m_visibleEnemies=0;
	bool Observe(CActor& actor, IActor& target, float now, bool verbose);
	bool m_visible = false;
	bool m_shotBlocked = false;
	Vec3 m_lastPosition = Vec3(0,0,0), m_lastAim = Vec3(0,0,0);
	float m_sampleTime = 0, m_nextDiagnostic = 0;
	EntityId m_target = 0;
	EntityId m_firingWeapon = 0;
	float m_lastSeen = 0;
	Vec3 m_heardPos = Vec3(0,0,0);
	float m_heardAt = 0;
	// wb_bot_finish (DS-015): damage this bot saw itself deal to its target (health drop while visible and firing)
	EntityId m_dmgTarget = 0; float m_dmgDealt = 0, m_dmgLastHp = 0, m_dmgAt = -100.0f;
	float m_nextBurst = 0;
	float m_burstEnd = 0;
	float m_burst = 0.2f, m_burstGap = 0.65f;
};
}
