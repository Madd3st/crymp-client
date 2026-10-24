// Human-like aim model (D-008 roadmap step 11, D-010 Claude module). Replaces the combat aim "low-pass filter + fixed
// 2.7/1.9/1.3 Hz sines" (BotAimControl.h:12-15, BotBrain.cpp:457-458 @ 5ce78d8) that the owner saw as aim wandering
// "here and there" and the external audit called "an ellipse, not a human":
//   * saccade (flick) to a new target or after a large error: duration from a Fitts-like law, ends with an
//     under/overshoot proportional to the jump - skill sets speed and precision;
//   * tracking: first-order lag toward the target plus partial lead from the target's angular velocity;
//   * noise: Ornstein-Uhlenbeck (filtered white) jitter, no fixed frequencies; amplitude grows with distance to
//     the target in angle terms only through the target size, never "random sine circles";
//   * a turn-rate cap per skill.
// Angles in degrees, yaw wrapped to [-180, 180). Deterministic RNG per bot (seed) for tests/demos.
// Pure logic, no engine types; Tests/BotAimModelTests.cpp.
#pragma once
#include <cmath>
#include <cstdint>

namespace WarsBots {

struct AimSkill {
	float saccadeBase = 0.12f;     // s, fixed part of a flick (elite 0.08, veteran 0.10, normal 0.12, beginner 0.18)
	float saccadeSlope = 0.045f;   // s per log2(1 + error / targetDeg) (elite 0.03, veteran 0.038, beginner 0.06)
	float flickError = 0.10f;      // end error as a fraction of the flick size (elite 0.05, beginner 0.20)
	float trackLag = 0.12f;        // s, tracking time constant (elite 0.07, beginner 0.20)
	float leadFraction = 0.6f;     // share of the target's angular velocity compensated (elite 0.85, beginner 0.3)
	float noiseDeg = 0.6f;         // OU jitter sigma, degrees (elite 0.3, beginner 1.2)
	float noiseTau = 0.25f;        // s, OU correlation time
	float maxTurn = 720.f;         // deg/s cap (elite 900, beginner 400)
	float flickThreshold = 8.f;    // deg: errors above this start a new saccade
};

inline AimSkill AimSkillPreset(int skill)   // 0 beginner, 1 normal, 2 veteran, 3 elite
{
	AimSkill s;
	switch (skill) {
	case 0: s.saccadeBase = 0.18f; s.saccadeSlope = 0.06f; s.flickError = 0.20f; s.trackLag = 0.20f; s.leadFraction = 0.30f; s.noiseDeg = 1.2f; s.maxTurn = 400.f; break;
	case 2: s.saccadeBase = 0.10f; s.saccadeSlope = 0.038f; s.flickError = 0.07f; s.trackLag = 0.09f; s.leadFraction = 0.75f; s.noiseDeg = 0.45f; s.maxTurn = 800.f; break;
	case 3: s.saccadeBase = 0.08f; s.saccadeSlope = 0.03f; s.flickError = 0.05f; s.trackLag = 0.07f; s.leadFraction = 0.85f; s.noiseDeg = 0.30f; s.maxTurn = 900.f; break;
	default: break;
	}
	return s;
}

struct AimInput {
	float dt = 0.05f;
	bool hasTarget = false;
	int targetId = 0;
	float targetYaw = 0.f, targetPitch = 0.f;        // angles to the aim point
	float targetYawRate = 0.f, targetPitchRate = 0.f; // deg/s, from successive observations
	float targetSizeDeg = 2.f;                        // angular size of the body at this distance (~atan(0.5/d))
};

struct AimOutput {
	float yaw = 0.f, pitch = 0.f;
	bool inSaccade = false;
	bool onTarget = false;          // within the target's angular size (fire gate)
	float errorDeg = 0.f;
};

class BotAimModel {
public:
	explicit BotAimModel(const AimSkill& s = AimSkill(), uint32_t seed = 1u) : m_s(s), m_rng(seed ? seed : 1u) {}
	void SetSkill(const AimSkill& s) { m_s = s; }
	void SetView(float yaw, float pitch) { m_yaw = Wrap(yaw); m_pitch = pitch; }

	AimOutput Update(const AimInput& in)
	{
		AimOutput o;
		const float dt = in.dt > 1e-4f ? in.dt : 1e-4f;
		// OU noise state always evolves (also without a target: a resting hand is not perfectly still)
		const float a = std::exp(-dt / m_s.noiseTau), b = m_s.noiseDeg * std::sqrt(1.f - a * a);
		m_nYaw = a * m_nYaw + b * Gauss(); m_nPitch = a * m_nPitch + b * Gauss();
		if (!in.hasTarget) { m_target = 0; m_sacT = -1.f; o.yaw = m_yaw; o.pitch = m_pitch; return o; }

		const float ey = Wrap(in.targetYaw - m_yaw), ep = in.targetPitch - m_pitch;
		const float err = std::sqrt(ey * ey + ep * ep);
		if (in.targetId != m_target || (m_sacT < 0.f && err > m_s.flickThreshold))
		{
			// new saccade: fixed duration, aimed at the predicted position with an end error
			m_target = in.targetId;
			m_sacDur = m_s.saccadeBase + m_s.saccadeSlope * std::log2(1.f + err / (in.targetSizeDeg > 0.2f ? in.targetSizeDeg : 0.2f));
			m_sacT = 0.f;
			m_sacFromYaw = m_yaw; m_sacFromPitch = m_pitch;
			const float endErr = m_s.flickError * err;
			m_sacOffYaw = endErr * Gauss() * 0.7f; m_sacOffPitch = endErr * Gauss() * 0.4f;
		}
		float wantYaw, wantPitch;
		if (m_sacT >= 0.f)
		{
			m_sacT += dt;
			const float u = m_sacT / m_sacDur;
			const float s = u >= 1.f ? 1.f : u * u * (3.f - 2.f * u);            // smooth bell-shaped velocity
			const float endYaw = in.targetYaw + in.targetYawRate * (m_sacDur - m_sacT) * m_s.leadFraction + m_sacOffYaw;
			const float endPitch = in.targetPitch + in.targetPitchRate * (m_sacDur - m_sacT) * m_s.leadFraction + m_sacOffPitch;
			wantYaw = m_sacFromYaw + Wrap(endYaw - m_sacFromYaw) * s;
			wantPitch = m_sacFromPitch + (endPitch - m_sacFromPitch) * s;
			o.inSaccade = u < 1.f;
			if (u >= 1.f) m_sacT = -1.f;
		}
		else
		{
			// tracking: lag toward target + lead, plus OU jitter
			const float k = 1.f - std::exp(-dt / m_s.trackLag);
			const float leadYaw = in.targetYawRate * m_s.trackLag * m_s.leadFraction;
			const float leadPitch = in.targetPitchRate * m_s.trackLag * m_s.leadFraction;
			wantYaw = m_yaw + Wrap(in.targetYaw + leadYaw - m_yaw) * k + (m_nYaw - m_pYaw);
			wantPitch = m_pitch + (in.targetPitch + leadPitch - m_pitch) * k + (m_nPitch - m_pPitch);
		}
		m_pYaw = m_nYaw; m_pPitch = m_nPitch;
		// turn-rate cap
		float dy = Wrap(wantYaw - m_yaw), dp = wantPitch - m_pitch;
		const float step = std::sqrt(dy * dy + dp * dp), cap = m_s.maxTurn * dt;
		if (step > cap) { dy *= cap / step; dp *= cap / step; }
		m_yaw = Wrap(m_yaw + dy); m_pitch += dp;
		if (m_pitch > 89.f) m_pitch = 89.f;
		if (m_pitch < -89.f) m_pitch = -89.f;
		const float ry = Wrap(in.targetYaw - m_yaw), rp = in.targetPitch - m_pitch;
		o.errorDeg = std::sqrt(ry * ry + rp * rp);
		o.onTarget = o.errorDeg <= in.targetSizeDeg;
		o.yaw = m_yaw; o.pitch = m_pitch;
		return o;
	}

	static float Wrap(float d) { d = std::fmod(d + 180.f, 360.f); if (d < 0.f) d += 360.f; return d - 180.f; }

private:
	float Uniform() { m_rng ^= m_rng << 13; m_rng ^= m_rng >> 17; m_rng ^= m_rng << 5; return (m_rng & 0xFFFFFF) / float(0x1000000); }
	float Gauss() { float s = 0.f; for (int i = 0; i < 4; ++i) s += Uniform(); return (s - 2.f) * 1.7320508f; }  // ~N(0,1)

	AimSkill m_s;
	uint32_t m_rng;
	float m_yaw = 0.f, m_pitch = 0.f;
	int m_target = 0;
	float m_sacT = -1.f, m_sacDur = 0.f, m_sacFromYaw = 0.f, m_sacFromPitch = 0.f, m_sacOffYaw = 0.f, m_sacOffPitch = 0.f;
	float m_nYaw = 0.f, m_nPitch = 0.f, m_pYaw = 0.f, m_pPitch = 0.f;
};

} // namespace WarsBots
