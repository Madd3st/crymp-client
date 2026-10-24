// Bot perception (D-008 roadmap step 10, D-010 Claude module). Today a bot "sees" every enemy within 80 m whose
// eye-to-chest ray is clear - no view cone, no cloak, no time to notice (BotCombat.cpp:71-86, 142-166 @ 5ce78d8,
// external audit). This module adds human rules that the engine ray cannot express:
//   * a view cone (default 110 deg): outside it a target is not seen even with a clear ray;
//   * awareness that builds up over time - faster close, in the centre of view, for moving or firing targets;
//     skill changes only how fast (reaction), never what can be seen (BotProfile.h rule);
//   * cloak: invisible beyond ~12 m unless the target fired recently or sprints in Speed mode; slow to notice inside;
//   * hearing: gunshots (silenced shorter) and sprint footsteps give a position, not a sighting;
//   * damage: the bot knows roughly where the shot came from.
// The engine still does the ray (LOS) and supplies positions; this module decides visible / heard per target.
// Pure logic, no engine types; Tests/BotPerceptionTests.cpp.
#pragma once
#include <cmath>

namespace WarsBots {

struct PVec { float x = 0, y = 0, z = 0; };

struct PerceptionParams {
	float coneDeg = 110.f;          // full view cone
	float maxSight = 80.f;          // m (unchanged from today)
	float cloakSight = 12.f;        // m: cloaked targets beyond this are invisible unless revealed
	float cloakRate = 0.25f;        // awareness rate multiplier for a cloaked target inside cloakSight
	float revealAfterShot = 1.5f;   // s a cloaked target stays revealed after firing
	float gunshotHear = 60.f;       // m
	float silencedHear = 12.f;      // m
	float sprintHear = 12.f;        // m, footsteps of a sprinting/Speed-mode target
	float decay = 0.6f;             // awareness lost per second without sight
	float reactionTime = 0.4f;      // s to notice a close target in the centre of view (skill: 0.6/0.4/0.3/0.22)
};

struct ObserverState {
	float now = 0.f;
	PVec eye;
	PVec forward;                   // unit view direction
};

struct TargetCue {                 // per enemy per tick
	int id = 0;
	PVec pos;                       // aim point (chest)
	bool losClear = false;          // engine ray result
	bool cloaked = false;
	float sinceShot = 1e9f;         // s since this target fired
	bool silenced = false;
	bool sprintSpeedMode = false;   // sprinting in Speed mode (loud, visible through cloak)
	float speed = 0.f;              // m/s horizontal
	bool damagedMe = false;         // hit this bot within the last tick window
};

struct TargetPercept {
	int id = 0;
	bool inCone = false;
	bool visible = false;           // awareness reached 1 with sight this tick
	bool heard = false;             // a sound gave a position this tick
	bool cloaked = false;
	float awareness = 0.f;
	float distance = 0.f;
};

class BotPerception {
public:
	explicit BotPerception(const PerceptionParams& p = PerceptionParams()) : m_p(p) {}
	void SetReactionTime(float s) { m_p.reactionTime = s > 0.05f ? s : 0.05f; }
	void Reset() { m_n = 0; m_lastNow = -1.f; }

	void BeginTick(const ObserverState& o)
	{
		m_dt = m_lastNow < 0.f ? 0.1f : o.now - m_lastNow;
		if (m_dt < 0.f || m_dt > 1.f) m_dt = 0.1f;
		m_lastNow = o.now; m_o = o;
		for (int i = 0; i < m_n; ++i) m_seenThisTick[i] = false;
	}

	TargetPercept Observe(const TargetCue& t)
	{
		const int k = Slot(t.id);
		TargetPercept r; r.id = t.id; r.cloaked = t.cloaked;
		const float dx = t.pos.x - m_o.eye.x, dy = t.pos.y - m_o.eye.y, dz = t.pos.z - m_o.eye.z;
		const float dist = std::sqrt(dx * dx + dy * dy + dz * dz); r.distance = dist;
		float cosAng = dist > 1e-3f ? (dx * m_o.forward.x + dy * m_o.forward.y + dz * m_o.forward.z) / dist : 1.f;
		const float halfCos = std::cos(m_p.coneDeg * 0.5f * 3.14159265f / 180.f);
		r.inCone = cosAng >= halfCos;
		const bool revealed = t.sinceShot <= m_p.revealAfterShot || t.sprintSpeedMode;
		bool sightPossible = t.losClear && r.inCone && dist < m_p.maxSight;
		if (t.cloaked && !revealed && dist > m_p.cloakSight) sightPossible = false;
		float& a = m_aw[k];
		if (sightPossible)
		{
			// centre of view 1.0 -> cone edge 0.5; near 1.0 -> max range 0.3; moving targets +20 %
			const float centre = 0.5f + 0.5f * Clamp01((cosAng - halfCos) / (1.f - halfCos));
			const float near = 1.f - 0.7f * Clamp01((dist - 10.f) / (m_p.maxSight - 10.f));
			float rate = centre * near * (t.speed > 1.f ? 1.2f : 1.f) / m_p.reactionTime;
			if (t.cloaked && !revealed) rate *= m_p.cloakRate;
			if (t.sinceShot <= 0.3f) a = a < 0.7f ? 0.7f : a;     // muzzle flash in view
			a += rate * m_dt;
		}
		else a -= m_p.decay * m_dt;
		if (t.damagedMe && t.losClear) a = a < 0.8f ? 0.8f : a;   // turned toward the shooter
		a = a < 0.f ? 0.f : (a > 1.f ? 1.f : a);
		r.awareness = a;
		r.visible = sightPossible && a >= 1.f;
		const float hear = t.silenced ? m_p.silencedHear : m_p.gunshotHear;
		r.heard = (t.sinceShot <= 0.2f && dist <= hear) || (t.sprintSpeedMode && dist <= m_p.sprintHear) || t.damagedMe;
		m_seenThisTick[k] = true;
		return r;
	}

	// Direction (unit) the bot should glance at for the strongest heard/remembered cue, if any.
	bool AttentionCue(PVec& out) const { if (m_hasCue) { out = m_cue; } return m_hasCue; }
	void SetAttention(const PVec& p) { m_cue = p; m_hasCue = true; }
	void ClearAttention() { m_hasCue = false; }

private:
	static float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
	int Slot(int id)
	{
		for (int i = 0; i < m_n; ++i) if (m_ids[i] == id) return i;
		if (m_n < kMax) { m_ids[m_n] = id; m_aw[m_n] = 0.f; return m_n++; }
		int worst = 0; for (int i = 1; i < kMax; ++i) if (m_aw[i] < m_aw[worst]) worst = i;   // reuse the least aware
		m_ids[worst] = id; m_aw[worst] = 0.f; return worst;
	}
	static const int kMax = 32;
	PerceptionParams m_p;
	ObserverState m_o;
	float m_dt = 0.1f, m_lastNow = -1.f;
	int m_ids[kMax] = {};
	float m_aw[kMax] = {};
	bool m_seenThisTick[kMax] = {};
	int m_n = 0;
	PVec m_cue; bool m_hasCue = false;
};

} // namespace WarsBots
