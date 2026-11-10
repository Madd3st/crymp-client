
#include "CryGame/Bots/BotJumpLink.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

using namespace WarsBots;

static LinkInput At(float t, float x, float y, float z, bool grounded = true, float speed = 4.f)
{
    LinkInput in;
    in.now = t;
    in.feet = {x, y, z};
    in.grounded = grounded;
    in.speed = speed;
    return in;
}

int main(int argc, char** argv)
{
    bool expectNew = true;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--expect-old") expectNew = false;
    }

    const JVec roofA{1340.64f, 1351.55f, 151.84f}, roofB{1332.83f, 1346.08f, 150.04f};

    // Scenario 1: Long Strength jump (flight 1.9s) landing on roof deck (z=151.91, dz=1.87m, db=1.58m).
    {
        BotJumpLink L;
        L.Start(0.f, LinkMove::Strength, roofA, roofB);
        LinkInput in = At(0.f, 1340.f, 1351.f, 151.84f, true, 4.f);
        in.energy = 180.f; in.strengthActive = true;
        LinkOutput o = L.Update(in);
        assert(o.phase == LinkPhase::Act);

        // Airborne from 0.2s to 2.1s (1.9s in air)
        o = L.Update(At(0.2f, 1338.f, 1349.f, 154.5f, false));
        assert(o.phase == LinkPhase::Air);
        o = L.Update(At(1.5f, 1336.f, 1348.f, 153.5f, false));
        assert(o.phase == LinkPhase::Air);

        // Touchdown at t=2.1s: feet at (1334.28, 1346.72, 151.91)
        in = At(2.1f, 1334.28f, 1346.72f, 151.91f, true);
        in.speed = 3.5f;
        o = L.Update(in);

        if (expectNew)
        {
            if (o.phase != LinkPhase::Done || L.Active())
            {
                std::fprintf(stderr, "FAIL: expected LinkPhase::Done on touchdown, got phase=%d fail=%d\n",
                    int(o.phase), int(o.fail));
                return 1;
            }
        }
        else
        {
            // Old code failed immediately with MissedLanding because flight > 0.6s and dz=1.87m > 1.5m
            if (o.phase != LinkPhase::Failed || o.fail != LinkFail::MissedLanding)
            {
                std::fprintf(stderr, "FAIL: expected old code to fail with MissedLanding, got phase=%d\n",
                    int(o.phase));
                return 1;
            }
        }
    }

    // Scenario 2: Roll-in after touchdown (lands 3.4m from B at t=2.0s, rolls within 1.5m at t=2.3s).
    if (expectNew)
    {
        BotJumpLink M;
        M.Start(0.f, LinkMove::Strength, roofA, roofB);
        LinkInput in = At(0.f, 1340.f, 1351.f, 151.84f, true, 4.f);
        in.energy = 180.f; in.strengthActive = true;
        M.Update(in);
        M.Update(At(0.2f, 1338.f, 1349.f, 154.5f, false));

        // Touchdown at t=2.0s: 3.4m away (outside 3.0m landRadius)
        in = At(2.0f, 1332.83f + 3.4f, 1346.08f, 150.04f, true);
        LinkOutput o = M.Update(in);
        assert(o.phase == LinkPhase::Air); // Grace period: still Air, waiting for roll

        // At t=2.3s (0.3s after landing): rolled to 1.5m from B
        in = At(2.3f, 1332.83f + 1.5f, 1346.08f, 150.04f, true);
        o = M.Update(in);
        assert(o.phase == LinkPhase::Done && !M.Active());
    }

    // Scenario 3: Real missed landing (fell off roof to ground z=136m, dz=14m).
    {
        BotJumpLink N;
        N.Start(0.f, LinkMove::Strength, roofA, roofB);
        LinkInput in = At(0.f, 1340.f, 1351.f, 151.84f, true, 4.f);
        in.energy = 180.f; in.strengthActive = true;
        N.Update(in);
        N.Update(At(0.2f, 1338.f, 1349.f, 154.5f, false));

        // Landed at z=136m at t=2.0s
        N.Update(At(2.0f, 1332.83f, 1346.08f, 136.0f, true));
        // After 0.7s on ground (t=2.7s), must fail with MissedLanding
        LinkOutput o = N.Update(At(2.7f, 1332.83f, 1346.08f, 136.0f, true));
        assert(o.phase == LinkPhase::Failed && o.fail == LinkFail::MissedLanding);
    }

    std::puts("PASS: bot jumplink roof landing caller test passed");
    return 0;
}
