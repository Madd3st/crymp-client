// Build: g++ -std=c++17 -O2 Tests/FPSAILab/test_fpsai.cpp -o build/test_fpsai   (MSVC: cl /std:c++17 /EHsc /O2)
#include "scenarios.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace fpsai;
static int g_fail = 0, g_pass = 0;
#define CHECK(c, msg) do { if (c) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, msg); } } while (0)

static bool Any(const RunStats& s, ActionId a) { for (ActionId x : s.actions) if (x == a) return true; return false; }
static ActionId First(const RunStats& s, std::initializer_list<ActionId> of) {
	for (ActionId x : s.actions) { for (ActionId y : of) if (x == y) return x; }
	return ActionId::Count; }

int main() {
	using namespace scenarios;
	// Planner unit checks.
	{
		Brain b; const Planner& p = b.GetPlanner(); Observation o; o.cover = { true, true, V3(1, 0, 0), 3, 0.1f, 1 };
		o.enemyKnown = true; o.enemyVisible = true; o.clip = 0; o.reserve = 30; o.altWeaponLoaded = true;
		Facts f = DeriveFacts(o, {});
		Plan plan = p.Build(f, F_EnemyDead, o, 0);
		CHECK(plan.steps.size() == 2 && plan.steps.back().action == ActionId::Attack, "empty clip -> X then Attack");
		CHECK(!plan.empty() && plan.steps.front().action == ActionId::ChangeWeapon, "switching beats reloading while exposed");
		o.altWeaponLoaded = false; f = DeriveFacts(o, {});
		plan = p.Build(f, F_EnemyDead, o, 0);
		CHECK(plan.steps.size() == 2 && plan.steps.front().action == ActionId::Reload, "no alt weapon -> Reload then Attack");
		o.cover.verified = false; f = DeriveFacts(o, {});
		plan = p.Build(f, F_Safe, o, 0);
		CHECK(plan.empty(), "no verified cover -> no Survive plan (never assume cover)");
		Plan banned = p.Build(DeriveFacts(o, {}), F_EnemyDead, o, (1u << unsigned(ActionId::Reload)) | (1u << unsigned(ActionId::ChangeWeapon)));
		CHECK(banned.empty(), "failure memory bans an action");
		PlanLimits tiny; tiny.nodeBudget = 1;
		CHECK(p.Build(f, F_EnemyDead, o, 0, tiny).empty(), "node budget is enforced");
	}
	for (const Scenario& sc : All()) {
		Brain brain; LegacyModel legacy;
		const float threatAt = sc.name[0] == 'E' ? 2.0f : -1.0f;
		RunStats b = Run(sc, brain, "planner", threatAt), l = Run(sc, legacy, "legacy", threatAt);
		CHECK(b.hiddenInfoViolations == 0, (sc.name + ": planner used hidden enemy position").c_str());
		CHECK(l.hiddenInfoViolations == 0, (sc.name + ": legacy model used hidden enemy position").c_str());
		CHECK(b.usPerDecisionP99 < 200.0, (sc.name + ": decision p99 under 200 us").c_str());
		switch (sc.name[0]) {
		case 'A': CHECK(First(b, { ActionId::Attack, ActionId::Reposition }) != ActionId::Count, "A: engages");
			CHECK(b.pingPong == 0 && b.movePingPong == 0, "A: no ping-pong"); break;
		case 'B': CHECK(First(b, { ActionId::SeekCover, ActionId::Retreat, ActionId::Attack }) != ActionId::Attack, "B: survives first");
			CHECK(b.endHealth > l.endHealth - 1e-3f, "B: not worse than legacy on health"); break;
		case 'C': CHECK(b.firstAbandonUnreachable >= 0 && b.firstAbandonUnreachable <= 1.0f, "C: never commits to unverified pickup");
			CHECK(!Any(b, ActionId::GoToWeapon), "C: no GoToWeapon"); break;
		case 'D': CHECK(Any(b, ActionId::Pursue), "D: pursues last seen"); break;
		case 'E': CHECK(b.firstReactionToThreat >= 0 && b.firstReactionToThreat <= 0.5f, "E: reacts within 0.5 s"); break;
		case 'F': CHECK(b.pingPong == 0 && b.actionSwitches <= 4, "F: no flapping"); break;
		case 'G': CHECK(Any(b, ActionId::Unstick), "G: detects stall"); break;
		case 'H': CHECK(Any(b, ActionId::ChangeWeapon) || Any(b, ActionId::Reload), "H: recovers ammo");
			CHECK(Any(b, ActionId::Attack), "H: resumes attack"); break;
		case 'I': CHECK(b.movePingPong < l.movePingPong, "I: fewer walk-back flips than legacy");
			CHECK(b.movePingPong == 0, "I: no walk-back flips"); break;
		}
	}
	std::printf("%d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
