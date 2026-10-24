#pragma once
#include "CryGame/Bots/BotLocomotion.h"
#include "CryGame/Bots/BotSuit.h"
#include "CryGame/Bots/BotCombat.h"
#include "CryGame/Bots/BotProfile.h"
#include <vector>
#include "CryGame/Bots/BotProgress.h"
#include "CryGame/Bots/BotTrapWatchdog.h"
#include "CryGame/Bots/BotLookPolicy.h"
#include "CryGame/Bots/BotGoalArbiter.h"
struct IConsoleCmdArgs;
class CPlayer;
struct IActor;
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
    bool RequestsMovement() const { return m_running && m_intent.move; }
	void SetVerbose(bool enabled) { m_verbose = enabled; }
	const char* StateName() const;
	EntityId Target() const { return m_combat.Target(); }
    // Read-only diagnostic snapshot, consumed after this instance's decision tick.
    unsigned DecisionSerial()const{return m_decisionSerial;}
    const BotIntent& Intent()const{return m_intent;}
    const BotCombat& Combat()const{return m_combat;}
    Vec3 NextCorner()const{return m_pathIndex<m_path.size()?m_path[m_pathIndex]:m_intent.destination;}
    const char* DecisionReason()const{return m_now<m_testUntil?"manual_ability":m_now<m_trapStepUntil?"trap_recovery":m_now<m_escapeUntil?"escape":m_now<m_withdrawUntil?"withdraw":StateName();}

private:
	void Change(State state,bool initialize=false);
    void UpdateTelemetry(CPlayer& actor);
    bool UpdateDead(BotInstance& bot,CPlayer& actor,float dt);
    bool UpdateTest(CPlayer& actor,float decisionTime);
    bool UpdateWeaponSeek(CPlayer& actor,float decisionTime,bool socomQA);
    bool UpdateUpgrade(BotInstance& bot,CPlayer& actor,IActor* enemy,float decisionTime,bool socomQA,bool force=false);
    void UpdateRoam(BotInstance& bot,CPlayer& actor,float decisionTime);
    void UpdateCombat(BotInstance& bot,CPlayer& actor,float decisionTime);
    void UpdateAttacking(CPlayer& actor,const Vec3& destination,const Vec3& aim,float distance,float desiredRange);
    void UpdateChasing(CPlayer& actor,const Vec3& destination,const Vec3& aim,float distance,bool visible);
    void UpdateCombatPost(BotInstance& bot,CPlayer& actor,float decisionTime);

	void ResetLife();
	void ExecuteIntent(CPlayer& actor, float dt);
	// Sprint C-047 (all default OFF): C-033 look policy, C-044 turn cap, C-032 goal arbiter, C-022 weapon rank.
	void ApplyLookPolicy(CPlayer& actor);
	void UpdateTurnCap(CPlayer& actor, float dt);
	Vec3 CappedLook(CPlayer& actor) const;
	float TurnRate() const;
	bool EyeRayClear(CPlayer& actor, const Vec3& from, const Vec3& to) const;
	bool UpdateArbitrated(BotInstance& bot, CPlayer& actor, IActor* enemy, float decisionTime, bool socomQA);
	EntityId UpgradeCandidate(CPlayer& actor, bool anyHeld);
	BotLookPolicy m_look;
	Vec3 m_lookDir = Vec3(0,1,0);
	bool m_lookValid = false;
	GoalArbiter m_arbiter;
	float m_nextRangeSelect = 0;
	bool NoProgress(CPlayer& actor,const char* kind,EntityId goal,bool protect=false);
	BotProgress m_progress;
    bool RecoverTrap(CPlayer& actor);
    BotTrapWatchdog m_trap;
    Vec3 m_trapDirection=Vec3(0,1,0);
    float m_trapStepUntil=0;
    bool m_trapJumpAttempted=false;
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
    unsigned m_decisionSerial=0;
    EntityId m_actorId=0;
	float m_phase = 0;
	BotLocomotion m_locomotion;
	BotCombat m_combat;
};
}
