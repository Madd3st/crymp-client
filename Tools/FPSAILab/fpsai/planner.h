// Bounded symbolic planner over Facts (uint32 bitset). Forward uniform-cost search with a
// hard node budget and depth limit: our action set is ~12 and plans are 1-3 steps, so a
// regressive search (as in F.E.A.R.) buys nothing measurable; the budget is the safety net.
#pragma once
#include "core.h"
#include <algorithm>
#include <functional>
#include <queue>
#include <vector>

namespace fpsai {

enum class ActionId : uint8_t {
	Attack, Reload, Retreat, SeekCover, Reposition, Pursue, ChangeWeapon, HoldPosition,
	GoToWeapon, Search, Unstick, Roam, Count
};

inline const char* Name(ActionId a) {
	static const char* n[] = { "Attack","Reload","Retreat","SeekCover","Reposition","Pursue",
		"ChangeWeapon","HoldPosition","GoToWeapon","Search","Unstick","Roam" };
	return n[int(a)];
}

enum class Status : uint8_t { Running, Succeeded, Failed };

struct ActionDef {
	ActionId id;
	Facts pre = 0;        // facts that must hold
	Facts preNot = 0;     // facts that must not hold
	Facts add = 0;        // expected effects
	Facts clear = 0;
	float baseCost = 1;
	float timeout = 5;    // seconds; a running step older than this fails
	// Context precondition on the real observation: navigation must confirm what the symbol
	// claims (e.g. SeekCover needs a verified cover route). Only for things earlier plan steps
	// cannot change; anything a step can change must be a symbolic precondition. Empty = valid.
	std::function<bool(const Observation&)> valid;
	// Dynamic cost from the observation (route length, exposure). Empty = baseCost.
	std::function<float(const Observation&)> cost;
	// Completion and abort tests evaluated on every tick while the step runs.
	std::function<Status(const Observation&, float started)> progress;
	bool interruptible = true;
};

struct PlanStep { ActionId action; Facts expected; };
struct Plan {
	std::vector<PlanStep> steps;
	float cost = 0;
	int expanded = 0;
	bool budgetHit = false;
	bool empty() const { return steps.empty(); }
};

struct PlanLimits { int maxDepth = 4; int nodeBudget = 256; };

class Planner {
public:
	explicit Planner(std::vector<ActionDef> actions) : m_actions(std::move(actions)) {}
	const ActionDef& Def(ActionId id) const { return m_actions[size_t(id)]; }
	const std::vector<ActionDef>& Actions() const { return m_actions; }

	// Finds the cheapest sequence turning `start` into a state containing all `goal` bits.
	// `banned` actions are skipped (failure memory). Deterministic: ties break by action order.
	Plan Build(Facts start, Facts goal, const Observation& obs, uint32_t bannedMask, PlanLimits lim = {}) const {
		struct Node { Facts f; float g; int parent; ActionId a; int depth; };
		std::vector<Node> nodes; nodes.reserve(size_t(lim.nodeBudget) + 16);
		auto cmp = [&](int a, int b) { return nodes[a].g > nodes[b].g || (nodes[a].g == nodes[b].g && a > b); };
		std::priority_queue<int, std::vector<int>, decltype(cmp)> open(cmp);
		std::vector<std::pair<Facts, float>> best;
		nodes.push_back({ start, 0, -1, ActionId::Count, 0 });
		open.push(0);
		Plan plan;
		float costs[size_t(ActionId::Count)];
		bool ok[size_t(ActionId::Count)];
		for (const ActionDef& d : m_actions) {
			ok[size_t(d.id)] = !(bannedMask & (1u << unsigned(d.id))) && (!d.valid || d.valid(obs));
			costs[size_t(d.id)] = d.cost ? d.cost(obs) : d.baseCost;
		}
		while (!open.empty()) {
			const int cur = open.top(); open.pop();
			++plan.expanded;
			if ((nodes[cur].f & goal) == goal && cur != 0) {
				for (int n = cur; nodes[n].parent >= 0; n = nodes[n].parent)
					plan.steps.push_back({ nodes[n].a, nodes[n].f });
				std::reverse(plan.steps.begin(), plan.steps.end());
				plan.cost = nodes[cur].g;
				return plan;
			}
			if (plan.expanded >= lim.nodeBudget) { plan.budgetHit = true; break; }
			if (nodes[cur].depth >= lim.maxDepth) continue;
			for (const ActionDef& d : m_actions) {
				if (!ok[size_t(d.id)]) continue;
				const Facts f = nodes[cur].f;
				if ((f & d.pre) != d.pre || (f & d.preNot)) continue;
				const Facts next = (f & ~d.clear) | d.add;
				if (next == f) continue; // no symbolic progress
				const float g = nodes[cur].g + costs[size_t(d.id)];
				bool dominated = false;
				for (auto& b : best) if (b.first == next) { if (b.second <= g) dominated = true; else b.second = g; break; }
				if (dominated) continue;
				best.push_back({ next, g });
				nodes.push_back({ next, g, cur, d.id, nodes[cur].depth + 1 });
				open.push(int(nodes.size() - 1));
			}
		}
		return plan; // empty: no plan within limits
	}

private:
	std::vector<ActionDef> m_actions;
};

} // namespace fpsai
