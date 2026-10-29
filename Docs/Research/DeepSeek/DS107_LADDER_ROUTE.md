# DS-107 revision 2: bounded native lower-Shore ladder patch (against exact 5ef9ee22)

Base: `warsbots/feature/claude-sprint` `5ef9ee22dd2a84896f8ed33cc6c32e6e1261ccb9`. Patch:
`DS107_LADDER_ROUTE.patch` (own branch only; never applied to `feature/*` by me). No game, build, install, launcher,
feature or QA write. Revision 1 `cdff1772` was BLOCKed by Codex; this revision addresses B1-B5.

## 1. Geometry/data support

DS106/DS106b and Codex's full-map V2 candidate (offline, not accepted):

* `QA/build/c154-bake/Shore-candidate-v2.receipt.json`: `--zmin 99.5 --retain-box 2103 1877 90 2138 1892 99.5
  --water 99.5 --ch 0.05`, navmesh sha `b60178d7090fd8403b4ba0f4407d6a722c0057afd559a0024148ef0645930609`,
  124/124 spawns project, 2 links. The links are exactly the accepted upper Strength row and the UP-only ladder
  `2116.125 1880.625 90.250 -> 2117.500 1880.600 99.768 0 ladder`.
* Real-core same data: mask 14 (drop/jump/strength) fails `other_component` at the ladder; **mask 46 (+ladder)**
  reaches ladder top (7 corners/1 link), upper (13/1), sea (23/2) and dry shore 654 (**40 corners/2 links**).
  31/31 native C146 adjacent points pass with `ch 0.05`. The earlier V1 exact-human-exit endpoint failed Detour's
  0.3 m attach radius and is archived as a data failure; V2's real adjacent floor edge connects.
* The native exit root motion lands near `2116.2,1879.7,99.54`; the link end is `2117.5,1880.6,99.768`. The bot must
  safely settle/walk ~1.5 m after the native exit. No snap.

## 2. What revision 2 changes

| File | Revision-2 change |
| --- | --- |
| `Code/CryGame/Bots/BotLadder.h` | Phases **Approach -> Grab -> Climb -> WaitExit -> Settle -> Done/Failed**. `LadderInput` has `grounded`, `onLadder`, `exiting`, `atTop`, `canGrab`. `SetNative` validates a real UP segment (rise >= 4 m, native bottom/top within 1.5/2.5 m of the requested edge, finite orientation) and returns false otherwise; `RejectNative` produces `WrongLadder`. Climb keeps forward into the ladder and never steers sideways. Native `exiting` starts `WaitExit` (no bot steering); `!onLadder && !exiting` starts `Settle`. Done requires **grounded + physical floor/target vicinity**. `atTop` without native `exiting` for 2 s is `BlockedTop`. |
| `Tests/BotLadderTests.cpp` | Engine-free revision-2 controls: wrong/short native ladder rejected; no grab without mat_ladder; forward climb; native `exiting` waits without success; pending exit not Done; native completion not grounded -> Settle; grounded near floor -> Done; blocked top (both `exiting` and `atTop` paths); wrong floor; premature detach -> Lost; gate/reset cancellation; timeout. |
| `BotConfig.h/.cpp` | `wb_bot_ladder` default `0`; help/exit contract. |
| `BotNavMesh.cpp` | Ladder kind only when `wb_bot_ladder==1`; jump kinds unchanged; union `jump14|ladder32=46`. |
| `BotLocomotion.h/.cpp` | `ExecuteLadder(..., bool nativeAttached)`: the SafeStep exception exists **only while native attached**; approach and post-exit settle use the ordinary safe executor. |
| `BotBrain.h/.cpp` | `ApplyLadderExecutor` returns bool and `ExecuteIntent` returns immediately, so ordinary movement/combat does not continue in the same frame. `UpdateLadder` probes `IsLadderUsable` **only before the grab**, validates and **locks** the native frame; no per-tick mutation mid-climb. It never calls `RequestLeaveLadder(eLAT_ExitTop)`; the stock `ProcessMovementOnLadder` ray/animation initiates it. Attached climb uses `ExecuteLadder(..., true)`; approach/settle use normal `Execute` (canMove/frozen/ragdoll/vehicle gates); `WaitExit` sends no movement. `CancelLadder` is the single fail/OFF/death/stop/reset cleanup path: stop fire/input, native `ReachedEnd` only as cleanup, ban/repath. |
| `PlayerMovement.cpp` | Strict `WarsBotLadderServer` requires `gEnv->bServer && IsWarsBotActor && BotManager.Owns && wb_bot_ladder==1`. The native top/bottom checks and exit-animation branch are widened only for that actor. If the owned bot has no animated character/graph/`OnLadder` output, the branch **returns without ReachedEnd** (fail closed; policy reports BlockedTop), never a premature success. Human/client behavior is unchanged. |

## 3. Native exit contract

1. Approach and grab: probe mat_ladder only before the grab; validate the native ladder frame against the requested
   UP edge; lock it. A wrong ladder/short segment is `WrongLadder` and banned.
2. Climb: keep a forward target into `-ladderOrientation`; native movement owns vertical speed and receives the real
   `desiredVelocity.y`. No sideways steer, no injected z, no SetPos/SetWorldTM.
3. At the native top band the **stock** `ProcessMovementOnLadder` ray check initiates `eLAT_ExitTop`; the adapter does
   not call it. `WaitExit` stops bot steering and waits for the native animation/`OnLadder` completion.
4. If the graph/output is missing, the native branch fails closed (no ReachedEnd); the policy reports `BlockedTop`
   after its real window. If the top ray is blocked, the native code clamps and the policy also reports `BlockedTop`.
5. `Settle` uses normal safe movement to the adjacent floor point; Done requires grounded + physical floor/target
   vicinity. A timer never declares success; timeout is a failure.

## 4. Verification performed here

* Patch applies cleanly to a detached exact-5ef worktree (`git apply --check` = 0).
* `Tests/BotLadderTests.cpp` compiled with MSVC `/std:c++17` against the applied revision and printed
  `BotLadder revision 2 tests PASS`.
* No engine compile, no game, no install, no QA/feature write. The engine adapter is not runtime-proven.

## 5. Limits / true blockers

* `BotBrain`/`PlayerMovement` engine code is not compiled here; Codex must compile and fix integration details.
* The dedicated-server native animation/`OnLadder` completion is still the critical unknown. This patch deliberately
  fails closed (`BlockedTop`) rather than faking `ReachedEnd`; if the graph cannot complete, report the blocker.
* Full-map V2 `--retain-box` + `ch 0.05` is Codex's offline candidate, not accepted/deployed; link endpoints must be
  re-derived after any further bake. Mask 46 (jump+ladder) is the intended gate union; mask 14 alone cannot cross.
* `BotLadder` must be added to the CI unit list by Codex; the patch does not edit `.github/workflows`.
* Net-zero flag rule: `wb_bot_ladder` is new/default OFF; the integration item must retire/promote one old `wb_*`
  flag or name a diagnostic exit date.
* No client/visual/full-Shore/sea/capture claim; no runtime pin until Codex integration, full CI and fresh review.
