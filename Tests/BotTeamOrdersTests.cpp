// SPEC-005 stage 1: team commander tests (engine-free). Unit cases plus the property test N = 3..16.
// Cases 1-11 are stage 1 unchanged (wb_bot_ps_hold 0 must plan exactly as before); 12-13 are D-059 (wb_bot_ps_hold 1).
#include "CryGame/Bots/BotTeamOrders.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <set>

using namespace WarsBots;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static TeamTarget T(int id, PsKind k, int owner, bool reach, float path)
{ TeamTarget t; t.id = id; t.kind = k; t.owner = owner; t.reachable = reach; t.path = path; return t; }
static TeamBot B(unsigned id, int skill, bool respawned = false) { TeamBot b; b.id = id; b.skill = skill; b.respawned = respawned; return b; }
static std::vector<TeamBot> Bots(int n, const int* skills = nullptr)
{ std::vector<TeamBot> v; for (int i = 0; i < n; ++i) v.push_back(B(100u + unsigned(i), skills ? skills[i] : 1)); return v; }
static const TeamOrder* Of(const std::vector<TeamOrder>& o, unsigned bot) { for (const TeamOrder& x : o) if (x.bot == bot) return &x; return nullptr; }
static int CountOn(const std::vector<TeamOrder>& o, int target, bool hold)
{ int c = 0; for (const TeamOrder& x : o) if (x.target == target && x.hold == hold) ++c; return c; }

// Shore-like set: 2 energy, 4 bunkers, 2 factories, 1 prototype.
static std::vector<TeamTarget> Shore(int team, int ownedId = 0)
{
	std::vector<TeamTarget> t = {
		T(64, PsKind::Energy, 0, true, 300), T(316, PsKind::Energy, 0, true, 450),
		T(211, PsKind::Bunker, 0, true, 350), T(96, PsKind::Bunker, 0, true, 200), T(689, PsKind::Bunker, 0, true, 500), T(243, PsKind::Bunker, 0, true, 600),
		T(654, PsKind::Factory, 0, true, 150), T(579, PsKind::Factory, 0, true, 400), T(344, PsKind::Prototype, 0, true, 520)};
	for (TeamTarget& x : t) if (x.id == ownedId) x.owner = team;
	return t;
}

// Tiny deterministic generator (no <random> distribution differences between compilers).
static unsigned g_seed = 12345u;
static unsigned Rnd() { g_seed = g_seed * 1103515245u + 12345u; return (g_seed >> 16) & 0x7fffu; }

int main()
{
	const std::vector<TeamOrder> none;
	// 1. Order: bunkers and energy before factories, by path; with prototypeFirst (>= 4 bots) the prototype leads.
	{
		const std::vector<TeamTarget> f = FreeTargets(Shore(1), 1, false);
		CHECK(f.size() == 9);
		CHECK(f[0].id == 96 && f[1].id == 64 && f[2].id == 211);            // rank 0 by path 200/300/350
		CHECK(f[6].id == 654 && f[7].id == 579 && f[8].id == 344);          // factories by path, prototype last (520)
		const std::vector<TeamTarget> g = FreeTargets(Shore(1), 1, true);
		CHECK(g[0].id == 344);                                              // owner 08.11: prototype first, no energy gate
	}
	// 2. Pairs: count first, then skill (strong with weak); odd count -> one triple on the lowest-sum pair.
	{
		const int s[5] = {3, 0, 2, 1, 1};
		const auto pairs = MakePairs(Bots(5, s));
		CHECK(pairs.size() == 2);
		CHECK(pairs[0].size() + pairs[1].size() == 5);
		CHECK(pairs[0][0].skill == 3 && pairs[0][1].skill == 0);
		const int s7[7] = {3, 3, 3, 3, 3, 3, 0};
		const auto p7 = MakePairs(Bots(7, s7));                              // skill never changes the size
		CHECK(p7.size() == 3);
		int triples = 0; for (const auto& p : p7) { CHECK(p.size() == 2 || p.size() == 3); triples += p.size() == 3; }
		CHECK(triples == 1);
	}
	// 3. Start, 7 bots, nothing owned: 3 pairs on 3 different free targets, no defenders.
	{
		const auto o = PlanTeamOrders(Shore(2), Bots(7), 2, 0.f, TeamEvent::None, none);
		CHECK(o.size() == 7);
		std::set<int> targets; for (const TeamOrder& x : o) { CHECK(!x.hold); CHECK(x.pair >= 1); targets.insert(x.target); CHECK(std::strcmp(x.reason, "start") == 0); }
		CHECK(targets.size() == 3);
		CHECK(CountOn(o, 344, false) >= 2 && CountOn(o, 96, false) >= 2 && CountOn(o, 64, false) >= 2);   // owner 08.11: prototype pair first
	}
	// 4. D-054: >= 5 bots -> exactly one defender on the taken point; the rest attack.
	{
		const auto o = PlanTeamOrders(Shore(1, 96), Bots(5), 1, 0.f, TeamEvent::Captured, none);
		CHECK(CountOn(o, 96, true) == 1);
		int attackers = 0; for (const TeamOrder& x : o) if (!x.hold) { ++attackers; CHECK(x.target != 96); }
		CHECK(attackers == 4);
	}
	// 5. < 5 bots: no defender while free targets remain.
	{
		const auto o = PlanTeamOrders(Shore(1, 96), Bots(4), 1, 0.f, TeamEvent::Captured, none);
		for (const TeamOrder& x : o) CHECK(!x.hold);
	}
	// 6. Many own points: at least one pair keeps attacking (owner ok 07.11); energy near the base loses its defender first.
	{
		std::vector<TeamTarget> t = Shore(1);
		for (TeamTarget& x : t) if (x.id != 579) x.owner = 1;               // 8 own, 1 free
		const auto o = PlanTeamOrders(t, Bots(6), 1, 0.f, TeamEvent::Captured, none);
		int def = 0, att = 0; for (const TeamOrder& x : o) (x.hold ? def : att)++;
		CHECK(def == 4 && att == 2);
		CHECK(CountOn(o, 344, true) == 1);                                  // prototype defended first
		CHECK(CountOn(o, 64, true) == 0 && CountOn(o, 316, true) == 0);     // energy left without a defender
		CHECK(CountOn(o, 579, false) == 2);
	}
	// 7. Reachability gate: no task on an unreachable target; nothing reachable -> hold the spawn, reason=unreachable.
	{
		std::vector<TeamTarget> t = Shore(1);
		t[0].reachable = false;                                              // 64
		const auto o = PlanTeamOrders(t, Bots(15), 1, 0.f, TeamEvent::None, none);
		CHECK(CountOn(o, 64, false) == 0 && CountOn(o, 64, true) == 0);
		for (TeamTarget& x : t) x.reachable = false;
		const auto s = PlanTeamOrders(t, Bots(7), 1, 0.f, TeamEvent::None, none);
		for (const TeamOrder& x : s) { CHECK(x.kind == PsKind::Spawn && x.target == 0 && x.hold); CHECK(std::strcmp(x.reason, "unreachable") == 0); }
		// the next plan without change keeps the same task and reason (no ping-pong, one log line per change)
		const auto s2 = PlanTeamOrders(t, Bots(7), 1, 30.f, TeamEvent::None, s);
		for (size_t i = 0; i < s2.size(); ++i) CHECK(s2[i].since == 0.f);
	}
	// 8. Stability: a quiet change waits 20 s; an event or a respawn changes at once.
	{
		std::vector<TeamOrder> prev = PlanTeamOrders(Shore(2), Bots(4), 2, 0.f, TeamEvent::None, none);
		const unsigned bot = prev[0].bot; const int was = prev[0].target;
		std::vector<TeamTarget> t = Shore(2);
		for (TeamTarget& x : t) if (x.id == 96) x.path = 900;               // the order of free targets changes quietly
		auto o = PlanTeamOrders(t, Bots(4), 2, 10.f, TeamEvent::None, prev);
		CHECK(Of(o, bot)->target == was && Of(o, bot)->since == 0.f);
		o = PlanTeamOrders(t, Bots(4), 2, 21.f, TeamEvent::None, prev);
		bool changed = false; for (const TeamOrder& x : o) if (x.since == 21.f) { changed = true; CHECK(std::strcmp(x.reason, "rebalance") == 0); }
		CHECK(changed);
		o = PlanTeamOrders(t, Bots(4), 2, 10.f, TeamEvent::Captured, prev);
		changed = false; for (const TeamOrder& x : o) if (x.since == 10.f) { changed = true; CHECK(std::strcmp(x.reason, "captured") == 0); }
		CHECK(changed);
		std::vector<TeamBot> rb = Bots(4); for (TeamBot& b : rb) b.respawned = true;
		o = PlanTeamOrders(t, rb, 2, 10.f, TeamEvent::None, prev);
		for (const TeamOrder& x : o) if (x.since == 10.f) CHECK(std::strcmp(x.reason, "respawn") == 0);
	}
	// 9. Owner 08.11: from 4 bots exactly one pair on the prototype from the first plan; with 3 bots none; extra pairs
	//    never stack on the prototype; an invalid old task changes inside 20 s.
	{
		auto o4 = PlanTeamOrders(Shore(2), Bots(4), 2, 0.f, TeamEvent::None, none);
		CHECK(CountOn(o4, 344, false) == 2);
		auto o3 = PlanTeamOrders(Shore(2), Bots(3), 2, 0.f, TeamEvent::None, none);
		CHECK(CountOn(o3, 344, false) == 0);
		std::vector<TeamTarget> two = {T(344, PsKind::Prototype, 0, true, 500), T(96, PsKind::Bunker, 0, true, 200)};
		auto o8 = PlanTeamOrders(two, Bots(8), 2, 0.f, TeamEvent::None, none);   // 4 pairs, 2 targets
		CHECK(CountOn(o8, 344, false) == 2 && CountOn(o8, 96, false) == 6);
		// Codex #35 BLOCK: the prototype is the only free reachable target - one pair, the rest is a reserve
		for (int nb : {4, 7, 8})
		{
			std::vector<TeamTarget> sole = {T(344, PsKind::Prototype, 0, true, 520)};
			auto os = PlanTeamOrders(sole, Bots(nb), 2, 0.f, TeamEvent::None, none);
			const int onP = CountOn(os, 344, false);
			CHECK(onP >= 2 && onP <= 3 && onP * 2 <= nb + 2);
			int reserve = 0; for (const TeamOrder& x : os) if (x.kind == PsKind::Spawn) { CHECK(x.hold && std::strcmp(x.reason, "reserve") == 0); ++reserve; }
			CHECK(reserve == nb - onP);
			auto again = PlanTeamOrders(sole, Bots(nb), 2, 5.f, TeamEvent::None, os);   // stable: the reserve stays a reserve
			CHECK(CountOn(again, 344, false) == onP);
			// with another own point and an unreachable bunker: one pair on 344, one D-054 defender from 5 bots, rest reserve
			std::vector<TeamTarget> mix = {T(344, PsKind::Prototype, 0, true, 520), T(96, PsKind::Bunker, 2, true, 200), T(211, PsKind::Bunker, 0, false, 350)};
			auto om = PlanTeamOrders(mix, Bots(nb), 2, 0.f, TeamEvent::None, none);
			const int onPm = CountOn(om, 344, false);
			CHECK(onPm >= 2 && onPm <= 3);
			CHECK(CountOn(om, 211, false) == 0 && CountOn(om, 211, true) == 0);
			CHECK(CountOn(om, 96, true) == (nb >= 5 ? 1 : 0));
			int reserveM = 0; for (const TeamOrder& x : om) if (x.kind == PsKind::Spawn && std::strcmp(x.reason, "reserve") == 0) ++reserveM;
			CHECK(reserveM == nb - onPm - (nb >= 5 ? 1 : 0));
		}
		std::vector<TeamOrder> prev = PlanTeamOrders(Shore(1), Bots(4), 1, 0.f, TeamEvent::None, none);
		std::vector<TeamTarget> t = Shore(1, prev[0].target);           // the first pair's target was just taken
		auto o = PlanTeamOrders(t, Bots(4), 1, 3.f, TeamEvent::None, prev);
		CHECK(o[0].target != prev[0].target && o[0].since == 3.f);
	}
	// 10. Event priority and combat-over definition.
	{
		CHECK(StrongerEvent(TeamEvent::Captured, TeamEvent::Lost) == TeamEvent::Lost);
		CHECK(StrongerEvent(TeamEvent::None, TeamEvent::Roster) == TeamEvent::Roster);
		CHECK(StrongerEvent(TeamEvent::Unreachable, TeamEvent::Captured) == TeamEvent::Captured);
		CHECK(!CombatOver(10.f, 6.f, 0.f) && !CombatOver(10.f, 0.f, 6.f) && CombatOver(10.f, 5.f, 5.f));
	}
	// 11. Property test: N = 3..16, 1000 random skill sets per N, random ownership/reachability.
	for (int n = 3; n <= 16; ++n)
		for (int rep = 0; rep < 1000; ++rep)
		{
			std::vector<TeamBot> bots;
			int smax = 0, smin = 3;
			for (int i = 0; i < n; ++i) { const int s = int(Rnd() % 4); smax = std::max(smax, s); smin = std::min(smin, s); bots.push_back(B(1000u + unsigned(i), s)); }
			std::vector<TeamTarget> t = Shore(1);
			for (TeamTarget& x : t) { const unsigned r = Rnd() % 10; x.owner = r < 3 ? 1 : r < 5 ? 2 : 0; x.reachable = Rnd() % 8 != 0; }
			const auto o = PlanTeamOrders(t, bots, 1, 0.f, TeamEvent::None, none);
			// every bot exactly one task
			CHECK(int(o.size()) == n);
			std::set<unsigned> seen; for (const TeamOrder& x : o) seen.insert(x.bot);
			CHECK(int(seen.size()) == n);
			// no task on an unreachable target; hold only on own, capture only on not own
			std::map<int, const TeamTarget*> byId; for (const TeamTarget& x : t) byId[x.id] = &x;
			int ownedReach = 0, freeReach = 0;
			for (const TeamTarget& x : t) if (x.reachable) (x.owner == 1 ? ownedReach : freeReach)++;
			for (const TeamOrder& x : o)
			{
				if (x.kind == PsKind::Spawn)
				{   // spawn hold: nothing reachable, or the sole-prototype reserve (Codex #35)
					bool onlyProto = freeReach > 0;
					for (const TeamTarget& y : t) if (y.reachable && y.owner != 1 && y.kind != PsKind::Prototype) onlyProto = false;
					CHECK((ownedReach == 0 && freeReach == 0) || (onlyProto && std::strcmp(x.reason, "reserve") == 0));
					continue;
				}
				const TeamTarget* tt = byId[x.target];
				CHECK(tt && tt->reachable);
				if (tt) CHECK(x.hold ? tt->owner == 1 : tt->owner != 1);
			}
			// attacking pairs: sizes 2, exactly one triple when the attacker count is odd (and > 1)
			std::map<int, std::vector<int>> pairs;
			for (const TeamOrder& x : o) if (x.pair > 0) { const TeamBot* b = nullptr; for (const TeamBot& y : bots) if (y.id == x.bot) b = &y; pairs[x.pair].push_back(b ? b->skill : 0); }
			int attackers = 0, triples = 0, lo = 1 << 30, hi = -1;
			for (auto& p : pairs)
			{
				attackers += int(p.second.size());
				CHECK(p.second.size() >= 1 && p.second.size() <= 3);
				triples += p.second.size() == 3;
				if (p.second.size() >= 2) { std::sort(p.second.begin(), p.second.end()); const int two = p.second.front() + p.second.back(); lo = std::min(lo, two); hi = std::max(hi, two); }
			}
			if (attackers > 1) CHECK(triples == (attackers & 1));
			if (hi >= 0) CHECK(hi - lo <= smax - smin);
			// D-054: >= 5 bots -> at most one defender per own point, exactly one while bots allow it
			std::map<int, int> def; for (const TeamOrder& x : o) if (x.hold && x.kind != PsKind::Spawn && freeReach) def[x.target]++;
			for (const auto& d : def) CHECK(d.second == 1);
			if (n >= 5 && freeReach) CHECK(int(def.size()) == std::min(ownedReach, n - 2));
			if (n < 5 && freeReach) CHECK(def.empty());
			if (freeReach) CHECK(attackers >= 2 || n < 2);
			// owner 08.11: from 4 bots and a free reachable prototype exactly one pair (2-3 bots) attacks it
			{
				bool protoFree = false; for (const TeamTarget& x : t) if (x.kind == PsKind::Prototype && x.owner != 1 && x.reachable) protoFree = true;
				int onProto = 0; std::set<int> protoPairs;
				for (const TeamOrder& x : o) if (!x.hold && x.kind == PsKind::Prototype) { ++onProto; protoPairs.insert(x.pair); }
				if (n >= 4 && protoFree && attackers >= 2 && freeReach > 1) CHECK(protoPairs.size() == 1 && onProto >= 2 && onProto <= 3);
				if (protoFree) CHECK(protoPairs.size() <= 1 && onProto <= 3);   // never a second pair, also when it is the sole target
				if (n < 4 && freeReach > 1) CHECK(protoPairs.size() <= 1);
			}
			// input order does not change the result
			std::vector<TeamBot> rev(bots.rbegin(), bots.rend());
			const auto o2 = PlanTeamOrders(t, rev, 1, 0.f, TeamEvent::None, none);
			for (const TeamOrder& x : o) { const TeamOrder* y = Of(o2, x.bot); CHECK(y && y->target == x.target && y->hold == x.hold); }
		}
	// ---- D-059 (owner 09.11, wb_bot_ps_hold 1) ----
	TeamPlanConfig D59; D59.d059 = true;
	auto holds = [](const std::vector<TeamOrder>& o) { int c = 0; for (const TeamOrder& x : o) if (x.hold) ++c; return c; };
	auto roams = [](const std::vector<TeamOrder>& o)
	{ int c = 0; for (const TeamOrder& x : o) if (x.kind == PsKind::Spawn && !x.hold && x.target == 0 && std::strcmp(x.reason, "roam") == 0) ++c; return c; };
	// 12a. neutral points left -> no defender on the taken point, all 5 attack (stage 1: one defender, case 4).
	{
		std::vector<TeamTarget> t = Shore(1, 96); for (TeamTarget& x : t) x.quiet = 0.f;   // even with an enemy near
		const auto o = PlanTeamOrders(t, Bots(5), 1, 0.f, TeamEvent::Captured, none, D59);
		CHECK(CountOn(o, 96, true) == 0);
		int attackers = 0; for (const TeamOrder& x : o) if (!x.hold) { ++attackers; CHECK(x.target != 96); }
		CHECK(attackers == 5);
	}
	// 12b. no neutral point left -> at most 2 defenders, only where an enemy was near in the last 45 s; the prototype
	//      keeps one defender until TAC outside the limit; a defender of a quiet point leaves at once (reason=quiet).
	{
		std::vector<TeamTarget> t = Shore(1);
		for (TeamTarget& x : t) { x.owner = (x.id == 579 || x.id == 243) ? 2 : 1; x.quiet = 10.f; }   // 7 own, 2 enemy, none neutral
		auto o = PlanTeamOrders(t, Bots(8), 1, 0.f, TeamEvent::Captured, none, D59);
		CHECK(holds(o) == 3 && CountOn(o, 344, true) == 1);                // 2 + the prototype
		CHECK(CountOn(o, 579, false) + CountOn(o, 243, false) == 5);
		TeamPlanConfig tac = D59; tac.teamHasTac = true;                     // TAC: the prototype is an ordinary point then
		CHECK(holds(PlanTeamOrders(t, Bots(8), 1, 0.f, TeamEvent::Captured, none, tac)) == 2);
		std::vector<TeamTarget> q = t; for (TeamTarget& x : q) x.quiet = 50.f;
		auto oq = PlanTeamOrders(q, Bots(8), 1, 5.f, TeamEvent::None, o, D59);
		CHECK(holds(oq) == 1 && CountOn(oq, 344, true) == 1);
		int quietReason = 0;
		for (const TeamOrder& x : oq) { const TeamOrder* w = Of(o, x.bot); if (w && w->hold && w->kind != PsKind::Prototype) { CHECK(!x.hold && x.since == 5.f && std::strcmp(x.reason, "quiet") == 0); ++quietReason; } }
		CHECK(quietReason == 2);
		std::vector<TeamTarget> pn = Shore(1, 344); for (TeamTarget& x : pn) x.quiet = 1e9f;   // prototype kept with neutral left
		CHECK(CountOn(PlanTeamOrders(pn, Bots(4), 1, 0.f, TeamEvent::Captured, none, D59), 344, true) == 1);
		CHECK(CountOn(PlanTeamOrders(pn, Bots(3), 1, 0.f, TeamEvent::Captured, none, D59), 344, true) == 0);
		// many own points, one enemy point: prototype + 2 bunkers by path, energy without a defender, the rest attacks
		std::vector<TeamTarget> m = Shore(1);
		for (TeamTarget& x : m) { x.owner = x.id == 579 ? 2 : 1; x.quiet = 0.f; }
		const auto om = PlanTeamOrders(m, Bots(6), 1, 0.f, TeamEvent::Captured, none, D59);
		CHECK(holds(om) == 3 && CountOn(om, 344, true) == 1 && CountOn(om, 96, true) == 1 && CountOn(om, 211, true) == 1);
		CHECK(CountOn(om, 64, true) == 0 && CountOn(om, 316, true) == 0 && CountOn(om, 579, false) == 3);
	}
	// 12c. Codex BLOCK 1588030a (defence-edges.cpp): no free target - the rest are never handed out as holds past the
	//      limit. 8 bots, one own bunker: quiet 100 -> 0 holds, 8 roam; enemy near -> 1 hold, 7 roam; a quiet bunker with
	//      8 old holders frees them all at once (reason=quiet); many own points -> at most 2 (+ prototype) hold.
	{
		std::vector<TeamTarget> one = {T(96, PsKind::Bunker, 1, true, 200)};
		one[0].quiet = 100.f;
		auto o = PlanTeamOrders(one, Bots(8), 1, 0.f, TeamEvent::Captured, none, D59);
		CHECK(o.size() == 8 && holds(o) == 0 && roams(o) == 8);
		one[0].quiet = 10.f;
		auto h = PlanTeamOrders(one, Bots(8), 1, 0.f, TeamEvent::Captured, none, D59);
		CHECK(holds(h) == 1 && CountOn(h, 96, true) == 1 && roams(h) == 7);
		std::vector<TeamOrder> eight; for (const TeamBot& b : Bots(8)) { TeamOrder x; x.bot = b.id; x.target = 96; x.kind = PsKind::Bunker; x.hold = true; x.reason = "captured"; eight.push_back(x); }
		one[0].quiet = 100.f;
		auto f = PlanTeamOrders(one, Bots(8), 1, 3.f, TeamEvent::None, eight, D59);
		CHECK(holds(f) == 0);
		for (const TeamOrder& x : f) CHECK(x.kind == PsKind::Spawn && !x.hold && x.since == 3.f && std::strcmp(x.reason, "quiet") == 0);
		auto again = PlanTeamOrders(one, Bots(8), 1, 8.f, TeamEvent::None, f, D59);   // roam stays roam, no new line
		for (const TeamOrder& x : again) CHECK(!x.hold && x.since == 3.f);
		std::vector<TeamTarget> all = Shore(1); for (TeamTarget& x : all) { x.owner = 1; x.quiet = 0.f; }
		for (int nb : {5, 8, 15})
		{
			auto a = PlanTeamOrders(all, Bots(nb), 1, 0.f, TeamEvent::Captured, none, D59);
			CHECK(holds(a) == 3 && CountOn(a, 344, true) == 1 && roams(a) == nb - 3);
		}
		// stage 1 for the same case keeps D-054 (every bot holds an own point) - the OFF behaviour is untouched
		CHECK(holds(PlanTeamOrders(one, Bots(8), 1, 0.f, TeamEvent::Captured, none)) == 8);
	}
	// 12d. Codex BLOCK 1588030a: "neutral points on the map" counts every neutral point, reachable or not. An
	//      unreachable neutral prototype 344 + an own bunker with an enemy near + a reachable enemy target -> 0 defenders.
	{
		std::vector<TeamTarget> t = {T(344, PsKind::Prototype, 0, false, 520), T(96, PsKind::Bunker, 1, true, 200), T(211, PsKind::Bunker, 2, true, 350)};
		t[1].quiet = 0.f;
		auto o = PlanTeamOrders(t, Bots(8), 1, 0.f, TeamEvent::Captured, none, D59);
		CHECK(holds(o) == 0 && CountOn(o, 211, false) == 8 && CountOn(o, 344, false) == 0);
		t[0].owner = 2;                                                       // no neutral left: the bunker gets its defender
		auto o2 = PlanTeamOrders(t, Bots(8), 1, 0.f, TeamEvent::Captured, none, D59);
		CHECK(holds(o2) == 1 && CountOn(o2, 96, true) == 1);
	}
	// 12e. the prototype pair: one of the pair stays on the taken prototype until TAC, the other moves at once.
	{
		std::vector<TeamOrder> prev = PlanTeamOrders(Shore(1), Bots(4), 1, 0.f, TeamEvent::None, none, D59);
		std::vector<TeamTarget> t = Shore(1, prev[0].target);
		auto o = PlanTeamOrders(t, Bots(4), 1, 3.f, TeamEvent::None, prev, D59);
		CHECK(prev[0].kind == PsKind::Prototype);
		const TeamOrder* a = Of(o, prev[0].bot); const TeamOrder* b = Of(o, prev[1].bot);
		CHECK(prev[1].target == prev[0].target && a && b);
		if (a && b)
		{
			CHECK(a->hold != b->hold);
			const TeamOrder* mover = a->hold ? b : a; const TeamOrder* keeper = a->hold ? a : b;
			CHECK(keeper->target == prev[0].target && mover->target != prev[0].target && mover->since == 3.f);
		}
		TeamPlanConfig tac = D59; tac.teamHasTac = true;                     // with TAC (and neutral left) nobody stays
		CHECK(holds(PlanTeamOrders(t, Bots(4), 1, 3.f, TeamEvent::None, prev, tac)) == 0);
	}
	// 13. D-059 property test: N = 3..16, 1000 random sets per N, random ownership/reachability/contact/TAC.
	for (int n = 3; n <= 16; ++n)
		for (int rep = 0; rep < 1000; ++rep)
		{
			std::vector<TeamBot> bots;
			for (int i = 0; i < n; ++i) bots.push_back(B(1000u + unsigned(i), int(Rnd() % 4)));
			std::vector<TeamTarget> t = Shore(1);
			for (TeamTarget& x : t) { const unsigned r = Rnd() % 10; x.owner = r < 3 ? 1 : r < 5 ? 2 : r < 7 ? 0 : (Rnd() % 2 ? 1 : 2); x.reachable = Rnd() % 8 != 0; x.quiet = Rnd() % 2 ? 10.f : 60.f; }
			TeamPlanConfig cfg = D59; cfg.teamHasTac = Rnd() % 4 == 0;
			const auto o = PlanTeamOrders(t, bots, 1, 0.f, TeamEvent::None, none, cfg);
			CHECK(int(o.size()) == n);
			std::set<unsigned> seen; for (const TeamOrder& x : o) seen.insert(x.bot);
			CHECK(int(seen.size()) == n);
			std::map<int, const TeamTarget*> byId; for (const TeamTarget& x : t) byId[x.id] = &x;
			int ownedReach = 0, freeReach = 0; bool neutral = false, protoOwn = false, protoHot = false; int hot = 0;
			for (const TeamTarget& x : t)
			{
				if (x.owner == 0) neutral = true;                                  // reachable or not
				if (!x.reachable) continue;
				(x.owner == 1 ? ownedReach : freeReach)++;
				if (x.owner == 1 && x.kind == PsKind::Prototype) { protoOwn = true; protoHot = x.quiet < 45.f; }
				else if (x.owner == 1 && x.quiet < 45.f) ++hot;
			}
			bool onlyProto = freeReach > 0;
			for (const TeamTarget& y : t) if (y.reachable && y.owner != 1 && y.kind != PsKind::Prototype) onlyProto = false;
			std::map<int, int> def; int holdsN = 0;
			for (const TeamOrder& x : o)
			{
				if (x.kind == PsKind::Spawn)
				{
					if (!x.hold) { CHECK(freeReach == 0 && ownedReach > 0 && std::strcmp(x.reason, "roam") == 0); continue; }
					CHECK((ownedReach == 0 && freeReach == 0) || (onlyProto && std::strcmp(x.reason, "reserve") == 0));
					continue;
				}
				const TeamTarget* tt = byId[x.target];
				CHECK(tt && tt->reachable);
				if (tt) CHECK(x.hold ? tt->owner == 1 : tt->owner != 1);
				if (x.hold) { def[x.target]++; ++holdsN; }
			}
			// the budget: never more than one per point, the prototype (from 4 bots, until TAC) + at most 2 others,
			// none while a neutral point is on the map, only on points with an enemy near in the last 45 s
			for (const auto& d : def) CHECK(d.second == 1);
			const int proto = (protoOwn && n >= 4 && !cfg.teamHasTac) ? 1 : 0;
			const int cap = freeReach ? std::max(0, std::min(2, n - 2 - proto)) : 2;
			const int rest = (neutral || n < 5) ? 0 : std::min(hot + (proto ? 0 : int(protoHot)), cap);
			if (ownedReach) CHECK(holdsN == proto + rest);
			for (const auto& d : def) { const TeamTarget* tt = byId[d.first]; CHECK(tt && ((tt->kind == PsKind::Prototype && proto) || (!neutral && tt->quiet < 45.f))); }
			std::vector<TeamBot> rev(bots.rbegin(), bots.rend());
			const auto o2 = PlanTeamOrders(t, rev, 1, 0.f, TeamEvent::None, none, cfg);
			for (const TeamOrder& x : o) { const TeamOrder* y = Of(o2, x.bot); CHECK(y && y->target == x.target && y->hold == x.hold); }
		}
	std::printf(failures ? "BotTeamOrdersTests FAIL %d\n" : "BotTeamOrdersTests PASS\n", failures);
	return failures ? 1 : 0;
}
