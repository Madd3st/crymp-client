#pragma once
#include "CryGame/Bots/BotLocomotion.h"
#include "CryGame/Bots/BotSuit.h"
#include "CryGame/Bots/BotCombat.h"
#include "CryGame/Bots/BotProfile.h"
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
	void AbilityCommand(BotInstance& bot, IConsoleCmdArgs* args);
	bool Running() const { return m_running; }
	void SetVerbose(bool enabled) { m_verbose = enabled; }
	const char* StateName() const;
	EntityId Target() const { return m_combat.Target(); }
private:
	void Change(State state);
	void ResetLife();
	void ExecuteIntent(CPlayer& actor, float dt);
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
	EntityId m_weaponTarget = 0;
	Vec3 m_patrolGoal = Vec3(0,0,0);
	float m_patrolUntil = 0;
	unsigned m_life = 0, m_patrolStep = 0;
	IEntityClass* m_previousWeaponClass = nullptr;
	BotProfile m_profile;
	int m_number = 0;
	float m_phase = 0;
	BotLocomotion m_locomotion;
	BotCombat m_combat;
};
}
