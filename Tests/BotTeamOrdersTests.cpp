// SPEC-005 stage 1: team commander tests (engine-free). Unit cases plus the property test N = 3..16.
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
	// SPEC-005b p. 8 (wb_bot_ps_hold): a taken prototype keeps one defender from 4 bots until TAC; off = as before.
	{
		TeamPlanConfig hold; hold.prototypeHoldFrom = 4;
		for (int nb = 3; nb <= 16; ++nb)
		{
			const std::vector<TeamTarget> t = Shore(1, 344);   // the team owns the prototype, the rest is free
			const auto off = PlanTeamOrders(t, Bots(nb), 1, 0.f, TeamEvent::None, none);
			const auto on = PlanTeamOrders(t, Bots(nb), 1, 0.f, TeamEvent::None, none, hold);
			const int defOn = CountOn(on, 344, true), defOff = CountOn(off, 344, true);
			if (nb < 4) CHECK(defOn == 0);
			else CHECK(defOn == 1);
			CHECK(defOff == (nb >= 5 ? 1 : 0));                 // flag OFF: D-054 as before
			int attackers = 0; for (const TeamOrder& x : on) attackers += !x.hold;
			if (nb == 4) CHECK(attackers == 3);                  // 4 bots: the defender and one triple attacking
			if (nb >= 4) CHECK(attackers >= 2);                  // at least one pair keeps attacking
			// after TAC the caller passes 0: the plan is exactly the stage-1 plan
			const auto tac = PlanTeamOrders(t, Bots(nb), 1, 0.f, TeamEvent::None, none, TeamPlanConfig());
			CHECK(tac.size() == off.size());
			for (const TeamOrder& x : off) { const TeamOrder* y = Of(tac, x.bot); CHECK(y && y->target == x.target && y->hold == x.hold); }
			// with >= 5 bots the rule changes nothing (the prototype is already defended first)
			if (nb >= 5) for (const TeamOrder& x : off) { const TeamOrder* y = Of(on, x.bot); CHECK(y && y->target == x.target && y->hold == x.hold); }
			// the defender stays on the prototype across an event re-plan
			const auto again = PlanTeamOrders(t, Bots(nb), 1, 30.f, TeamEvent::Captured, on, hold);
			if (nb >= 4) CHECK(CountOn(again, 344, true) == 1);
			for (const TeamOrder& x : on)
				if (x.hold && x.target == 344) { const TeamOrder* y = Of(again, x.bot); CHECK(y && y->target == 344 && y->hold); }
		}
		// a non-prototype own point below 5 bots gets no defender even with the rule on
		const auto o = PlanTeamOrders(Shore(1, 96), Bots(4), 1, 0.f, TeamEvent::None, none, hold);
		CHECK(CountOn(o, 96, true) == 0);
		// prototype and a bunker owned, 4 bots: only the prototype is defended
		std::vector<TeamTarget> two = Shore(1, 344); for (TeamTarget& x : two) if (x.id == 96) x.owner = 1;
		const auto o2 = PlanTeamOrders(two, Bots(4), 1, 0.f, TeamEvent::None, none, hold);
		CHECK(CountOn(o2, 344, true) == 1 && CountOn(o2, 96, true) == 0);
	}
	std::printf(failures ? "BotTeamOrdersTests FAIL %d\n" : "BotTeamOrdersTests PASS\n", failures);
	return failures ? 1 : 0;
}
