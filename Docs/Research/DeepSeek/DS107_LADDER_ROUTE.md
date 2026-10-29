# DS-107: bounded native lower-Shore ladder patch (against exact 5ef9ee22)

Base: `warsbots/feature/claude-sprint` `5ef9ee22dd2a84896f8ed33cc6c32e6e1261ccb9`. Patch:
`DS107_LADDER_ROUTE.patch` (own branch only; never applied to `feature/*` by me). No game, build, install, launcher,
feature or QA write.

## 1. Why this patch is supported by real geometry

DS106/DS106b established, offline:

* The lower dry deck exists at `90.1-90.4`; the accepted E223 bake clipped it only because C145 used `--zmin 99.5`.
* One real native long-shaft ladder is placed at `(2116.125,1880.125)`, z `90.25..100.29`
  (`sub_ladder_longshaft.cgf`, `ladderproxy`/`mat_ladder`); it was never represented in the nav graph.
* A single **UP-only** `ladder` row from the baked lower floor to the nearest baked deck poly gives a directed
  path `545 -> 99.768` (5 corners, 1 ladder link) and the reverse correctly has no route. The edge is one-way
  production; down is untested and not offered.
* C146 proves the 99 -> 103 corridor physically: grounded `jump=0`, Speed-suit `0` rows from the ladder exit
  `(2116.211,1879.724,99.540)` through `(2119.703,1883.319,99.750)`, `(2115.957,1883.350,102.330)`,
  `(2113.942,1883.214,103.335)` to `(2111.212,1883.218,103.125)`. On E223 the ladder exit -> takeoff is a
  walk-only 8-corner path. The C154 bake also shows `ch 0.05` closes both remaining corridor gaps (31/31 adjacent
  paths) while `cs 0.05` alone does not; the matched no-retain baseline is bit-identical to archived US-low90
  `E1ADC571…`. So the data fix is `--retain-box` + `ch 0.05`, not an extra invented stair link.

## 2. What the patch changes (exact 5ef)

| File | Change |
| --- | --- |
| `Code/CryGame/Bots/BotLadder.h` (new) | Engine-free `BotLadder` policy: Approach -> Grab -> Climb -> Exit -> Done/Failed. Grab requires `canGrab` (native `IsLadderUsable`); climb moves horizontally into `-ladderOrientation`; exit is requested at the native top band and **only** completes on native `!onLadder && !exiting` plus a physical floor/position check. Timeout/blocked/wrong-floor/lost are failures. No `SetPos`/`SetWorldTM`, no height-only detach. |
| `Tests/BotLadderTests.cpp` (new) | Engine-free RED/GREEN policy oracle: no grab without a mat_ladder match, native onLadder transition, climb direction/no injected z, native exit pending is not success, native completion -> Done, blocked top -> FAIL, wrong floor -> FAIL, lost -> FAIL, total timeout -> FAIL, reset -> Idle. |
| `BotConfig.h/.cpp` | `wb_bot_ladder` default `0`; explicit help/exit contract. |
| `BotNavMesh.cpp` | Ladder kind is enabled **only** when `wb_bot_ladder==1` and a links file exists; jump kinds stay exactly as before. Union when both gates are on: `jump14 | ladder32 = 46`. |
| `BotLocomotion.h/.cpp` | `ExecuteLadder`: uses the real `BotMovement::Move`/movement-controller path with `jumpTraverse=true` so `canMove`/`SafeStep` cannot Stop the climb, while native `ProcessMovementOnLadder` still receives `desiredVelocity.y`. No jump, no injected z. |
| `BotBrain.h/.cpp` | Starts at a `NavLinkLadder` corner, owns the whole 10 Hz decision tick while active (before weapon/combat/roam), performs `IsLadderUsable`/`RequestGrabOnLadder(eLAT_Use)`, reads native `ladderBottom/ladderTop/ladderOrientation`, requests `eLAT_ExitTop`, and advances the path only after native completion. Failed links are noted/banned and the path is cleared. Death/stop/ResetLife leave the native ladder safely; no residual fire/motion. |
| `PlayerMovement.cpp` | Widens the native top/bottom `IsClient()` checks and the `gEnv->bClient` exit-animation check **only** for `gEnv->bServer && IsWarsBotActor() && wb_bot_ladder==1`. Humans, clients, other actors and the flag-OFF build keep the old behavior. This is what lets the stock collision/ray/animation `eLAT_ExitTop` run for an owned server bot instead of a manual detach. |

No network/wire/IsPlayer/ShotValidator/default changes. The production ladder row is UP only:
`2116.100 1880.600 90.168 -> 2116.700 1879.800 99.768 0 ladder`; re-derive it after the full-map `--retain-box`
+ `ch 0.05` bake, because the deck poly may move slightly.

## 3. Native exit contract (the part that must not be faked)

1. Approach the base; face the real ladder line.
2. Grab only after `IsLadderUsable()` (mat_ladder ray/geometry). `GrabOnLadder` is native and may align the body;
   bot logic does not call `SetPos`/`SetWorldTM`.
3. Climb with a horizontal target into `-ladderOrientation`; native movement owns vertical speed.
4. At `native ladderTop - 2.5 m` (LADDER_TOP_DISTANCE 2.41 + margin) request `eLAT_ExitTop`; the stock
   collision/ray check and animation/`OnLadder` completion decide whether the exit is allowed. Pending
   `isExitingLadder` is **not** success.
5. Done only when native `!isOnLadder && !isExitingLadder` and the body is on the physical deck (C146 exit
   `99.540` at `2116.211,1879.724`; nav top `99.768`). Otherwise FAIL and re-path.
6. If a dedicated server cannot complete the native animation/`OnLadder` exit, report that as a **blocker**; do not
   replace it with `eLAT_ReachedEnd`, a timer, or a position snap.

## 4. Verification performed here

* Patch applies cleanly to a detached exact-5ef worktree: `git apply --check` = 0.
* `Tests/BotLadderTests.cpp` compiled with MSVC `/std:c++17` against the applied patch and printed
  `BotLadder tests PASS` (engine-free only).
* No engine compile, no game, no install, no QA/feature write. The engine adapter is **not** runtime-proven.

## 5. Limits / true blockers

* `PlayerMovement`/`BotBrain` engine code is not compiled here; Codex must compile and fix any integration detail.
* Dedicated-server native animation/`OnLadder` completion for an owned bot is unproven. If `isExitingLadder` never
  clears, this patch fails closed (no fake success) and the task is blocked on a real native-graph solution.
* The full-map `--retain-box` and `ch 0.05` bake is Codex work and pending review; the link endpoints must be
  re-derived after that bake.
* `BotLadder` must be added to the CI unit list by Codex (the patch does not edit `.github/workflows`).
* Net-zero flag rule: `wb_bot_ladder` is new/default OFF; the batch must retire or promote one old `wb_*` flag, or
  name a diagnostic exit date, in the C154 integration item.
* No client/visual/full-Shore/sea/capture claim; only the lower ladder edge and the existing upper corridor are in
  scope.

## 6. Patch identity

`DS107_LADDER_ROUTE.patch` is the deliverable; the own-branch commit SHA is reported in the outbox. Codex owns
integration, CI, data bake, and the eventual runtime pin.
