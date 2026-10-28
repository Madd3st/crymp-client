# DS-100: pre-CI review of local 41a72087 (game code = 2e595ccb) and harness 57a41e6a

Inputs (read-only, `git -C C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build/ds076-code show SHA:path`):
final local code **41a7208755017d425a465b42eebd308105d18947** (parent 2e595ccb → afa96b35 → c00cae46; game code
byte-identical to 2e595ccb, the only change is wrapping `Tests/test_bot_weapon_goal_caller.py` in `main()`), and harness
`57a41e6a:Tools/Invoke-WarsBotsC153Batch.ps1` blob `f22fbfcae1da6cc3c9b1616625e9c8c4b3628cdb` +
`Tools/Analyze-WarsBotsC153.py`. No game/build/install/feature write; runtime source c00 stays HOLD.

**Verdict: code 41a72087 = BLOCK (one required fix, loot regression). Harness 57a41e6a = OK.**

## 1. R1 is fixed as intended (verified)

* Unchosen candidates refresh on the existing 3 s throttle (`BotWeaponGoal.h:12-15`, `pursuing=false`), active pursuit
  is held (`pursuing=true`), and an empty scan clears the old candidate (`BotBrain.cpp:803`). No new timestamp/sentinel.
* Shared `WeaponGoalWorthwhile` (`BotGoalArbiter.h:64-68`) now drives both scoring (`:136`) and stale-goal cancellation
  (`BotBrain.cpp:1813-1814`), so a worthless candidate no longer carries the old 0.05 floor below Roam.
* Inventory hooks clear the goal without banning: PS primary buy (`:560`), seek-pickup (`:750`), test grant (`:759`).
* Independent real-body caller test (I re-ran the same extraction with MSVC because `python` is not installed here):
  c00 source → exit **7** ("unchosen candidate did not refresh"), 41a72087 source → exit **0**
  ("candidate refresh, pursuit hold, empty-search throttle"). `BotWeaponGoalTests` and `BotGoalArbiterTests` compile and
  pass, including the new 4-arg gate cases and the stale-inventory arbiter case (`heldWeaponValue=1.0` → not Weapon).
* The 41a72087 `main()` guard is correct and makes test discovery inert; CI step (`warsbots-ci.yml:64-66`) runs after
  the MSVC setup in the same step, so `cl` is on PATH and a failure is fatal.

## 2. BLOCK: the Worthwhile cancel also kills deliberate loot goals

`wb_bot_tactics 1` (C153 preset) loots a dead enemy's gun: `UpdateRoam` sets the goal at `BotBrain.cpp:899-906`
(`SetUpgradeTarget(drop)`, after `m_lootUntil=0` at `:903`) and walks it via `:910`. On the next decision tick
`UpdateArbitrated` runs first and `:1813-1814` cancels any goal that is not a ≥ 0.3 upgrade. A same-class drop (the
common ammo case) has `weaponGoalValue - heldWeaponValue == 0` (`HeldValue` = max held `FightValue`,
`BotWeapons.cpp:241-247`), so it is cancelled before `UpdateRoam` can continue; `m_lootUntil` was already zeroed, so
the drop is never re-selected. Net: loot still works when the bot is unarmed or the drop is a real upgrade, and is
silently disabled for an armed bot with ammo — a behaviour change in the same subsystem, not covered by any test.

Minimal fix (pick one):

* **A (one line):** cancel only while the arbiter is committed to the pursuit:
  `if (m_upgradeTarget && m_arbiter.Current()==ArbiterGoal::Weapon && !m_arbiter.WeaponGoalWorthwhile(in))`.
  A stale pursuit was chosen as Weapon; a loot goal is Roam-driven, so it survives. The explicit buy/pickup hooks still
  clear immediately.
* **B (explicit origin):** add `bool m_upgradeDeliberate` (set by the loot path, cleared in `SetUpgradeTarget`) and skip
  the cancel while it is set.

Required test: loot-style goal (gain 0, arbiter current Roam) must not be cleared by the tick; a pursued worthless goal
(current Weapon, buy/held gain) must be cleared. If the intent is instead “loot only upgrades”, say so and update the
tactics text/tests — but it should not happen silently.

Everything else in the diff is accepted; the only blocking item is this unconditional cancellation.

## 3. Harness 57a41e6a: OK

Reproduced its roster logic on the owner's real archive `%LOCALAPPDATA%/WarsBots/Logs/S150M-20261028-223049.bots.log`
with the harness's own regexes: 16 `created` lines → 8 slots, 16 historic pickup actors, **8 current**, **292**
`ATTACKING`, **56** `revive result … sameEntity=1 alive=1`, 0 test grants. Old criterion (`-eq 8`) fails on the archive;
the new one (latest id per slot, all 8 current ids with a real primary pickup, combat/revive after the last creation,
roster stable ≥ 10 s) passes — the reset fix is correct. `quality_pass` is now separate from `runtime_criteria_pass`, so
a post-run review-head mismatch cannot masquerade as an accepted run; `failure` is recorded. `Analyze-WarsBotsC153.py`
independently checks restore names/hashes against the live directories, demo header `build` prefix vs source, armed
inventory for every current bot, and goal-event hygiene (no mid-pursuit replacement, no repeated pursue anchor), which
also protects the new 3 s refresh from a timer restart.

Minor, non-blocking notes: the analyzer loads `build/ds076-triage/Tools/SteelmillTriage/wblab/demo.py` from a local
triage path (vendor it into `Tools/` for portability); its demo-actor assertions are fail-closed if a reset happens in
the last seconds of the run; the pre-stage review check is still not written to `review-checks.jsonl` (out created
later).

## 4. Gate

Fix the loot cancellation, push a new local SHA, CI green; then a quick re-review of that guard is enough (harness
unchanged, its SHA can be pinned as is). I stay idle after this push so the review-head pin stays fresh.
