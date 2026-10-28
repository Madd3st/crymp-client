# DS-099: independent review of C153 code c00cae46 and harness 27e6c746

Exact inputs (read-only, `git show`): code `c00cae46319a2bd8540ca3f85c184787996e778f` on `claude-sprint` (base
`b8b444d6`), harness `27e6c746:Tools/Invoke-WarsBotsC153Batch.ps1` blob `c3784f8a9734617baa6c7b8b11918f970d7b0cf4`,
helper `27e6c746:Tools/Launch-WarsBotsShowcase.ps1` SHA256 `d2a7cb30…` (matches the harness pin and the frozen C150
Run.ps1). No game, build, install, feature or triage write.

**Verdict: code `c00cae46` = OK. Harness `c3784f8a` = BLOCK** — one result criterion cannot pass on a normal Steelmill IA
run (game reset creates a second actor set), so the single shared acceptance would be burned.

## 1. Code review (c00cae46 vs b8)

* **DS097 integration is faithful.** `BotWeaponGoal.h` and `BotWeaponGoalTests.cpp` are byte-identical to the patch in
  `21d730b5`; `BotBrain.cpp` has the same single writer (`TryAcquireUpgradeGoal`), together-reset
  (`SetUpgradeTarget`), and the pursuit-anchored budget with `WeaponGoalExpired`; the only additions are two WB_LOG(2)
  attribution events (`WEAPON GOAL … event=acquire|clear|pursue`). My caller contract check passes on this SHA:
  one `UpgradeCandidate` call (799), both callers through the writer (816, 1794), one `m_upgradeSince = m_now` and it
  is pursuit-anchored (821).
* **FirstPrimaryCombatUrgent** (`BotAuditFixes.h:7-11`) is scoped: it only suppresses the early rifle shortcut when a
  damage timestamp is fresh and the target is either visible or seen <1 s ago; a null `enemy` cannot trigger it
  (`BotBrain.cpp:423-426`), so an unknown shooter still sends the bot to a rifle — consistent with the old behaviour.
  The 5th parameter keeps a default, so no other caller changes.
* **Arbiter first-tick interrupt** (`BotGoalArbiter.h:75-78`): the condition `damagedRecently && (visible ||
  lastSeenAge < 1)` is the same one the committed-tick interrupt already used (`:86-87`), so this removes an
  asymmetry instead of adding a new rule; `Switch` is the same path, so `m_since`/`switched` stay meaningful.
* **Units (independent run, MSVC 14.44, CI recipe `/std:c++17 /EHsc /I . /I Code`):** `BotWeaponGoalTests`,
  `BotGoalArbiterTests`, `BotAuditFixesTests`, `BotUpgradeGateTests` all PASS. The new arbiter case is RED on the old
  first-tick policy (old code returns Weapon for `damagedRecently && lastSeenAge=0.3`) and green now, with the
  `lastSeenAge=2` / `1e9` controls kept.
* **Not re-run here:** engine compile of `BotBrain.cpp`, full CI (40785778523 units PASS, full build pending per the
  item), any runtime. No blocking code defect found; nothing here contradicts the batch plan.

## 2. Harness review (c3784f8a)

Verified good:

* Frozen inputs: package manifest hash pin + per-file sha256, canonical C119 data hashes
  (`cfc11810 / 7c384d12 / 25c3eecf`) cross-checked against the package record, helper hash `d2a7cb30…`, `Quote-Arg`
  rejects quotes/newlines, command size `< 2048`.
* Readiness strings all exist at b8 and in a real IA log (`RULES INFO class=InstantAction`, `#8 created`,
  `NAVMESH loaded Multiplayer/IA/Steelmill: 9333 polys`, `NAVMESH links ON:`, `FRAME … bots=8 server=1`).
  `+wb_debug 1` is required for `defaultVerbose=1` and for the FRAME line (`BotManager.cpp:903`, `:520`) — correct.
* Review pin: `Assert-ReviewFresh` before staging, after preflight, every 15 s and after the run, comparing against
  `quality.review_branch_head`; fail-closed. Correct for a pinned review head.
* Safety: actorless preflight first, owner-game/port/duplicate guards, snapshot before staging, restore in `finally`,
  `Stop-OwnServer` PID+path+start-time ownership, no client, error/`STATE CONFLICT` abort, no automatic repeat.
* Result intent is right (no arbitrary kill/stall target): 8 real primaries, some combat, a native revive, 0 test
  grants, 90–300 s, `client_visual=NOT TESTED`.

**Blocking issue — `:107` `$pickupActors.Count -eq 8`.** On the owner's real 8-bot Steelmill IA run
(`%LOCALAPPDATA%/WarsBots/Logs/S150M-20261028-223049`, build b8) the game reset at **31.62 s**
(`#8 removed externally (game reset?); will re-create` → 8 new EntityIds `131…`) and the whole 303 s log contains
**16 distinct pickup actors** (97 `REAL PICKUP … acquired=1`, classes SCAR/FY71/SMG/Shotgun/Hurricane — all inside the
harness regex). With `-eq 8` the single acceptance fails at 300 s even though every criterion it means to test is met,
and the harness is explicitly not allowed to repeat.

Minimal fix (keeps the intent): build the current roster from the last `#N created EntityId=(\d+)` per slot and require
each of those 8 ids to have a real primary pickup (stricter and reset-safe), or at least `-ge 8` plus one pickup after
the last `Game reset: restored` line. Then push a new harness SHA; re-review is a few minutes.

Minor, non-blocking:

* the pre-stage `Assert-ReviewFresh` runs before `$out` exists, so that first check is not recorded in
  `review-checks.jsonl` (create `$out` earlier or buffer it);
* `ready_elapsed_seconds` is elapsed run time, not readiness time;
* the post-pass `Assert-ReviewFresh` can throw after `$passed=true`, making a passed acceptance look failed.

## 3. Gate for the acceptance

1. Code side is review-clean at `c00cae46`; the run stays blocked until CI40785778523 full build is green and Codex
   ships the fixed harness (`-ge`/roster criterion) with a new SHA and regenerates `c153-quality.json` with the
   post-review DeepSeek head.
2. After this review I stay idle (no pushes) so the review-head pin stays fresh through the single run.

## 4. Limits

No runtime, no demo, no client; the actor-count evidence comes from the owner's b8 Steelmill showcase log, not from the
new SHA. Everything above is static/code + archived-log evidence.
