// SPEC-004 (wb_bot_unstick, default OFF): engine-free parts of the stuck handling.
//  1. remaining navmesh path + path-signature rule (shared with the weapon goal, wb_bot_path_progress);
//  2. stuck sensor on the best remaining path (not on displacement: a bot sliding 1-2 m on a ~41 deg slope resets a
//     displacement anchor forever, owner run Shore 04.11: 137 stalls up to 96 s, 0 TRAP RECOVERY lines);
//  3. exit steps: sprint -> jump -> strength jump (one per transition) -> back off and replan; success = progress of
//     the remaining path, never a timer; only movement requests (no SetPos, no impulses);
//  4. directional climb cost for the Detour filter (uphill > 38 deg is expensive, downhill is free);
//  5. memory of directed from->to transitions by coordinates (2 m cells), navmesh hash, half-life, cap, solo-only rule;
//  6. spread: trodden transitions (decay in minutes) and a fixed per-bot cost noise (+-15 %).
// No CryEngine types: the Brain adapts Vec3, the Detour filter calls the cost functions (Tests/BotUnstickTests.cpp).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace WarsBots {

// ---------- 1. remaining path ----------
// Distance feet -> current corner plus the rest of the corner polyline (exactly the old wb_bot_path_progress sum).
template<class C, class P> float PathRemaining(const C& path, size_t index, const P& feet)
{
	if (path.empty()) return 0.f;
	auto len = [](float dx, float dy, float dz) { return std::sqrt(dx * dx + dy * dy + dz * dz); };
	const size_t i = index < path.size() ? index : path.size() - 1;
	float rem = len(path[i].x - feet.x, path[i].y - feet.y, path[i].z - feet.z);
	for (size_t k = index + 1; k < path.size(); ++k)
		rem += len(path[k].x - path[k - 1].x, path[k].y - path[k - 1].y, path[k].z - path[k - 1].z);
	return rem;
}
template<class C> float PathSignature(const C& path)
{ return path.empty() ? 0.f : path.front().x * 0.37f + path.front().y * 1.13f + (float)path.size() * 7.0f; }
// DS-S1 #1: Steer repaths every 5 s. A new (longer) path restarts progress from its own remaining length, so the old
// best does not age into a false timeout; a shorter new path keeps the clock and counts as progress by the caller.
inline bool ApplyPathSignature(float& lastSig, float& best, float& bestAt, float sig, float rem, float now)
{
	if (std::fabs(sig - lastSig) <= 0.01f) return false;
	lastSig = sig;
	if (rem > best) { best = rem; bestAt = now; }
	return true;
}

// ---------- 2. stuck sensor ----------
struct UnstickTuning {
	float window = 4.f;     // no 1 m of remaining-path progress for this long -> stuck
	float minDrop = 1.f;
	float armAbove = 2.5f;  // jitter at the goal never arms
};

class UnstickSensor {
public:
	void Reset() { m_armed = false; }
	// muted: link/ladder (incl. waiting at a ladder), hold without move, combat, water. While muted nothing accumulates.
	// Returns true while the bot is stuck.
	// Progress is counted only along one path: a new path (other signature) starts its own baseline at its own remaining
	// length but keeps the clock, so a walking bot progresses on the new path within a second while a bot that only gets
	// alternating long/short replans (Codex review of 96a09959: 20/22 m every 3 s) is never credited with progress.
	// The weapon goal keeps its own legacy rule (ApplyPathSignature).
	bool Update(float rem, float sig, float now, bool muted, const UnstickTuning& t = UnstickTuning())
	{
		m_progressed = false;
		if (muted || rem <= t.armAbove || (m_armed && now < m_bestAt)) { m_armed = false; return false; }
		if (!m_armed) { m_armed = true; m_sig = sig; m_best = rem; m_bestAt = now; return false; }
		if (std::fabs(sig - m_sig) > 0.01f) { m_sig = sig; m_best = rem; }   // new path: new baseline, same clock
		else if (rem < m_best - t.minDrop) { m_best = rem; m_bestAt = now; m_progressed = true; }
		return now - m_bestAt >= t.window;
	}
	bool Armed() const { return m_armed; }
	bool Progressed() const { return m_progressed; }  // this update dropped the best remaining by minDrop
	float Best() const { return m_best; }
	float StuckFor(float now) const { return m_armed ? now - m_bestAt : 0.f; }
private:
	bool m_armed = false, m_progressed = false;
	float m_sig = 0.f, m_best = 0.f, m_bestAt = 0.f;
};

// ---------- 3. exit steps ----------
enum class UnstickStep { None = 0, Sprint, Jump, Strength, Backoff, Wait };
inline const char* UnstickStepName(UnstickStep s)
{ static const char* n[] = {"none", "sprint", "jump", "strength", "backoff", "wait"}; return n[int(s)]; }

// A directed transition: 2 m cell of the bot -> 2 m cell of the corner ahead it did not reach.
inline uint64_t TransitionKey(float fx, float fy, float tx, float ty, float cell = 2.f)
{
	auto c = [cell](float v) { return (uint64_t)(uint16_t)(int16_t)std::floor(v / cell); };
	return c(fx) << 48 | c(fy) << 32 | c(tx) << 16 | c(ty);
}

struct UnstickInput {
	bool stuck = false;        // sensor says stuck
	bool progressed = false;   // sensor saw real progress this tick
	bool grounded = true;
	bool canJump = true;       // no ceiling / free space ahead
	bool canStrength = false;  // enough suit energy and free space ahead (no crowd in front)
	uint64_t transition = 0;
};
enum class UnstickEvent { None = 0, Started, Exited, Failed };

class UnstickSteps {
public:
	float stepTime = 2.5f, backoffTime = 3.0f, waitTime = 5.f, banTime = 30.f;
	int backoffsBeforeWait = 2;

	void Reset() { m_step = UnstickStep::None; m_backoffs = 0; m_lastExitByStrength = false; }
	UnstickStep Step() const { return m_step; }
	UnstickEvent Event() const { return m_event; }
	bool LastExitByStrength() const { return m_lastExitByStrength; }
	bool StrengthUsed(uint64_t tr) const { return m_strengthUsed.count(tr) != 0; }
	// Closed for this bot uphill: strength did not help on it (permanent for the life of the brain) or a back-off ban.
	bool Closed(uint64_t tr, float now) const
	{
		if (m_closed.count(tr)) return true;
		auto it = m_banned.find(tr);
		return it != m_banned.end() && now < it->second;
	}
	const std::unordered_set<uint64_t>& ClosedSet() const { return m_closed; }

	UnstickStep Update(float now, const UnstickInput& in)
	{
		m_event = UnstickEvent::None;
		if (m_step != UnstickStep::None && in.progressed)
		{   // success is progress of the remaining path, whatever step got it
			m_lastExitByStrength = m_step == UnstickStep::Strength;
			m_step = UnstickStep::None; m_backoffs = 0; m_event = UnstickEvent::Exited;
			return m_step;
		}
		if (m_step == UnstickStep::None)
		{
			if (!in.stuck || !in.grounded) return m_step;
			m_lastExitByStrength = false;
			Begin(UnstickStep::Sprint, now, in);
			return m_step;
		}
		const float dur = m_step == UnstickStep::Backoff ? backoffTime : m_step == UnstickStep::Wait ? waitTime : stepTime;
		if (now - m_since < dur || !in.grounded) return m_step;   // the next step only on the ground
		switch (m_step)
		{
		case UnstickStep::Sprint: Begin(UnstickStep::Jump, now, in); break;
		case UnstickStep::Jump: Begin(UnstickStep::Strength, now, in); break;
		case UnstickStep::Strength: Begin(UnstickStep::Backoff, now, in); break;
		case UnstickStep::Backoff:
			// back off done: the caller replans with the transition banned; if still stuck the chain restarts
			m_event = UnstickEvent::Failed;
			if (++m_backoffs >= backoffsBeforeWait) { m_backoffs = 0; m_step = UnstickStep::Wait; m_since = now; }
			else m_step = UnstickStep::None;
			break;
		case UnstickStep::Wait: m_step = UnstickStep::None; break;
		default: m_step = UnstickStep::None; break;
		}
		return m_step;
	}
	float StepAge(float now) const { return m_step == UnstickStep::None ? 0.f : now - m_since; }

private:
	void Begin(UnstickStep s, float now, const UnstickInput& in)
	{
		// skip what cannot work (ceiling, no energy, crowd, strength already spent on this transition)
		if (s == UnstickStep::Jump && !in.canJump) s = UnstickStep::Strength;
		if (s == UnstickStep::Strength && (!in.canJump || !in.canStrength || StrengthUsed(in.transition))) s = UnstickStep::Backoff;
		if (m_step == UnstickStep::Strength) m_closed.insert(m_transition);  // strength did not help: closed uphill
		if (s == UnstickStep::Strength) m_strengthUsed.insert(in.transition);
		if (s == UnstickStep::Backoff) m_banned[in.transition] = now + banTime;
		m_step = s; m_since = now; m_transition = in.transition; m_event = UnstickEvent::Started;
	}
	UnstickStep m_step = UnstickStep::None;
	UnstickEvent m_event = UnstickEvent::None;
	float m_since = 0.f;
	int m_backoffs = 0;
	bool m_lastExitByStrength = false;
	uint64_t m_transition = 0;
	std::unordered_set<uint64_t> m_strengthUsed, m_closed;
	std::unordered_map<uint64_t, float> m_banned;
};

// Memory is written only for a lone bot: no other stuck ally within 5 m and no combat (crowds and fights are not the
// place's fault). Exits that needed strength count as "the place is hard".
inline bool UnstickShouldRecord(bool failedOrStrength, int stuckAlliesWithin5m, bool combat)
{ return failedOrStrength && stuckAlliesWithin5m == 0 && !combat; }

// ---------- 4. directional climb cost ----------
// Multiplier for walking a -> b (Crysis frame, z up). Downhill and flat: 1. Uphill: 1 up to `fromDeg`, then linear to
// `atDeg` = `atMul`, continuing with the same slope to 2*atMul-1 (the baked mesh tops at 45 deg; segment ends between
// polygon edges can read a little steeper).
inline float ClimbCostMultiplier(float ax, float ay, float az, float bx, float by, float bz,
	float fromDeg = 38.f, float atDeg = 45.f, float atMul = 3.f)
{
	const float dz = bz - az;
	if (dz <= 0.f) return 1.f;
	const float h = std::sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
	const float deg = std::atan2(dz, h) * 57.29578f;
	if (deg <= fromDeg) return 1.f;
	const float m = 1.f + (atMul - 1.f) * (deg - fromDeg) / (atDeg - fromDeg);
	const float cap = 2.f * atMul - 1.f;
	return m < cap ? m : cap;
}

// ---------- 5. memory of directed transitions ----------
struct TransitionPenalty { float fx, fy, fz, tx, ty, tz; float value; float at; };

class TransitionMemory {
public:
	float halfLife = 1800.f;  // game seconds (accumulated over matches)
	float add = 1.f;          // one solo failure adds +100 % of the base cost
	float cap = 3.f;          // at most x4 the base cost

	void Clear() { m_items.clear(); }
	size_t Size() const { return m_items.size(); }
	const std::unordered_map<uint64_t, TransitionPenalty>& Items() const { return m_items; }

	float Decayed(const TransitionPenalty& p, float now) const
	{ return now <= p.at ? p.value : p.value * std::pow(0.5f, (now - p.at) / halfLife); }
	void Record(float fx, float fy, float fz, float tx, float ty, float tz, float now)
	{
		const uint64_t k = TransitionKey(fx, fy, tx, ty);
		auto it = m_items.find(k);
		if (it == m_items.end()) { m_items[k] = TransitionPenalty{fx, fy, fz, tx, ty, tz, add < cap ? add : cap, now}; return; }
		float v = Decayed(it->second, now) + add;
		it->second.value = v < cap ? v : cap; it->second.at = now;
	}
	// Extra cost factor (0 = none) for the stored transition with this key.
	float Penalty(uint64_t key, float now) const
	{ auto it = m_items.find(key); return it == m_items.end() ? 0.f : Decayed(it->second, now); }
	// Forget what decayed below 2 %.
	void Prune(float now) { for (auto it = m_items.begin(); it != m_items.end();) it = Decayed(it->second, now) < 0.02f ? m_items.erase(it) : std::next(it); }

	// Text file: "WBUNSTK1 <navmesh hash> <clock>" then "fx fy fz tx ty tz value at" lines. Another navmesh -> nothing.
	std::string Save(const std::string& navHash, float clock) const
	{
		std::ostringstream o; o << "WBUNSTK1 " << (navHash.empty() ? "-" : navHash) << ' ' << clock << '\n';
		for (const auto& kv : m_items)
		{ const auto& p = kv.second; o << p.fx << ' ' << p.fy << ' ' << p.fz << ' ' << p.tx << ' ' << p.ty << ' ' << p.tz << ' ' << p.value << ' ' << p.at << '\n'; }
		return o.str();
	}
	// Returns the stored clock (game seconds) or -1 when the file is foreign / for another navmesh.
	float Load(const std::string& text, const std::string& navHash)
	{
		m_items.clear();
		std::istringstream in(text); std::string magic, hash; float clock = 0.f;
		if (!(in >> magic >> hash >> clock) || magic != "WBUNSTK1" || navHash.empty() || hash != navHash) return -1.f;
		TransitionPenalty p;
		while (in >> p.fx >> p.fy >> p.fz >> p.tx >> p.ty >> p.tz >> p.value >> p.at)
		{
			if (!(p.value > 0.f)) continue;
			if (p.value > cap) p.value = cap;
			if (p.at > clock) p.at = clock;
			m_items[TransitionKey(p.fx, p.fy, p.tx, p.ty)] = p;
		}
		return clock;
	}
private:
	std::unordered_map<uint64_t, TransitionPenalty> m_items;
};

// Does walking a -> b go the same way as a stored uphill transition? (2D direction within 45 deg, and uphill.)
inline bool SameDirectedTransition(const TransitionPenalty& p, float ax, float ay, float az, float bx, float by, float bz)
{
	if (bz - az <= 0.05f) return false;
	const float ux = p.tx - p.fx, uy = p.ty - p.fy, vx = bx - ax, vy = by - ay;
	const float lu = std::sqrt(ux * ux + uy * uy), lv = std::sqrt(vx * vx + vy * vy);
	if (lu < 1e-3f || lv < 1e-3f) return false;
	return (ux * vx + uy * vy) / (lu * lv) >= 0.7071f;
}

// Game-seconds clock of the memory across matches: base from the file plus the engine timer since the level started;
// a restarted timer keeps the total. Due() asks for a periodic save so a match without new records still ages it.
class UnstickClockState {
public:
	float saveEvery = 60.f;
	void Start(float base) { m_base = base; m_at = -1.f; m_last = 0.f; m_nextSave = 0.f; }
	float Now(float t)
	{
		if (m_at < 0.f) { m_at = t; m_nextSave = t + saveEvery; }
		else if (t < m_at) { m_base += m_last - m_at; m_at = t; m_nextSave = t + saveEvery; }
		m_last = t;
		return m_base + (t - m_at);
	}
	float Last() const { return m_at < 0.f ? m_base : m_base + (m_last - m_at); }
	bool Due(float t) { if (m_at < 0.f || t < m_nextSave) return false; m_nextSave = t + saveEvery; return true; }
private:
	float m_base = 0.f, m_at = -1.f, m_last = 0.f, m_nextSave = 0.f;
};

// Allies for the crowd rule and the spread: same non-zero team only. Instant Action (team 0) has no allies.
inline bool UnstickAlly(int myTeam, int otherTeam) { return myTeam > 0 && otherTeam == myTeam; }

// ---------- 6. spread ----------
// Trodden transitions: every pass adds a little temporary cost that decays within minutes (no file).
class TroddenMap {
public:
	float add = 0.1f, cap = 1.0f, halfLife = 120.f;
	void Pass(uint64_t id, float now)
	{
		auto& e = m_items[id];
		float v = Value(e, now) + add; e.v = v < cap ? v : cap; e.at = now;
	}
	float Extra(uint64_t id, float now) const { auto it = m_items.find(id); return it == m_items.end() ? 0.f : Value(it->second, now); }
	void Prune(float now) { for (auto it = m_items.begin(); it != m_items.end();) it = Value(it->second, now) < 0.01f ? m_items.erase(it) : std::next(it); }
	size_t Size() const { return m_items.size(); }
private:
	struct E { float v = 0.f, at = 0.f; };
	float Value(const E& e, float now) const { return now <= e.at ? e.v : e.v * std::pow(0.5f, (now - e.at) / halfLife); }
	std::unordered_map<uint64_t, E> m_items;
};

// Fixed per-bot noise on a polygon's cost, in [1-amp, 1+amp]. Same bot + same polygon -> same value every query (the
// path does not flicker); different bots disagree a little, so a team does not walk one column into one pocket.
inline float BotCostNoise(uint32_t botSeed, uint64_t polyRef, float amp = 0.15f)
{
	uint64_t h = polyRef * 0x9E3779B97F4A7C15ull ^ (uint64_t(botSeed) + 0x632BE59BD9B4E019ull);
	h ^= h >> 33; h *= 0xFF51AFD7ED558CCDull; h ^= h >> 33; h *= 0xC4CEB9FE1A85EC53ull; h ^= h >> 33;
	const float u = float(h >> 40) / float(1ull << 24);   // [0,1)
	return 1.f + amp * (2.f * u - 1.f);
}

// An ally standing within `radius` of the path ahead (first `horizon` m from the feet) -> replan around it.
template<class C, class P, class Q> bool AllyNearPathAhead(const C& path, size_t index, const P& feet, const Q& ally,
	float radius = 4.f, float horizon = 30.f)
{
	if (path.empty()) return false;
	auto segDist2 = [](float ax, float ay, float bx, float by, float px, float py) {
		const float vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy;
		float t = l2 > 1e-6f ? ((px - ax) * vx + (py - ay) * vy) / l2 : 0.f;
		t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
		const float dx = ax + vx * t - px, dy = ay + vy * t - py; return dx * dx + dy * dy; };
	float ax = feet.x, ay = feet.y, walked = 0.f;
	for (size_t k = index < path.size() ? index : path.size() - 1; k < path.size() && walked < horizon; ++k)
	{
		const float len = std::sqrt((path[k].x - ax) * (path[k].x - ax) + (path[k].y - ay) * (path[k].y - ay));
		const float f = len > horizon - walked && len > 1e-6f ? (horizon - walked) / len : 1.f;   // clip at the horizon
		if (segDist2(ax, ay, ax + (path[k].x - ax) * f, ay + (path[k].y - ay) * f, ally.x, ally.y) <= radius * radius) return true;
		walked += len; ax = path[k].x; ay = path[k].y;
	}
	return false;
}

} // namespace WarsBots
