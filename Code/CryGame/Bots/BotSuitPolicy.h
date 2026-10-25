// Nanosuit policy (roadmap step 12 "one suit policy", D-010 Claude module). Today suit modes are set in ~25 places of
// BotBrain (travel gait, chase, withdraw, dodge, falling, combat override, pace, cloak experiment), and the last writer
// wins. This module is ONE decision per tick with priorities, energy budgets and hysteresis; BotBrain fills the input
// and passes the result to BotSuit::Apply (which still enforces the native 2 s switch cooldown).
// Energy is the native 0..200 scale. Pure logic, no engine types; Tests/BotSuitPolicyTests.cpp.
#pragma once

namespace WarsBots {

enum class SuitMode { Armor = 0, Speed, Strength, Cloak };
enum class SuitReason { Default = 0, Falling, UnderFire, StrengthJump, Travel, StealthApproach, Fight, Hold };

struct SuitInput {
	float now = 0.f;
	float energy = 200.f;           // 0..200
	int health = 100;
	bool airborne = false;
	float verticalSpeed = 0.f;      // m/s, negative = falling
	bool damagedRecently = false;   // hit within ~0.5 s
	bool enemyVisible = false;
	float enemyDistance = 1e9f;     // m, current/remembered target
	bool enemyKnownHidden = false;  // a remembered target that is not visible now
	bool wantStrengthJump = false;  // a jump link / dodge asks for a Strength jump this tick
	float travelDistance = 0.f;     // m to the current goal while moving
	bool moving = false;
	bool firing = false;            // the bot fires this tick (cloak breaks, Speed burns energy for nothing)
	bool stealthStyle = false;      // Flanker / Hunter profiles
	bool cloakAllowed = false;      // wb_bot_cloak_experimental
	bool canUseSpeed = true;        // beginner gate (BotProfile::useSpeed)
};

struct SuitDecision {
	SuitMode mode = SuitMode::Armor;
	SuitReason reason = SuitReason::Default;
	bool changed = false;
};

struct SuitParams {
	float fallArmor = -6.f;         // m/s: Armor before landing below this vertical speed
	float strengthJumpEnergy = 70.f;
	float speedEnter = 130.f, speedExit = 60.f;   // energy hysteresis (BotPace C018 numbers)
	float speedMinTravel = 20.f, speedKeepTravel = 8.f;
	float cloakEnter = 120.f, cloakExit = 40.f;
	float cloakMin = 12.f, cloakMax = 35.f;       // m to the hidden enemy for a stealth approach
	float underFireArmorEnergy = 20.f;            // below this Armor gives nothing extra, but it is still the safest
	float minHold = 1.5f;                         // s before a non-urgent change
};

class BotSuitPolicy {
public:
	explicit BotSuitPolicy(const SuitParams& p = SuitParams()) : m_p(p) {}
	void Reset() { m_mode = SuitMode::Armor; m_since = -1e9f; }
	SuitMode Current() const { return m_mode; }

	SuitDecision Update(const SuitInput& in)
	{
		SuitMode want = SuitMode::Armor; SuitReason why = SuitReason::Default; bool urgent = false;
		if (in.airborne && in.verticalSpeed < m_p.fallArmor) { want = SuitMode::Armor; why = SuitReason::Falling; urgent = true; }
		else if (in.wantStrengthJump && in.energy >= m_p.strengthJumpEnergy && !in.airborne)
		{ want = SuitMode::Strength; why = SuitReason::StrengthJump; urgent = true; }
		else if (in.damagedRecently) { want = SuitMode::Armor; why = SuitReason::UnderFire; urgent = true; }
		else if (in.enemyVisible || in.firing) { want = SuitMode::Armor; why = SuitReason::Fight; urgent = m_mode != SuitMode::Armor; }
		else if (StealthOk(in)) { want = SuitMode::Cloak; why = SuitReason::StealthApproach; }
		else if (SpeedOk(in)) { want = SuitMode::Speed; why = SuitReason::Travel; }

		SuitDecision d;
		if (want != m_mode && !urgent && in.now - m_since < m_p.minHold) { d.mode = m_mode; d.reason = SuitReason::Hold; return d; }
		d.changed = want != m_mode;
		if (d.changed) { m_mode = want; m_since = in.now; }
		else if (why == SuitReason::Fight || why == SuitReason::UnderFire) m_since = in.now; // fight keeps Armor fresh
		d.mode = m_mode; d.reason = why;
		return d;
	}

private:
	bool SpeedOk(const SuitInput& in) const
	{
		if (!in.canUseSpeed || !in.moving) return false;
		if (m_mode == SuitMode::Speed) return in.energy > m_p.speedExit && in.travelDistance > m_p.speedKeepTravel;
		return in.energy >= m_p.speedEnter && in.travelDistance >= m_p.speedMinTravel;
	}
	bool StealthOk(const SuitInput& in) const
	{
		if (!in.cloakAllowed || !in.stealthStyle || !in.enemyKnownHidden || in.firing) return false;
		if (in.enemyDistance < m_p.cloakMin || in.enemyDistance > m_p.cloakMax) return false;
		return m_mode == SuitMode::Cloak ? in.energy > m_p.cloakExit : in.energy >= m_p.cloakEnter;
	}

	SuitParams m_p;
	SuitMode m_mode = SuitMode::Armor;
	float m_since = -1e9f;
};

} // namespace WarsBots
