// Vantage points (owner 25.10: "nobody holds the roofs with a sniper rifle / Gauss"; CS sniper spots, Killzone position
// picking). Spots come from Tools/RecastBake/tactical_spots (flag 4 = sniper: high, top-5 % view of the map, best view
// yaw). A sniper bot claims a spot, travels there, holds it looking along the best view, and relocates after a hold
// time, after scoring kills there, or after being hit there (good players never stay on a known spot).
// Pure logic, no engine types; Tests/BotVantageTests.cpp.
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace WarsBots {

struct VSpot { float x = 0, y = 0, z = 0; unsigned flags = 0; float view = 0; float yaw = 0; };

struct VantageParams {
	float holdMin = 25.f, holdMax = 45.f;   // s on a spot
	float claimRadius = 6.f;                // m: one bot per spot cluster
	float travelWeight = 0.6f;              // view points lost per metre of straight-line distance
	float maxTravel = 120.f;                // m
	float arriveRadius = 1.2f;              // m
	float hitLeave = 2.f;                   // s after a hit taken on the spot -> relocate
	int killsLeave = 2;                     // kills from one spot -> relocate
	float banTime = 60.f;                   // s a left spot is not taken again by the same bot
};

// Shared per map: spots + claims (which bot holds/travels to which spot).
class VantageMap {
public:
	int Load(const char* text, unsigned needFlags = 4)
	{
		m_spots.clear(); m_claims.clear();
		const char* p = text;
		while (p && *p)
		{
			const char* nl = std::strchr(p, '\n');
			if (*p != '#')
			{
				VSpot s; float cover, low, open; float yaw = 0;
				const int n = std::sscanf(p, "%f %f %f %u %f %f %f %f %f", &s.x, &s.y, &s.z, &s.flags, &cover, &low, &open, &s.view, &yaw);
				if (n >= 8 && (s.flags & needFlags)) { s.yaw = n >= 9 ? yaw : 0.f; m_spots.push_back(s); }
			}
			p = nl ? nl + 1 : nullptr;
		}
		return int(m_spots.size());
	}
	int Count() const { return int(m_spots.size()); }
	const VSpot& Spot(int i) const { return m_spots[i]; }
	void Claim(int bot, int spot) { Release(bot); m_claims.push_back({bot, spot}); }
	void Release(int bot) { for (size_t i = 0; i < m_claims.size(); ++i) if (m_claims[i].bot == bot) { m_claims.erase(m_claims.begin() + i); return; } }
	bool ClaimedNear(int bot, const VSpot& s, float r) const
	{
		for (const C& c : m_claims)
		{
			if (c.bot == bot) continue;
			const VSpot& o = m_spots[c.spot];
			if ((o.x - s.x) * (o.x - s.x) + (o.y - s.y) * (o.y - s.y) + (o.z - s.z) * (o.z - s.z) < r * r) return true;
		}
		return false;
	}
	// Best spot for `bot` at (x,y,z): view minus travel, unclaimed, reachable (callback), not banned.
	template <class Reach, class Banned>
	int Pick(int bot, float x, float y, float z, const VantageParams& p, Reach reach, Banned banned) const
	{
		int best = -1; float bs = -1e9f;
		for (int i = 0; i < int(m_spots.size()); ++i)
		{
			const VSpot& s = m_spots[i];
			const float d = std::sqrt((s.x - x) * (s.x - x) + (s.y - y) * (s.y - y) + (s.z - z) * (s.z - z));
			if (d > p.maxTravel || banned(i) || ClaimedNear(bot, s, p.claimRadius)) continue;
			const float score = s.view - d * p.travelWeight;
			if (score > bs && reach(s)) { bs = score; best = i; }
		}
		return best;
	}
private:
	struct C { int bot, spot; };
	std::vector<VSpot> m_spots;
	std::vector<C> m_claims;
};

enum class VantagePhase { None = 0, Travel, Hold };

struct VantageOutput {
	VantagePhase phase = VantagePhase::None;
	bool move = false; float x = 0, y = 0, z = 0;   // travel target
	float lookYaw = 0;                              // hold: look along the best view (deg, 0 = +Y, clockwise)
	bool relocated = false;
};

// One bot's sniper routine.
class BotVantage {
public:
	explicit BotVantage(const VantageParams& p = VantageParams()) : m_p(p) {}
	void Reset(VantageMap* map, int bot) { if (map) map->Release(bot); m_spot = -1; m_phase = VantagePhase::None; m_kills = 0; m_bans.clear(); }
	int SpotIndex() const { return m_spot; }
	VantagePhase Phase() const { return m_phase; }
	void OnKill() { if (m_phase == VantagePhase::Hold) ++m_kills; }
	void OnHit(float now) { if (m_phase == VantagePhase::Hold) m_hitAt = now; }

	template <class Reach>
	VantageOutput Update(VantageMap& map, int bot, float now, float x, float y, float z, unsigned rng, Reach reach)
	{
		VantageOutput o;
		bool leave = false;
		if (m_phase == VantagePhase::Hold)
			leave = now >= m_holdUntil || m_kills >= m_p.killsLeave || (m_hitAt > 0 && now - m_hitAt < m_p.hitLeave);
		if (m_phase == VantagePhase::Travel && now - m_since > 40.f) leave = true;   // could not get there
		if (leave) { m_bans.push_back({m_spot, now + m_p.banTime}); map.Release(bot); m_spot = -1; m_phase = VantagePhase::None; o.relocated = true; }
		if (m_spot < 0)
		{
			m_spot = map.Pick(bot, x, y, z, m_p, reach, [&](int i) {
				const VSpot& c = map.Spot(i);
				for (const B& b : m_bans)
				{
					if (now >= b.until) continue;
					const VSpot& o = map.Spot(b.spot);      // the whole cluster around a left spot is banned
					if ((o.x - c.x) * (o.x - c.x) + (o.y - c.y) * (o.y - c.y) + (o.z - c.z) * (o.z - c.z) < m_p.claimRadius * m_p.claimRadius) return true;
				}
				return false; });
			if (m_spot < 0) return o;
			map.Claim(bot, m_spot); m_phase = VantagePhase::Travel; m_since = now; m_kills = 0; m_hitAt = -1.f;
		}
		const VSpot& s = map.Spot(m_spot);
		const float d2 = (s.x - x) * (s.x - x) + (s.y - y) * (s.y - y);
		if (m_phase == VantagePhase::Travel && d2 < m_p.arriveRadius * m_p.arriveRadius && std::fabs(s.z - z) < 1.5f)
		{
			m_phase = VantagePhase::Hold; m_since = now;
			m_holdUntil = now + m_p.holdMin + (m_p.holdMax - m_p.holdMin) * float(rng % 1000) / 1000.f;
		}
		o.phase = m_phase; o.x = s.x; o.y = s.y; o.z = s.z; o.lookYaw = s.yaw;
		o.move = m_phase == VantagePhase::Travel || d2 > 4.f;     // pushed off the spot -> step back on it
		return o;
	}
private:
	struct B { int spot; float until; };
	VantageParams m_p;
	int m_spot = -1;
	VantagePhase m_phase = VantagePhase::None;
	float m_since = 0, m_holdUntil = 0, m_hitAt = -1.f;
	int m_kills = 0;
	std::vector<B> m_bans;
};

} // namespace WarsBots
