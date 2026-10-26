#include "decision_commit.h"
#include <cstdio>
#include <cmath>
using namespace WarsBots;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

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
        DecisionCommit c; c.Reset(); c.BeginTick(1, 10.0f);
        CHECK(c.Offer(DecisionOwner::Roam, DecisionState::Search, 10.0f, false));
        CHECK(c.Offer(DecisionOwner::Combat, DecisionState::Search, 10.0f, false));
        CHECK(c.Conflicts() == 0);
        CHECK(c.Owner() == DecisionOwner::Combat);
    }
    {
        DecisionCommit c; c.Reset(); c.BeginTick(1, 0.0f);
        CHECK(c.Offer(DecisionOwner::Roam, DecisionState::Search, 0.0f, false));
        c.BeginTick(2, 0.1f);
        CHECK(!c.Offer(DecisionOwner::Combat, DecisionState::Attack, 0.1f, false)); // dwell 0.3
        c.BeginTick(4, 0.3f);
        CHECK(c.Offer(DecisionOwner::Combat, DecisionState::Attack, 0.3f, false));
        CHECK(c.State() == DecisionState::Attack);
    }
    {
        DecisionCommit c; c.Reset(); c.BeginTick(1, 0.0f);
        CHECK(c.Offer(DecisionOwner::Roam, DecisionState::Search, 0.0f, false));
        c.BeginTick(2, 0.1f);
        CHECK(c.Offer(DecisionOwner::Combat, DecisionState::Attack, 0.1f, true)); // urgent
        CHECK(c.State() == DecisionState::Attack);
    }
    {
        DecisionCommit c; c.Reset(); c.BeginTick(1, 0.0f);
        CHECK(c.Offer(DecisionOwner::Combat, DecisionState::Attack, 0.0f, true));
        c.BeginTick(2, 0.3f);
        CHECK(!c.Offer(DecisionOwner::Combat, DecisionState::Chase, 0.3f, false)); // attack leave 0.5
        c.BeginTick(6, 0.5f);
        CHECK(c.Offer(DecisionOwner::Combat, DecisionState::Chase, 0.5f, false));
    }
    {
        DecisionCommit c; c.Reset();
        int changes = 0;
        DecisionState last = DecisionState::Search;
        for (int i = 0; i < 100; ++i)
        {
            const float t = i * 0.01f; // 100 Hz input, but state changes are gated
            c.BeginTick(uint32_t(i + 1), t);
            const DecisionState s = (i % 2) ? DecisionState::Search : DecisionState::MoveToWeapon;
            if (c.Offer(DecisionOwner::Roam, s, t, false) && s != last) { ++changes; last = s; }
        }
        CHECK(changes <= 4); // 1 s / 0.3 s dwell
    }
    {
        MoveCommit m;
        CHECK(!m.Valid(1, 2, 3, 0.0f));
        m.Set(1, 2, 3, 0.0f);
        CHECK(m.Valid(1.2f, 2.0f, 3.0f, 0.5f));
        CHECK(!m.Valid(2.0f, 2.0f, 3.0f, 0.5f)); // moved > 0.75
        CHECK(!m.Valid(1.1f, 2.0f, 3.0f, 3.0f)); // too old
    }
    {
        DecisionCommit c; c.Reset(); c.BeginTick(1, 0.0f);
        c.Offer(DecisionOwner::Roam, DecisionState::Search, 0.0f, false);
        CHECK(!c.Offer(DecisionOwner::Upgrade, DecisionState::MoveToWeapon, 0.0f, false));
        CHECK(c.ShouldLogConflict(0.0f));
        CHECK(!c.ShouldLogConflict(0.5f));
        CHECK(c.ShouldLogConflict(1.0f));
    }

    if (failures) { printf("decision_commit: %d FAILURES\n", failures); return 1; }
    printf("decision_commit: all tests passed\n");
    return 0;
}

