// DS-044 audit fixes: engine-free decision helpers (first primary, finish lock).
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

// Keep a <=25 HP target until it is dead/lost > 1 s, unless a new enemy within 8 m is the one hitting us.
inline bool KeepLowHpTarget(bool enabled, bool oldValid, bool newTarget, int oldHealth, float lastSeenAge,
    float newDistance, bool newIsRetaliator, bool retaliatorActive)
{
    if (!enabled || !oldValid || !newTarget || oldHealth > 25 || lastSeenAge >= 1.0f) return false;
    return !(newDistance <= 8.0f && newIsRetaliator && retaliatorActive);
}

} // namespace WarsBots
