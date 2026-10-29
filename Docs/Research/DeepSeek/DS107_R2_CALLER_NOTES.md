# DS-107 revision 2: caller defects found by Codex (read-only notes)

Status: **revision 2 patch is frozen**. Per owner/Codex instruction I am not producing further patch mutations
until the exact localSHA review request. This note records the concrete caller defects and the intended integration
fixes so they are not lost; it does not change `DS107_LADDER_ROUTE.patch`.

## 1. Grab-lock ordering (B2 regression)

In the r2 adapter the native frame is validated and `m_ladderNativeLocked` is set during `Approach` as soon as
`IsLadderUsable()` is true (e.g. at 1.7 m). `in.canGrab` is a local default-false field; on the next tick the probe
is skipped because the frame is locked, so `Grab` sees `canGrab=false` and never calls `RequestGrabOnLadder`.
Result: the bot reaches the base and times out instead of grabbing.

Intended fix: keep a persistent `m_ladderReady` bit set when `SetNative` succeeds, and consume it in `Grab`
(`in.canGrab = m_ladderReady`) instead of requiring a fresh ray every tick; or probe only in `Grab`. The frame must
remain locked once validated. Add an actual caller regression for this exact sequence (probe at >approachRadius,
then walk into Grab).

## 2. `CancelLadder` early return leaves `m_ladderCancelPending`

`ResetLife` sets `m_ladderCancelPending` when a climb was active. The next `CancelLadder` can hit the early return
(policy already reset, native stats cleared by native revive) without clearing the pending flag. The next Update
repeats the cancel and clears the path every frame.

Intended fix: clear `m_ladderCancelPending` at the start (or in the early return) of `CancelLadder`; make the
reset cleanup strictly one-shot. Add a caller regression: ResetLife -> native revive clears stats -> exactly one
cancel/path clear, no repeat.

## 3. Settle/Done is too loose

r2 `Settle` accepts `feet.z >= b.z - floorDz` and `Dist2D <= 2.5 m`, and does not explicitly re-check
`!onLadder && !exiting` at the Done point. The native exit lands ~1.5 m from the link end, so 2.5 m can declare
Done before the bot actually walks/settles; a one-sided z check can also accept a higher/lower floor.

Intended fix: Done requires **native `!onLadder && !exiting`**, `grounded`, `fabs(feet.z - b.z) <= floorDz` (both
sides), and a real arrival radius (~0.8-1.0 m), not 2.5 m. Keep the normal safe settle movement until that arrival.
No height-only or radius-only success.

## 4. Native stats field names

`SPlayerStats` has `isRagDoll` (capital D), not `isRagdoll`; `isFrozen` is a `CCoherentValue<bool>` (`.Value()` is
valid, and `actor.IsFrozen()` is available). The r2 adapter must be compiled; field typos are caller bugs, not policy
bugs. Verify `GetLinkedVehicle()` and the frozen/ragdoll/vehicle gate ordering in the real adapter.

## 5. Native exit graph completion

The r2 guard rejects a null animated-character/graph/output but accepts an **empty or invalid** output string; a
valid `"0"` alone can still trigger `ReachedEnd` before the physical root motion/egress is complete. Intended fix:
owned-only completion helper that validates a real `"0"`/`"1"` output **and** bounded physical egress/progress
against the locked native frame (root-motion displacement toward the exit/landing, grounded/physical floor), while
preserving the exact human/client path. No direct `eLAT_ExitTop` from bot logic; timeout remains a failure.

## 6. Data metadata correction

The actual water gate is **`wb_ps_capture`**; `wb_ps_swim` does not exist. V4 metadata should use
`wb_ps_capture`. Binary V3 nav `b601…` is unchanged; the new manifest `c7e96b3d…` is a staged QA-doc update, not an
install/game. Do not cite `wb_ps_swim` anywhere.

## 7. Requested next review

Codex will add real caller regression extracts for the grab-lock + Cancel sequences and the gate truth table (not
only policy tests), then provide the exact integrated localSHA. I will independently review that exact SHA plus the
frozen data/current harness when named. Until then: read-only, no patch mutations, no game.
