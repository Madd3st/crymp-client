// Spectator camera for a WarsBot replica (owner playtest 3, 26.10: "FP spectator is super jerky now").
// The server sends the bot's aim ~10 Hz. BotReplicaSmooth (exponential, tau 50 ms, reset while firing) reaches each new
// point in 2-3 frames and then stands still until the next packet -> jump/stop/jump; with wb_bot_spectator_aim the
// CryMP camera lerp that hid it was gone. This is snapshot interpolation as in shooters: keep the last samples and
// show the aim one packet interval in the past, linearly between the two samples around that time. Constant latency
// (~1 interval, measured), constant angular speed in a steady turn, no reset while firing.
// Pure math, no engine types; Tests/BotAimInterpTests.cpp. Angles in radians, yaw wraps.
#pragma once
#include <cmath>

namespace WarsBots {

class BotAimInterp {
public:
	void Reset() { m_n = 0; m_interval = 0.1f; }
	bool Ready() const { return m_n > 0; }

	// A new replicated aim (yaw/pitch relative to the replica eye) arrived at time t (s).
	void Push(float t, float yaw, float pitch)
	{
		if (m_n > 0)
		{
			const Sample& last = m_s[(m_head + m_n - 1) % kN];
			const float dt = t - last.t;
			if (dt <= 1e-4f) { Sample& l = m_s[(m_head + m_n - 1) % kN]; l.yaw = Unwrap(yaw, l.yaw); l.pitch = pitch; return; }
			if (dt > 1.0f) m_n = 0;   // long silence (respawn, no change for a while): start over, no slow drift
			else m_interval += (Clamp(dt, 0.03f, 0.3f) - m_interval) * 0.2f;
			yaw = m_n > 0 ? Unwrap(yaw, last.yaw) : yaw;
		}
		if (m_n == kN) { m_head = (m_head + 1) % kN; --m_n; }
		m_s[(m_head + m_n) % kN] = Sample{t, yaw, pitch};
		++m_n;
	}

	// Aim to show at time now: one measured interval behind, interpolated; clamps to the newest when starved.
	void At(float now, float& yaw, float& pitch) const
	{
		if (m_n == 0) { yaw = pitch = 0.f; return; }
		const float rt = now - Delay();
		const Sample& first = m_s[m_head];
		if (m_n == 1 || rt <= first.t) { yaw = Wrap(first.yaw); pitch = first.pitch; return; }
		for (int i = 0; i + 1 < m_n; ++i)
		{
			const Sample& a = m_s[(m_head + i) % kN];
			const Sample& b = m_s[(m_head + i + 1) % kN];
			if (rt <= b.t)
			{
				const float k = (rt - a.t) / (b.t - a.t);
				yaw = Wrap(a.yaw + (b.yaw - a.yaw) * k); pitch = a.pitch + (b.pitch - a.pitch) * k;
				return;
			}
		}
		const Sample& last = m_s[(m_head + m_n - 1) % kN];
		yaw = Wrap(last.yaw); pitch = last.pitch;
	}
	float Delay() const { return m_interval * 1.1f; }   // a little more than one interval absorbs jitter

	static float Wrap(float a) { const float pi = 3.14159265f; a = std::fmod(a + pi, 2.f * pi); if (a < 0.f) a += 2.f * pi; return a - pi; }

private:
	struct Sample { float t, yaw, pitch; };
	static const int kN = 6;
	static float Clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
	static float Unwrap(float a, float ref) { return ref + Wrap(a - ref); }
	Sample m_s[kN] = {};
	int m_head = 0, m_n = 0;
	float m_interval = 0.1f;
};

} // namespace WarsBots
