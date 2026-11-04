// Shore playtest 03.11: WarsBot_10 killed teammate WarsBot_12 with 7 rifle hits. Target choice skips allies and the
// muzzle ray to the aim point was clear, but bullets follow the real weapon direction with spread. Hold fire while a
// friendly body is near that line before the target; the body is the feet-to-head segment.
// Pure logic, no engine types; Tests/BotFireLaneTests.cpp.
#pragma once
#include <algorithm>
#include <cmath>
#include <utility>

namespace WarsBots {

struct LaneVec { float x, y, z; };

inline float LaneDot(const LaneVec& a, const LaneVec& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline LaneVec LaneSub(const LaneVec& a, const LaneVec& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline LaneVec LaneAt(const LaneVec& p, const LaneVec& d, float t) { return {p.x + d.x * t, p.y + d.y * t, p.z + d.z * t}; }

// Closest distance between segments [p1,q1] and [p2,q2]; s is the parameter (0..1) on the first one.
inline float SegmentDistance(const LaneVec& p1, const LaneVec& q1, const LaneVec& p2, const LaneVec& q2, float& s)
{
	const LaneVec d1 = LaneSub(q1, p1), d2 = LaneSub(q2, p2), r = LaneSub(p1, p2);
	const float a = LaneDot(d1, d1), e = LaneDot(d2, d2), f = LaneDot(d2, r), eps = 1e-6f;
	float t = 0.f;
	s = 0.f;
	if (a <= eps && e <= eps) {}
	else if (a <= eps) t = std::clamp(f / e, 0.f, 1.f);
	else
	{
		const float c = LaneDot(d1, r);
		if (e <= eps) s = std::clamp(-c / a, 0.f, 1.f);
		else
		{
			const float b = LaneDot(d1, d2), denom = a * e - b * b;
			s = denom > eps ? std::clamp((b * f - c * e) / denom, 0.f, 1.f) : 0.f;
			t = (b * s + f) / e;
			if (t < 0.f) { t = 0.f; s = std::clamp(-c / a, 0.f, 1.f); }
			else if (t > 1.f) { t = 1.f; s = std::clamp((b - c) / a, 0.f, 1.f); }
		}
	}
	const LaneVec gap = LaneSub(LaneAt(p1, d1, s), LaneAt(p2, d2, t));
	return std::sqrt(LaneDot(gap, gap));
}

// dir is normalised, range is the distance to the target. The lane widens with distance for aim noise and spread.
// radius 0.7: Shore server run, a bullet 0.52 m from the ally's feet-to-head axis still hit the shoulder.
inline bool FriendlyInLane(const LaneVec& muzzle, const LaneVec& dir, float range, const LaneVec& feet, const LaneVec& head,
	float radius = 0.7f, float spread = 0.03f)
{
	if (!(range > 0.f)) return false;
	// an ally beside or behind the shooter is not in the lane, even within the radius of the muzzle
	const LaneVec chest{(feet.x + head.x) * 0.5f, (feet.y + head.y) * 0.5f, (feet.z + head.z) * 0.5f};
	if (LaneDot(LaneSub(chest, muzzle), dir) <= 0.f) return false;
	float s = 0.f;
	const float distance = SegmentDistance(muzzle, LaneAt(muzzle, dir, range), feet, head, s);
	return distance < radius + spread * s * range;
}

// An ally inside a vehicle: the shot hits the vehicle's world box (min/max), grown by margin, before range.
// A shooter inside the box (its own vehicle) never blocks; one next to it tests the bare box.
inline bool LaneHitsBox(const LaneVec& muzzle, const LaneVec& dir, float range, const LaneVec& lo, const LaneVec& hi,
	float margin = 0.3f)
{
	if (!(range > 0.f)) return false;
	const float p[3] = {muzzle.x, muzzle.y, muzzle.z}, d[3] = {dir.x, dir.y, dir.z};
	const float a[3] = {lo.x, lo.y, lo.z}, b[3] = {hi.x, hi.y, hi.z};
	auto inside = [&](float m) { for (int i = 0; i < 3; ++i) if (p[i] < a[i] - m || p[i] > b[i] + m) return false; return true; };
	if (inside(0.f)) return false;
	const float m = inside(margin) ? 0.f : margin;
	float tmin = 0.f, tmax = range;
	for (int i = 0; i < 3; ++i)
	{
		if (std::fabs(d[i]) < 1e-6f) { if (p[i] < a[i] - m || p[i] > b[i] + m) return false; continue; }
		float t1 = (a[i] - m - p[i]) / d[i], t2 = (b[i] + m - p[i]) / d[i];
		if (t1 > t2) std::swap(t1, t2);
		tmin = std::max(tmin, t1); tmax = std::min(tmax, t2);
		if (tmin > tmax) return false;
	}
	return true;
}

} // namespace WarsBots
