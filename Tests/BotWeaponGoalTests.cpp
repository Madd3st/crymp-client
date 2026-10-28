// DS-097 tests: weapon-goal lifecycle (engine-free, no engine headers).
// The rules under test are the ones BotBrain::UpdateUpgrade / BotBrain::UpdateArbitrated call at b8b444d6:
//   * a goal is created once and held while it exists (no re-scan/switch every tick),
//   * the total budget starts on the first tick that pursues the goal, not on a candidate look,
//   * progress/replan refreshes only the progress stamp, never the total budget.
#include "CryGame/Bots/BotWeaponGoal.h"
#include <cassert>
#include <cstdio>

using namespace WarsBots;

// The b8b444d6 behaviour (BotBrain.cpp:788-792): every call re-picked the candidate and reset both stamps, so the
// 60 s / 90 s branch at :820 could not fire. Reproduced here only as the red baseline for the new rules.
static bool OldBudgetExpires(float now, float progressTimeout, float totalTimeout)
{
    float since = 0.0f, bestAt = 0.0f;
    for (float t = 0.0f; t <= now + 0.001f; t += 0.1f) { since = t; bestAt = t; } // unconditional reset each tick
    return WeaponGoalExpired(now, since, bestAt, progressTimeout, totalTimeout);
}

int main()
{
    // Acquire gate: free goal + expired throttle only. An existing goal is never re-acquired (hold, no switch).
    assert(MayAcquireWeaponGoal(0u, 10.0f, 10.0f));
    assert(!MayAcquireWeaponGoal(0u, 9.9f, 10.0f));
    assert(!MayAcquireWeaponGoal(7u, 100.0f, 0.0f));      // repeated ticks with the same target
    assert(!WeaponGoalPursues(0.0f));
    assert(WeaponGoalPursues(0.1f));

    // Repeated ticks, same target: pursuit starts once. Steady progress keeps the progress stamp fresh, but the total
    // budget is anchored at the first pursuit tick and still ends the trip.
    const float acquiredAt = 1.0f;
    float since = 0.0f, bestAt = 0.0f;
    bool totalFired = false;
    for (int tick = 0; tick < 700; ++tick)
    {
        const float now = acquiredAt + 0.1f * float(tick);
        if (!WeaponGoalPursues(since)) { since = now; bestAt = now; } // first pursuit tick
        assert(!MayAcquireWeaponGoal(9u, now, since + 3.0f));         // tracked goal is held, never re-acquired
        bestAt = now;                                                 // walking towards it / replanning
        if (WeaponGoalExpired(now, since, bestAt, 8.0f, 60.0f))
        {
            totalFired = (now - since > 60.0f);
            break;
        }
    }
    assert(totalFired);
    assert(since == acquiredAt);
    assert(!WeaponGoalExpired(59.9f, since, 59.9f, 8.0f, 60.0f));
    assert(WeaponGoalExpired(61.1f, since, 61.0f, 8.0f, 60.0f));  // total budget, even with fresh progress

    // Progress timeout: 8 s general, 20 s sniper; progress/replan refreshes bestAt but must not touch since.
    assert(WeaponGoalExpired(9.1f, 1.0f, 1.0f, 8.0f, 60.0f));
    assert(!WeaponGoalExpired(9.1f, 1.0f, 1.0f, 20.0f, 90.0f));
    assert(WeaponGoalExpired(21.1f, 1.0f, 1.0f, 20.0f, 90.0f));
    // A replan every 5 s (new navmesh path) is progress, not a new budget.
    for (int t = 5; t <= 55; t += 5) assert(!WeaponGoalExpired(float(t), 1.0f, float(t), 8.0f, 60.0f));
    assert(WeaponGoalExpired(61.1f, 1.0f, 56.0f, 8.0f, 60.0f));

    // Candidate look only (goal arbiter, since == 0): no progress and no total budget yet -> never a false ban.
    assert(!WeaponGoalExpired(1.0e6f, 0.0f, 0.0f, 8.0f, 60.0f));
    // ... and when the arbiter finally chases it, the budget starts there, not at the earlier look.
    assert(!WeaponGoalExpired(1000.1f, 1000.0f, 1000.0f, 8.0f, 60.0f));
    assert(WeaponGoalExpired(1060.2f, 1000.0f, 1000.0f, 8.0f, 60.0f));

    // Old block (red baseline) vs the new rules: the same 60 s trip at the same tick rate.
    assert(!OldBudgetExpires(600.0f, 8.0f, 60.0f));               // b8: never gives up
    assert(WeaponGoalExpired(61.1f, 1.0f, 1.0f, 8.0f, 60.0f));    // patched: fires

    // Item unavailable/stolen: the caller clears the goal; the 3 s throttle still applies, then a retry is allowed.
    assert(!MayAcquireWeaponGoal(0u, 1.0f, 4.0f));                // cleared early, throttle not expired
    assert(MayAcquireWeaponGoal(0u, 4.0f, 4.0f));                 // retry after the throttle
    // Timeout/ban: the caller marks the item bad and clears; the gate allows the next candidate after the throttle.
    assert(MayAcquireWeaponGoal(0u, 63.0f, 3.0f));

    // Primary acquired: no early re-acquire while the throttle runs, and a visible-threat tick is refused by the
    // callers' ShouldSearchUpgrade (BotUpgradeGateTests), so acquisition is not even reached there.
    assert(!MayAcquireWeaponGoal(0u, 0.5f, 3.0f));
    assert(MayAcquireWeaponGoal(0u, 3.0f, 3.0f));

    std::printf("weapon goal: all tests passed (old budget fires=%d, patched fires=%d)\n",
        OldBudgetExpires(600.0f, 8.0f, 60.0f) ? 1 : 0, WeaponGoalExpired(61.1f, 1.0f, 1.0f, 8.0f, 60.0f) ? 1 : 0);
    return 0;
}
