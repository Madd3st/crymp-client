// SPEC-005 stage 1 (owner ok 07.11, D-054..D-056): team commander for Power Struggle. One commander per team hands
// every bot a task {target, role capture|hold, pair}. Pure logic, no engine types: the engine side (BotPSCapture.cpp)
// supplies targets with the navmesh path-length cache built at level load, the team's bots with their skill, and the
// events since the last plan. Tests/BotTeamOrdersTests.cpp (unit + property test N = 3..16).
#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

namespace WarsBots {

enum class PsKind { Energy = 0, Factory = 1, Prototype = 2, Bunker = 3, Spawn = 4 };

inline const char* PsKindName(PsKind k)
{
	switch (k)
	{
	case PsKind::Energy: return "energy";
	case PsKind::Factory: return "factory";
	case PsKind::Prototype: return "prototype";
	case PsKind::Bunker: return "bunker";
	default: return "spawn";
	}
}

struct TeamTarget
{
	int id = 0;
	PsKind kind = PsKind::Factory;
	int owner = 0;            // 0 neutral, else team id
	bool reachable = false;   // a navmesh path from the team base or from one of the team's own points (cache)
	float path = 1e9f;        // shortest cached path length to it, metres
	float quiet = 1e9f;       // D-059: seconds since an enemy was within 40 m of it (engine; capture counts as a contact)
};

struct TeamBot
{
	unsigned id = 0;          // actor entity id
	int skill = 1;            // BotSkill: 0 beginner .. 3 elite
	bool respawned = false;   // event: came back to life since the last plan
};

struct TeamOrder
{
	unsigned bot = 0;
	int target = 0;           // 0 with kind Spawn: hold the own spawn (nothing reachable)
	PsKind kind = PsKind::Spawn;
	bool hold = false;        // false = capture
	int pair = 0;             // 1.. for attacking pairs, 0 for defenders and spawn holders
	const char* reason = "start";
	float since = 0.f;        // time this task was given (the 20 s quiet-change rule)
};

// Events of one plan, highest priority first (SPEC-005 p. 5). The caller passes the strongest one seen since the
// last plan; respawn is per bot (TeamBot::respawned).
enum class TeamEvent { None = 0, Lost = 1, Captured = 2, Unreachable = 3, Roster = 5 };

inline const char* TeamEventName(TeamEvent e)
{
	switch (e)
	{
	case TeamEvent::Lost: return "lost";
	case TeamEvent::Captured: return "captured";
	case TeamEvent::Unreachable: return "unreachable";
	case TeamEvent::Roster: return "roster";
	default: return "rebalance";
	}
}

// Lower number = stronger event (respawn is handled per bot and beats all of these).
inline TeamEvent StrongerEvent(TeamEvent a, TeamEvent b)
{
	if (a == TeamEvent::None) return b;
	if (b == TeamEvent::None) return a;
	return int(a) <= int(b) ? a : b;
}

// SPEC-005 p. 6: combat is over when the bot has had no visible target for 5 s and was not hit for 5 s.
inline bool CombatOver(float now, float lastSeen, float lastHit, float quiet = 5.f)
{
	return now - lastSeen >= quiet && now - lastHit >= quiet;
}

struct TeamPlanConfig
{
	int defendFrom = 5;        // defenders only from 5 bots in the team (D-054 threshold; D-059 sets how many)
	int prototypeFrom = 4;     // owner 08.11: from 4 bots one pair goes to the prototype from the first plan (no energy gate);
	                           // D-059/SPEC-005b: from 4 bots the taken prototype keeps one defender until the team has TAC
	float quietChange = 20.f;  // a task changes without an event at most once per 20 s
	int maxDefenders = 2;      // D-059 (owner 09.11): no neutral point left -> at most 2 defenders per team (prototype apart)
	float releaseAfter = 45.f; // D-059: a point with no enemy within 40 m for 45 s loses its defender, who attacks
	bool teamHasTac = false;   // per call: the team got TAC from the prototype (no detection in this PR -> always false)
};

namespace TeamOrdersDetail {

inline int Rank(PsKind k) { return (k == PsKind::Bunker || k == PsKind::Energy) ? 0 : 1; }

// Defence order of own points: prototype, bunkers, factories, then energy; energy close to the own base is the first
// to go without a defender (the owner's ok 07.11: "energy far from the enemy").
inline int DefenceRank(PsKind k)
{
	switch (k)
	{
	case PsKind::Prototype: return 0;
	case PsKind::Bunker: return 1;
	case PsKind::Factory: return 2;
	default: return 3;
	}
}

inline const TeamOrder* FindPrev(const std::vector<TeamOrder>& prev, unsigned bot)
{
	for (const TeamOrder& o : prev) if (o.bot == bot) return &o;
	return nullptr;
}

} // namespace TeamOrdersDetail

// Free targets in commander order: the prototype first when `prototypeFirst` (owner 08.11: from 4 bots, from the first
// plan, no energy precondition), then bunkers and energy before factories, each by cached path length, id as tie-break.
inline std::vector<TeamTarget> FreeTargets(const std::vector<TeamTarget>& targets, int team, bool prototypeFirst)
{
	std::vector<TeamTarget> out;
	for (const TeamTarget& t : targets)
		if (t.kind != PsKind::Spawn && t.owner != team && t.reachable) out.push_back(t);
	std::sort(out.begin(), out.end(), [prototypeFirst](const TeamTarget& a, const TeamTarget& b)
	{
		const bool pa = prototypeFirst && a.kind == PsKind::Prototype, pb = prototypeFirst && b.kind == PsKind::Prototype;
		if (pa != pb) return pa;
		const int ra = TeamOrdersDetail::Rank(a.kind), rb = TeamOrdersDetail::Rank(b.kind);
		if (ra != rb) return ra < rb;
		if (a.path != b.path) return a.path < b.path;
		return a.id < b.id;
	});
	return out;
}

// Pairs from bots sorted by skill (count first, then skill): first with last; with an odd count the middle bot joins
// the pair with the lowest skill sum. Skill never changes a pair size. Input order does not matter.
inline std::vector<std::vector<TeamBot>> MakePairs(std::vector<TeamBot> bots)
{
	std::sort(bots.begin(), bots.end(), [](const TeamBot& a, const TeamBot& b)
	{ return a.skill != b.skill ? a.skill > b.skill : a.id < b.id; });
	const size_t n = bots.size();
	std::vector<std::vector<TeamBot>> pairs;
	if (n == 1) { pairs.push_back({bots[0]}); return pairs; }
	for (size_t i = 0; i < n / 2; ++i) pairs.push_back({bots[i], bots[n - 1 - i]});
	if (n & 1)
	{
		size_t best = 0; int bestSum = 1 << 30;
		for (size_t i = 0; i < pairs.size(); ++i)
		{
			const int s = pairs[i][0].skill + pairs[i][1].skill;
			if (s < bestSum) { bestSum = s; best = i; }
		}
		pairs[best].push_back(bots[n / 2]);
	}
	return pairs;
}

// One plan for one team. `prev` are the orders of the last plan (for the 20 s rule and to keep defenders in place).
inline std::vector<TeamOrder> PlanTeamOrders(const std::vector<TeamTarget>& targets, const std::vector<TeamBot>& bots,
	int team, float now, TeamEvent event, const std::vector<TeamOrder>& prev,
	const TeamPlanConfig& cfg = TeamPlanConfig())
{
	using namespace TeamOrdersDetail;
	std::vector<TeamOrder> desired;
	if (bots.empty()) return desired;
	const bool prototypeFirst = int(bots.size()) >= cfg.prototypeFrom;
	const std::vector<TeamTarget> freeT = FreeTargets(targets, team, prototypeFirst);
	// extra pairs (more pairs than free targets) go to the most valuable target that is not the prototype: one pair there
	size_t extra = 0;
	while (extra + 1 < freeT.size() && freeT[extra].kind == PsKind::Prototype) ++extra;
	// Codex #35 BLOCK: the prototype is the only free target - one pair there, the extra pairs hold the spawn as a reserve
	// (reason=reserve; own points keep their single D-054 defender), never a second pair on the prototype (D-057).
	const bool reserveOnly = !freeT.empty() && freeT[extra].kind == PsKind::Prototype;
	std::vector<TeamTarget> owned;
	for (const TeamTarget& t : targets) if (t.kind != PsKind::Spawn && t.owner == team && t.reachable) owned.push_back(t);
	std::sort(owned.begin(), owned.end(), [](const TeamTarget& a, const TeamTarget& b)
	{
		const int ra = DefenceRank(a.kind), rb = DefenceRank(b.kind);
		if (ra != rb) return ra < rb;
		if (a.kind == PsKind::Energy && a.path != b.path) return a.path > b.path; // far from the base = near the enemy
		if (a.path != b.path) return a.path < b.path;
		return a.id < b.id;
	});

	std::vector<TeamBot> pool(bots);
	auto take = [&pool](size_t i) { TeamBot b = pool[i]; pool.erase(pool.begin() + std::ptrdiff_t(i)); return b; };
	auto order = [](const TeamBot& b, const TeamTarget* t, bool hold, int pair)
	{
		TeamOrder o; o.bot = b.id; o.hold = hold; o.pair = pair;
		if (t) { o.target = t->id; o.kind = t->kind; } else { o.target = 0; o.kind = PsKind::Spawn; o.hold = true; }
		return o;
	};

	if (freeT.empty() && owned.empty())
	{
		for (const TeamBot& b : pool) { TeamOrder o = order(b, nullptr, true, 0); o.reason = "unreachable"; desired.push_back(o); }
		pool.clear();
	}

	// D-059 (owner 09.11, replaces D-054 "a defender on every taken point"): while a neutral reachable point is left, a
	// taken point keeps no defender (capture and go on); with no neutral point left at most `maxDefenders` per team, only
	// on points an enemy came within 40 m of in the last `releaseAfter` s. Exception: the taken prototype keeps one
	// defender from `prototypeFrom` bots until the team has TAC, outside the limit. With free targets one pair attacks.
	const int n = int(bots.size());
	bool neutralLeft = false;
	for (const TeamTarget& t : targets) if (t.kind != PsKind::Spawn && t.owner == 0 && t.reachable) neutralLeft = true;
	std::vector<const TeamTarget*> guard;
	if (!pool.empty() && n >= cfg.prototypeFrom && !cfg.teamHasTac)
		for (const TeamTarget& t : owned) if (t.kind == PsKind::Prototype) { guard.push_back(&t); break; }
	const int protoGuards = int(guard.size());
	if (!pool.empty() && n >= cfg.defendFrom && !neutralLeft)
	{
		const int cap = freeT.empty() ? cfg.maxDefenders : std::max(0, std::min(cfg.maxDefenders, n - 2 - protoGuards));
		int count = 0;
		for (const TeamTarget& t : owned)
		{
			if (count >= cap) break;
			if (protoGuards && t.id == guard[0]->id) continue;
			if (t.quiet >= cfg.releaseAfter) continue;   // no enemy near for 45 s: no defender, the bot attacks
			guard.push_back(&t); ++count;
		}
	}
	auto released = [&](int id)
	{
		for (const TeamTarget& t : owned)
			if (t.id == id) { for (const TeamTarget* g : guard) if (g->id == id) return false; return true; }
		return false;
	};
	{
		for (size_t d = 0; d < guard.size() && !pool.empty(); ++d)
		{
			const TeamTarget& t = *guard[d];
			size_t pick = pool.size();
			for (size_t i = 0; i < pool.size(); ++i)
			{
				const TeamOrder* p = FindPrev(prev, pool[i].id);
				if (p && p->target == t.id && p->hold) { pick = i; break; }   // keep the current defender
			}
			if (pick == pool.size())
				for (size_t i = 0; i < pool.size(); ++i)
				{
					const TeamOrder* p = FindPrev(prev, pool[i].id);
					if (p && p->target == t.id) { pick = i; break; }          // one of the pair that took it
				}
			if (pick == pool.size())
			{
				pick = 0;   // otherwise the weakest bot (lowest skill, then id)
				for (size_t i = 1; i < pool.size(); ++i)
					if (pool[i].skill < pool[pick].skill || (pool[i].skill == pool[pick].skill && pool[i].id < pool[pick].id)) pick = i;
			}
			desired.push_back(order(take(pick), &t, true, 0));
		}
	}

	if (!pool.empty())
	{
		const std::vector<std::vector<TeamBot>> pairs = MakePairs(pool);
		for (size_t p = 0; p < pairs.size(); ++p)
		{
			const bool reserve = reserveOnly && p >= freeT.size();
			const bool attack = !freeT.empty() && !reserve;
			const TeamTarget* t = attack ? &freeT[p < freeT.size() ? p : extra] : (reserve || owned.empty() ? nullptr : &owned[p % owned.size()]);
			for (const TeamBot& b : pairs[p])
			{
				TeamOrder o = order(b, t, !attack, attack ? int(p) + 1 : 0);
				if (reserve && !t) o.reason = "reserve";
				else if (!t) o.reason = "unreachable";
				desired.push_back(o);
			}
		}
	}

	// Stability: an event or a respawn changes a task at once; otherwise at most once per `quietChange` seconds, and only
	// while the old task is still valid (target known, reachable, and capture/hold still matches its owner).
	auto stillValid = [&](const TeamOrder& o)
	{
		if (o.kind == PsKind::Spawn) return (freeT.empty() && owned.empty()) || reserveOnly;
		for (const TeamTarget& t : targets)
			if (t.id == o.target) return t.reachable && (o.hold ? t.owner == team : t.owner != team);
		return false;
	};
	std::vector<TeamOrder> out;
	for (TeamOrder o : desired)
	{
		const TeamOrder* p = FindPrev(prev, o.bot);
		// D-059: a defender whose point lost its defender slot (quiet 45 s, a neutral point again, over the limit)
		// leaves at once, not after the 20 s quiet-change rule
		const bool freed = p && p->hold && p->kind != PsKind::Spawn && !(o.hold && o.target == p->target) && released(p->target);
		bool respawned = false;
		for (const TeamBot& b : bots) if (b.id == o.bot) respawned = b.respawned;
		if (p && p->target == o.target && p->hold == o.hold && p->kind == o.kind)
		{
			TeamOrder keep = o; keep.since = p->since; keep.reason = p->reason; out.push_back(keep); continue;
		}
		const bool valid = p && stillValid(*p);
		if (p && valid && !freed && !respawned && event == TeamEvent::None && now - p->since < cfg.quietChange)
		{
			out.push_back(*p); continue;   // a quiet change waits for the 20 s rule
		}
		o.since = now;
		if (o.kind == PsKind::Spawn && o.reason && o.reason[0] == 'r') o.reason = "reserve";   // sole-prototype reserve
		else if (o.kind == PsKind::Spawn || (o.reason && o.reason[0] == 'u')) o.reason = "unreachable";
		else if (!p) o.reason = "start";
		else if (respawned) o.reason = "respawn";
		else if (freed) o.reason = "quiet";
		else if (!valid) o.reason = event != TeamEvent::None ? TeamEventName(event) : "invalid";
		else if (event != TeamEvent::None) o.reason = TeamEventName(event);
		else o.reason = "rebalance";
		out.push_back(o);
	}
	return out;
}

} // namespace WarsBots
