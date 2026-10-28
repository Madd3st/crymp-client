# DS-101: tiny-delta review of local 5ef9ee22 (loot-preserving cancellation guard)

Inputs (read-only, `git -C C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build/ds076-code show SHA:path`): local game
**5ef9ee22dd2a84896f8ed33cc6c32e6e1261ccb9** (parent 41a72087), harness `57a41e6a` (unchanged, retained OK). No game,
build, install or feature write.

**Verdict: code 5ef9ee22 = OK. Harness 57a41e6a = OK (retained, blob unchanged).**

## 1. Delta (exactly as described)

* `BotBrain.cpp:1813`: `if (m_upgradeTarget && m_arbiter.Current()==ArbiterGoal::Weapon && !m_arbiter.WeaponGoalWorthwhile(in))`
  — one line, no new flag/state, no other game-code change (`git diff 41a72087 5ef9ee22 -- Code/` = 1 file, 1 line).
* `Tests/test_bot_weapon_goal_caller.py`: now extracts the REAL cancellation predicate by text (from `SetUpgradeTarget(0);
  in.weaponGoalAvailable=false;` back to the surrounding `if (`) and injects it into `CancellationFromBrain(...)`, then
  runs two fixtures: gain-0 with a fresh (Roam) arbiter must NOT cancel, gain-0 with a Weapon-committed arbiter must
  cancel. The acquisition/reset method controls and the `main()` discovery guard are retained; the CI step is unchanged.

## 2. Independent verification (no python on this machine)

I mirrored the Python extraction with PowerShell + MSVC and executed the same fixtures against both revisions:

* extracted predicate 5ef: `m_upgradeTarget && m_arbiter.Current()==ArbiterGoal::Weapon && !m_arbiter.WeaponGoalWorthwhile(in)`
* extracted predicate 41a: `m_upgradeTarget && !m_arbiter.WeaponGoalWorthwhile(in)`
* 5ef: `PASS real Brain callers: refresh, pursuit hold, empty search, loot survival, upgrade cancel` → exit **0**
* 41a: `FAIL: same-class tactics loot cancelled in Roam` → exit **8**

That reproduces the claimed RED/GREEN (exit 8 on the old predicate, green on 5ef) and confirms the refresh/hold/
empty-search controls still pass on the real method bodies.

## 3. Semantics of the guard

* **Loot preserved:** `UpdateRoam` sets the loot goal while the arbiter's current goal is Roam/Hunt, so the next tick
  does not cancel it; if the drop is a real upgrade the arbiter switches to Weapon and the pursuit is legitimate.
* **Obsolete upgrade still cancels:** `Current()==Weapon` is true on the tick where worthiness drops (the buy hooks at
  `:560/:750/:759` clear immediately anyway), so the stale-inventory fix is not weakened; the 0.05-score fix
  (`WeaponGoalWorthwhile` in `Scores`) is untouched.
* **R1 still bounded by the 3 s refresh:** a cached non-pursued candidate is no longer cancelled by this guard, but it is
  replaced on the existing throttle, which is the mechanism that fixed R1; active pursuit remains held.
* If an inventory gain happens while the arbiter is in a Fight/Hunt interrupt, cancellation lands on the next Weapon
  tick or is already handled by the explicit hook — bounded, no freeze.

## 4. Harness

`57a41e6a:Tools/Invoke-WarsBotsC153Batch.ps1` blob `f22fbfcae1da6cc3c9b1616625e9c8c4b3628cdb` is also the blob at the
current `weapon-goals` head `0490fddb`, i.e. unchanged since my DS100 OK (`6b63552e`). The reset-safe roster criterion,
`quality_pass` split and the archive numbers I reproduced (8 current of 16 historic, 292 attacks, 56 revives) still apply.

## 5. Gate

OK to push 5ef for CI. Runtime still requires full CI green and the fresh final outbox read before the single acceptance;
c00 stays HOLD until then. No further changes requested; idle after this push.

## 6. Addendum: final harness a21d0e2e (supersedes only the harness side)

`a21d0e2e147a2c613a5d39332128969a5d45dd91` on `weapon-goals` (blob `8dfe8ee6ff0e749cfd02e51ba8907555475aac44`; analyzer
`15b61c988164d1183a54d391e9d0a7ccbc6cf838`). Code `5ef9ee22` is unchanged (local worktree clean at 5ef, no new delta).

* Harness delta vs 57a41e6a is confined to the acceptance criterion and its evidence: a `#N AI state=DEAD` must be
  followed by that **same slot's** `revive result … sameEntity=1 alive=1` before `native_death_revive_cycles` increments;
  a revive without a preceding DEAD (spawn-guard recovery) no longer satisfies the run. `$passed` now requires
  `native_death_revive_cycles > 0` instead of any revive. Roster/reset, `quality_pass`, snapshots, timing, review pin and
  `PrepareOnly` are untouched (the new code sits inside the runtime loop; the PrepareOnly return at line 42 is before it).
* Reproduced on the owner's b8 archive with the harness's own regex: 57 `AI state=DEAD`, 56 `alive=1` revives,
  **56 matched same-slot cycles** — the stricter criterion passes on real data and would have ignored a revive that never
  followed a death.
* Analyzer delta: additionally requires a real kill event between two demo bot actors
  (`actor != other`, both in the demo actor set). Note the recorder stores `Event(3, victim, killer)` (BotDemoRecorder
  `Kill`), so the check is direction-agnostic — correct for "bot-vs-bot kill" and it still excludes a bot killed by a
  human (the killer id is not a bot). It also asserts `runtime.evidence.native_death_revive_cycles > 0`, matching the new
  harness criterion. The local `demo.py` dependency noted in DS100 remains.
* Verdict for this delta: harness **a21d0e2e = OK**; final code **5ef9ee22 = OK** (unchanged); runtime gate unchanged
  (full CI green + fresh final outbox read before the single acceptance).

