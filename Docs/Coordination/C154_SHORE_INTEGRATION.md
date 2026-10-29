# C154 Shore lower-route integration — source and offline checks

Game source: **`3cfd859676d83bdecc1b091d3b9202323b039135`**, `warsbots/feature/claude-sprint`.
CI: [40855444355](https://github.com/Madd3st/crymp-client/actions/runs/40855444355), initially pending.
Independent pre-CI review: DeepSeek **`e8e63bd8`**, DS109 OK for this exact code, caller tests and frozen data.
No physical route acceptance is claimed by this document.

## Changes

- A bounded native UP-ladder executor approaches the real ladder, validates its native material/frame before
  grabbing, locks that frame after attachment, climbs with normal forward input and waits for the stock exit.
- The native collision check initiates top exit. Missing/invalid graph output cannot detach the bot. Graph
  completion also requires a bounded physical egress position; route completion requires native flags cleared,
  grounded feet and arrival on the landing floor. No custom actor position changes or timed fake completion.
- Approach and settle retain ordinary safe movement. Attached native climb is the only ladder movement exception.
- Failure cancels input and bans the failed edge. Stop, reset, death and gate OFF clean up without banning a
  valid route. Pending cleanup is consumed once; Stop clears running before invoking cleanup callbacks.
- During native top exit only, an owned server bot with the ladder gate ON retains animation-graph movement.
  Ordinary motion, native climbing, humans, non-owned actors, replicas and gate OFF retain the existing override.
- The generic jump executor rejects Ladder/Lift instead of treating unsupported kinds as Drop.

`wb_bot_ladder` defaults **0**. Net-zero flag accounting is **56 → 56**: diagnostic `wb_nav_knee_fix` is retired;
its variants remain available through existing `wb_nav_audit [0|1|2|stop]` (default variant 0, no-actor guard).
No gameplay default is promoted. Full-route and owner visual/feel acceptance remain required before considering it.

## Checks and rejected intermediate versions

DS107r2 local baseline `01e15a93` was never pushed or run. Actual SDK Brain syntax failed on the wrong
`isRagdoll` spelling and invalid intent field macro use. Integration corrected those and the caller defects above.
Actual extracted caller controls reproduce **17 semantic failures** on that baseline and pass on the final source.
The reference is compiled with its own policy header; every mock tick constructs fresh input like the real caller.
A compiler failure cannot count as semantic RED. These corrections fix two weaknesses in the original DS108 fixture.

Actual extracted Player movement-mode override: old `0b02dc27` RED, scoped fix `18abab5a`/final source GREEN.
Checks cover attached exit versus ordinary motion, non-attached, gate OFF, non-owner, human, client and null game.
Player.cpp actual SDK syntax passes with experimental ON and OFF. Brain, Locomotion, NavMesh, Manager and
PlayerMovement ON plus PlayerMovement OFF also pass actual SDK syntax. Eight related engine-free C++ units and
the existing real weapon-goal caller regression pass. No local game executable was built or linked.

CI includes the ladder unit and both real caller/movement-mode scripts. Mocked extraction covers real small
methods/blocks, not the whole engine: dedicated-server native animation, collision and root motion still require QA.
Raw evidence: `build/c154-integration/`, `build/c154-run-callers.ps1`, `build/c154-player-check.ps1` in the QA clone.

## Data and shared acceptance

Frozen data V4: [candidate and hashes](C154_SHORE_DATA_CANDIDATE.md). Navmesh `b60178d7…`, manifest `c7e96b3d…`.
The lower dry room is retained as whole source triangles; vertical cell size 0.05 fixes the two observed stair
connectivity gaps without reducing agent dimensions. All 12 lower spawns have correct-floor projections and
paths to shore with the ladder enabled; without it all 12 fail closed. This is graph evidence only.

QA harness `fe42d8ef`: [PowerShell launcher](../../Tools/Invoke-WarsBotsC154Route.ps1) and
[scalar analyzer](../../Tools/Diagnostics/Analyze-C154Route.py). PrepareOnly passes 1460/1506-byte command lines;
13 synthetic positive/negative/duplicate-chunk analyzer controls pass. No game was launched by these checks.

One explicit native registered-spawn revive places US slot #2 at lower spawn **545**, once. Its setup death is
excluded and disclosed; this is a deterministic route test, not natural random-spawn coverage. Thereafter no
death, revive, rescue, replacement or repeated ladder attempt is allowed. The ordered gate requires the same actor
to climb, perform native exit, settle on the correct floor, traverse stairs and the upper Strength link, swim, and
remain grounded on dry shore for seven seconds within 40 m of native target 654. No full capture, match or visual
claim. Archive/demo/source, endpoint NavMeshCore classification/reachability and exact restore checks are additional
acceptance gates. Runtime remains gated on full exact-SHA CI and fresh independent code/data/harness approval.
