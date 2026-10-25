// WarsBots local movement prototype. See LICENSE.txt.
#pragma once
#include "CryCommon/CryMath/Cry_Math.h"
#include "CryCommon/CryAISystem/IAgent.h"
class CActor;
namespace WarsBots
{
class BotMovement
{
public:
	void Reset();
	void Stop(CActor& actor);
	// Returns whether a safe local movement request was accepted, not arrival.
	bool Move(CActor& actor, const Vec3& destination, const Vec3& lookAt, float dt, float speed=2.5f, EStance stance=STANCE_STAND, float probe=1.5f, bool jumpTraverse=false, bool strafe=false, float pseudo=-1.f, bool meshGuided=false);
	void Aim(CActor& actor, const Vec3& target);
	// Look-only request (view/aim/fire targets); move target, speed and body target stay as requested last.
	void Look(CActor& actor, const Vec3& target);
private:
	bool SafeStep(CActor& actor, const Vec3& direction, float probe, bool meshGuided=false) const;
	Vec3 m_sample = Vec3(0,0,0);
	float m_sampleTime = 0;
	float m_avoidTime = 0;
	float m_side = 1;
	bool m_turned = false;
public:
	// Last Move had to turn more than 90 degrees away from the destination.
	bool Turned() const { return m_turned; }
};
}
