// Goal arbiter (C-032, D-008/D-010): one place that decides WHAT the bot is doing, with commitment windows and
// hysteresis, so CHASING_ENEMY <-> MOVING_TO_WEAPON can no longer flip every tick (DECISION_FLIPS.md: the upgrade
// gate used instantaneous !Visible() against chase-on-visible, BotBrain.cpp:334/344 at fb578d52/5ce78d8).
// Pure logic, no engine types: BotBrain fills ArbiterInput from the blackboard/combat/weapons each decision tick
// and maps the returned Goal onto its existing states (Fight -> ATTACKING/CHASING_ENEMY, Weapon -> MOVING_TO_WEAPON,
// Hunt -> CHASING_ENEMY toward the last known position, Roam -> SEARCHING_ENEMY). Written by Claude, integrated by
// Codex; unit tests in Tests/BotGoalArbiterTests.cpp.
#pragma once

namespace WarsBots {

enum class ArbiterGoal { Roam = 0, Hunt, Weapon, Fight };

enum class ArbiterReason {
	Hold = 0,          // keep the current goal (commitment window or no clearly better option)
	Start,             // first decision of this life
	EnemyClose,        // interrupt: visible enemy inside engage range
	Damaged,           // interrupt: took damage
	BetterScore,       // commitment over and another goal scores higher by the margin
	GoalGone,          // current goal impossible (weapon taken, memory expired)
};

struct ArbiterInput {
	float now = 0.f;               // game time, s
	bool  hasPrimary = false;      // holds anything better than the spawn pistol
	bool  enemyVisible = false;    // raw line of sight this tick (debounced inside)
	float enemyDistance = 1e9f;    // m, to the current/remembered target
	float lastSeenAge = 1e9f;      // s since the target was last visible
	float engageRange = 25.f;      // m, from the bot profile / current weapon
	bool  damagedRecently = false; // took damage within the last ~0.5 s
	bool  weaponGoalAvailable = false; // a reachable weapon worth taking exists
	float weaponGoalDistance = 1e9f;   // m
	float weaponGoalValue = 0.f;   // 0..1, tier of the best reachable weapon (SCAR = 1)
	float heldWeaponValue = 0.f;   // 0..1, tier of the held weapon (pistol ~0.1)
};

struct ArbiterParams {
	float visibleDebounce = 1.0f;  // s an enemy stays "visible" after LOS is lost
	float memorySeconds = 8.0f;    // s the last known position is worth hunting
	float commitWeapon = 3.0f;     // s minimum on Weapon
	float commitFight = 0.75f;     // s minimum on Fight
	float commitOther = 1.0f;      // s minimum on Hunt/Roam
	float switchMargin = 0.15f;    // score margin needed to leave a goal after its commitment
	float pistolEngage = 15.f;     // m: with only a pistol, fight only this close (owner, 19:25) or when damaged
	float upgradeGain = 0.3f;      // weapon value gain that makes an upgrade worth a detour
};

struct ArbiterDecision {
	ArbiterGoal goal = ArbiterGoal::Roam;
	ArbiterReason reason = ArbiterReason::Start;
	bool switched = false;
	float score = 0.f;
};

class GoalArbiter {
public:
	explicit GoalArbiter(const ArbiterParams& p = ArbiterParams()) : m_p(p) {}

	void Reset() { m_started = false; m_lastVisible = -1e9f; }

	ArbiterGoal Current() const { return m_goal; }
	float Since() const { return m_since; }

	ArbiterDecision Update(const ArbiterInput& in)
	{
		if (in.enemyVisible) m_lastVisible = in.now;
		const bool visible = in.now - m_lastVisible <= m_p.visibleDebounce;
		float s[4];
		Scores(in, visible, s);

		ArbiterDecision d;
		if (!m_started)
		{
			m_started = true;
			return Switch(in.now, Best(s), ArbiterReason::Start, s);
		}
		const int cur = int(m_goal);
		// Interrupts ignore the commitment window.
		const bool close = visible && in.enemyDistance <= in.engageRange &&
			(in.hasPrimary || in.enemyDistance <= m_p.pistolEngage);
		if (m_goal != ArbiterGoal::Fight && close)
			return Switch(in.now, ArbiterGoal::Fight, ArbiterReason::EnemyClose, s);
		if (m_goal != ArbiterGoal::Fight && in.damagedRecently && (visible || in.lastSeenAge < 1.f))
			return Switch(in.now, ArbiterGoal::Fight, ArbiterReason::Damaged, s);
		// Current goal became impossible.
		if (s[cur] <= 0.f)
		{
			const ArbiterGoal b = Best(s);
			if (b != m_goal) return Switch(in.now, b, ArbiterReason::GoalGone, s);
		}
		// After the commitment window: switch only for a clearly better goal.
		if (in.now - m_since >= Commit(m_goal))
		{
			const ArbiterGoal b = Best(s);
			if (b != m_goal && s[int(b)] > s[cur] + m_p.switchMargin)
				return Switch(in.now, b, ArbiterReason::BetterScore, s);
		}
		d.goal = m_goal; d.reason = ArbiterReason::Hold; d.switched = false; d.score = s[cur];
		return d;
	}

	static const char* GoalName(ArbiterGoal g)
	{
		switch (g) { case ArbiterGoal::Fight: return "fight"; case ArbiterGoal::Weapon: return "weapon";
			case ArbiterGoal::Hunt: return "hunt"; default: return "roam"; }
	}
	static const char* ReasonName(ArbiterReason r)
	{
		switch (r) { case ArbiterReason::Start: return "start"; case ArbiterReason::EnemyClose: return "enemy_close";
			case ArbiterReason::Damaged: return "damaged"; case ArbiterReason::BetterScore: return "better";
			case ArbiterReason::GoalGone: return "goal_gone"; default: return "hold"; }
	}

private:
	void Scores(const ArbiterInput& in, bool visible, float* s) const
	{
		// Fight: a visible enemy; with only a pistol and no damage, weak unless close (owner: find a weapon first).
		float fight = 0.f;
		if (visible)
		{
			fight = 1.0f;
			if (!in.hasPrimary && !in.damagedRecently && in.enemyDistance > m_p.pistolEngage) fight = 0.35f;
		}
		// Weapon: no primary -> strong; with a primary only a clear upgrade.
		float weapon = 0.f;
		if (in.weaponGoalAvailable)
		{
			const float distCost = in.weaponGoalDistance / 200.f;
			if (!in.hasPrimary) weapon = 0.85f - distCost;
			else if (in.weaponGoalValue - in.heldWeaponValue >= m_p.upgradeGain) weapon = 0.45f - distCost;
			if (weapon < 0.05f) weapon = 0.05f;
		}
		// Hunt: last known position, decaying over the memory window.
		float hunt = 0.f;
		if (!visible && in.lastSeenAge < m_p.memorySeconds) hunt = 0.6f * (1.f - in.lastSeenAge / m_p.memorySeconds);
		s[int(ArbiterGoal::Roam)] = 0.1f;
		s[int(ArbiterGoal::Hunt)] = hunt;
		s[int(ArbiterGoal::Weapon)] = weapon;
		s[int(ArbiterGoal::Fight)] = fight;
	}

	static ArbiterGoal Best(const float* s)
	{
		int b = 0;
		for (int i = 1; i < 4; ++i) if (s[i] > s[b]) b = i;
		return ArbiterGoal(b);
	}

	float Commit(ArbiterGoal g) const
	{
		return g == ArbiterGoal::Weapon ? m_p.commitWeapon : g == ArbiterGoal::Fight ? m_p.commitFight : m_p.commitOther;
	}

	ArbiterDecision Switch(float now, ArbiterGoal g, ArbiterReason r, const float* s)
	{
		ArbiterDecision d;
		d.switched = g != m_goal || r == ArbiterReason::Start;
		m_goal = g; m_since = now;
		d.goal = g; d.reason = r; d.score = s[int(g)];
		return d;
	}

	ArbiterParams m_p;
	ArbiterGoal m_goal = ArbiterGoal::Roam;
	float m_since = 0.f;
	float m_lastVisible = -1e9f;
	bool m_started = false;
};

} // namespace WarsBots
