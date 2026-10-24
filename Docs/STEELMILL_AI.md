# Steelmill AI: measured geometry and remaining problems

Current P0 candidate and testing isolation limits: [fire control](Experiments/fire-control-2026-10-24.md).
No geometry/navigation changes in this increment; shared map memory of the active owner
server was not touched. Native QA cannot be isolated by port alone.

Map: `multiplayer/ia/steelmill`, original Crysis resources. No manually invented rooftop coordinates.

Engine scan in j13: 96 x 63 columns, 3 m spacing, vertical scan z=121..187;
7,633 accepted floor nodes and 41,618 walkable directed edges. Graph build consumed
264/272 ms summed across frames, not one frame. Spawn samples include different heights
136.35..147.49. This proves multiple sampled floors, not connectivity to every roof.

j13 `df3ceb8e`: 6 bots/240 s had 40% MOVING_TO_WEAPON samples and 54 upgrade timeouts;
15 bots/300 s had 31% and 115. The shared item search chooses by distance without a
route test, while travelling previously did not recheck ownership or visibility.
Confirmed code defect: an item could become owned/hidden after selection yet remain
an active movement goal until the 10-second timeout. The graph also could report no
route while the weapon task continued straight toward the item.

First experiment `20bf32bf`: revalidate native pickup eligibility and cancel unknown
upgrade routes after NAV ready. Six-bot QA reduced weapon movement share but also
reduced pickups and increased blocked movement; do not promote that rejection policy.
The revised variant preserves local steering fallback and fixes the failed-query retry
cache: an empty failed path previously bypassed its two-second deadline.
A missing graph route is not proof that a human cannot reach that item: 3 m sampling
misses narrow passages and there are no explicit Strength jump links. Before NAV ready,
the existing local steering fallback remains. Pickup reach and native movement are unchanged.

Next geometry work: log failed start/goal projections and target item positions; validate
actual progress and walkable entry/exit connectors. Only then add verified roof transitions.
Dynamic obstacles and moving items still need runtime local avoidance.

## Actual movement observations (2026-10-23)

[Tactical experiment](Experiments/tactics-2026-10-23.md) records actual stationary/loop
windows and visible-chase stalls that blocked flags alone missed. Broad search/weapon/
combat recovery harmed combat and was rejected. Final combat-only
`wb_progress_experiment=0` remains OFF; two ON smokes triggered no recoveries, so
its efficacy is unproved. Sparse A* fallback remains; no new jump links or blanket
missing-path rejection. Diagnose canMove/reload/freefall before further route changes.

## Native mechanics checked for this increment

`Actors/Player/NanoSuit.cpp::Update/UpdateSprinting` owns recharge and sprint drain;
`Actors/Player/PlayerMovement.cpp` owns jump acceptance and energy consumption.
No bot-side energy refill or health adjustment was introduced. `BotSuit::Apply` uses
energy hysteresis and a two-second mode-switch delay (Armor can interrupt).
`BotLocomotion::JumpSpace` checks overhead clearance, touchdown surface and far landing
for moving Strength jumps. These safety checks explain some rejected requests; raising
jump frequency alone does not create safe roof routes. Cloak remains opt-in OFF.

The JSON exporter lists frequent failed-route goal coordinates from actual engine logs.
They identify graph projection/connectivity investigations, not proven impossible geometry.
