# DS-099 R1: candidate cache can freeze weapon search (confirmed)

Scope: exact code `c00cae46319a2bd8540ca3f85c184787996e778f` (`claude-sprint`), read via `git show`. Engine-free oracle
built against the real `BotWeaponGoal.h` + `BotGoalArbiter.h` of that SHA. No game/build/feature write.

**Verdict: R1 is real and permanent, not a timing artefact.** A candidate the arbiter scores below Roam stays cached
with `since == 0`, `TryAcquireUpgradeGoal` then refuses every non-zero target, and nothing else ages a non-pursued
candidate — so a newly available better gun is never scanned while that item stays available. This is a regression
against b8, which re-scanned every tick.

## 1. Why it happens (exact lines)

| Step | Code | Effect |
| --- | --- | --- |
| 1 | `BotGoalArbiter.h:143-151` | with `hasPrimary` and gain `< upgradeGain (0.3)` the weapon score is clamped to **0.05** inside `if (weaponGoalAvailable)`, below Roam's **0.1** (`:153-156`) |
| 2 | `BotBrain.cpp:1793-1795` (c00) | `UpdateArbitrated` calls `TryAcquireUpgradeGoal` every tick; the found item is stored in `m_upgradeTarget` for the arbiter's candidate view |
| 3 | `SetUpgradeTarget` (`:786-792`) | stores `m_upgradeSince = 0` ("acquired, not pursued yet") |
| 4 | `MayAcquireWeaponGoal` (`BotWeaponGoal.h:14-17`) | `target == 0u` is required — a cached target blocks every later scan |
| 5 | `UpdateUpgrade` (`:816-821`) | the only place that ages a candidate/timeout runs **only** when the arbiter chose Weapon (`:1801`) |
| 6 | `BotBrain.cpp:1796` | clear only when the item is stolen/unavailable — an item nobody takes never clears |

So with `weaponRank=1` + `arbiter=1` (the C153 preset) and a marginal or far upgrade, the bot can freeze its weapon
search for the rest of the life. With no primary the weapon score is ≥ 0.05 but usually wins, so the freeze needs a
primary — exactly the ranked case.

**Stale pursued goal (second half of R1):** a goal that is being pursued is only ended by pickup, unavailable,
8/20 s progress or 60/90 s total. After a PS buy / test fallback primary the premise can be gone, yet the bot keeps
walking up to 60 s. Bounded, not infinite, but wasteful and it hides the new inventory from the arbiter.

## 2. Red/green evidence (actual policy code, executed)

`ds099_r1_oracle.cpp` (in this folder) includes the real `BotWeaponGoal.h` and `BotGoalArbiter.h`, replays the
`UpdateArbitrated` weapon path at 10 Hz and uses the real `MayAcquireWeaponGoal`/`WeaponGoalPursues`/`GoalArbiter`.
MSVC 14.44, `/std:c++17 /EHsc`:

```
RED   R1: better gun selected=0 at t=-1.0, final target=1          (cached A, 60 s, never rescanned)
GREEN1 R1: better gun selected=1 at t=6.1, final target=2          (candidate TTL only)
GREEN2 R1: better gun selected=1 at t=3.0, final target=2          (3 s replace, optional)
GREEN controls: pursued goal stable (both fixes)
RED  stale pursued goal: alive at 59.0s=1 (bounded by 60 s, not cleared by the buy)
GREEN stale pursued goal: cleared at the inventory change, not banned
```

A first version of the oracle used `0.0f` as "no candidate stamp" and failed exactly the way the game would: a scan at
`m_now == 0` is legitimate, so the stamp must use `-1.0f` (or target-presence) as "none".

## 3. Minimal correction proposal (not implemented here)

1. `BotWeaponGoal.h`: add one pure rule, keep the existing gate signature:
   ```cpp
   inline bool WeaponCandidateExpired(float now, float candidateAt, float ttl)
   { return candidateAt >= 0.0f && now - candidateAt > ttl; }
   ```
2. `BotBrain.h`: `float m_upgradeCandidateAt = -1.0f;` (never `0` — see above).
3. `SetUpgradeTarget(id)`: `m_upgradeCandidateAt = id ? m_now : -1.0f;` (all existing call sites already route here).
4. Start of `TryAcquireUpgradeGoal` (both callers get it):
   ```cpp
   if (m_upgradeTarget && !WarsBots::WeaponGoalPursues(m_upgradeSince)
       && WarsBots::WeaponCandidateExpired(m_now, m_upgradeCandidateAt, 6.0f))
   { WB_LOG(2, "... WEAPON GOAL previous=%u target=0 event=expire.", m_upgradeTarget); SetUpgradeTarget(0); }
   ```
   No ban, no flag, no change to `MayAcquireWeaponGoal`, active pursuit or the 8/20 s + 60/90 s budgets.
5. Inventory change: after a successful pickup in `UpdateWeaponSeek` and after `PSTryBuy`/`GiveTestSCAR` succeed,
   `SetUpgradeTarget(0)` (no ban) so the arbiter re-scores against the new inventory.
6. Optional, only if Codex wants ≤3 s latency instead of ≤6 s: let `MayAcquireWeaponGoal` accept a non-pursued target
   on the 3 s throttle (extra `pursuing` parameter) — GREEN2 above; not needed for correctness.

Boundedness after the fix: a scored-only candidate can hide a better gun for at most the TTL (6 s); an active pursuit is
never reselected; a pursued-but-obsolete goal is cleared at the next inventory change instead of waiting for 60 s.

## 4. Gate

`c00cae46` runtime HOLD stands until this is patched, CI is green and the new SHA is reviewed. My independent harness
BLOCK on `27e6c746` (`$pickupActors.Count -eq 8` breaks on a normal game reset → 16 actors) also still stands; the
fixed harness needs a new SHA. No game/build/install/feature edits; I stay idle after this push.
