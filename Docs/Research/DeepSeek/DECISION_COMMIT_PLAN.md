# DecisionCommit integration plan (< 2 flips/min)

Date: 2026-10-26. Target: C-111 `wb_bot_state_commit=1` gives **6.91 A→B→A per bot-minute** (off: 81.79); the
owner's bar is **< 2**. Reversals are 15.58/bot-min (off 13.29); kills 34 vs 39, stalls 9 vs 11 — the remaining work
must not lower combat output. Module: `proto/decision_commit.h` (already built/tested, MSVC).

## Where the remaining 6.91 comes from

1. **ATTACKING↔CHASING** still flips at the range boundary (`BotBrain.cpp:766-768`). The 0.5 s `attackDwell` only
   delays the first leave; a target oscillating around `holdRange` (22 m Universal, 38 Elite after DS-035) still
   produces A→B→A within 2 s.
2. **One `Change` per writer is not enforced**: `UpdateRoam`/`UpdateUpgrade` are ordered now, but any future writer
   (vantage, hold-angle, loot, chase-abandon) can still change state in the same tick. C-111 has no conflict counter.
3. **Movement destination still changes per tick**: `m_intent` is rebuilt (`:322`); roam/weapon/combat can write a new
   destination; the body reverses before the state flips, so `reversals` stay above the owner's 8/min.

## Integration steps

### 1. Commit gate in `Change` (P1, core)

`proto/decision_commit.h` → `Code/CryGame/Bots/BotDecisionCommit.h` (new file, no engine types). Add to `BotBrain.h`:

```cpp
#include "CryGame/Bots/BotDecisionCommit.h"
WarsBots::DecisionCommit m_commit;
```

At the top of the 10 Hz block (`BotBrain.cpp:316-319`): `m_commit.BeginTick(m_decisionSerial, m_now);`

Change the signature to `Change(State s, bool init=false, DecisionOwner owner=DecisionOwner::Arbiter)`; at
`BotBrain.cpp:50-58`:

```cpp
if (!init && !m_commit.Offer(owner, ToDecision(s), m_now, IsUrgent(s))) {
    if (m_commit.ShouldLogConflict(m_now))
        WB_LOG(1,"[WarsBots] #%d STATE CONFLICT first=%s second=%s actor=%u.", ...);
    return;
}
```

- one accepted change per decision serial (10 Hz);
- 0.3 s non-urgent dwell;
- urgent: `DEAD`, `RESPAWNING`, a newly visible target → `ATTACKING`;
- `STATE CONFLICT` count in the log, and in `flips.ps1` (`bots.state_conflicts`).

Map owners at the call sites: `UpdateRoam` → Roam, `UpdateUpgrade` → Upgrade, `UpdateCombat` → Combat, `UpdateVantage`
→ Roam, death/respawn → Arbiter/urgent. This is the T-1 "one owner" rule, observable.

### 2. Attack band: leave later and wider

- `LinkParams`/`BotParams`: `attackLeave 0.5 → 1.5 s` (C-111 already has the field).
- In `UpdateCombat` (`:766`), once `ATTACKING`, leave only when
  `distance > max(holdRange + 4, preferredRange + 6)` **and** the 1.5 s dwell elapsed; enter `ATTACKING` at
  `engageRange` as today. This gives a 4 m hysteresis band instead of a point.
- Keep `UpdateChasing` firing at `fireRange` (the DS-035 behavior) so the 1.5 s hold does not lose damage.

### 3. Move destination commit

Use `MoveCommit` from the same header:

```cpp
if (!m_move.Valid(x,y,z,m_now) || owner == m_moveOwner) {
    m_move.Set(x,y,z,m_now); m_moveOwner = owner; WBI_MOVETO(...);
}
```

- hold a destination for 0.75 m / 2.5 s; only the owning state may replace it;
- `UpdateAttacking`'s strafe lateral is exempt (it is a body offset, not a destination);
- reset on `DEAD`/link start/trap recovery.

### 4. Verify

Run `Tools/PlaytestReport/flips.ps1` on C-111 A/B and compare:

| Field | C-111 on now | Target |
| --- | ---: | ---: |
| `bots.state_ab_a_2s_per_bot_min` | 6.91 | **< 2.0** |
| `bots.attacking_chasing_per_bot_min` | 14.01 (C-108) | < 2 |
| `bots.reversals_per_bot_min` | 15.58 | < 10 |
| `bots.state_conflicts` | 0 (not logged) | visible, then 0 |
| kills / stalls | 34 / 9 | within noise of off (39 / 11) |

Repeat the same 6-bot, 300 s recipe and the same memory/roster; do not compare a different RNG seed as a fix.

## Order / risk

1. Gate first (no behavior change beyond which writer wins).
2. Then `attackLeave=1.5` + 4 m band; this is the biggest single drop (ATTACKING↔CHASING).
3. Then `MoveCommit` (reversals).
4. Keep `wb_bot_state_commit=1`; make it the default only after two paired runs hit <2 without a kill drop.

Risks: an over-long dwell can delay a legitimate retreat/finish; keep `DEAD`/`RESPAWNING`/new-target urgent. A
destination hold can fight `UpdateAttacking` strafe; exempt lateral offsets. `stateTime` is also used by reaction and
trap checks — the gate must not change `m_stateTime` on a rejected offer.

## Expected

The 26k `SEARCHING↔MOVING` pair is already gone; the remaining 6.91 is mostly ATTACKING↔CHASING and writer races.
With a 4 m band + 1.5 s leave + one-writer gate, the expected range is **1–2 flips/bot-min**, with reversals near
10/min and kills/stalls within the C-111 noise band.
