// DS-044 audit fixes: engine-free decision helpers (first primary, finish lock).
// The withdraw cover rule reuses BotTactics::PickCover, tested in BotAuditFixesTests too.
#pragma once

namespace WarsBots {

// Spawn pistol -> rifle first, unless an enemy is close enough to fight now.
inline bool ShouldSeekFirstPrimary(bool enabled, bool hasPrimary, float enemyDistance, bool socomQA)
{
    return enabled && !socomQA && !hasPrimary && enemyDistance > 10.0f;
}

// Keep a <=25 HP target until it is dead/lost > 1 s, unless a new enemy within 8 m is the one hitting us.
inline bool KeepLowHpTarget(bool enabled, bool oldValid, bool newTarget, int oldHealth, float lastSeenAge,
    float newDistance, bool newIsRetaliator, bool retaliatorActive)
{
    if (!enabled || !oldValid || !newTarget || oldHealth > 25 || lastSeenAge >= 1.0f) return false;
    return !(newDistance <= 8.0f && newIsRetaliator && retaliatorActive);
}

} // namespace WarsBots
