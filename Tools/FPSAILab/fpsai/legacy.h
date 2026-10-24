// Decision-level model of the current WarsBots rule set (BotBrain::Update at
// warsbots/feature/weapon-goals 0f338b12), for side-by-side scenario comparison only.
// Thresholds are the BotProfile defaults (universal style). Movement, aiming noise, suit
// hysteresis and native weapon behaviour are NOT modelled; this compares choices, not play.
#pragma once
#include "brain.h"

namespace fpsai {

class LegacyModel {
public:
	struct Profile {           // BotProfile.h defaults
		float engageRange = 18, holdRange = 22, retreatEnter = 6, retreatExit = 9, fireRange = 40;
		float retreatHealth = 22, withdrawFor = 2.5f, withdrawEvery = 5.0f;
		float upgradeTimeout = 10, badItemFor = 90, upgradeSearchRadius = 45;
		float targetHold = 4.0f;  // keep a lost target this long (approximation of BotCombat)
	} p;

	struct Entry { float time; ActionId action; const char* state; };
	const std::vector<Entry>& Log() const { return m_log; }

	Command Tick(const Observation& o) {
		Command c; const char* state = "SEARCHING_ENEMY";
		const bool enemy = o.enemyKnown && (o.enemyVisible || o.enemyLastSeenAge < p.targetHold);
		const bool visible = o.enemyVisible;
		// BotBrain.cpp:329 - pistol only and no visible enemy: go for a rifle, no route test.
		if ((!enemy || !visible) && !o.hasPrimary && o.weaponItem.exists && !Bad(o.weaponItem.id, o.time)
			&& (o.weaponItem.pos - o.self).Len() <= p.upgradeSearchRadius) {
			if (m_upgrade != o.weaponItem.id) { m_upgrade = o.weaponItem.id; m_upgradeSince = o.time; }
			if (o.time - m_upgradeSince > p.upgradeTimeout) { m_bad.push_back({ m_upgrade, o.time + p.badItemFor }); m_upgrade = 0; }
			else { c.action = ActionId::GoToWeapon; c.move = true; c.moveTo = o.weaponItem.pos; state = "MOVING_TO_WEAPON"; return Done(c, o, state); }
		}
		// BotBrain.cpp:376-420 - roam between waypoints; stuck reactions need BotMovement's
		// `blocked` flag, which an accepted-but-motionless step never raises (Steelmill F1).
		if (!enemy) {
			c.action = o.roam.exists ? ActionId::Roam : ActionId::Search; state = "SEARCHING_ENEMY";
			if (o.roam.exists) { c.move = true; c.moveTo = o.roam.pos; }
			return Done(c, o, state);
		}
		const float d = o.EnemyDistance();
		c.aim = true; c.aimAt = o.enemyLastSeen;
		// BotBrain.cpp:447-463 - withdraw 4 m straight away from the enemy for 2.5 s.
		if (o.health <= p.retreatHealth && o.time >= m_nextWithdraw) { m_withdrawUntil = o.time + p.withdrawFor; m_nextWithdraw = o.time + p.withdrawEvery; }
		if (o.time < m_withdrawUntil) {
			const V3 away = (o.self - o.enemyLastSeen) * (4.0f / std::max(0.1f, d));
			c.action = ActionId::Retreat; c.move = true; c.moveTo = o.self + away; state = "WITHDRAW";
			return Done(c, o, state);
		}
		if (visible && d <= (m_attacking ? p.holdRange : p.engageRange)) {
			m_attacking = true;
			c.action = ActionId::Attack; c.fire = o.clip > 0; state = "ATTACKING";
			if (d < (m_retreating ? p.retreatExit : p.retreatEnter)) {
				m_retreating = true; c.move = true; c.moveTo = o.self + (o.self - o.enemyLastSeen) * (3.0f / std::max(0.1f, d));
			} else m_retreating = false;
			return Done(c, o, state);
		}
		m_attacking = false;
		c.action = ActionId::Pursue; c.move = true; c.moveTo = o.enemyLastSeen; c.fire = visible && d <= p.fireRange && o.clip > 0;
		state = "CHASING_ENEMY";
		return Done(c, o, state);
	}

private:
	bool Bad(uint32_t id, float now) const { for (auto& b : m_bad) if (b.id == id && b.until > now) return true; return false; }
	Command Done(const Command& c, const Observation& o, const char* s) { m_log.push_back({ o.time, c.action, s }); return c; }
	struct BadItem { uint32_t id; float until; };
	std::vector<BadItem> m_bad;
	std::vector<Entry> m_log;
	uint32_t m_upgrade = 0; float m_upgradeSince = 0;
	float m_withdrawUntil = -1, m_nextWithdraw = 0;
	bool m_attacking = false, m_retreating = false;
};

} // namespace fpsai
