// DS-044 audit fixes: engine-free decision helpers (first primary; finish lock removed by D-037).
// The withdraw cover rule reuses BotTactics::PickCover, tested in BotAuditFixesTests too.
#pragma once

namespace WarsBots {

// Match the damage interrupt before the first-primary shortcut can bypass the goal arbiter.
// Hearing/damage without a known opponent is not a visual sighting.
inline bool FirstPrimaryCombatUrgent(bool damagedRecently, bool enemyVisible, float lastSeenAge)
{
    return damagedRecently && (enemyVisible || lastSeenAge < 1.0f);
}

// Spawn pistol -> rifle first, unless an enemy is close enough or a known threat just hit us.
inline bool ShouldSeekFirstPrimary(bool enabled, bool hasPrimary, float enemyDistance, bool socomQA, bool combatUrgent = false)
{
    return enabled && !socomQA && !hasPrimary && !combatUrgent && enemyDistance > 10.0f;
}

// Audit 14.11 p. 5: "enemy visible" and its distance are facts of one decision tick. Only the fight and the roam wrote
// them, so a tick owned by the weapon goal, the upgrade or the ladder kept the value of the last fight: unstick stayed
// muted and the sprint stayed off. The brain clears both at the start of every tick; the fight writes them again.
inline void BeginTickSight(bool& visible, float& distance) { visible = false; distance = 0.0f; }

// The two readers of that fact, as the brain uses them (BotBrain::UpdateUnstick, ExecuteIntent).
inline bool UnstickCombatMute(bool visible, bool attacking, bool combatIntent) { return visible || attacking || combatIntent; }
inline bool ProFireBlocksSprint(bool proFire, bool visible, float distance, float holdRange, bool jumping, bool sprinting)
{
    return proFire && visible && distance <= holdRange && !jumping && sprinting;
}

} // namespace WarsBots
