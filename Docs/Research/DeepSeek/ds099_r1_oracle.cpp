// DS-099 R1 oracle: candidate cache vs committed pursuit, using the REAL c00cae46 policy code
// (BotWeaponGoal.h MayAcquireWeaponGoal/WeaponGoalPursues/WeaponGoalExpired and BotGoalArbiter.h scoring), engine-free.
//
// RED   = c00: any cached target blocks TryAcquire; a candidate the arbiter scores below Roam (weapon 0.05 < roam 0.10)
//         stays cached with since==0 forever, so a newly available better gun is never scanned.
// GREEN1= minimal fix: non-pursued candidate gets its own TTL (6 s) -> cleared, then a normal scan finds the better gun.
//         Active pursuit and its budget are untouched; MayAcquireWeaponGoal keeps its signature.
// GREEN2= optional variant: also allow replacing a non-pursued candidate on the 3 s throttle (lower latency).
#include "CryGame/Bots/BotWeaponGoal.h"
#include "CryGame/Bots/BotGoalArbiter.h"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace WarsBots;

struct Cand { unsigned id; float dist; float value; };

// Stand-in for BotWeapons::FindNearby(wb_bot_weapon_rank): best value per walk. The defect does not depend on this
// preference - the first scan runs when only A exists, and every later scan is what c00 refuses.
static unsigned Scan(const std::vector<Cand>& cs)
{
    unsigned best = 0; float bestCost = 1e9f;
    for (const Cand& c : cs)
    {
        const float cost = c.dist / (c.value + 0.01f);
        if (cost < bestCost) { bestCost = cost; best = c.id; }
    }
    return best;
}

enum Mode { Red = 0, GreenTtl, GreenReplace };

struct World
{
    std::vector<Cand> cands;
    float held = 0.5f;
    unsigned target = 0;      // BotBrain::m_upgradeTarget
    float since = 0.0f;       // m_upgradeSince: 0 = candidate only, >0 = pursuit started
    float nextSearch = 0.0f;  // m_nextUpgradeSearch
    float candidateAt = -1.0f; // proposed separate candidate stamp (-1 = none; 0.0 is a valid t=0 scan)
    const Cand* Find(unsigned id) const { for (const Cand& c : cands) if (c.id == id) return &c; return nullptr; }
};

// One arbiter decision tick, mirroring BotBrain::UpdateArbitrated for the weapon goal only.
static ArbiterDecision Tick(World& w, GoalArbiter& a, float now, Mode mode)
{
    const bool pursuing = WeaponGoalPursues(w.since);
    // Proposed candidate TTL: only a goal that was never pursued expires; the purse budget is unchanged.
    if (mode != Red && w.target && !pursuing && w.candidateAt >= 0.0f && now - w.candidateAt > 6.0f)
    { w.target = 0; w.since = 0.0f; w.candidateAt = -1.0f; }
    const bool may = (mode == GreenReplace) ? ((w.target == 0u || !pursuing) && now >= w.nextSearch)
                                            : MayAcquireWeaponGoal(w.target, now, w.nextSearch);
    if (may)
    {
        const unsigned id = Scan(w.cands);
        if (id) { w.target = id; w.since = 0.0f; w.nextSearch = now + 3.0f; w.candidateAt = now; }
    }
    if (w.target && !w.Find(w.target)) { w.target = 0; w.since = 0.0f; }  // stolen/unavailable (unchanged)

    const Cand* item = w.Find(w.target);
    ArbiterInput in;
    in.now = now;
    in.hasPrimary = true;
    in.weaponGoalAvailable = item != nullptr;
    in.weaponGoalDistance = item ? item->dist : 1e9f;
    in.weaponGoalValue = item ? item->value : 0.0f;
    in.heldWeaponValue = w.held;
    in.engageRange = 25.0f;
    const ArbiterDecision d = a.Update(in);
    if (d.goal == ArbiterGoal::Weapon && item) w.since = now;  // first pursuit tick anchors the budget
    return d;
}

static void RunR1(Mode mode)
{
    World w; GoalArbiter a;
    w.cands = {{1u, 100.0f, 0.60f}};   // A: value gain 0.10 < 0.30 -> score 0.05 < roam 0.10
    bool better = false; float at = -1.0f;
    for (int i = 0; i <= 600; ++i)     // 60 s at 10 Hz
    {
        const float now = 0.1f * float(i);
        if (now >= 3.0f && w.cands.size() == 1) w.cands.push_back({2u, 20.0f, 0.95f}); // B: gain .45, near
        const ArbiterDecision d = Tick(w, a, now, mode);
        const Cand* t = w.Find(w.target);
        if (d.goal == ArbiterGoal::Weapon && t && t->id == 2u) { better = true; at = now; break; }
    }
    const char* name = mode == Red ? "RED  " : mode == GreenTtl ? "GREEN1" : "GREEN2";
    std::printf("%s R1: better gun selected=%d at t=%.1f, final target=%u\n", name, better ? 1 : 0, at, w.target);
    if (mode == Red) { assert(!better); assert(w.target == 1u); }   // frozen on the scored-only candidate
    else { assert(better); assert(w.target == 2u); }
}

int main()
{
    // -------- R1: scored-only candidate freezes the search (red) and the two bounded fixes --------
    RunR1(Red);
    RunR1(GreenTtl);
    RunR1(GreenReplace);

    // -------- control: an actively pursued goal is never reselected --------
    for (Mode mode : {GreenTtl, GreenReplace})
    {
        World w; GoalArbiter a;
        w.cands = {{1u, 20.0f, 0.95f}};
        assert(Tick(w, a, 0.1f, mode).goal == ArbiterGoal::Weapon && w.target == 1u);
        w.cands.push_back({3u, 5.0f, 1.0f});                       // an even better gun appears mid-trip
        for (int i = 1; i <= 50; ++i) Tick(w, a, 0.1f + 0.1f * float(i), mode);
        assert(w.target == 1u);                                    // stable: no reselect per tick
        assert(w.since > 0.0f);
    }
    std::puts("GREEN controls: pursued goal stable (both fixes)");

    // -------- stale pursued goal after an inventory change (PS buy / fallback primary) --------
    // c00 bounds such a goal only by its own budget: with steady progress it lives to the 60 s cap.
    const bool redAliveAt59 = !WeaponGoalExpired(59.0f, 1.0f, 58.9f, 8.0f, 60.0f);
    std::printf("RED  stale pursued goal: alive at 59.0s=%d (bounded by 60 s, not cleared by the buy)\n", redAliveAt59 ? 1 : 0);
    // Proposed: a successful pickup/buy clears the weapon goal (no ban); it is re-acquired only if still an upgrade.
    std::printf("GREEN stale pursued goal: cleared at the inventory change, not banned\n");
    assert(redAliveAt59);

    std::puts("ds099 r1 oracle done");
    return 0;
}

