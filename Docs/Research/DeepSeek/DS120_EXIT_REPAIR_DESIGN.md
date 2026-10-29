# DS120 — reviewable upper-exit repair design (offline; no runtime)

Owner hold: no game/server/probe/client/live-data/install actions. This is a design for Codex to review and
implement; DeepSeek writes no code and runs nothing.

## 1. First audit: does the server character actually update? (code contract)

- `CS_FLAG_UPDATE_ALWAYS` is set **only** inside `if (!IsClient()) { if (IEntityRenderProxy* pProxy = …) { … } }`
  (`Player.cpp:4449-4469`). A dedicated-server actor without a render proxy never gets the flag; whether a server
  bot has a render proxy is a **runtime unknown**, not a code fact.
- `SetForceSkeletonUpdate(4)` (`Player.cpp:1718-1723`, under `m_warsBotPose`) forces skeleton-pose updates; it is
  **not** the character cull/update flag and does not prove `CS_FLAG_UPDATE_ALWAYS`.
- The graph itself does evaluate for the bot: the owner log shows real state changes
  (`RelaxedIdleNW -> RelaxedWalkNW -> relaxed_walkToIdle_nw_01`), so "the AG never ticks" is disproven. What
  remains unproven is whether a ladder exit state, if entered, would drive **root motion** on the dedicated server.
- First-person contract: the graph only skips `skipFirstPerson` states for a non-first-person actor; there is no
  evidence that `IdleSignalPlayerAnimation` is player-only. Do not flip `IsPlayer`/channel identity.

Verdict: the native graph is **not disproven** as the exit owner; the observed failure is state selection (the
exit state never holds). Levers for selection are unproven and a standalone instrumentation run is not authorised,
so the fallback below is specified as the "if needed" branch.

## 2. Second: collision-safe server movement fallback (only if the graph cannot own the exit)

Scope: `WarsBotLadderServer(player)` && `isOnLadder && isExitingLadder` && alive && !frozen && !ragdoll &&
`wb_bot_ladder==1` && BotManager owns. Humans/clients/OFF/non-owned are untouched.

- **Frame**: the native link frame only — `m_stats.ladderTop/Bottom/orientation/upDir`. No global/magic
  coordinates, no replay of the human trace.
- **Phase 1 (up)**: keep the stock native climb request (`eCMT_Fly`, same axis/speed rules) while the top checks
  say the shaft continues; never exceed the native top.
- **Phase 2 (lateral egress)**: native movement request along `ladderOrientation` (off the shaft), not a
  teleport. Because `GrabOnLadder` sets `eColliderMode_PushesPlayersOnly` (`Player.cpp:7255-7256`) and that mode
  does **not** give capsule/wall collision, phase 2 must have its own collision contract:
  1. explicit swept clearance along the step (e.g. `RayWorldIntersection` at feet/mid/head through the capsule
     radius against `ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid`, the same object mask the stock top ray
     uses at `PlayerMovement.cpp:1657`), and
  2. a ground probe under the landing point before committing; and
  3. a collider-mode lifecycle: request a normal collider mode for the owned bot before phase 2, and let the
     stock leave path restore `eColliderMode_Undefined` (`Player.cpp:7343-7344`) when the actor actually leaves.
  Any hit/negative probe aborts to the existing `blocked_top` fail-closed path; no pass-through, no partial
  teleport, no timer.
- **Completion**: stock `RequestLeaveLadder(eLAT_ReachedEnd)` only when `NativeLadderExitPhysicalReady`
  (`BotLadder.h:18-29`: near top **and** laterally out of the shaft) is true **and** the policy sees the actor
  grounded on the upper floor band derived from the native top/landing probe. No manual ReachedEnd, no height-only
  success.
- **Cleanup/bounds**: reuse the DS115 restore paths (leave/cancel/Done/Revive/OFF); bounded distance/time →
  existing failure + link ban; no retry.
- **Tradeoffs**: adds a scripted motion phase (from the native frame, not the human trajectory); needs careful
  collider restore to avoid leaving the bot with a normal capsule on the ladder or with a ghost collider after
  a failed step; acceptance still depends on a real physical landing.

## 3. Batch item 2: preview must not fail+ban before the viewable attempt

- Cause: the initial bot attempt runs immediately, fails at the top and bans the link; the delayed 545 revive
  then has nothing to climb (`60.07 revive ok`, no second `LADDER start`).
- Design: keep the ladder closed until the reveal. Minimal diagnostic mechanism, local-IA-gated only: generalise
  the existing delayed-command pattern used by `wb_revive_at after <s> <spawn> <bot>`
  (`BotManager.cpp:951-959`, `Script.SetTimer` -> `System.ExecuteCommand`) into `wb_after <1..300> <command…>`, so
  a preview can start with `wb_bot_ladder 0`, let the client join, then enable the ladder and revive at 545 in one
  moment. Alternative with no code change is weaker (far initial spawn) and still racy. Frozen package stays
  untouched; any preview uses a new versioned, reviewed package.

## 4. Controls (actual-caller / counterexample, not self-fulfilling)

- RED (existing): `abac72a9` exit branch leaves the actor waiting with no motion and no state hold (owner log).
- Fallback GREEN (mock world, extracted real code): blocked hatch/wall in the swept clearance → no lateral
  motion, `blocked_top`; missing floor under the landing probe → fail; after leave/cancel/death the collider mode
  and graph state are restored exactly; human/client/OFF/non-owned → zero calls; no `SetPos`/impulse/timer in the
  extracted block.
- Contract vs runtime: the above can be asserted offline; whether the native exit state can own the motion stays a
  runtime question for the single authorised run.

## 5. If no safe candidate is justified

Then the missing observation is: (a) whether the server bot's character has `CS_FLAG_UPDATE_ALWAYS` / a render
proxy, and (b) whether the exit state can be entered when the movement request is neutral at signal time. Both
would be folded into the next coherent batch, not a standalone probe.
