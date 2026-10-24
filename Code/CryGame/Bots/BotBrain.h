#pragma once
#include "CryGame/Bots/BotLocomotion.h"
#include "CryGame/Bots/BotSuit.h"
#include "CryGame/Bots/BotCombat.h"
#include "CryGame/Bots/BotProfile.h"
#include <vector>
#include "CryGame/Bots/BotProgress.h"
#include "CryGame/Bots/BotTrapWatchdog.h"
struct IConsoleCmdArgs;
class CPlayer;
namespace WarsBots
{
class BotInstance;
class BotBrain
{
public:
	enum class State { SPAWNING, SEEKING_WEAPON, MOVING_TO_WEAPON, SEARCHING_ENEMY,
		CHASING_ENEMY, ATTACKING, DEAD, RESPAWNING };
	// Per-instance tuning and de-synchronisation phase; call once before Start.
	void Configure(const BotProfile& profile, int number);
	// Requires an existing owned actor; BotManager creates actors.
	bool Start(BotInstance& bot);
	void Stop(BotInstance& bot);
	void Update(BotInstance& bot, float dt);
	void Status() const;
	void NotifyDamage(EntityId shooter) { m_combat.NotifyDamage(shooter,m_now); }
	void AbilityCommand(BotInstance& bot, IConsoleCmdArgs* args);
	bool Running() const { return m_running; }
	void SetVerbose(bool enabled) { m_verbose = enabled; }
	const char* StateName() const;
	EntityId Target() const { return m_combat.Target(); }
private:
	void Change(State state);
	void ResetLife();
	void ExecuteIntent(CPlayer& actor, float dt);
	bool NoProgress(CPlayer& actor,const char* kind,EntityId goal,bool protect=false);
	BotProgress m_progress;
    bool RecoverTrap(CPlayer& actor);
    BotTrapWatchdog m_trap;
    Vec3 m_trapDirection=Vec3(0,1,0);
    float m_trapStepUntil=0;
    bool m_trapJumpAttempted=false;
    bool m_trapComponentChecked=false,m_trapClosed=false;
	float m_escapeUntil=0;
	BotBlackboard m_blackboard;
	BotIntent m_intent;
	BotSuit m_suit;
	BotJump m_testJump = BotJump::None;
	int m_testGait = -1, m_testMode = -1;
	float m_testUntil = 0, m_testModeUntil = 0, m_testJumpAt = 0;
	Vec3 m_testGoal = Vec3(0,0,0), m_testLook = Vec3(0,0,0);
	int m_testStrafe = 0, m_strafeSide = 1;
	float m_nextStrafe = 0;
	bool m_running = false;
	bool m_verbose = false;
	bool m_fallbackGiven = false;
	bool m_retreating = false;
	State m_state = State::SPAWNING;
	float m_now = 0, m_tick = 0, m_stateTime = 0, m_weaponSearchStarted = 0, m_nextSearch = 0, m_deadTime = 0;
	float m_nextTelemetry = 0;
    Vec3 m_trackedAim=Vec3(0,0,0);
    EntityId m_trackedTarget=0;
    float m_trackedSelection=-1;
	EntityId m_weaponTarget = 0;
	Vec3 m_patrolGoal = Vec3(0,0,0);
	float m_patrolUntil = 0, m_blockedSince = 0, m_chaseBlockedSince = 0, m_shotBlockedSince = 0;
	// Path following on the shared BotNav graph.
	Vec3 Steer(const Vec3& feet, const Vec3& goal);
	std::vector<Vec3> m_path;
	size_t m_pathIndex = 0;
	Vec3 m_pathGoal = Vec3(0,0,0);
	float m_nextRepath = 0, m_minRepath = 0;
	// Movement abilities and weapon upgrades.
	void TravelGait(float distance);
	float Rand01();
	unsigned m_rng = 1;
	bool m_speedTravel = false;
	float m_withdrawUntil=0, m_nextWithdraw=0;
	float m_nextTravelJump = 0, m_nextDodge = 0, m_lastHitSeen = 0, m_nextUpgradeSearch = 0;
	EntityId m_upgradeTarget = 0;
	float m_upgradeSince = 0;
	// Items that could not be reached/picked recently: skipped for a while.
	struct BadItem { EntityId id; float until; };
	BadItem m_badItems[8] = {};
	bool IsBadItem(EntityId id) const;
	void MarkBadItem(EntityId id);
	bool m_stuckReported = false;
	bool m_detour = false;
	unsigned m_life = 0, m_patrolStep = 0;
	IEntityClass* m_previousWeaponClass = nullptr;
	BotProfile m_profile;
	int m_number = 0;
    EntityId m_actorId=0;
	float m_phase = 0;
	BotLocomotion m_locomotion;
	BotCombat m_combat;
};
}
