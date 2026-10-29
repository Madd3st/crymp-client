#pragma once
#include "CryGame/Bots/BotMovement.h"
#include "CryGame/Bots/BotBlackboard.h"
class CPlayer;
namespace WarsBots
{
// Ability executor; BotMovement retains local steering and obstacle avoidance.
class BotLocomotion
{
public:
	void Reset();
	void Stop(CPlayer& actor);
	void Observe(CPlayer& actor, BotBlackboard& state, float now);
	BotJump Obstacle(CPlayer& actor, const Vec3& direction) const;
	bool Execute(CPlayer& actor, const BotIntent& intent, BotBlackboard& state, float now, float dt);
	// DS-107: only while the native ladder is actually attached may SafeStep be bypassed; approach/settle use the
	// normal safe executor. Keep the real movement request so ProcessMovementOnLadder sees desiredVelocity.y.
	bool ExecuteLadder(CPlayer& actor, const Vec3& moveTo, const Vec3& aim, float dt, bool nativeAttached);
	void Look(CPlayer& actor, const Vec3& target);
private:
	bool JumpSpace(CPlayer& actor, const Vec3& direction, bool moving, bool strength) const;
	void Refused(CPlayer& actor, const char* reason, float now);
	BotMovement m_movement;
	bool m_pending = false, m_airborne = false, m_strength = false, m_sprinting=false;
	float m_requested = 0, m_startZ = 0, m_apex = 0, m_nextLog = 0;
};
}
