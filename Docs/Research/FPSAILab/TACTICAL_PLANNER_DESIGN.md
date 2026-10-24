# Tactical decision layer: design, prototype results, integration plan

Prototype: `Tools/FPSAILab/fpsai/*.h` (header-only C++17, no engine includes), tests
`Tests/FPSAILab/test_fpsai.cpp` (47 checks), comparison and cost bench `Tools/FPSAILab/fpsai_bench.cpp`.
Measured output: `Docs/Research/FPSAILab/BENCH_RESULTS.md`.

## Decision

Build a **utility goal selector with commitment**, fed by navigation-verified options, executing
short action sequences. Keep the bounded planner behind it as an optional sequencer.
Do **not** introduce full GOAP into BotBrain now.

Reasons, from measurement rather than taste:
- Plans found in all scenarios and 9,000 randomised decisions had 1–2 steps; the planner
  expanded at most 4 nodes (budget 256). The search did no real work.
- Every scenario that improved did so through verification, commitment, the stuck goal or the
  survival utility (FPS_AI_COMPARISON §"simplest thing"). Each can be written without a planner.
- The planner showed the classic GOAP failure: with a loose goal symbol it built "Retreat (breaks
  LOS) → Pursue" to satisfy Patrol. Per-goal action masks fixed it, which also shows that the
  goals, not the search, carry the design.
- Where multi-step composition will matter (PS: buy → equip → reach point → hold), the planner is
  ready and cheap (1.1 µs per decision worst case).

## Layers

```
CryMP world ──► Observation builder (only what the bot perceives + BotNav answers)
                    │  Facts = DeriveFacts(Observation)            (core.h)
                    ▼
               Goal selection: mask(rules) × Scorer(utility)       (brain.h)
               commitment: min time, switch margin, interrupt priority
                    ▼
               Sequencer: bounded planner over Facts, per-goal action mask  (planner.h)
                    ▼
               Step executor: valid / progress / timeout / failure memory
                    ▼
               Command ──► existing BotIntent → BotMovement/BotLocomotion/BotCombat (unchanged)
```

### Observation contract (no hidden information)

`fpsai::Observation` has no field for an unseen enemy's true position. `enemyLastSeen` is written
only while visible; `enemyLastSeenAge` grows otherwise. Every enemy-directed target the brain
emits is `enemyLastSeen`. The scenario runner counts any target equal to the true hidden position
as a leak: 0 leaks across all scenarios for both policies.

Navigation answers are `NavOption {exists, verified, pos, routeCost, exposure, id}`. `verified`
means BotNav returned a route; actions that move there require `verified` in their context check,
so the brain never assumes cover or a path the navigation did not confirm.

### Goals (prototype values)

| Goal | Applicable when | Utility (heuristic-v1) | Interrupt priority | Min commit | Allowed actions |
| --- | --- | --- | --- | --- | --- |
| Unstick | stall ≥ 6 s while requesting movement | 1.0 | 1.0 | 2 s | Unstick |
| Survive | low HP and (enemy known or under fire) | 1.2·hurt − 0.15 + 0.15·threat ± cover | 0.8 | 2.5 s | SeekCover, Retreat, HoldPosition |
| KillEnemy | enemy visible, armed | 0.6 + 0.3·health | 0.6 | 1.5 s | Attack, Reload, ChangeWeapon, Reposition |
| AcquireWeapon | no primary, verified pickup, no visible enemy | 0.55 − 0.25·routeCost | 0.3 | 3 s | GoToWeapon |
| Investigate | enemy known, not visible, verified route, not at last seen | 0.5·(1 − age) + 0.1 | 0.4 | 2 s | Pursue, Reload |
| Patrol | always | 0.1 | 0 | 1 s | Roam, Search, Reload |

Switching rule: a challenger must exceed the current utility by 0.12, and during the current
goal's minimum commitment time only a goal with higher interrupt priority may take over. A
finished or failed plan is rebuilt in the same tick, so there are no idle ticks between steps.

### Actions

Each `ActionDef`: symbolic `pre/preNot/add/clear`, context check `valid(obs)` (only for things
earlier steps cannot change — e.g. a verified route, never ammo), dynamic `cost(obs)`,
`progress(obs, started)` → Running/Succeeded/Failed, `timeout`, `interruptible`.

| Action | Requires | Succeeds when | Fails / replans when |
| --- | --- | --- | --- |
| Attack | enemy visible, loaded | — (runs until goal changes) | enemy not visible, clip empty |
| Reload | armed, not loaded, reserve | clip > 0 and not reloading | timeout 4 s (not interruptible) |
| ChangeWeapon | another loaded weapon | clip > 0 or 1 s | timeout 2 s |
| SeekCover | verified cover with exposure ≤ 0.35 | within 1.5 m | cover no longer verified, timeout 6 s |
| Retreat | enemy known, verified cover | at cover | timeout 4 s |
| Reposition | verified flank point, enemy known | at point or enemy visible | timeout 6 s |
| Pursue | enemy known, verified route to last seen | enemy visible or at last seen | timeout 10 s |
| GoToWeapon | verified pickup | has primary | timeout 12 s → banned 8 s for that item |
| HoldPosition | in cover | 2 s elapsed | — |
| Roam | verified roam point | within 2 m | timeout 30 s → banned for that point |
| Search | enemy not visible | 2.5 s elapsed | — |
| Unstick | stuck | displacement resumes | timeout 4 s; escalation is the adapter's job |

Failure memory (Quake III idea): only actions bound to a navigation target are banned, per bot,
per target id, for 8 s. Running dry during Attack is a normal ending and is not banned (an early
version banned Attack and the bot stopped fighting for 2 s; the test caught it).

## Results (synthetic, decision level)

From `BENCH_RESULTS.md`. The world is a crude straight-line simulation; numbers describe
decisions, not Crysis combat.

| Scenario | Legacy rules | Decision layer | Read |
| --- | --- | --- | --- |
| A visible enemy, full clip | attack, kill | same | equal |
| B low HP under fire, cover 6 m | attacks, withdraws blind; 23 dmg taken, ends at 2 HP, kills | cover at once; 4 dmg, 21 HP, no kill | survives, trades the kill |
| C weapon on unreachable ledge | walks into it 10 s | never commits (0.6 s) | better |
| D enemy hides | pursues last seen | same, 1 fewer switch | equal |
| E threat appears during pickup | reacts at once, does not finish the enemy (78 dealt), dies (101 dmg) | reacts at once, reloads/switches, kills, resumes pickup (66 dmg) | better, mostly from ammo handling |
| F near-equal utilities, noisy exposure | 2 switches | 2 switches | equal |
| G nav says yes, body cannot move | stalls 15 s | Unstick after 6.6 s | better |
| H empty clip in a fight | native auto-reload; 30 dmg | switches to pistol; 54 dmg | **worse** (switch cost too low) |
| I pistol, flickering target, rifle reachable | 4 walk-back flips | 0 | better |

Cost: 0.6–0.8 µs mean per decision in scenarios, 1.1 µs in the randomised stress (replanning every
tick); 15 bots at 10 Hz = 0.16 ms per game second. Legacy model 0.07 µs. Both negligible next to
the measured bot Update cost (0.23 ms per frame for 15 bots, Codex P1).

## Limits

- No movement, aiming or suit physics; scenario H shows the cost model can be wrong in ways only
  the game will reveal.
- The legacy model is a transcription of decisions, not BotBrain itself; it omits BotCombat's
  exact target hold, strafing and suit hysteresis.
- Cover and flank options are given by the scenario; generating them from BotNav is unsolved and
  is the hardest real dependency.
- Unstick only decides; the escalation ladder (repath, side step, jump back, respawn) is unproven.

## Integration plan (each step separately gated, nothing changed yet)

Stage 1 — small fixes inside BotBrain, no new framework (Codex):
1. Displacement watchdog (Steelmill F1/F2).
2. Commitment on the weapon-upgrade branch: once MOVING_TO_WEAPON is chosen, keep it ≥ 3 s unless a
   target is *visible within engage range* or damage is taken; once CHASING, require target loss
   ≥ 1.5 s before switching to the weapon. Removes the G2 flip without new structure.
3. Withdraw only to a verified point (BotNav route to a sampled point ≥ 8 m away with lower
   exposure by ray); if none, keep fighting.

Stage 2 — decision layer (after Stage 1 soaks):
- New `Code/CryGame/Bots/BotDecision.{h,cpp}` = this prototype's brain with WarsBots types;
  `BotBrain` builds the Observation from `BotBlackboard`/`BotCombat`/`BotNav` and executes the
  `Command` through the existing `ExecuteIntent`. Combat aiming/firing stays in BotCombat.
- CVar `wb_decision 0|1`, default 0. Legacy path untouched when 0.
- Log one `DECISION actor= goal= action= u= candidates=[goal:u,...] mask= reason=` line per change.

Stage 3 — cover generation and jump links (needs `wb_nav_export`), then PS tasks as goals.

## Minimal safe A/B experiment (for Stage 1 item 2)

- Variable: only the upgrade-branch commitment, behind `wb_upgrade_commit 0|1` (default 0).
- Same build, same Steelmill memory snapshot restored before each run, profiles fixed.
- Pairs: 6 bots × 15 min and 15 bots × 15 min, A and B alternating, 4 pairs each (16 runs).
- Primary metric (pre-registered): CHASING↔MOVING_TO_WEAPON A→B→A flips per live bot-minute
  (`Tools/SteelmillTriage`). Success: ≥ 50% reduction in every pair.
- Guard metrics, must not get worse by more than noise across pairs: pickups per bot-minute,
  kills and deaths per bot-minute, pinned and trap %, NAV fallbacks per bot-minute, PROFILE worst frame.
- Then one owner session against the bots with both settings, blind order.
- If the primary improves and guards hold, promote to default 1; otherwise revert. No tuning
  between pairs.
