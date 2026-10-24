// Tactical brain: utility goal selection with commitment + bounded planner + step executor.
// The output is a Command for an engine adapter (BotIntent in WarsBots); it never touches
// game state itself. A Scorer interface lets a learned model replace the goal utilities
// without changing safety: masks come from preconditions, never from the model.
#pragma once
#include "planner.h"
#include <cstdio>
#include <memory>
#include <string>

namespace fpsai {

enum class GoalId : uint8_t { Unstick, Survive, KillEnemy, AcquireWeapon, Investigate, Patrol, Count };
inline const char* Name(GoalId g) {
	static const char* n[] = { "Unstick","Survive","KillEnemy","AcquireWeapon","Investigate","Patrol" };
	return n[int(g)];
}

enum class Suit : uint8_t { Keep, Armor, Speed, Strength };
struct Command {
	ActionId action = ActionId::HoldPosition;
	bool move = false; V3 moveTo;
	bool aim = false; V3 aimAt;        // only ever a visible enemy or a remembered point
	bool fire = false, reload = false, switchWeapon = false;
	Suit suit = Suit::Keep;
};

// `allowed` limits which actions a goal's plan may use, so an under-constrained goal symbol
// cannot be reached by an absurd chain (e.g. "retreat to break line of sight, then pursue").
struct GoalDef { GoalId id; Facts want; float interruptPriority; float minCommit; uint32_t allowed; };
inline constexpr uint32_t Bit(ActionId a) { return 1u << unsigned(a); }

// Features handed to a scorer. Kept flat and numeric so a contextual bandit can consume them.
struct GoalFeatures {
	float health, energy, clipFrac, enemyVisible, enemyKnown, enemyAge, enemyDist, visibleEnemies,
		underFire, inCover, coverAvail, coverCost, hasPrimary, upgradeReach, upgradeCost, stall, exposure;
	static GoalFeatures From(const Observation& o, Facts f) {
		return { o.health / 100.f, o.energy / 200.f, o.clipMax ? float(o.clip) / o.clipMax : 0.f,
			float(!!(f & F_EnemyVisible)), float(!!(f & F_EnemyKnown)), std::min(o.enemyLastSeenAge, 30.f) / 30.f,
			std::min(o.EnemyDistance(), 100.f) / 100.f, float(o.visibleEnemies), float(!!(f & F_UnderFire)),
			float(!!(f & F_InCover)), float(!!(f & F_CoverAvailable)), std::min(o.cover.routeCost, 50.f) / 50.f,
			float(o.hasPrimary), float(!!(f & F_UpgradeReach)), std::min(o.weaponItem.routeCost, 60.f) / 60.f,
			std::min(o.stallSeconds, 20.f) / 20.f, o.positionExposure };
	}
};

class Scorer {
public:
	virtual ~Scorer() = default;
	// Utility in [0,1] for a goal that already passed its applicability mask.
	virtual float Score(GoalId g, const GoalFeatures& x) const = 0;
	virtual const char* Name() const = 0;
};

// Hand-written utilities; documented curves, no learning.
class HeuristicScorer : public Scorer {
public:
	float Score(GoalId g, const GoalFeatures& x) const override {
		switch (g) {
		case GoalId::Unstick: return x.stall >= 0.3f ? 1.0f : 0.0f;
		case GoalId::Survive: {
			const float hurt = 1.0f - x.health;                       // 0..1
			const float threat = std::max(x.underFire, std::min(1.f, x.visibleEnemies / 2.f));
			const float base = std::max(0.f, 1.2f * hurt - 0.15f) + 0.15f * threat;
			return std::min(0.95f, base + (x.coverAvail > 0.5f || x.inCover > 0.5f ? 0.05f : -0.1f));
		}
		case GoalId::KillEnemy:                                        // unseen enemies go to Investigate
			return x.enemyVisible > 0.5f ? 0.6f + 0.3f * x.health : 0.0f;
		case GoalId::AcquireWeapon:
			return x.hasPrimary > 0.5f ? 0.0f : 0.55f - 0.25f * x.upgradeCost;
		case GoalId::Investigate:
			return x.enemyKnown > 0.5f && x.enemyVisible < 0.5f ? 0.5f * (1.0f - x.enemyAge) + 0.1f : 0.0f;
		case GoalId::Patrol: return 0.1f;
		default: return 0;
		}
	}
	const char* Name() const override { return "heuristic-v1"; }
};

struct Decision {
	float time; GoalId goal; ActionId action; float utility; int planLen; int expanded;
	bool goalChanged, actionChanged; std::string why;
};

struct BrainConfig {
	Tuning tune;
	float switchMargin = 0.12f;   // a challenger must beat the current goal by this utility
	float failBanSeconds = 8.0f;  // per-bot, per-target failure memory (Q3-style avoid list)
	PlanLimits limits;
};

class Brain {
public:
	explicit Brain(BrainConfig cfg = {}, std::shared_ptr<Scorer> scorer = std::make_shared<HeuristicScorer>())
		: m_cfg(cfg), m_scorer(std::move(scorer)), m_planner(MakeActions(cfg.tune)) {
		using A = ActionId;
		m_goals[size_t(GoalId::Unstick)]       = { GoalId::Unstick, F_Unstuck, 1.0f, 2.0f, Bit(A::Unstick) };
		m_goals[size_t(GoalId::Survive)]       = { GoalId::Survive, F_Safe, 0.8f, 2.5f, Bit(A::SeekCover) | Bit(A::Retreat) | Bit(A::HoldPosition) };
		m_goals[size_t(GoalId::KillEnemy)]     = { GoalId::KillEnemy, F_EnemyDead, 0.6f, 1.5f, Bit(A::Attack) | Bit(A::Reload) | Bit(A::ChangeWeapon) | Bit(A::Reposition) };
		m_goals[size_t(GoalId::AcquireWeapon)] = { GoalId::AcquireWeapon, F_HasPrimary, 0.3f, 3.0f, Bit(A::GoToWeapon) };
		m_goals[size_t(GoalId::Investigate)]   = { GoalId::Investigate, F_SearchedArea, 0.4f, 2.0f, Bit(A::Pursue) | Bit(A::Reload) };
		m_goals[size_t(GoalId::Patrol)]        = { GoalId::Patrol, F_SearchedArea, 0.0f, 1.0f, Bit(A::Roam) | Bit(A::Search) | Bit(A::Reload) };
	}

	const std::vector<Decision>& Log() const { return m_log; }
	GoalId CurrentGoal() const { return m_goal; }
	const Plan& CurrentPlan() const { return m_plan; }
	const Planner& GetPlanner() const { return m_planner; }

	Command Tick(const Observation& o) {
		const Facts f = DeriveFacts(o, m_cfg.tune);
		const GoalFeatures x = GoalFeatures::From(o, f);
		// 1. Applicability masks (rules), then utilities (scorer).
		float u[size_t(GoalId::Count)] = {};
		for (size_t g = 0; g < size_t(GoalId::Count); ++g) u[g] = Applicable(GoalId(g), f, o) ? m_scorer->Score(GoalId(g), x) : 0.f;
		// 2. Commitment: keep the current goal unless it finished, became inapplicable, its plan
		//    failed, or a challenger beats it by the margin after the minimum commitment time.
		const GoalId prevGoal = m_goal; const ActionId prevAction = CurrentAction();
		const bool curValid = m_hasGoal && u[size_t(m_goal)] > 0.f && !m_planFailed;
		GoalId best = m_goal; float bestU = curValid ? u[size_t(m_goal)] : -1.f;
		for (size_t g = 0; g < size_t(GoalId::Count); ++g) {
			if (!curValid) { if (u[g] > bestU) { best = GoalId(g); bestU = u[g]; } continue; }
			if (GoalId(g) == m_goal) continue;
			const bool committed = o.time - m_goalSince < m_goals[size_t(m_goal)].minCommit;
			const bool interrupts = m_goals[g].interruptPriority > m_goals[size_t(m_goal)].interruptPriority;
			if (u[g] > bestU + m_cfg.switchMargin && (!committed || interrupts)) { best = GoalId(g); bestU = u[g]; }
		}
		if (bestU <= 0.f) { best = GoalId::Patrol; bestU = u[size_t(GoalId::Patrol)]; }
		std::string why;
		bool replan = !m_hasGoal || best != m_goal || m_plan.empty() || m_planFailed || ReplanNeeded(f, o);
		if (best != m_goal || !m_hasGoal) { m_goal = best; m_goalSince = o.time; m_hasGoal = true; }
		if (replan) {
			m_planFailed = false;
			m_plan = m_planner.Build(f, m_goals[size_t(m_goal)].want, o, Banned(o) | ~m_goals[size_t(m_goal)].allowed, m_cfg.limits);
			m_step = 0; m_stepStarted = o.time; m_planFacts = f;
			if (m_plan.empty()) { // goal impossible now: fall back to the next applicable goal once
				why = std::string("no plan for ") + Name(m_goal);
				for (size_t g = 0; g < size_t(GoalId::Count) && m_plan.empty(); ++g) {
					if (GoalId(g) == m_goal || u[g] <= 0) continue;
					Plan p = m_planner.Build(f, m_goals[g].want, o, Banned(o) | ~m_goals[g].allowed, m_cfg.limits);
					if (!p.empty()) { m_goal = GoalId(g); m_goalSince = o.time; m_plan = p; }
				}
			}
		}
		// 3. Execute the current step. Success advances; failure records memory. Either way a
		//    finished or broken plan is rebuilt in the same tick (at most twice), so the bot never
		//    idles for a tick between steps and that gap never counts as a decision switch.
		Command cmd = Emit(ActionId::HoldPosition, o);
		for (int attempt = 0; attempt < 3; ++attempt) {
			if (m_plan.empty() || m_step >= m_plan.steps.size()) {
				if (attempt == 0) break;
				m_plan = m_planner.Build(f, m_goals[size_t(m_goal)].want, o, Banned(o) | ~m_goals[size_t(m_goal)].allowed, m_cfg.limits);
				m_step = 0; m_stepStarted = o.time; m_planFacts = f; m_planFailed = false;
				if (m_plan.empty()) break;
			}
			const ActionDef& d = m_planner.Def(m_plan.steps[m_step].action);
			Status s = Status::Running;
			if (d.valid && !d.valid(o)) s = Status::Failed;
			else if (d.progress) s = d.progress(o, m_stepStarted);
			if (s == Status::Running && o.time - m_stepStarted > d.timeout) s = Status::Failed;
			if (s == Status::Running) { cmd = Emit(d.id, o); break; }
			if (s == Status::Succeeded) { ++m_step; m_stepStarted = o.time; if (m_step < m_plan.steps.size()) continue; }
			else { Remember(d.id, o); m_planFailed = true; }
			m_plan.steps.clear(); m_step = 0;
		}
		if (why.empty()) why = std::string(Name(m_goal)) + " u=" + Fmt(u[size_t(m_goal)]) + " facts=" + FactsToString(f);
		m_log.push_back({ o.time, m_goal, cmd.action, u[size_t(m_goal)], int(m_plan.steps.size()), m_plan.expanded,
			m_goal != prevGoal, cmd.action != prevAction, why });
		m_lastAction = cmd.action;
		return cmd;
	}

private:
	static std::string Fmt(float v) { char b[16]; std::snprintf(b, sizeof b, "%.2f", v); return b; }
	ActionId CurrentAction() const { return m_lastAction; }

	bool Applicable(GoalId g, Facts f, const Observation& o) const {
		switch (g) {
		case GoalId::Unstick: return (f & F_Stuck) != 0;
		case GoalId::Survive: return (f & (F_EnemyKnown | F_UnderFire)) && (f & F_LowHealth);
		case GoalId::KillEnemy: return (f & F_EnemyVisible) && (f & F_HasWeapon);
		case GoalId::AcquireWeapon: return !(f & F_HasPrimary) && (f & F_UpgradeReach) && !(f & F_EnemyVisible);
		case GoalId::Investigate: return (f & F_EnemyKnown) && !(f & F_EnemyVisible) && (f & F_RouteLastSeen) && !(f & F_AtLastSeen);
		case GoalId::Patrol: return true;
		default: (void)o; return false;
		}
	}

	// Event-driven replanning: only when a fact the current step depends on changed.
	bool ReplanNeeded(Facts f, const Observation& o) const {
		if (m_plan.empty() || m_step >= m_plan.steps.size()) return true;
		const Facts watched = F_EnemyVisible | F_UnderFire | F_LowHealth | F_WeaponLoaded | F_CoverAvailable | F_Stuck;
		const bool changed = ((f ^ m_planFacts) & watched) != 0;
		const ActionDef& d = m_planner.Def(m_plan.steps[m_step].action);
		return changed && d.interruptible && o.time - m_stepStarted > 0.2f;
	}

	// Failure memory: an action that failed against the same target is banned for a while,
	// so an unreachable weapon or cover is not retried every tick.
	// Only actions bound to a navigation target are remembered; running dry during Attack or a
	// slow reload are normal endings, not evidence that the action is impossible.
	void Remember(ActionId a, const Observation& o) {
		const uint32_t target = TargetOf(a, o);
		if (target) m_fail.push_back({ a, target, o.time + m_cfg.failBanSeconds });
	}
	static uint32_t TargetOf(ActionId a, const Observation& o) {
		switch (a) {
		case ActionId::GoToWeapon: return o.weaponItem.id;
		case ActionId::SeekCover: case ActionId::Retreat: return o.cover.id;
		case ActionId::Reposition: return o.flank.id;
		case ActionId::Roam: return o.roam.id;
		default: return 0;
		}
	}
	uint32_t Banned(const Observation& o) {
		uint32_t mask = 0;
		m_fail.erase(std::remove_if(m_fail.begin(), m_fail.end(), [&](const Fail& f) { return f.until <= o.time; }), m_fail.end());
		for (const Fail& f : m_fail) {
			if (f.target == TargetOf(f.action, o)) mask |= 1u << unsigned(f.action);
		}
		return mask;
	}

	Command Emit(ActionId a, const Observation& o) const {
		Command c; c.action = a;
		const bool see = o.enemyVisible;
		if (see) { c.aim = true; c.aimAt = o.enemyLastSeen; }
		switch (a) {
		case ActionId::Attack: c.fire = see && o.clip > 0; c.suit = o.lastDamageAge < 1.0f ? Suit::Armor : Suit::Keep; break;
		case ActionId::Reload: c.reload = true; break;
		case ActionId::ChangeWeapon: c.switchWeapon = true; break;
		case ActionId::SeekCover: c.move = true; c.moveTo = o.cover.pos; c.suit = Suit::Armor; break;
		case ActionId::Retreat: {
			c.move = true; c.suit = see ? Suit::Armor : Suit::Speed;
			// Away along a verified option when one exists; otherwise hold (never a blind "away" vector).
			if (o.cover.verified) c.moveTo = o.cover.pos; else c.move = false;
			break; }
		case ActionId::Reposition: c.move = true; c.moveTo = o.flank.pos; c.fire = see && o.clip > 0; break;
		case ActionId::Pursue: c.move = true; c.moveTo = o.enemyLastSeen; c.aim = true; c.aimAt = o.enemyLastSeen;
			c.suit = o.EnemyDistance() > 20 && o.energy > 120 ? Suit::Speed : Suit::Keep; break;
		case ActionId::GoToWeapon: c.move = true; c.moveTo = o.weaponItem.pos; c.suit = o.weaponItem.routeCost > 15 && o.energy > 120 ? Suit::Speed : Suit::Keep; break;
		case ActionId::Search: c.move = false; break;
		// The adapter owns the escalation ladder (repath, side step, jump back, last-resort respawn);
		// the planner only decides that unsticking outranks everything else.
		case ActionId::Unstick: c.move = true; c.moveTo = o.self; c.suit = Suit::Strength; break;
		case ActionId::HoldPosition: c.fire = see && o.clip > 0; break;
		case ActionId::Roam: c.move = true; c.moveTo = o.roam.pos; break;
		default: break;
		}
		return c;
	}

	static std::vector<ActionDef> MakeActions(const Tuning& t) {
		std::vector<ActionDef> a(size_t(ActionId::Count));
		auto at = [](const V3& p, const Observation& o, float r) { return (p - o.self).Len2D() < r; };
		a[0] = { ActionId::Attack, F_EnemyVisible | F_WeaponLoaded, 0, F_EnemyDead, 0, 1.0f, 6.0f,
			[](const Observation& o) { return o.enemyVisible; },  // ammo is symbolic (F_WeaponLoaded), not context
			[](const Observation& o) { return 1.0f + 0.03f * o.EnemyDistance() + o.positionExposure * 0.5f; },
			[](const Observation& o, float) { return !o.enemyVisible ? Status::Failed : o.clip == 0 ? Status::Failed : Status::Running; } };
		a[1] = { ActionId::Reload, F_HasWeapon, F_WeaponLoaded, F_WeaponLoaded, 0, 1.0f, 4.0f,
			[](const Observation& o) { return o.reserve > 0 && o.clip < o.clipMax; },
			[](const Observation& o) { return o.enemyVisible && o.positionExposure > 0.5f ? 3.0f : 1.0f; },
			[](const Observation& o, float) { return o.clip > 0 && !o.reloading ? Status::Succeeded : Status::Running; }, false };
		a[2] = { ActionId::Retreat, F_EnemyKnown, 0, F_Safe, F_EnemyVisible, 3.0f, 4.0f,
			[](const Observation& o) { return o.cover.verified; },
			[](const Observation& o) { return 3.0f + o.cover.routeCost * 0.05f; },
			[at](const Observation& o, float) { return at(o.cover.pos, o, 1.5f) ? Status::Succeeded : Status::Running; } };
		a[3] = { ActionId::SeekCover, F_CoverAvailable, F_InCover, F_InCover | F_Safe, 0, 1.5f, 6.0f,
			[t](const Observation& o) { return o.cover.exists && o.cover.verified && o.cover.exposure <= t.coverExposure; },
			[](const Observation& o) { return 1.0f + o.cover.routeCost * 0.08f; },
			[at](const Observation& o, float) { return at(o.cover.pos, o, 1.5f) ? Status::Succeeded : Status::Running; } };
		a[4] = { ActionId::Reposition, F_FlankAvailable | F_EnemyKnown, 0, F_EnemyVisible, 0, 2.5f, 6.0f,
			[](const Observation& o) { return o.flank.verified; },
			[](const Observation& o) { return 2.0f + o.flank.routeCost * 0.06f; },
			[at](const Observation& o, float) { return at(o.flank.pos, o, 1.5f) || o.enemyVisible ? Status::Succeeded : Status::Running; } };
		a[5] = { ActionId::Pursue, F_EnemyKnown | F_RouteLastSeen, F_EnemyVisible | F_AtLastSeen, F_AtLastSeen | F_SearchedArea, 0, 2.0f, 10.0f,
			[](const Observation& o) { return o.routeToLastSeen; },
			[](const Observation& o) { return 1.0f + o.EnemyDistance() * 0.05f; },
			[at, t](const Observation& o, float) { return o.enemyVisible || at(o.enemyLastSeen, o, t.atPointRadius) ? Status::Succeeded : Status::Running; } };
		a[6] = { ActionId::ChangeWeapon, F_AltLoaded, F_WeaponLoaded, F_WeaponLoaded, 0, 0.8f, 2.0f, nullptr, nullptr,
			[](const Observation& o, float started) { return o.clip > 0 || o.time - started > 1.0f ? Status::Succeeded : Status::Running; } };
		a[7] = { ActionId::HoldPosition, F_InCover, 0, F_Safe, 0, 2.0f, 3.0f, nullptr, nullptr,
			[](const Observation& o, float started) { return o.time - started > 2.0f ? Status::Succeeded : Status::Running; } };
		a[8] = { ActionId::GoToWeapon, F_UpgradeReach, F_HasPrimary, F_HasPrimary, 0, 2.0f, 12.0f,
			[](const Observation& o) { return o.weaponItem.exists && o.weaponItem.verified; },
			[](const Observation& o) { return 1.0f + o.weaponItem.routeCost * 0.05f; },
			[](const Observation& o, float) { return o.hasPrimary ? Status::Succeeded : Status::Running; } };
		a[9] = { ActionId::Search, 0, F_EnemyVisible, F_SearchedArea, 0, 3.0f, 3.0f, nullptr, nullptr,
			[](const Observation& o, float started) { return o.time - started > 2.5f ? Status::Succeeded : Status::Running; } };
		a[11] = { ActionId::Roam, F_RoamAvailable, F_EnemyVisible, F_SearchedArea, 0, 2.0f, 30.0f,
			[](const Observation& o) { return o.roam.exists && o.roam.verified; }, nullptr,
			[at](const Observation& o, float) { return at(o.roam.pos, o, 2.0f) ? Status::Succeeded : Status::Running; } };
		a[10] = { ActionId::Unstick, F_Stuck, 0, F_Unstuck, F_Stuck, 1.0f, 4.0f, nullptr, nullptr,
			[](const Observation& o, float) { return o.stallSeconds < 0.5f ? Status::Succeeded : Status::Running; }, false };
		return a;
	}

	struct Fail { ActionId action; uint32_t target; float until; };
	BrainConfig m_cfg;
	std::shared_ptr<Scorer> m_scorer;
	Planner m_planner;
	GoalDef m_goals[size_t(GoalId::Count)];
	GoalId m_goal = GoalId::Patrol; bool m_hasGoal = false; float m_goalSince = 0;
	Plan m_plan; size_t m_step = 0; float m_stepStarted = 0; Facts m_planFacts = 0; bool m_planFailed = false;
	ActionId m_lastAction = ActionId::Count;
	std::vector<Fail> m_fail;
	std::vector<Decision> m_log;
};

} // namespace fpsai
