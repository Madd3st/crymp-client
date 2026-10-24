// Scenario comparison + cost benchmark. Prints a Markdown report to stdout.
// Build: g++ -std=c++17 -O2 Tools/FPSAILab/fpsai_bench.cpp -o build/fpsai_bench
//   (MSVC: cl /std:c++17 /EHsc /O2 Tools\FPSAILab\fpsai_bench.cpp)
// Usage: fpsai_bench [--timeline]
#include "../../Tests/FPSAILab/scenarios.h"
#include <cstdio>
#include <cstring>
#include <random>

using namespace fpsai;

static std::string Timeline(const RunStats& s, float dt) {
	std::string out; ActionId cur = ActionId::Count; int n = 0; float start = 0;
	auto flush = [&](float t) { if (cur != ActionId::Count) { char b[64]; std::snprintf(b, sizeof b, "%s %.1f-%.1f; ", Name(cur), start, t); out += b; } };
	for (size_t i = 0; i < s.actions.size(); ++i) {
		if (s.actions[i] != cur) { flush(float(i) * dt); cur = s.actions[i]; start = float(i) * dt; }
		++n;
	}
	flush(float(s.actions.size()) * dt);
	return out;
}

int main(int argc, char** argv) {
	const bool timeline = argc > 1 && !std::strcmp(argv[1], "--timeline");
	std::printf("| Scenario | Policy | Action switches | Ping-pong (A-B-A < 3 s) | Walk-back ping-pong | Stalled s | Unreachable abandoned at s | Threat reaction s | Damage taken | Damage dealt | End HP | Hidden-info leaks | us/decision mean | p99 |\n");
	std::printf("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
	for (const Scenario& sc : scenarios::All()) {
		const float threatAt = sc.name[0] == 'E' ? 2.0f : -1.0f;
		Brain brain; LegacyModel legacy;
		RunStats r[2] = { Run(sc, legacy, "legacy", threatAt), Run(sc, brain, "planner", threatAt) };
		for (const RunStats& s : r)
			std::printf("| %s | %s | %d | %d | %d | %.1f | %s | %s | %.0f | %.0f | %.0f | %d | %.2f | %.2f |\n", sc.name.c_str(), s.policy.c_str(),
				s.actionSwitches, s.pingPong, s.movePingPong, s.stalledSeconds,
				s.firstAbandonUnreachable < 0 ? "-" : std::to_string(s.firstAbandonUnreachable).substr(0, 4).c_str(),
				s.firstReactionToThreat < 0 ? "-" : std::to_string(s.firstReactionToThreat).substr(0, 4).c_str(),
				s.damageTaken, s.damageDealt, s.endHealth, s.hiddenInfoViolations, s.usPerDecisionMean, s.usPerDecisionP99);
		if (timeline) {
			std::printf("|  | legacy: %s |\n|  | planner: %s |\n", Timeline(r[0], sc.dt).c_str(), Timeline(r[1], sc.dt).c_str());
			int shown = 0; float lastT = -1;
			for (const Decision& d : brain.Log()) if ((d.goalChanged || d.actionChanged) && shown < 12 && d.time != lastT) {
				std::printf("|  |   t=%.1f %s -> %s : %s |\n", d.time, Name(d.goal), Name(d.action), d.why.c_str()); ++shown; lastT = d.time; }
		}
	}
	// Cost: 15 bots, randomised observations, 10 Hz decisions, 60 simulated seconds.
	std::mt19937 rng(1);
	std::uniform_real_distribution<float> u01(0, 1);
	std::vector<Brain> bots(15); LegacyModel legacyBots[15];
	double planner = 0, legacy = 0; int calls = 0, plans = 0; int maxExpanded = 0;
	for (int tick = 0; tick < 600; ++tick) for (int b = 0; b < 15; ++b) {
		Observation o; o.time = tick * 0.1f; o.self = { u01(rng) * 50, u01(rng) * 50, 0 };
		o.health = 10 + u01(rng) * 90; o.clip = int(u01(rng) * 30); o.clipMax = 30; o.reserve = 60; o.hasPrimary = u01(rng) > 0.3f;
		o.altWeaponLoaded = u01(rng) > 0.5f; o.enemyVisible = u01(rng) > 0.5f; o.enemyKnown = o.enemyVisible || u01(rng) > 0.5f;
		o.enemyLastSeen = { u01(rng) * 50, u01(rng) * 50, 0 }; o.enemyLastSeenAge = o.enemyVisible ? 0 : u01(rng) * 10;
		o.visibleEnemies = o.enemyVisible ? 1 + int(u01(rng) * 2) : 0; o.lastDamageAge = u01(rng) * 3;
		o.cover = { u01(rng) > 0.3f, u01(rng) > 0.2f, { u01(rng) * 50, u01(rng) * 50, 0 }, u01(rng) * 30, u01(rng) * 0.6f, uint32_t(1 + tick % 5) };
		o.weaponItem = { !o.hasPrimary, u01(rng) > 0.4f, { 10, 10, 0 }, u01(rng) * 40, 1, 9 };
		o.flank = { u01(rng) > 0.5f, u01(rng) > 0.5f, { 5, 5, 0 }, u01(rng) * 20, 0.4f, 3 };
		o.routeToLastSeen = u01(rng) > 0.2f; o.positionExposure = u01(rng); o.stallSeconds = u01(rng) > 0.98f ? 7 : 0;
		auto t0 = std::chrono::steady_clock::now(); bots[b].Tick(o);
		auto t1 = std::chrono::steady_clock::now(); legacyBots[b].Tick(o);
		auto t2 = std::chrono::steady_clock::now();
		planner += std::chrono::duration<double, std::micro>(t1 - t0).count();
		legacy += std::chrono::duration<double, std::micro>(t2 - t1).count(); ++calls;
		const Decision& d = bots[b].Log().back(); maxExpanded = std::max(maxExpanded, d.expanded); plans += d.expanded > 0;
	}
	std::printf("\nRandomised stress (15 bots, 10 Hz, 60 s, %d decisions; observations change every tick, so this is a replanning worst case):\n", calls);
	std::printf("- planner %.2f us/decision mean, legacy model %.2f us/decision; 15 bots at 10 Hz = %.3f ms per second of game for the planner\n",
		planner / calls, legacy / calls, planner / calls * 150 / 1000.0);
	std::printf("- max planner nodes expanded in one decision: %d (budget 256)\n", maxExpanded);
	return 0;
}
