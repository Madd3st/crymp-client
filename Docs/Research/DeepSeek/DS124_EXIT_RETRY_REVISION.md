# DS124 — bounded native exit-retry revision (supersedes the blocked DS122 patch)

Status: proposal + engine-free actual-caller controls. No game/server/client/probe/QA/install/live-data action.
Source read-only `warsbots/feature/claude-sprint` `abac72a9a1eb02793a2f4ab2b1a72f7c243dd72e`. Own branch only.
`ds122_exit_signal_hold.patch` is **blocked and superseded**; do not integrate it. Use the `ds124_*` files.

## 1. Why the DS122 per-frame re-signal was wrong (and what changed)

DS122 re-issued `Signal=exit_ladder_top` on every exit-branch frame before checking completion. That can:

- re-signal after a real successful exit (the graph is mutated before the completion test);
- restart/reset a playing or pending off-top clip;
- read `QueryOutput`/`GetCurrentStateName` pointers after `SetInput` may have replaced the state;
- fire without alive/frozen/ragdoll/mode/graph-ownership guards;
- treat `IsSignalledInput` as a documented “pending” flag (it is not).

DS124 fix, all in `ds124_exit_retry.patch`:

1. **Completion first.** The exit branch copies the output byte and state text before any graph write, then:
   - invalid output -> fail closed; `OnLadder==1` -> keep waiting (clip plays);
   - output `0`/empty + `NativeLadderExitPhysicalReady` -> stock `RequestLeaveLadder(eLAT_ReachedEnd)`
     and `return` immediately, no mutation;
   - otherwise -> call the retry hook once and `return`.
2. **At most one recovery signal per exit** via actor latches `m_warsBotLadderExitRetryUsed` /
   `m_warsBotLadderExitSignalPending` (`Player.h:656-657`). The hook `CPlayer::TryWarsBotLadderExitRetry`
   (`Player.cpp`, new) runs only when all hold: same stored graph + override, server/owned/`wb_bot_ladder==1`,
   alive/not frozen/not ragdoll, `isOnLadder && isExitingLadder`, the frame’s AC mode already contains
   `eACF_PerAnimGraph`, latch unused, valid `Signal` input id **and** name.
3. **After the stock signal is consumed/default**: read the current `Signal` input; only empty/`none` is retried,
   a still-pending `exit_ladder_top` (or any other signal) is never overwritten.
4. **Preserve a playing/pending clip**: state text is copied before mutation and `ladderOffTop` blocks the retry.
5. **No graph Update, no identity/IsPlayer/channel change, no position/velocity/collider/completion write.**
6. **Lifecycle**: `LeaveLadder(eLAT_ExitTop)` resets the latch before the stock signal; `LeaveLadder` other reasons
   and `RestoreWarsBotLadderGraph` clear it (Restore is the funnel used by Done `BotBrain.cpp:1618`, cancel/OFF
   `BotBrain.cpp:1557`, Revive `Player.cpp:3923`, and graph replacement `Player.cpp:7460`). Cleanup clears **only**
   our own still-current `exit_ladder_top` on the same owned graph; unrelated inputs and a replaced graph are
   untouched. Latches always reset.
7. **Diagnostics** (folded, not a standalone run): the existing exit line gains `held`, `signal`, `pseudo`, `upd`;
   a one-shot `LADDER EXIT RETRY actor=… state=… previousSignal=… one_shot=1` records the handoff, and cleanup logs
   `LADDER EXIT RETRY cleared`.

## 2. Actual ordering that motivates one bounded retry (code contract, not proven cause)

- `CPlayer::PrePhysicsUpdate` computes the bot exit mode from `m_stats.isOnLadder && m_stats.isExitingLadder`
  (`Player.cpp:1578-1580`) *before* the movement object runs. On the trigger frame `isExitingLadder` is still false:
  the stock top check calls `RequestLeaveLadder(eLAT_ExitTop)` later inside `ProcessMovementOnLadder`
  (`PlayerMovement.cpp:1675`), which runs from the same `PrePhysicsUpdate` (`Player.cpp:1662-1666`).
- Therefore the trigger frame uses `eACF_AlwaysPhysics` for the owned bot (`1581-1585`), and the original signal is
  emitted in `LeaveLadder` -> `UpdateLadderAnimation` *after* that mode decision (`Player.cpp:7333-7337, 7498-7519`).
  Only the **next** frame’s `PrePhysicsUpdate` sets `eACF_PerAnimGraph`.
- This handoff window is a plausible reason the one-shot signal is consumed before the off-top state can own the
  exit, but it is **not proven** to be the physical cause (the human client has a different caller). Hence the
  design is exactly one bounded retry after the handoff, never a per-frame resend, and the trace decides.

## 3. Exit failure budget (failure only)

`LadderParams` gains `exitBudget = 3.0f` used only by `WaitExit` (`BotLadder.h:55-60, 160-167`); `exitWait = 2.0f`
remains for the “top band but exit not started” detection. The observed human same-shaft exit is ~2.0 s (DS119
`186.398 -> 188.399`), and the old 2.0 s window left no headroom for the 10 Hz brain / 30 Hz graph cadence. The
timer is a **failure** bound: success still requires native `!onLadder && !exiting` plus the physical/grounded
Settle (never timer/height success). This is device/scenario scoped to the ladder policy; no other timer changes.

## 4. Native pseudo parity (separate commit, valid consistency gap)

`ds124_ladder_pseudo_parity.patch` (unchanged from DS122): `ExecuteLadder` passes `-1.f` to `Move` arg 10, which
the tail maps to 0.5 for stance speed >= 2 (`BotMovement.cpp:138`); the native non-sprint value is
`CPlayer::CalculatePseudoSpeed(false)==1.0` (`Player.cpp:2503-2515`, used by `PlayerInput.cpp:896-910`). The patch
passes the real native value for the ladder executor only. Verified arg positions: probe 7, pseudo 10
(`BotMovement.h:14`). Still an input-shape consistency fix, **not** the exit root cause.

## 5. Actual-caller oracle (not a substring test)

`ds124_exit_retry_caller_test.py` extracts the real `ProcessMovementOnLadder` exit block, the real
`WarsBotLadderServer` helper, the real `Try/Reset/Clear/RestoreWarsBotLadderGraph` methods, the real
`CalculatePseudoSpeed`, the real `BotLocomotion::ExecuteLadder` and the real `BotLadder.h`, compiles them against a
mock graph/world, and runs two expectations:

- `--rev abac72a9… --expect old` -> `DS124 caller RED PASS` (old semantics: no retry, no latch reset/cleanup,
  2.05 s failure, pseudo 0.5).
- patched tree `--expect new` -> `DS124 caller GREEN PASS` (one retry, completion-first, guards, cleanup,
  3.0 s failure bound, pseudo 1.0).

Cross-controls (the oracle is not self-fulfilling):
`old` run with `--expect new` fails exactly 7 new properties; patched run with `--expect old` fails 6 old
expectations. A compiler failure raises and is never counted as RED. `IsSignalledInput` is instrumented and asserted
never called. Executed locally with the bundled Python
(`C:/Users/Magic8/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`) and the VS2022
BuildTools vcvars; only an engine-free unit compile, no game build/QA writes. Logs are in the scratch out dirs, not
committed. Codex/CI should re-run with its normal compiler.

Covered controls: retry bounded, no retry after completion, playing clip preserved, pending stock signal preserved,
null graph, invalid input id/name, human/client, OFF, non-owned, dead, frozen, ragdoll, pre-handoff mode, replaced
graph, ExitTop reset, Restore cleanup, unrelated signal untouched, OFF/revive cleanup, budget failure vs grounded
Done, and real `CalculatePseudoSpeed`/`ExecuteLadder` arg positions.

## 6. Integration order and residual unknowns

1. Review `ds124_exit_retry.patch` (P1) and `ds124_ladder_pseudo_parity.patch` (P2) separately; do **not** take
   `ds122_exit_signal_hold.patch`.
2. New exact full-green CI pair, fresh independent review, then **one** lower-545 -> ladder -> upper floor -> water
   -> shore route when the owner is free. No runtime now (the owner may have the classic package open).
3. The run must show the `LADDER EXIT RETRY` line and then either `held=1`/`ladderOffTop` with physical egress, or a
   fail-closed `blocked_top`. Retry sent but `held=0` -> selection-rule issue; the next step is a reviewed forced
   native state or the swept-capsule fallback (design in `DS122_UPPER_EXIT_REPAIR.md` §5), not a resend. `upd=0` ->
   character update lifecycle gap; fold a scoped owned-bot flag fix instead.
4. Not claimed: that the retry works at runtime, that the signal is definitely lost in the handoff, that 3.0 s is
   sufficient in all frame rates, or that pseudo parity changes the exit. No default/flag promotion.
