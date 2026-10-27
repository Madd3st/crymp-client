// DS-048 #3: PS-1 capture goal planner tests (engine-free).
#include "CryGame/Bots/BotPSCapture.h"
#include <cassert>
#include <cstdio>

using namespace WarsBots;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static PsCapturePoint P(int id, int owner, int value, float x, float y, bool power = false)
{
    PsCapturePoint p;
    p.hasDestination=true; p.id = id; p.ownerTeam = owner; p.value = value; p.x = x; p.y = y; p.z = 0; p.power = power;
    return p;
}

int main()
{
    // 1. Own points are ignored; the nearest non-owned point wins.
    {
        PsCapturePoint pts[3] = { P(1, 1, 1, 5, 0), P(2, 0, 1, 20, 0), P(3, 2, 1, 10, 0) };
        BotPSCapture c;
        const PsCapturePlan plan = c.Update(pts, 3, 1, 0, 0, 0);
        CHECK(plan.valid && plan.id == 3);
        CHECK(!plan.hold);
        CHECK(c.Target() == 3);
    }

    // 2. Near a destination is insufficient; native area membership of the selected target owns holding.
    {
        PsCapturePoint pts[1] = { P(7, 0, 1, 3, 0) };
        BotPSCapture c;
        CHECK(!c.Update(pts,1,2,0,0,0,false).hold);
        const PsCapturePlan plan = c.Update(pts,1,2,0,0,0,true);
        CHECK(plan.valid && plan.id == 7 && plan.hold);
    }

    // 3. Value and power weight an otherwise farther point.
    {
        PsCapturePoint pts[2] = { P(10, 0, 1, 12, 0, false), P(11, 0, 3, 30, 0, false) };
        BotPSCapture c;
        // 12 - 8 = 4 vs 30 - 24 = 6 -> the low-value point is still first.
        CHECK(c.Update(pts, 2, 1, 0, 0, 0).id == 10);
        pts[1].power = true; // 30 - 24 - 6 = 0 -> power point wins
        BotPSCapture c2;     // fresh commitment: the hysteresis must not hide the weighted choice
        CHECK(c2.Update(pts, 2, 1, 0, 0, 0).id == 11);
    }

    // 4. Hysteresis: a slightly closer point does not cause a switch; a much closer one does.
    {
        PsCapturePoint pts[2] = { P(20, 0, 1, 20, 0), P(21, 0, 1, 10, 0) };
        BotPSCapture c;
        CHECK(c.Update(pts, 2, 1, 0, 0, 0).id == 21);
        // Reorder: make the current 20 m away and the alternative 10 m away (10 m better < 15 m margin).
        pts[0] = P(21, 0, 1, 20, 0); pts[1] = P(20, 0, 1, 10, 0);
        CHECK(c.Update(pts, 2, 1, 0, 0, 0).id == 21);
        // Alternative 1 m away (19 m better) -> switch.
        pts[1] = P(20, 0, 1, 1, 0);
        CHECK(c.Update(pts, 2, 1, 0, 0, 0).id == 20);
    }

    // 5. A captured target is dropped and the next point is selected.
    {
        PsCapturePoint pts[2] = { P(30, 0, 1, 5, 0), P(31, 0, 1, 9, 0) };
        BotPSCapture c;
        CHECK(c.Update(pts, 2, 1, 0, 0, 0).id == 30);
        pts[0].ownerTeam = 1;
        CHECK(c.Update(pts, 2, 1, 0, 0, 0).id == 31);
    }

    // 6. No team / no points -> invalid, no crash.
    {
        PsCapturePoint p = P(1, 0, 1, 1, 1);
        BotPSCapture c;
        CHECK(!c.Update(&p, 1, 0, 0, 0, 0).valid);
        CHECK(!c.Update(nullptr, 0, 1, 0, 0, 0).valid);
    }

    // An unvalidated/unreachable destination is never a goal.
    {PsCapturePoint p=P(80,0,1,0,0);p.hasDestination=false;BotPSCapture c;CHECK(!c.Update(&p,1,1,0,0,0).valid);}
    if (failures) { std::printf("ps_capture: %d FAILURES\n", failures); return 1; }
    std::printf("ps_capture: all tests passed\n");
    return 0;
}
