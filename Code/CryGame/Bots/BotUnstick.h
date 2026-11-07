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
#include <algorithm>
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
	// jitter at the goal never arms: the corner path arrives within 0.2 m, the plain path within 0.8 m. Server run f9d33a47
	// (Codex 06.11): with 2.5 m a bot that stood 2.1 m short of an unreachable goal for 24 s was never armed.
	float armAbove = 1.0f;
	float placeRadius = 2.5f; // server acceptance metric (stops.awk): within 2.5 m of an anchor -> standing
	float placeWindow = 3.f;  // standing this long within placeRadius -> stuck (leaves room for the steps before 10 s)
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
	{ return Update(rem, sig, now, muted, nullptr, t); }
	// With the bot's XY position: also stuck when it has stayed within placeRadius of one anchor for the window, whatever
	// the remaining length does. Server run 3ba5119c: on the slope a bot sliding 1-2 m got a fresh path baseline at every
	// replan and "progressed" by 1 m on it, so the path rule alone restarted the clock (stalls 16/17 s, Codex 06.11).
	bool Update(float rem, float sig, float now, bool muted, const float* xy, const UnstickTuning& t = UnstickTuning())
	{
		m_progressed = false;
		if (muted || rem <= t.armAbove || (m_armed && now < m_bestAt)) { m_armed = false; return false; }
		const bool place = xy != nullptr;
		if (!m_armed)
		{
			m_armed = true; m_sig = sig; m_best = rem; m_bestAt = now;
			m_hasAnchor = place; if (place) { m_ax = xy[0]; m_ay = xy[1]; } m_anchorAt = now;
			return false;
		}
		if (std::fabs(sig - m_sig) > 0.01f) { m_sig = sig; m_best = rem; }   // new path: new baseline, same clock
		else if (rem < m_best - t.minDrop) { m_best = rem; m_bestAt = now; m_progressed = true; }
		bool standing = false;
		if (place)
		{
			const float dx = xy[0] - m_ax, dy = xy[1] - m_ay;
			if (!m_hasAnchor || dx * dx + dy * dy > t.placeRadius * t.placeRadius)
			{ m_hasAnchor = true; m_ax = xy[0]; m_ay = xy[1]; m_anchorAt = now; }
			standing = now - m_anchorAt >= t.placeWindow;
		}
		return standing || now - m_bestAt >= t.window;
	}
	bool Armed() const { return m_armed; }
	bool Progressed() const { return m_progressed; }  // this update dropped the best remaining by minDrop
	float Best() const { return m_best; }
	float StuckFor(float now) const
	{
		if (!m_armed) return 0.f;
		const float path = now - m_bestAt, place = m_hasAnchor ? now - m_anchorAt : 0.f;
		return path > place ? path : place;
	}
private:
	bool m_armed = false, m_progressed = false;
	float m_sig = 0.f, m_best = 0.f, m_bestAt = 0.f;
	bool m_hasAnchor = false;
	float m_ax = 0.f, m_ay = 0.f, m_anchorAt = 0.f;
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
	bool escaped = false;      // physical exit: moved >= 2.5 m from the stuck point toward the corner (UnstickEscaped)
	bool grounded = true;
	bool canJump = true;       // the NORMAL jump's own probe passed (head room 2.5 m, landing 2 m ahead)
	bool canStrength = false;  // the STRENGTH jump's own probe passed (head room 7 m, landing 3 m + 5.5 m), energy, no crowd
	uint64_t transition = 0;
};

// The caller's jump abilities, each from its own executor probe (BotLocomotion::JumpSpace with strength=false/true).
// The two probes check different landings, so one may pass while the other fails (Codex review of 3834ac64).
struct UnstickJumpAbility { bool canJump, canStrength; };
inline UnstickJumpAbility UnstickJumpInput(bool needJump, bool normalProbe, bool strengthProbe, float energy, bool crowdAhead)
{ return { needJump && normalProbe, needJump && strengthProbe && energy >= 130.f && !crowdAhead }; }
enum class UnstickEvent { None = 0, Started, Exited, Failed };

// The physical exit criterion: real new movement since the attempt began, never a position the bot already had.
// s = feet when the chain started, c = the corner it could not reach, next = the path point after it (or null), f = feet now.
// Server run 3ba5119c: exits on 0.4-1.3 m of remaining-path jitter while the bot stayed in its stall circle.
// Server run f9d33a47 (Codex 06.11): the corner was already 0.6-0.9 m away at the start, so "within reach of the corner"
// held at once and the steps exited with no movement at all (310 exits, actor 130953 at (2117,1880), 131018 at (2108,1384)).
// So: target = the corner, or the next point when the corner was in reach at the start (no next: no exit, the chain runs
// on to back-off); at least minMove of XY displacement from s; height within maxDz of the straight line s -> target
// (close XY on another floor or under a ledge does not count); then the target reached or its distance cut by `advance`.
inline bool UnstickEscaped(const float s[3], const float c[3], const float* next, const float f[3],
	float advance = 2.5f, float reach = 1.f, float minMove = 1.f, float maxDz = 1.f)
{
	auto d2 = [](const float* a, const float* b) { return (a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]); };
	const float* target = c;
	if (d2(s, c) <= reach * reach)
	{
		if (!next || d2(s, next) <= reach * reach) return false;
		target = next;
	}
	const float mx = f[0] - s[0], my = f[1] - s[1];
	if (mx * mx + my * my < minMove * minMove) return false;
	float dx = target[0] - s[0], dy = target[1] - s[1];
	const float len = std::sqrt(dx * dx + dy * dy);
	dx /= len; dy /= len;
	const float along = mx * dx + my * dy;
	const float k = along <= 0.f ? 0.f : along >= len ? 1.f : along / len;
	const float zLine = s[2] + (target[2] - s[2]) * k;
	if (std::fabs(f[2] - zLine) > maxDz) return false;
	if (d2(f, target) <= reach * reach) return true;
	// Codex 07.11 (0101d15e): a projection counts sideways movement while the distance grows (s=0, c=(1.1,0), f=(0.2,1)).
	// The exit is a real drop of the distance to the target: `advance`, or all the way into reach on a short leg.
	const float drop = len - std::sqrt(d2(f, target));
	return drop >= (advance < len - reach ? advance : len - reach);
}

// Back-off target: of the candidates (previous corner, both sides, straight back) the one farthest from allies within
// 12 m; on a tie the first. Server run 3ba5119c: backing off to the previous corner walked into the allies coming up
// the same path (4 bots within 5 m at 133-138 s).
template <class P>
int UnstickPickBackoff(const P* c, int n, const P* allies, int allyCount)
{
	int best = -1; float bestScore = -1.f;
	for (int i = 0; i < n; ++i)
	{
		float nearest = 12.f;
		for (int j = 0; j < allyCount; ++j)
		{
			const float d = std::sqrt((c[i].x - allies[j].x) * (c[i].x - allies[j].x) + (c[i].y - allies[j].y) * (c[i].y - allies[j].y));
			if (d < nearest) nearest = d;
		}
		if (nearest > bestScore + 0.25f) { bestScore = nearest; best = i; }
	}
	return best;
}

class UnstickSteps {
public:
	// 4 s to notice + 1.5 + 1.5 + 2 s of attempts: the back-off starts before the 10 s stall limit (server run 3ba5119c:
	// with 2.5 s steps the chain alone ran past 10 s)
	float sprintTime = 1.5f, jumpTime = 1.5f, strengthTime = 2.0f, backoffTime = 3.0f, waitTime = 5.f, banTime = 30.f;
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
		const bool trying = m_step == UnstickStep::Sprint || m_step == UnstickStep::Jump || m_step == UnstickStep::Strength;
		if (trying && in.escaped)
		{   // success is a physical exit during an attempt; back-off and wait end as Failed / None
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
		const float dur = m_step == UnstickStep::Backoff ? backoffTime : m_step == UnstickStep::Wait ? waitTime :
			m_step == UnstickStep::Sprint ? sprintTime : m_step == UnstickStep::Jump ? jumpTime : strengthTime;
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
		// Strength stands on its own probe: a failed normal landing must not veto a safe strength one (review of 3834ac64)
		if (s == UnstickStep::Strength && (!in.canStrength || StrengthUsed(in.transition))) s = UnstickStep::Backoff;
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

// ---------- path query timing (D-042 gate: p95 FindPathLinks <= 2 ms) ----------
// Wall-clock milliseconds of each FindPathLinks, measured with the cost layer ON and OFF alike (the control run needs the
// same numbers). Nearest-rank percentiles over the last window and over the whole level (capped sample count).
struct PathTimeReport { size_t n = 0; float p50 = 0.f, p95 = 0.f, max = 0.f; };
inline PathTimeReport PathTimePercentiles(std::vector<float> v)
{
	PathTimeReport r; r.n = v.size();
	if (v.empty()) return r;
	std::sort(v.begin(), v.end());
	const auto rank = [&](float p) { size_t k = size_t(std::ceil(p * float(v.size()))); return v[(k ? k : 1) - 1]; };
	r.p50 = rank(0.50f); r.p95 = rank(0.95f); r.max = v.back();
	return r;
}
class PathQueryTiming {
public:
	float period = 60.f;
	size_t levelCap = 200000;   // ~0.8 MB; later samples count only in the window report
	void Add(float ms) { m_window.push_back(ms); if (m_level.size() < levelCap) m_level.push_back(ms); else ++m_dropped; }
	bool Due(float now)
	{
		if (m_at < 0.f || now < m_at) { m_at = now; return false; }   // first call or restarted timer
		if (now - m_at < period) return false;
		m_at = now; return true;
	}
	PathTimeReport TakeWindow() { PathTimeReport r = PathTimePercentiles(m_window); m_window.clear(); return r; }
	PathTimeReport Level() const { return PathTimePercentiles(m_level); }
	size_t Dropped() const { return m_dropped; }
	void Clear() { m_window.clear(); m_level.clear(); m_dropped = 0; m_at = -1.f; }
private:
	std::vector<float> m_window, m_level;
	size_t m_dropped = 0;
	float m_at = -1.f;
};

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
