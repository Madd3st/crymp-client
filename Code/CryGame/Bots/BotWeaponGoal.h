// DS-097: weapon-goal lifecycle, engine-free. One writer may create a goal; a tick that merely looks at a candidate
// (goal arbiter) never starts the budget, and repeated ticks with the same target never restart it.
// The b8 defect: BotBrain.cpp:788-792 re-ran UpgradeCandidate and reset m_upgradeSince/m_nextUpgradeSearch on every
// UpdateUpgrade call, so the 60 s/90 s total budget at :820 could not fire and the 3 s re-search throttle was moot.
// Tests/BotWeaponGoalTests.cpp; callers are BotBrain::UpdateUpgrade and BotBrain::UpdateArbitrated.
#pragma once

namespace WarsBots {

// Hold an active pursuit, but refresh an unchosen candidate on the existing search throttle.
// A cached low-value candidate must not hide newly available weapons for the rest of the life.
inline bool MayAcquireWeaponGoal(unsigned target, float now, float nextSearch, bool pursuing = true)
{
    return (target == 0u || !pursuing) && now >= nextSearch;
}

// pursuitSince == 0 means "acquired, not yet pursued" (candidate only). The total budget starts at the first tick that
// actually walks to the goal, so a candidate the arbiter looked at but never chased cannot age into a false ban.
inline bool WeaponGoalPursues(float pursuitSince)
{
    return pursuitSince > 0.0f;
}

// Progress timeout (the goal stopped getting closer) or the total budget from the first pursuit tick. Sniper goals pass
// 20 s/90 s, general goals 8 s/60 s; the caller keeps path progress and replan behaviour.
inline bool WeaponGoalExpired(float now, float pursuitSince, float bestAt, float progressTimeout, float totalTimeout)
{
    return pursuitSince > 0.0f && (now - bestAt > progressTimeout || now - pursuitSince > totalTimeout);
}

} // namespace WarsBots
