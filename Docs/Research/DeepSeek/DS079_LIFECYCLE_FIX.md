# DS-079: explicit-removal lifecycle fix + US2 native-water diagnosis

Base `f66406d2`; patch `patches/ds079_removal_lifecycle.patch`. Read-only source/log review; no build, game, install or
triage edit.

## Joint diagnosis from the raw v3 run (9 rows, t 0.265..2.293)

1. US2 (actor 65481, group 537, spawn 563) starts on the submarine's **dry interior floor** at
   `(2109.0,1882.5,103.37)`, settles at z 103.125, grounded, speed rising to 6.7 m/s east; `swim=0` in every row.
2. `waterZ=99.5` for the first ~1.3 s, then the native **no-water sentinel `-1e6`**; `bottomDepth=-1e6`,
   `relativeWaterLevel` clamped at 5.00. The engine tests the local water volume, not a global sea plane.
3. So `z > 99.5` does **not** mean swimming: the interior is dry, `ShouldSwim()` is false, and the sea plane outside
   the hull is irrelevant until the bot leaves the enclosed volume.
4. From 1.5 s it descends (`vz -3.15`, z 103.0 -> 100.78) with `grounded`/`flying` flipping; this is walking/falling
   down an interior deck/ramp, not entering water.
5. 2.293 s of data cannot establish a lower-deck exit, vertical actuation or a swim transition; exit/door/capture are
   UNKNOWN.
6. The one remaining observation: continue the same retained native US2 run past the lower deck until either
   `waterZ` returns to 99.5 with finite `bottomDepth` and `swim=1` (exterior water), or the 180 s cap ends with
   `swim=0` and no exit. No hull links, teleport or new swim controller.

## Lifecycle bug (reproduced in the same log)

Explicit `wb_remove 1` logs `#1 removal pending ... until ENTITY_EVENT_DONE` (t 0.06), but at t 0.27 the Manager logs
`#1 removed externally (game reset?); will re-create` and then `Game reset: restored 0/1 bots`. Cause:
`BotManager::OnEvent` checked only `Removing()` (true only while our own `RemoveEntity` call is on the stack) and
ignored `RemovalPending()` (durable intent), so the deferred `ENTITY_EVENT_DONE` looked external, queued an unwanted
restore/`nameoccupied` re-create.

## Fix

* New engine-free `BotRemovalPolicy.h`: `BotDoneDecision(known, levelReady, removing, removalPending, expired)`.
  Restore only for `known && levelReady && !removing && !removalPending && !expired`.
* `BotManager::OnEvent` uses the decision; explicit immediate/deferred removal no longer queues a restore. A true
  external reset (native RestartGame/Reset, no explicit intent) still restores the same bot; level end clears slots
  before DONE; a duplicate DONE after `OnEntityDone` is ignored via `Expired()`; a foreign id never resolves (`Find`).
* No new flag/default/subsystem; identity and `ENTITY_EVENT_DONE` handling are unchanged.

## Regression (`Tests/BotRemovalPolicyTests.cpp`, CI `BotRemovalPolicy`)

Immediate removal, deferred removal (the C145 bug), true external reset, level end, duplicate DONE, deferred+duplicate,
and foreign/unknown id. The deferred-removal case fails on the old `!Removing()`-only logic.

Apply check: clean on `f66406d2` (temp worktree, no build). Unknowns: whether the lower deck leads out of the hull,
and the harness's pending-aware startup count (Codex fix) must land before the next run.
