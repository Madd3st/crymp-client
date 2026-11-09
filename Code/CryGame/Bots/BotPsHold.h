// SPEC-005b (owner ok 09.11, wb_bot_ps_hold): where a defender of a taken PS point stands. Pure logic, no engine types:
// the engine side (BotBrain::UpdatePSSquad) generates the ring candidates around the zone centre, projects them on the
// navmesh and fills cover/height/sight from rays; this header filters by the approach sector, scores, picks and times
// the next rotation. Tests/BotPsHoldTests.cpp.
#pragma once
#include <cmath>
#include <cstddef>
#include <vector>

namespace WarsBots {

struct HoldConfig
{
	float ringMin = 15.f, ringMax = 20.f;   // SPEC-005b p. 1: ring 15-20 m around the zone centre
	float sectorDeg = 45.f;                 // p. 2: +-45 deg around the approach direction
	float pathMax = 35.f;                   // p. 1: path from the centre at most 35 m (same main part of the mesh)
	float spacing = 6.f;                    // p. 3: not closer than 6 m to another defender
	float energyUp = 2.f;                   // p. 4: on energy points positions >= 2 m above the centre go first
	float rotateMin = 20.f, rotateMax = 40.f; // p. 5: change position every 20-40 s (or at once after a hit)
	float backAfterFight = 10.f;            // p. 5: back on the position within 10 s after combat over
};

struct HoldCandidate
{
	float x = 0, y = 0, z = 0;   // projected on the navmesh
	bool onMesh = false;         // projection found
	float path = 1e9f;           // path length from the zone centre (1e9 = no path)
	bool cover = false;          // low ray towards the approach hits geometry within 3 m, the 1.6 m ray is free
	bool seesCenter = false;     // eye (1.6 m) sees the zone centre
};

// Unit vector of the approach in XY (from the zone centre towards the nearest enemy point or base).
struct HoldApproach { float x = 1.f, y = 0.f; bool valid = false; };

inline HoldApproach MakeApproach(float cx, float cy, float ex, float ey)
{
	HoldApproach a; const float dx = ex - cx, dy = ey - cy, len = std::sqrt(dx * dx + dy * dy);
	if (len > 0.5f) { a.x = dx / len; a.y = dy / len; a.valid = true; }
	return a;
}

// p. 2: the candidate's bearing from the centre lies within +-sectorDeg of the approach.
inline bool InSector(float cx, float cy, const HoldApproach& a, float px, float py, float sectorDeg)
{
	if (!a.valid) return true;   // no enemy point/base known: every direction is the approach
	const float dx = px - cx, dy = py - cy, len = std::sqrt(dx * dx + dy * dy);
	if (len < 0.5f) return false;
	const float cosv = (dx * a.x + dy * a.y) / len;
	return cosv >= std::cos(sectorDeg * 3.14159265f / 180.f) - 1e-4f;
}

// The ring directions that can pass the sector filter (16 directions, every 22.5 deg), so the engine casts rays and
// path queries only for those. Returns angles in radians.
inline std::vector<float> SectorDirections(const HoldApproach& a, float sectorDeg, int count = 16)
{
	std::vector<float> out; const float step = 2.f * 3.14159265f / float(count);
	for (int i = 0; i < count; ++i)
	{
		const float ang = step * float(i);
		if (InSector(0.f, 0.f, a, std::cos(ang), std::sin(ang), sectorDeg)) out.push_back(ang);
	}
	return out;
}

// p. 3: cover +3, +1 per metre above the centre up to +5, centre visible +2.
inline int HoldScore(const HoldCandidate& c, float centerZ)
{
	int s = c.cover ? 3 : 0;
	const float up = c.z - centerZ;
	if (up > 0.f) s += up >= 5.f ? 5 : int(up);
	if (c.seesCenter) s += 2;
	return s;
}

struct HoldPos { float x = 0, y = 0, z = 0; };

// Picks the defender position, or -1. `energy` turns on p. 4 (a valid candidate >= energyUp above the centre wins even
// with a lower score). `others` are the positions other defenders of the team stand on (p. 3 spacing).
inline int PickHold(const std::vector<HoldCandidate>& cands, float cx, float cy, float cz, const HoldApproach& a, bool energy,
	const std::vector<HoldPos>& others, const HoldConfig& cfg = HoldConfig())
{
	int best = -1, bestScore = -1; bool bestHigh = false; float bestDist = 1e9f;
	for (size_t i = 0; i < cands.size(); ++i)
	{
		const HoldCandidate& c = cands[i];
		if (!c.onMesh || c.path > cfg.pathMax) continue;
		const float dx = c.x - cx, dy = c.y - cy, dist = std::sqrt(dx * dx + dy * dy);
		if (dist < cfg.ringMin - 1.5f || dist > cfg.ringMax + 1.5f) continue;   // projection may slide 1.5 m
		if (!InSector(cx, cy, a, c.x, c.y, cfg.sectorDeg)) continue;
		bool crowded = false;
		for (const HoldPos& o : others)
		{
			const float ox = c.x - o.x, oy = c.y - o.y, oz = c.z - o.z;
			if (ox * ox + oy * oy + oz * oz < cfg.spacing * cfg.spacing) { crowded = true; break; }
		}
		if (crowded) continue;
		const bool high = energy && c.z - cz >= cfg.energyUp;
		const int score = HoldScore(c, cz);
		const bool better = best < 0 || (high != bestHigh ? high : (score != bestScore ? score > bestScore : dist < bestDist));
		if (better) { best = int(i); bestScore = score; bestHigh = high; bestDist = dist; }
	}
	return best;
}

// p. 5: the next rotation period, uniform in [rotateMin, rotateMax] from a 0..1 random value.
inline float HoldPeriod(float rand01, const HoldConfig& cfg = HoldConfig())
{
	const float r = rand01 < 0.f ? 0.f : (rand01 > 1.f ? 1.f : rand01);
	return cfg.rotateMin + (cfg.rotateMax - cfg.rotateMin) * r;
}

enum class HoldReason { None = 0, Start, Rotate, Hit, Capture, Back };

inline const char* HoldReasonName(HoldReason r)
{
	switch (r)
	{
	case HoldReason::Start: return "start";
	case HoldReason::Rotate: return "rotate";
	case HoldReason::Hit: return "hit";
	case HoldReason::Capture: return "capture";
	case HoldReason::Back: return "back";
	default: return "none";
	}
}

// Codex #44 BLOCK: arrival on the hold position is 3D. XY within 1.2 m alone took the floor under a roof/platform for
// the chosen upper position (energy rule: >= 2 m above), so the bot stood below and never climbed. The navmesh-snapped
// position and the feet differ by well under 1 m on one floor; floors are >= 2 m apart.
inline bool HoldArrived(float dx, float dy, float dz, float xy = 1.2f, float up = 1.0f)
{
	return dx * dx + dy * dy <= xy * xy && std::fabs(dz) <= up;
}

// Per-defender timing state: when to pick a new position. A hit re-picks at once (p. 5); otherwise the period runs.
struct HoldTimer
{
	bool has = false; float since = 0.f, period = 0.f; float lastHit = -1e9f;
	void Assign(float now, float periodSeconds) { has = true; since = now; period = periodSeconds; }
	void Reset() { has = false; since = period = 0.f; lastHit = -1e9f; }
	// `hitAt` = time of the latest hit on the bot (or a large negative value).
	HoldReason Due(float now, float hitAt)
	{
		if (!has) return HoldReason::Start;
		if (hitAt > lastHit && hitAt >= since) { lastHit = hitAt; return HoldReason::Hit; }
		if (now - since >= period) return HoldReason::Rotate;
		return HoldReason::None;
	}
};

} // namespace WarsBots
