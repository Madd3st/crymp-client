# DS122 successor brief — upper ladder exit repair (handoff)

Session handoff: the previous DeepSeek session reached ~460k tokens. Nothing here is an implemented fix; the
current game source is unchanged (`abac72a9`). Read `Docs/Research/DeepSeek/DS118_UPPER_EXIT_DIAGNOSIS.md`,
`DS119_HUMAN_VS_BOT_EXIT.md`, `DS120_EXIT_REPAIR_DESIGN.md` before acting.

## State of the work

- Code/runtime pinned: `abac72a9a1eb02793a2f4ab2b1a72f7c243dd72e`, full CI `40890940918`, pair
  `0f8f250f…`/`b7c18962…`, V5 data `b601/0230/d95e/a95a`, harness `e973b905` (`87c3f5ee…`), analyzer `2a642aeb…`.
- Four controlled C154 attempts failed at the top (latest freeze `97.920`, `blocked_top` ~14.4 s); the owner
  preview failed at `97.932`. Climb itself is fixed (270 native samples, `90.33 -> 97.86`).
- DS121 delivered: C155 classic package OK (`5c12d27b`) at
  `E:\WarsBots\Showcases\Steelmill-Classic-abac72a9` — unrelated to the exit; do not reuse it for Shore claims.
- DS119 (`735f052d`): a human client at the same shaft exits to `99.750` (`QA 3e0d9713`, 46 samples); client
  root motion vs server-side bot is the concrete caller difference; the physical root cause is **unproven**.
- DS118 v1 (`129e11ed`) is **withdrawn**; corrected v2 (`150feb4b`) accepted by the owner: `MOTION INTENT` is
  telemetry, `ExecuteLadder` uses stance max speed + pseudo 1.5, `Stop` overwrites aim, the exit signal precedes
  the next ~10 Hz brain tick (about three graph frames at 30 Hz), native input is refreshed every frame and is
  not "always zero". `Stance` is a soft preference (`forceGuard=0`), not a hard filter.
- DS120 (`3f57c40a`): `CS_FLAG_UPDATE_ALWAYS` is set only when a render proxy exists (`Player.cpp:4449-4469`);
  `SetForceSkeletonUpdate(4)` is skeleton-pose only. The bot's AG *does* evaluate states (IdleNW -> WalkNW ->
  walkToIdle / RunNW), so "graph never ticks" is disproven; whether an entered exit state would drive root
  motion on the server is unproven. The native graph path is **not** disproven.

## DS122 requirements (from the task, binding)

- Coherent, reviewable patch with real source-caller controls; first inspect native graph state selection and the
  character update lifecycle. Keep `IsPlayer`/channel identity and defaults; graph asset hash `80232bb2…`. Do not
  claim a player-only template from its name, and do not treat request telemetry as actual input.
- If proposing server exit movement: `GrabOnLadder` sets `eColliderMode_PushesPlayersOnly`, so a physics request
  alone is **not** wall/capsule safe, and feet/mid/head **point rays are not a capsule sweep** — require an actual
  swept shape/clearance (and collider lifecycle with restore). Native top checks, landing/floor proof, bounds,
  cancellation/restore, and human/OFF/non-owned/dead/frozen controls are mandatory.
- Never: `SetPos`, impulse, timer success, height-only detach, manual `ReachedEnd`; never replay the human global
  coordinates — use the native link frame and landing geometry.
- Own branch only; no game/runtime/install/QA writes. Check game-free before any eventual runtime (the owner may
  launch the classic showcase). Codex integrates, then a new exact full-green CI, fresh independent review, and
  ONE whole lower-545 -> shore route only after the owner explicitly finishes. No physical guard weakening, no
  extra approval invented for routine design choices.
- Future preview must not attempt, fail and ban the link before the owner can watch; the frozen package stays
  untouched (any corrected package is versioned/reviewed). No whole-PS/client/default promotion.

## Suggested offline sequence

1. Finish the code-contract audit of the server character update (render proxy presence policy, what
   `CS_FLAG_UPDATE_ALWAYS` changes, whether the AG tick implies AC/root-motion update).
2. Decide native-graph vs fallback on that evidence only; write the decision with the unknowns listed.
3. If fallback: specify the swept clearance (shape sweep or step-wise capsule queries, not point rays), collider
   mode request/restore, native top checks, landing probe, bounds and cleanup.
4. Implement the candidate with an engine-free actual-caller test: RED on `abac72a9` for the observed failure,
   GREEN for the candidate, plus counterexamples (blocked hatch/wall, missing floor, human/OFF/dead/frozen).
5. Hand to Codex for integration; then CI -> fresh review -> single route.
