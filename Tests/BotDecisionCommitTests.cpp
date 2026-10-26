// DS-043 P1 tests: one state change per decision serial, dwell, attack band and move commit (engine-free).
#include "CryGame/Bots/BotDecisionCommit.h"
#include <cassert>
#include <cstdio>

using namespace WarsBots;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main()
{
    {
        DecisionCommit c; c.Reset(); c.BeginTick(1, 0.0f);
        CHECK(c.Offer(DecisionOwner::Roam, DecisionState::Search, 0.0f, false));
        CHECK(!c.Offer(DecisionOwner::Upgrade, DecisionState::MoveToWeapon, 0.0f, false));
        CHECK(c.TickConflicts() == 1 && c.Conflicts() == 1);
        CHECK(c.State() == DecisionState::Search);
        CHECK(c.FirstOwner() == DecisionOwner::Arbiter && c.SecondOwner() == DecisionOwner::Upgrade);
    }
    {
        // Death in the same tick must win over a roam change.
        DecisionCommit c; c.Reset(); c.BeginTick(1, 0.0f);
        CHECK(c.Offer(DecisionOwner::Roam, DecisionState::Search, 0.0f, false));
        CHECK(c.Offer(DecisionOwner::Arbiter, DecisionState::Dead, 0.0f, true));
        CHECK(c.State() == DecisionState::Dead);
        CHECK(c.TickConflicts() == 1);
    }
    {
        // Same state from another writer is not a conflict.
        DecisionCommit c; c.Reset(); c.BeginTick(1, 10.0f);
        CHECK(c.Offer(DecisionOwner::Roam, DecisionState::Search, 10.0f, false));
        CHECK(c.Offer(DecisionOwner::Combat, DecisionState::Search, 10.0f, false));
        CHECK(c.Conflicts() == 0);
        CHECK(c.Owner() == DecisionOwner::Combat);
    }
    {
        // 0.3 s non-urgent dwell.
        DecisionCommit c; c.Reset(); c.BeginTick(1, 0.0f);
        CHECK(c.Offer(DecisionOwner::Roam, DecisionState::Search, 0.0f, false));
        c.BeginTick(2, 0.1f);
        CHECK(!c.Offer(DecisionOwner::Combat, DecisionState::Attack, 0.1f, false));
        c.BeginTick(4, 0.3f);
        CHECK(c.Offer(DecisionOwner::Combat, DecisionState::Attack, 0.3f, false));
        CHECK(c.State() == DecisionState::Attack);
    }
    {
        // P1 attack band: 1.5 s before falling back to CHASE.
        DecisionCommit c; c.Reset(); c.BeginTick(1, 0.0f);
        CHECK(c.Offer(DecisionOwner::Combat, DecisionState::Attack, 0.0f, true));
        c.BeginTick(2, 1.4f);
        CHECK(!c.Offer(DecisionOwner::Combat, DecisionState::Chase, 1.4f, false));
        c.BeginTick(3, 1.6f);
        CHECK(c.Offer(DecisionOwner::Combat, DecisionState::Chase, 1.6f, false));
    }
    {
        // 100 Hz input still yields at most ~4 changes in 1 s (0.3 s dwell), and at most one per tick.
        DecisionCommit c; c.Reset();
        int changes = 0;
        DecisionState last = DecisionState::Search;
        for (int i = 0; i < 100; ++i)
        {
            const float t = i * 0.01f;
            c.BeginTick(uint32_t(i + 1), t);
            const DecisionState s = (i % 2) ? DecisionState::Search : DecisionState::MoveToWeapon;
            if (c.Offer(DecisionOwner::Roam, s, t, false) && s != last) { ++changes; last = s; }
        }
        CHECK(changes <= 4);
    }
    {
        // Rate-limited conflict log.
        DecisionCommit c; c.Reset(); c.BeginTick(1, 0.0f);
        c.Offer(DecisionOwner::Roam, DecisionState::Search, 0.0f, false);
        CHECK(!c.Offer(DecisionOwner::Upgrade, DecisionState::MoveToWeapon, 0.0f, false));
        CHECK(c.ShouldLogConflict(0.0f));
        CHECK(!c.ShouldLogConflict(0.5f));
        CHECK(c.ShouldLogConflict(1.0f));
    }
    {
        // MoveCommit holds a destination for 0.75 m / 2.5 s and can be read back.
        MoveCommit m;
        CHECK(!m.Has());
        CHECK(!m.Valid(1, 2, 3, 0.0f));
        m.Set(1, 2, 3, 0.0f);
        float x = 0, y = 0, z = 0; m.Get(x, y, z);
        CHECK(x == 1 && y == 2 && z == 3);
        CHECK(m.Has() && m.Valid(1.2f, 2.0f, 3.0f, 0.5f));
        CHECK(!m.Valid(2.0f, 2.0f, 3.0f, 0.5f));
        CHECK(!m.Valid(1.1f, 2.0f, 3.0f, 3.0f));
        m.Reset();
        CHECK(!m.Has());
    }

    if (failures) { std::printf("decision_commit: %d FAILURES\n", failures); return 1; }
    std::printf("decision_commit: all tests passed\n");
    return 0;
}
