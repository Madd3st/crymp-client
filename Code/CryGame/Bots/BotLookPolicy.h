// Look policy (C-033 / C-042, D-010 Claude module): WHERE the bot wants to look each tick, before the aim model
// turns the view there. Fixes "bots stare at walls": in the owner's 23:33 demo bots without a visible target looked
// > 90 deg away from their travel direction 32 % of moving time, and wall stares (view ray < 1.5 m) were the bot
// looking exactly at its BRAIN aim point 10 m behind a wall (search point / weapon / last known position).
//
// Rules, in priority order:
//   1. visible target                         -> the target (aim model does the flick/tracking);
//   2. hit recently, attacker direction known -> the attacker direction for a short glance;
//   3. point of interest (search/weapon/LKP) with a CLEAR eye ray, or very close, and not far behind the travel
//      direction                               -> the point of interest;
//   4. otherwise                              -> along the path: the next corner, blending to the corner after it
//      when the bot is about to turn; standing still -> keep the current view.
// A choice is held for a minimum time (hysteresis) so the view does not ping-pong between two rules.
// Angles in degrees, yaw 0 = +Y, clockwise (wbdemo convention: yaw = atan2(dx, dy)). Pure logic, no engine types.
#pragma once
#include <cmath>

namespace WarsBots {

enum class LookReason { Keep = 0, Target, Attacker, Interest, Path, NextCorner };

struct LookVec { float x = 0.f, y = 0.f, z = 0.f; };

struct LookInput {
	float now = 0.f;
	LookVec eye;                       // eye position
	float viewYaw = 0.f;               // current view
	float speed = 0.f;                 // horizontal m/s
	// 1. target
	bool targetVisible = false;
	LookVec target;
	// 2. damage
	float sinceDamaged = 1e9f;         // s
	bool attackerKnown = false;
	LookVec attacker;
	// 3. point of interest
	bool hasInterest = false;
	LookVec interest;
	bool interestRayClear = false;     // engine ray eye -> interest is unobstructed (cheap ray, may be cached)
	// 4. path
	bool hasCorner = false;
	LookVec corner;                    // look-ahead point ~3 m (~0.5-1 s) along the path; fallback: next corner.
	                                   // Offline check on the 23:33 demo: looking 0.5-1 s ahead gives > 90 deg off
	                                   // travel 3-5 % (now 32 %); aiming at the raw next corner hits walls at turns.
	bool hasNextCorner = false;
	LookVec nextCorner;                // the one after it, if known
};

struct LookOutput {
	float yaw = 0.f, pitch = 0.f;
	LookReason reason = LookReason::Keep;
	bool changed = false;              // reason changed this tick (useful for logs / INTENT)
};

struct LookParams {
	float attackerGlance = 1.2f;       // s after damage to look toward the attacker
	float interestNear = 3.f;          // m: always look at a point of interest this close
	float interestMax = 40.f;          // m: farther points are not "interest" while moving
	float interestBehind = 100.f;      // deg: a point of interest farther than this from travel is ignored while moving
	float cornerBlend = 3.f;           // m to the corner where the view starts turning to the next corner
	float minHold = 0.4f;              // s a non-urgent choice is kept
	float moving = 0.8f;               // m/s: below this the bot counts as standing
};

class BotLookPolicy {
public:
	explicit BotLookPolicy(const LookParams& p = LookParams()) : m_p(p) {}
	void Reset() { m_reason = LookReason::Keep; m_since = -1e9f; }

	static float Wrap(float d) { d = std::fmod(d + 180.f, 360.f); if (d < 0.f) d += 360.f; return d - 180.f; }
	static float YawTo(const LookVec& a, const LookVec& b) { return std::atan2(b.x - a.x, b.y - a.y) * 57.2957795f; }
	static float PitchTo(const LookVec& a, const LookVec& b)
	{
		const float h = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
		return std::atan2(b.z - a.z, h > 1e-3f ? h : 1e-3f) * 57.2957795f;
	}
	static float Dist2D(const LookVec& a, const LookVec& b) { return std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y)); }

	LookOutput Update(const LookInput& in)
	{
		LookOutput o;
		LookReason want = LookReason::Keep; LookVec at; bool urgent = false;
		const bool moving = in.speed >= m_p.moving;
		const float travelYaw = in.hasCorner ? YawTo(in.eye, in.corner) : in.viewYaw;

		if (in.targetVisible) { want = LookReason::Target; at = in.target; urgent = true; }
		else if (in.sinceDamaged < m_p.attackerGlance && in.attackerKnown) { want = LookReason::Attacker; at = in.attacker; urgent = true; }
		else if (in.hasInterest && InterestOk(in, moving, travelYaw)) { want = LookReason::Interest; at = in.interest; }
		else if (in.hasCorner && moving)
		{
			const float dc = Dist2D(in.eye, in.corner);
			if (in.hasNextCorner && dc < m_p.cornerBlend) { want = LookReason::NextCorner; at = in.nextCorner; }
			else { want = LookReason::Path; at = in.corner; }
		}

		// hysteresis: a non-urgent change waits until the previous choice was held minHold; Path <-> NextCorner is
		// one continuous behaviour and switches freely
		const bool pathFamily = (want == LookReason::Path || want == LookReason::NextCorner) &&
			(m_reason == LookReason::Path || m_reason == LookReason::NextCorner);
		if (want != m_reason && !urgent && !pathFamily && in.now - m_since < m_p.minHold && m_reason != LookReason::Keep)
		{
			want = m_reason; at = m_at;
		}
		if (want == LookReason::Keep) { o.yaw = in.viewYaw; o.pitch = m_pitch; o.reason = LookReason::Keep; o.changed = m_reason != LookReason::Keep; m_reason = LookReason::Keep; return o; }

		o.changed = want != m_reason;
		if (o.changed) { m_reason = want; m_since = in.now; }
		m_at = at;
		o.reason = want;
		o.yaw = YawTo(in.eye, at);
		// along the path look level (eye height) unless the corner is a real climb/drop; at targets use the true pitch
		o.pitch = (want == LookReason::Path || want == LookReason::NextCorner) ? Clamp(PitchTo(in.eye, Lift(at, 1.6f)), -20.f, 20.f)
		                                                                       : PitchTo(in.eye, at);
		m_pitch = o.pitch;
		return o;
	}

private:
	bool InterestOk(const LookInput& in, bool moving, float travelYaw) const
	{
		const float d = Dist2D(in.eye, in.interest);
		if (d <= m_p.interestNear) return true;
		if (!in.interestRayClear) return false;                     // never stare at a point through a wall
		if (!moving) return true;                                   // standing: watching a clear point is fine
		if (d > m_p.interestMax) return false;
		return std::fabs(Wrap(YawTo(in.eye, in.interest) - travelYaw)) <= m_p.interestBehind;
	}
	static LookVec Lift(LookVec v, float dz) { v.z += dz; return v; }
	static float Clamp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

	LookParams m_p;
	LookReason m_reason = LookReason::Keep;
	float m_since = -1e9f, m_pitch = 0.f;
	LookVec m_at;
};

// Point `ahead` metres further along a path polyline from `pos` (projected onto the nearest segment). BotNav/Detour
// fill `pts` with the remaining corners (pts[0] = current corner); used to feed LookInput::corner.
inline LookVec PathLookAhead(const LookVec& pos, const LookVec* pts, int n, float ahead = 3.f)
{
	if (n <= 0) return pos;
	LookVec a = pos; float left = ahead;
	for (int i = 0; i < n; ++i)
	{
		const LookVec& b = pts[i];
		const float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z, L = std::sqrt(dx * dx + dy * dy);
		if (L >= left && L > 1e-4f) { const float f = left / L; return LookVec{a.x + dx * f, a.y + dy * f, a.z + dz * f}; }
		left -= L; a = b;
	}
	return pts[n - 1];
}

} // namespace WarsBots
