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

    // Audit 14.11 p. 5: sight is cleared at the start of every tick. A fight tick writes it; the next ticks belong to
    // the weapon goal (nobody writes): unstick must not stay muted and the sprint must not stay blocked.
    {
        bool visible = false; float distance = 0.0f;
        const float holdRange = 40.0f;
        // fight tick: begin, then the fight writes what it sees
        BeginTickSight(visible, distance); visible = true; distance = 12.0f;
        CHECK(UnstickCombatMute(visible, true, true));
        CHECK(ProFireBlocksSprint(true, visible, distance, holdRange, false, true));
        // the bug: the fight is over, the tick goes to the weapon goal and nothing clears the fields
        CHECK(UnstickCombatMute(visible, false, false));
        CHECK(ProFireBlocksSprint(true, visible, distance, holdRange, false, true));
        // the fix: the same weapon-goal tick after BeginTickSight - unstick listens again, the sprint is allowed
        BeginTickSight(visible, distance);
        CHECK(!visible && distance == 0.0f);
        CHECK(!UnstickCombatMute(visible, false, false));
        CHECK(!ProFireBlocksSprint(true, visible, distance, holdRange, false, true));
        // a real fight still mutes and still blocks; the state and the intent mute on their own
        BeginTickSight(visible, distance); visible = true; distance = 39.0f;
        CHECK(UnstickCombatMute(visible, false, false) && ProFireBlocksSprint(true, visible, distance, holdRange, false, true));
        CHECK(!ProFireBlocksSprint(true, visible, 41.0f, holdRange, false, true));   // beyond the hold range: sprint
        CHECK(!ProFireBlocksSprint(false, visible, distance, holdRange, false, true));
        CHECK(!ProFireBlocksSprint(true, visible, distance, holdRange, true, true));
        CHECK(!ProFireBlocksSprint(true, visible, distance, holdRange, false, false));
        CHECK(UnstickCombatMute(false, true, false) && UnstickCombatMute(false, false, true));
    }

    if (failures) { std::printf("audit_fixes: %d FAILURES\n", failures); return 1; }
    std::printf("audit_fixes: all tests passed\n");
    return 0;
}
