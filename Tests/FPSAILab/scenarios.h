// Scenarios A-F from the task, plus G (trap), H (empty clip in a fight) and I (the
// CHASING<->MOVING_TO_WEAPON flicker measured in the owner's 11-Elite log).
#pragma once
#include "../../Tools/FPSAILab/fpsai/sim.h"
#include <cmath>

namespace fpsai { namespace scenarios {

inline bool AlwaysVisible(float, const V3&, const V3&) { return true; }

// A: enemy visible at 15 m, full clip, a verified flank position exists. Expect Attack (or a
// deliberate Reposition), and no oscillation between them.
inline Scenario A() {
	Scenario s; s.name = "A_visible_full_clip"; s.expectation = "attack or reposition, no oscillation";
	s.start = { 0, 0, 0 }; s.enemyPos = [](float) { return V3(15, 0, 0); }; s.visible = AlwaysVisible;
	s.exposure = [](const V3&) { return 0.6f; };
	s.flanks = { { V3(8, 8, 0), 31, true, 0.5f } };
	s.covers = { { V3(-3, 4, 0), 21, true, 0.1f } };
	return s;
}

// B: under fire at 25 HP, verified cover 6 m away. Expect SeekCover/Retreat quickly.
inline Scenario B() {
	Scenario s = A(); s.name = "B_low_health_under_fire"; s.expectation = "seek cover or retreat within 0.5 s";
	s.health = 25; s.enemyDps = 6;
	s.covers = { { V3(-4, 4, 0), 22, true, 0.05f } };
	s.exposure = [](const V3& p) { return (p - V3(-4, 4, 0)).Len2D() < 1.5f ? 0.05f : 0.8f; };
	s.visible = [](float, const V3& p, const V3&) { return (p - V3(-4, 4, 0)).Len2D() >= 1.5f; };
	return s;
}

// C: pistol only, the rifle is on a ledge with no route. Expect the planner never to commit
// to an unverified pickup; the legacy rule walks at it until its 10 s timeout.
inline Scenario C() {
	Scenario s; s.name = "C_unreachable_weapon"; s.expectation = "abandon (or never start) the unreachable pickup";
	s.start = { 0, 0, 0 }; s.weapon = Weapon::Pistol; s.hasPrimary = false; s.clip = 12; s.clipMax = 12; s.altLoaded = false;
	s.enemyPos = [](float) { return V3(500, 500, 0); }; s.visible = [](float, const V3&, const V3&) { return false; };
	s.items = { { V3(12, 0, 4), 41, false, 1.0f } };
	s.reachable = [](const V3& target, const V3&) { return std::fabs(target.z) < 1.0f; };
	s.duration = 15;
	return s;
}

// D: enemy seen for 1 s, then hides and walks away. Every target must come from last seen.
inline Scenario D() {
	Scenario s; s.name = "D_enemy_hides"; s.expectation = "pursue last seen point only; no hidden coordinates";
	s.start = { 0, 0, 0 };
	s.enemyPos = [](float t) { return t < 1.0f ? V3(20, 0, 0) : V3(20, 3.0f * (t - 1.0f), 0); };
	s.visible = [](float t, const V3&, const V3&) { return t < 1.0f; };
	return s;
}

// E: bot walks to a rifle; at t=2 a visible threat appears. Expect the plan to be interrupted.
inline Scenario E() {
	Scenario s; s.name = "E_new_threat_mid_plan"; s.expectation = "interrupt pickup and engage within 0.5 s";
	s.start = { 0, 0, 0 }; s.weapon = Weapon::Pistol; s.hasPrimary = false; s.clip = 12; s.clipMax = 12;
	s.items = { { V3(30, 0, 0), 42, true, 1.0f } };
	s.enemyPos = [](float) { return V3(10, 8, 0); };
	s.visible = [](float t, const V3&, const V3&) { return t >= 2.0f; };
	return s;
}

// F: two close options. Enemy visible at a distance where attacking and repositioning score
// near-equal, with exposure noise every tick. Expect no flapping.
inline Scenario F() {
	Scenario s = A(); s.name = "F_close_utilities"; s.expectation = "at most a few action changes";
	s.exposure = [](const V3& p) { return 0.5f + 0.05f * std::sin(p.x * 7.0f + p.y * 3.0f); };
	s.health = 60; s.enemyDps = 2;
	return s;
}

// G: travel target sits inside a pocket that cannot be left (Steelmill F2).
inline Scenario G() {
	Scenario s; s.name = "G_trap_pocket"; s.expectation = "detect the stall and switch to Unstick";
	s.start = { 0, 0, 0 }; s.enemyPos = [](float) { return V3(40, 0, 0); };
	s.visible = [](float t, const V3&, const V3&) { return t < 0.5f; };
	// BotNav reports a route to the roam point (verified) but the body cannot move: Steelmill F1/F2.
	s.roams = { { V3(15, 5, 0), 51, true, 1.0f } };
	s.moves = [](const V3&, const V3&) { return false; };
	s.duration = 15;
	return s;
}

// H: in a fight with an empty clip, pistol loaded.
inline Scenario H() {
	Scenario s = A(); s.name = "H_empty_clip_in_fight"; s.expectation = "switch weapon or reload, then keep attacking";
	s.clip = 0; s.reserve = 60;
	return s;
}

// I: pistol only, enemy visibility flickers every 1.5 s, rifle reachable 20 m away.
inline Scenario I() {
	Scenario s; s.name = "I_visibility_flicker_pistol"; s.expectation = "no CHASE<->PICKUP ping-pong";
	s.start = { 0, 0, 0 }; s.weapon = Weapon::Pistol; s.hasPrimary = false; s.clip = 12; s.clipMax = 12;
	s.enemyPos = [](float t) { return V3(25, 6.0f * std::sin(t * 0.4f), 0); };
	s.visible = [](float t, const V3&, const V3&) { return std::fmod(t, 3.0f) < 1.5f; };
	s.items = { { V3(-20, 0, 0), 43, true, 1.0f } };
	s.exposure = [](const V3&) { return 0.7f; }; s.enemyDps = 3;
	return s;
}

inline std::vector<Scenario> All() { return { A(), B(), C(), D(), E(), F(), G(), H(), I() }; }

}} // namespace fpsai::scenarios
