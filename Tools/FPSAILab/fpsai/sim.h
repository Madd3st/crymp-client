// Tiny closed-loop scenario world for decision tests. Deliberately crude: straight-line
// movement at 5 m/s, scripted enemy, exposure numbers per position. It exists to test
// decision properties (commitment, abandonment, interruption, information hygiene), and its
// numbers must never be reported as game results.
#pragma once
#include "brain.h"
#include "legacy.h"
#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace fpsai {

struct SimPoint { V3 pos; uint32_t id; bool reachable; float exposure; };

struct Scenario {
	std::string name, expectation;
	float duration = 12, dt = 0.1f;
	V3 start; float health = 100; Weapon weapon = Weapon::Rifle; bool hasPrimary = true; int clip = 30, clipMax = 30, reserve = 90;
	bool altLoaded = true;
	std::function<V3(float t)> enemyPos;                         // true position (hidden from policy)
	std::function<bool(float t, const V3& self, const V3& enemy)> visible;
	std::function<float(const V3& self)> exposure = [](const V3&) { return 1.0f; };
	// `reachable` is what navigation claims (BotNav route exists); `moves` is what physics does.
	// They differ exactly in the Steelmill F1/F2 failures.
	std::function<bool(const V3& target, const V3& self)> reachable = [](const V3&, const V3&) { return true; };
	std::function<bool(const V3& target, const V3& self)> moves;
	float enemyDps = 12, enemyHealth = 100;
	std::vector<SimPoint> covers, items, flanks, roams;
	int visibleEnemies = 1;
	std::function<void(float t, Scenario&)> script;              // mid-run events
};

struct RunStats {
	std::string policy;
	int decisions = 0, actionSwitches = 0, pingPong = 0, goalSwitches = 0;
	int movePingPong = 0;                 // A->B->A within 3 s between two *movement* actions (walks back and forth)
	float damageTaken = 0, damageDealt = 0, endHealth = 0;
	float stalledSeconds = 0;             // requested movement that could not happen
	float firstAbandonUnreachable = -1;   // time the policy stopped walking at an unreachable target
	int hiddenInfoViolations = 0;         // aim/move at the true enemy while not visible and not equal to last seen
	float firstReactionToThreat = -1;     // time from `threatAt` to first Attack/SeekCover/Retreat
	double usPerDecisionMean = 0, usPerDecisionP99 = 0;
	std::vector<ActionId> actions;        // per tick
	std::vector<std::string> explain;     // brain only: rationale per action change
};

template <class Policy>
RunStats Run(Scenario sc, Policy& policy, const char* label, float threatAt = -1) {
	RunStats st; st.policy = label;
	V3 self = sc.start; float health = sc.health; int clip = sc.clip, reserve = sc.reserve; bool primary = sc.hasPrimary;
	Weapon weapon = sc.weapon; float reloadLeft = 0; float enemyHp = sc.enemyHealth;
	bool known = false; V3 lastSeen; float lastSeenAt = -1e9f, lastDamageAt = -1e9f, stall = 0;
	ActionId prev = ActionId::Count, prev2 = ActionId::Count; float prevChange = -1e9f;
	ActionId mPrev = ActionId::Count, mPrev2 = ActionId::Count; float mChange = -1e9f;
	std::vector<double> times;
	for (float t = 0; t <= sc.duration + 1e-4f; t += sc.dt) {
		if (sc.script) sc.script(t, sc);
		const V3 enemy = sc.enemyPos ? sc.enemyPos(t) : V3(1e6f, 1e6f, 0);
		const bool alive = enemyHp > 0;
		const bool vis = alive && sc.visible && sc.visible(t, self, enemy);
		if (vis) { known = true; lastSeen = enemy; lastSeenAt = t; }
		Observation o;
		o.time = t; o.self = self; o.health = health; o.weapon = weapon; o.hasPrimary = primary; o.altWeaponLoaded = sc.altLoaded;
		o.clip = clip; o.clipMax = sc.clipMax; o.reserve = reserve; o.reloading = reloadLeft > 0;
		o.enemyVisible = vis; o.enemyKnown = known && alive; o.enemyId = 7; o.enemyLastSeen = lastSeen;
		o.enemyLastSeenAge = known ? t - lastSeenAt : 1e9f; o.visibleEnemies = vis ? sc.visibleEnemies : 0;
		o.lastDamageAge = t - lastDamageAt; o.positionExposure = sc.exposure(self); o.stallSeconds = stall;
		auto pick = [&](const std::vector<SimPoint>& pts) {
			NavOption best; float bestD = 1e9f;
			for (const SimPoint& p : pts) { float d = (p.pos - self).Len(); if (d < bestD) { bestD = d; best = { true, p.reachable && sc.reachable(p.pos, self), p.pos, d, p.exposure, p.id }; } }
			return best;
		};
		o.cover = pick(sc.covers); o.weaponItem = primary ? NavOption{} : pick(sc.items); o.flank = pick(sc.flanks); o.roam = pick(sc.roams);
		o.routeToLastSeen = known && sc.reachable(lastSeen, self);
		const auto t0 = std::chrono::steady_clock::now();
		const Command c = policy.Tick(o);
		times.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count());
		++st.decisions; st.actions.push_back(c.action);
		if (c.action != prev) {
			++st.actionSwitches;
			if (c.action == prev2 && t - prevChange < 3.0f) ++st.pingPong;
			prev2 = prev; prev = c.action; prevChange = t;
		}
		if (c.move && c.action != mPrev) {
			if (c.action == mPrev2 && t - mChange < 3.0f) ++st.movePingPong;
			mPrev2 = mPrev; mPrev = c.action; mChange = t;
		}
		if (threatAt >= 0 && t >= threatAt && st.firstReactionToThreat < 0 &&
			(c.action == ActionId::Attack || c.action == ActionId::SeekCover || c.action == ActionId::Retreat))
			st.firstReactionToThreat = t - threatAt;
		// Information hygiene: any enemy-directed target while unseen must equal the remembered point.
		auto leaks = [&](const V3& p) { return !vis && (p - enemy).Len() < 0.01f && (p - lastSeen).Len() > 0.01f; };
		if ((c.aim && leaks(c.aimAt)) || (c.move && leaks(c.moveTo))) ++st.hiddenInfoViolations;
		// World response.
		if (c.reload && reserve > 0 && reloadLeft <= 0 && clip < sc.clipMax) reloadLeft = 2.0f;
		if (clip == 0 && reserve > 0 && reloadLeft <= 0) reloadLeft = 2.0f;        // native auto-reload
		if (reloadLeft > 0) { reloadLeft -= sc.dt; if (reloadLeft <= 0) { const int n = std::min(reserve, sc.clipMax - clip); clip += n; reserve -= n; } }
		if (c.switchWeapon && sc.altLoaded) { clip = 12; sc.clipMax = 12; sc.altLoaded = false; weapon = Weapon::Pistol; reloadLeft = 0; }
		if (c.fire && vis && clip > 0 && reloadLeft <= 0) {
			clip = std::max(0, clip - 1); const float dmg = (weapon == Weapon::Pistol ? 9.f : 14.f) * sc.dt * 10.f / 3.f;
			enemyHp -= dmg; st.damageDealt += dmg;
		}
		if (alive && vis) { const float dmg = sc.enemyDps * sc.dt * sc.exposure(self); if (dmg > 0) { health -= dmg; st.damageTaken += dmg; lastDamageAt = t; } }
		if (c.move) {
			const V3 d = c.moveTo - self; const float len = d.Len2D();
			if (len > 0.05f) {
				if (sc.moves ? sc.moves(c.moveTo, self) : sc.reachable(c.moveTo, self)) { const float step = std::min(len, 5.0f * sc.dt); self = self + d * (step / len); stall = 0; }
				else { stall += sc.dt; st.stalledSeconds += sc.dt; }
			}
		} else stall = 0;
		for (const SimPoint& it : sc.items) if (!primary && it.reachable && (it.pos - self).Len2D() < 1.5f) { primary = true; weapon = Weapon::Rifle; clip = 30; sc.clipMax = 30; }
		if (st.firstAbandonUnreachable < 0 && !sc.items.empty() && !sc.items[0].reachable) {
			const bool walking = c.move && (c.moveTo - sc.items[0].pos).Len() < 0.01f;
			if (!walking && t > 0.5f) st.firstAbandonUnreachable = t;
		}
		if (health <= 0) break;
	}
	st.endHealth = health;
	double sum = 0; for (double v : times) sum += v; st.usPerDecisionMean = times.empty() ? 0 : sum / times.size();
	std::sort(times.begin(), times.end()); st.usPerDecisionP99 = times.empty() ? 0 : times[size_t(times.size() * 0.99)];
	return st;
}

} // namespace fpsai
