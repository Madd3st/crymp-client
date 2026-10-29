# DS125 — pre-CI review of local game candidate `307037bc` (bounded native exit retry)

Verdict: **PRE-CI OK** for exact `307037bc08050d39f26310dc6cc7071dddddf4a6` (parent `abac72a9`), with no blocking
source/test/CI finding. This is a pre-CI code review only: not pushed, no CI/runtime, no game/QA/install writes by
me. Physical exit is **not** claimed fixed; only the authorised common run can decide.

## 1. Tree and scope

- `QA/build/ds076-code`, HEAD `307037bc…`, `git status` clean; cumulative diff `abac72a9..307037bc` = exactly
  **10 files, +650/−15**, matching the DS125 description.
- Files: `.github/workflows/warsbots-ci.yml` (+3), `Player.cpp` (+76), `Player.h` (+7), `PlayerMovement.cpp`
  (+64/−?), `BotLadder.h` (+8), `BotLocomotion.cpp` (+5), `Tests/BotLadderTests.cpp` (+2),
  `Tests/test_bot_ladder_caller.py` (+15), `Tests/test_bot_ladder_exit_retry_caller.py` (+484 new),
  `Tests/test_bot_ladder_graph_caller.py` (+1).
- No new cvar/default/wire field, no position/velocity/collider/physics-guard change, no identity/`IsPlayer`/
  channel change, no graph `Update`, no CMake change. The only gameplay constants touched are the ladder policy
  struct defaults (`exitBudget`) and the ladder executor pseudo argument.

## 2. Hardening checklist (verified against the committed tree)

| Requirement | Evidence |
| --- | --- |
| Feature-OFF native leave calls guarded | `Player.cpp:7335-7337, 7343-7345`; method definitions inside `#if` `7454-7572`; `PlayerMovement` call inside `#if` `1640-1665`. Any OFF reference would be a link error; the OFF syntax check passed. |
| Real SDK `GetFlags` non-const | `ICryAnimation.h:430 virtual int GetFlags()=0`; `PlayerMovement.cpp` uses non-const `ICharacterInstance*` (`1609`) and the fixture mock matches (`Tests/test_bot_ladder_exit_retry_caller.py:109-110`). |
| Consume retry before `SetInput` even if rejected | `Player.cpp:7487` sets `m_warsBotLadderExitRetryUsed=true` **before** `7488`; pending set only on success (`7489`). Rejected control asserts one attempt and no pending ownership (`test…:307-312`). |
| `IsDefaultInputValue` API, not guessed `none` | `Player.cpp:7481`; `ClearInput` default at `7519` (not a hard-coded string). Test forces `signal="none"` + `signalDefault=false` and expects no send (`test…:313-315`). |
| Clear orphan latches before Restore early return | `Player.cpp:7557` before `7558`; dedicated control `test…:316-319`. |
| Output NUL `'0'` vs null `-1` preserved | `PlayerMovement.cpp:1598-1600` (`warsBotHasOutput`, `warsBotOutput`); log prints byte or `-1`; empty output still follows the stock physical guard (`1645-1662`). |
| PseudoSpeed input id/name validated before read | `PlayerMovement.cpp:1612-1617`; only then `GetInputAsFloat`. |
| One bounded recovery after PerAnimGraph handoff, default signal, not playing | `Player.cpp:7473` mode, `7481` default, `7486` `ladderOffTop` block, `7474` latch; caller only after output != `'1'`; `7490` one-shot log. |
| Completion actual output + PhysicalReady FIRST | `PlayerMovement.cpp:1645-1658`: invalid → fail closed; `0`/empty + physical ready → stock `ReachedEnd` + immediate `return`; only otherwise `Try` + `return`. |
| WaitExit failure budget 3 s; top wait 2 s unchanged | `BotLadder.h:58-61` (`exitWait=2.0`, `exitBudget=3.0`); `167-170` WaitExit uses `exitBudget`; Climb-at-top still uses `exitWait`. |
| Cleanup only accepted recovery signal; original stock pending not claimed | `m_warsBotLadderExitSignalPending` set only on accepted `SetInput`; `Clear` rechecks same graph/override/ownership and the exact current value (`7506-7520`). A pending original is left alone. |
| Pseudo parity scope | Only `BotLocomotion::ExecuteLadder` passes `actor.CalculatePseudoSpeed(false)` (`BotLocomotion.cpp:162-163`); ordinary `Move`/replica keep the legacy 0.3/0.5 path. |

## 3. Lifecycle / mutation order review

- Trigger frame: `PrePhysicsUpdate` computes `warsBotNativeLadderExit` with `isExitingLadder==false`
  (`Player.cpp:1578-1585`), then `ProcessMovementOnLadder` issues the stock `eLAT_ExitTop`
  (`PlayerMovement.cpp:1675`). The stock signal is emitted in `LeaveLadder` after the mode decision; the next
  frame is `PerAnimGraph`. The retry therefore cannot run before the handoff (`7473`), and it is a single attempt,
  not a per-frame write.
- The exit branch copies output/state before any mutation (`1598-1610`); the completion path mutates nothing and
  returns; the retry path consumes the latch before `SetInput`, and returns without re-reading the old pointers.
  `GetInput` before mutation is used only for the log/cleanup comparison.
- Lifecycle: reset on `ExitTop`; `Clear` on every other `LeaveLadder` reason; `Restore` (funnel for Done/cancel/
  OFF/Revive/graph replacement) calls `Clear` before its early return. Only two writers of
  `m_warsBotLadderGraphOverride` exist (`7543 true`, `7559 false`), so the only orphan-latch case is the
  cleared-before-early-return one, and it is covered.
- Death/OFF/non-owner: guards in `Try`; `Clear` does not require health/gate, so cleanup still runs during death
  or gate-off while ownership exists. Replaced graph: pointer equality and override checks prevent touching it.

## 4. Test/CI review

- New `Tests/test_bot_ladder_exit_retry_caller.py` extracts the real exit block, `WarsBotLadderServer`, Retry/
  Reset/Clear/Restore, native `ExitTop` body, `CalculatePseudoSpeed`, `ExecuteLadder`, the real `BotLadder.h`, and
  the **actual** `request.SetPseudoSpeed(...)` statement from `BotMovement::Move` (`re.search` at `:431-433`).
  No copied ternary: `effectivePseudo()` executes the extracted statement (`:202-206`).
- Controls cover: bounded one-shot (direct + three repeated exit-block frames), no retry after completion, off-top
  clip preserved, pending stock signal preserved, null/invalid name/replaced graph/OFF/non-owner/dead/frozen/
  ragdoll/pre-handoff mode, rejected `SetInput` attempt not repeated, non-default API even with string `none`,
  orphan latches, ExitTop reset, restore cleanup, unrelated signal untouched, replaced graph untouched, OFF/revive
  cleanup, 3 s failure vs grounded Done, pseudo parity, and `IsSignalledInput` never called.
- Existing `test_bot_ladder_caller.py` / `test_bot_ladder_graph_caller.py` changes are explicitly isolated stubs
  (`Try` returns false; `Reset`/`Clear` no-ops) so the route/graph tests keep their old scope; the real lifecycle
  has the dedicated fixture. No existing assertion was weakened; `BotLadderTests.cpp` only advances the old
  10.1 s expectation to the 3 s budget and re-checks 11.1 s `BlockedTop`.
- CI: one step added after the graph caller, same `python`/`cl` job, failure propagation and artifact upload
  unchanged. Compiler failure is raised as a fixture error, not semantic RED (`test…:474-476`).
- QA evidence read (Codex artifacts, not produced by me): `build/c154-ds124-check/old-red-final/run.log` = 11
  semantic failures for old source with new semantics; `new-green-final/run.log` = GREEN PASS; BotLadder/BotConfig
  units and climb/graph/ladder/mode callers PASS; `build/c154-integration/ds124-player` Player ON/OFF and
  `ds124-adapters-fixed` Brain/Loco/NavMesh/Manager/Movement ON + Movement OFF syntax checks have empty error
  logs. No local game link/build is claimed; the new FULL CI is the link/compile gate.

## 5. Non-blocking observations for the trace / next revision

1. The caller retries only when `OnLadder` output is not `'1'`. If the graph ever stays in the climb state
   (output `1`, not `ladderOffTop`) after the exit trigger, the one recovery is not attempted and the policy waits
   out the 3 s budget. This is fail-closed and matches the observed `1 -> 0` sequence, but the `LADDER EXIT` log
   will show it if it happens; a future revision could move the `ladderOffTop` decision to the caller.
2. `IsDefaultInputValue` is the wrapper AND-all-layers value; a residual signal in one layer suppresses the retry
   (conservative). The log prints the wrapper `signal=`, so the trace can distinguish.
3. `GetInput` on the wrapper is documented as "assert all equal" across layers; it is used only for the log and
   the cleanup value check. Release builds are unaffected; if a debug build ever asserts on split layers, guard
   the log read.
4. The QA `BotConfig`/adapter logs contain only source filenames (syntax-only, exit code), not a printed PASS
   line. Acceptable pre-CI; the full CI pair is the authoritative compile/link check.
5. `exitBudget=3.0` is a policy struct default used by `BotLadder`, not a cvar; it affects only the owned native
   ladder route. Intended, but the run must show the exit inside the budget and the unchanged physical/grounded
   success.

## 6. Gate statement

`307037bc` may proceed to push + **new exact FULL CI pair**, then the separate fresh all code/data/harness/quality
gate; only after that, ONE whole lower-545 → ladder → upper floor → water → shore when the owner is free and the
game is free. No runtime/default/PS/client promotion; no claim that the physical exit is fixed. The review is
published on `warsbots/research/deepseek` only, not on the feature branch.
