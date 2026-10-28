# Review 90cb4148 (cumulative Shore packet, base 2e0a6641) — OK

Exact-SHA review of `90cb4148de477f4a8cadde5d31d7150c67a6e989`: the full packet of `00e58632` plus the two-file
compile/count correction. `00e58632` was blocked only at `BotManager.cpp:859`; that caller is fixed. No build/game.

## The two-file fix vs 00e58632

* `BotManager.cpp:859` now calls `PSCaptureSnapshot(points, *rules, nullptr)`; the summary no longer needs an actor
  route filter. Compiles against the 3-arg declaration (`BotPSCaptureRuntime.h:10`), and the comment states the
  ownership-count contract.
* `BotPSCapture.cpp:150-205`: a capturable with no valid destination is no longer dropped; it is pushed with
  `hasDestination=false`, `areaId=0` and destination `(0,0,0)`. The planner rejects `!hasDestination`
  (`BotPSCapture.h:Update`), so it can never become a pivot goal; the console summary still counts its native
  ownership. `PSCaptureSnapshot:215` now applies the per-actor reachability filter only when `from != nullptr`, so the
  summary is independent of any one bot's route.
* The `BasePoints` cache condition is level name + ready-mesh pointer (`cachedMesh`), so a mesh that appears after the
  first call (null -> ready) or is replaced by another ready object invalidates the list. A same-pointer in-place
  re-bake is not detected, but the loader only reloads on a level change (safe in the current flow).

## Whole packet audit (unchanged from DS-073)

Accepted: one `BotWaterFacts` gate (experimental+MP+PS+owned+botActor+psCapture+navmesh flag+ready mesh+nanosuit) for
`canMove`, SafeStep ground-ray, trap and native oxygen; the singleplayer 30 HP/s branch untouched; no HP/energy grant;
swim stance/no sprint/strafe/no jump; body/cliff rays kept. Door-only scan before capture, vehicle exit before capture
and enter after; 0.25 s scan bound, 2 s per-door. Capture destination from the local `captureAreaId` + explicit
`<Entities>` target binding, live classes/pos/proxy ID, world TM, `IsPointWithin`, navmesh projection, 250k cap,
fail-closed; native membership hold; planner tests actual inside state and `hasDestination=false`. Nav 16384 and
`DT_OUT_OF_NODES -> search_exhausted`; MOVE TRACE water fields. No default/net-layout/SP-damage/normal-player/Lua
changes; new tests registered in CI.

## Conditions before the named runtime observation (not blocks)

1. 344 binds shapes 330+323 but only the most interior destination is stored; if that shape is unreachable the whole
   point is filtered even if the other shape is reachable. Log the unreachable filter and prefer per-shape
   destinations if the run shows 344 skipped.
2. `PSCaptureSnapshot` still clears a point silently when `Reachable(from,goal)` fails; add
   `PS CAPTURE skip id=... unreachable` so "no target" is distinguishable from "no binding" in the run.
3. `LevelAreaShapes` is still cached by level name only; an in-process gamerule/mission switch on the same level can
   reuse stale bindings.
4. The runtime still accepts a 256-poly partial path (rolling prefix); the tool reports `partial=true`. Do not use the
   tool's `path` JSON as route evidence until the runtime logs/rejects the same PARTIAL/BUFFER/OUT_OF_NODES detail.
5. Live `proxy->GetID()==localAreaId` and XML target id == runtime EntityId are assumed; the first `dest`/`skip` log
   lines are the required verification. Old `00e58632` binary must not run; wait for CI 40606278441 green.

Verdict: `review 90cb4148de477f4a8cadde5d31d7150c67a6e989: OK` for the compile/count correction and the cumulative
packet. The conditions above remain for the first named runtime observation; no BLOCK.
