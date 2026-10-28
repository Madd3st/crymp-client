// DS-044 audit-fix tests: first primary, finish lock and the withdraw cover rule (engine-free).
#include "CryGame/Bots/BotAuditFixes.h"
#include "CryGame/Bots/BotTactics.h"
#include <cassert>
#include <cstdio>

using namespace WarsBots;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main()
{
    // #1: spawn pistol -> rifle, unless an enemy is within 10 m.
    CHECK(ShouldSeekFirstPrimary(true, false, 20.0f, false));
    CHECK(!ShouldSeekFirstPrimary(true, false, 9.9f, false));
    CHECK(!ShouldSeekFirstPrimary(true, true, 20.0f, false));
    CHECK(!ShouldSeekFirstPrimary(true, false, 20.0f, true));
    CHECK(!ShouldSeekFirstPrimary(false, false, 20.0f, false));

    // C153: the early rifle shortcut must not consume a tick the damage interrupt needs for a known enemy.
    CHECK(FirstPrimaryCombatUrgent(true, true, 5.0f));
    CHECK(FirstPrimaryCombatUrgent(true, false, 0.5f));
    CHECK(!FirstPrimaryCombatUrgent(true, false, 1.0f));
    CHECK(!FirstPrimaryCombatUrgent(true, false, 1e9f));
    CHECK(!FirstPrimaryCombatUrgent(false, true, 0.0f));
    CHECK(!ShouldSeekFirstPrimary(true, false, 20.0f, false, FirstPrimaryCombatUrgent(true, true, 0.0f)));
    CHECK(ShouldSeekFirstPrimary(true, false, 20.0f, false, FirstPrimaryCombatUrgent(false, true, 0.0f)));

    // #3: finish lock at <=25 HP; a new enemy within 8 m that is hitting us wins.
    CHECK(KeepLowHpTarget(true, true, true, 25, 0.5f, 12.0f, false, false));
    CHECK(KeepLowHpTarget(true, true, true, 10, 0.9f, 8.0f, true, false));
    CHECK(!KeepLowHpTarget(true, true, true, 25, 0.5f, 7.9f, true, true));
    CHECK(!KeepLowHpTarget(true, true, true, 26, 0.5f, 12.0f, false, false));
    CHECK(!KeepLowHpTarget(true, true, true, 10, 1.0f, 12.0f, false, false));
    CHECK(!KeepLowHpTarget(true, false, true, 10, 0.2f, 12.0f, false, false));
    CHECK(!KeepLowHpTarget(false, true, true, 10, 0.2f, 12.0f, false, false));

    // #2: withdraw cover = a reachable, LOS-breaking spot within 8 m (maxMove), not a clear one farther away.
    {
        const CoverCandidate spots[3] = {
            { { 5.0f, 0.0f, 0.0f }, 0 },
            { { 9.0f, 0.0f, 0.0f }, 0 },
            { { 0.0f, 5.0f, 0.0f }, 0 },
        };
        const TVec bot{ 0.0f, 0.0f, 0.0f }, threat{ 20.0f, 0.0f, 0.0f };
        const int i = PickCover(spots, 3, bot, threat, 15.0f,
            [](const TVec& p) { return p.x == 5.0f || p.x == 9.0f; }, 8.0f);
        CHECK(i == 0); // the 5 m blocked spot; the 9 m one is outside maxMove
        const int none = PickCover(spots, 3, bot, threat, 15.0f,
            [](const TVec&) { return false; }, 8.0f);
        CHECK(none < 0); // no cover within 8 m -> abort the withdraw and fight
    }

    if (failures) { std::printf("audit_fixes: %d FAILURES\n", failures); return 1; }
    std::printf("audit_fixes: all tests passed\n");
    return 0;
}
