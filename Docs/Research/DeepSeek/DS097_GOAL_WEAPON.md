# DS-097: weapon-goal lifecycle patch against exact b8b444d6 (C153 batch 1)

Base: `b8b444d6c858f27a1fdde0d93b25a318cafc6937` (`warsbots/feature/claude-sprint`), read with
`git show b8b444d6:<path>`, never from this research branch's working copy.
Deliverable: [`DS097_GOAL_WEAPON.patch`](DS097_GOAL_WEAPON.patch) (applies to b8 with `git apply`, checked in a clean
detached worktree), new engine-free header `Code/CryGame/Bots/BotWeaponGoal.h`, new test
`Tests/BotWeaponGoalTests.cpp` (+ one line in the CI test list). No game, build, install or feature-branch write.

**Коротко по-русски:** у цели «идти за оружием» было два писателя и безусловный перезапуск таймеров на каждом тике
(`BotBrain.cpp:788-792` и `:1761-1762`), поэтому общий бюджет 60/90 с не мог истечь, а троттлинг поиска 3 с не работал.
Патч сводит создание цели в одну функцию, запускает общий бюджет с первого тика реального движения (а не с «посмотрел
кандидата»), сохраняет прогресс/реплан и таймауты 8/20 с и 60/90 с. Флаги, Lua и поведение боя не меняются.

## 1. Defect in b8 (source lines, exact SHA)

| Where | What | Effect |
| --- | --- | --- |
| `BotBrain.cpp:788-793` (`UpdateUpgrade`) | `UpgradeCandidate()` + `m_upgradeSince = m_now` + `m_nextUpgradeSearch = m_now + 3.0f` run on **every** call that passes `ShouldSearchUpgrade` | total budget (`:820`, 60 s / 90 s) can never fire; the 3 s throttle is overwritten each tick; a proximity query (`BotWeapons::FindNearby`) plus one Detour `FindPath` per candidate (`pathCost`) runs at 10 Hz |
| `BotBrain.cpp:796` | tracker reset compares `m_upgradeTarget != m_upgradeTracked` **after** the candidate was replaced | a target switch also restarts the 8 s/20 s progress clock, so both timeouts can be postponed indefinitely |
| `BotBrain.cpp:1761-1762` (`UpdateArbitrated`) | second writer with its own predicate `(!hasPrimary \|\| ranked)`, same unconditional stamps | whichever caller runs last owns the goal; the arbiter's look-ahead also restarts a goal's budget it may never chase |
| `BotBrain.cpp:873` (loot) | `m_upgradeSince = m_now` on assignment | third budget start, no tracker reset |
| `BotBrain.cpp:820` | `m_now - m_upgradeSince > totalTimeout` | dead code in practice because of the resets above |

Latent under defaults (`goalArbiter=0`, `tactics=0`, `firstPrimary=0`), but the legacy path `Update:439` runs the block
whenever `ShouldSearchUpgrade` is true (spawn-pistol phase runs it every tick), and `firstPrimary=1` (`:423`),
`wb_bot_arbiter=1`, `wb_bot_tactics=1` make it permanent. So the bug is reachable with shipped flags, not only theoretical.

## 2. Patch

One writer, one budget anchor, same timeouts:

* `TryAcquireUpgradeGoal(actor)` is the only place that calls `UpgradeCandidate`; it refuses when a goal exists or the
  3 s throttle has not expired, and also throttles "nothing found" (no proximity query per tick).
* `SetUpgradeTarget(id)` resets target + tracker + budget together, so a stale stamp can never outlive its target.
* `UpdateUpgrade`: acquisition through the writer, then the budget is anchored **on the first tick that pursues** the
  goal (`WeaponGoalPursues`), before the unchanged distance/path-progress/replan bookkeeping; expiry uses
  `WeaponGoalExpired` (progress 8 s / 20 s, total 60 s / 90 s, sniper variants unchanged).
* `UpdateArbitrated`: same writer, same throttle; it still caches a candidate for `ArbiterInput`, but the look does not
  start the budget, so a scored-but-not-chased goal cannot age into a false "unreachable" ban.
* `UpdateRoam` loot: `SetUpgradeTarget(drop)`, budget anchored by the forced `UpdateUpgrade` on the same tick.
* Pure rules in `BotWeaponGoal.h`: `MayAcquireWeaponGoal`, `WeaponGoalPursues`, `WeaponGoalExpired` — no engine types.

Preserved deliberately: `ShouldSearchUpgrade` policy (visible threat / primary / marksman), the arbiter's own
eligibility `(!hasPrimary || ranked)` and its threat scoring, `firstPrimary` pathCost bypass, sniper `sniperReach`
candidate filter, 8 s/20 s + 60 s/90 s bounds, `linkFailFast` ageing pause, bad-item ban (90 s) and unavailable-item
handling, `pathProgress` replan signature, no new flags, no defaults changed.

## 3. Red-old / green-patched evidence (offline)

1. **Unit test (executed)** — `Tests/BotWeaponGoalTests.cpp`, compiled with the CI recipe
   (`cl /std:c++17 /EHsc /I . /I Code`, MSVC 14.44):
   `weapon goal: all tests passed (old budget fires=0, patched fires=1)`.
   Covers: repeated ticks with the same target, no re-acquire while tracked, progress/replan not touching the total
   budget, 8 s/20 s progress expiry, 60 s/90 s total expiry, candidate-look-only (`since==0`) never expiring,
   unavailable/stolen clear + retry after throttle, ban + retry, primary acquired + throttle, old-block baseline.
2. **Caller contract on the real source (red on b8, green on patched)** — `ds097_caller_contract.ps1` reads
   `BotBrain.cpp` text and checks the invariant that the patch introduces:

   | Check | b8b444d6 | patched |
   | --- | --- | --- |
   | `UpgradeCandidate()` call sites | FAIL — lines 790, 1762 | PASS — line 794 only |
   | both callers use the writer | FAIL — none | PASS — lines 811, 1786 |
   | `m_upgradeSince = m_now` once, pursuit-anchored | FAIL — lines 791, 873, 1762, not anchored | PASS — line 815, anchored |

3. **Patch integrity** — `git apply --check` = 0 against a clean detached worktree of `b8b444d6`; the applied tree
   recompiles the test and passes the contract check (`patched-from-patch`, 0 failures).

Not executed here (Codex's part): engine compile of `BotBrain.cpp`, CI, and any game run. The patch changes only the
lifecycle; it needs the normal CI + independent review before a runtime acceptance, and a shared runtime batch (not a
standalone baseline game) per C152.

## 4. Behaviour deltas to expect at runtime (and to test in the batch)

* A tracked goal is held until pickup / unavailable / timeout — no mid-flight switch to a "better" candidate that
  happened to score lower. Expected: fewer route reversals, and goals that never arrive now give up after 60 s (90 s
  sniper) instead of forever.
* `TryAcquireUpgradeGoal` also throttles failed searches: at most one proximity+path scan per 3 s instead of 10 Hz while
  the bot holds a pistol. If the first pickup after a ban is delayed, the bound is ≤3 s.
* Arbiter mode no longer starts a budget on a candidate look; if a bot was being banned "unreachable" while it was
  actually fighting, those false bans disappear.
* Budget anchor uses `0` as the "not pursued yet" sentinel, so a goal acquired at `m_now == 0` starts its budget on the
  next 10 Hz tick (≤0.1 s later). Cosmetic only.

## 5. Files

* `Code/CryGame/Bots/BotWeaponGoal.h` (new, engine-free rules)
* `Code/CryGame/Bots/BotBrain.{h,cpp}` (single writer, pursuit anchor, expiry call)
* `Tests/BotWeaponGoalTests.cpp` (new) + `.github/workflows/warsbots-ci.yml` test list entry `BotWeaponGoal`
* `DS097_GOAL_WEAPON.patch` (the diff), `ds097_caller_contract.ps1` (the caller check used above)
