# Review f66406d2 (diagnostic MOVE TRACE eligibility) — OK

Exact review of `f66406d29f91e1950710907b02ad67aa9ec3825a` (parent `90cb4148`, four files); no build/game.
This is the DS-076 patch (`74ca4fe9`) unchanged as far as the diff shows.

## Adapter wiring (not just the predicate)

* `BotMoveTrace.cpp:15-24` maps the live rules through the existing `BotMatch` policy:
  `IsPowerStruggle -> MoveTraceMode::PowerStruggle`, else `IsBotMode -> InstantAction`, else `Unsupported`.
  `BotMatch::IsBotMode` is documented as IA or PS and `IsPowerStruggle` is checked first, so PS cannot be mislabelled.
  `BotMatch` already null-guards `rules.GetEntity()`/class, so the old direct `rules->GetEntity()->GetClass()->GetName()`
  dereference is gone without a new assumption.
* `MoveTraceEligible` keeps the old contract: `wb_move_trace` must be 1 or 2, mode must be IA/PS, and the guard is
  `server ? CanRunLocalIA() : gEnv->bMultiplayer`. `CanRunLocalIA()` calls
  `CryMPEngineAdapter::LocalIAGate(..., diagnostic=false, allowPowerStruggle=true)` (`BotManager.cpp`), and
  `LocalIAGate` accepts `sv_gamerules == "PowerStruggle"` when `allowPowerStruggle` is true. So the server PS path
  the C145 observation needs is actually reachable; the old hardcoded `"InstantAction"` class test was the blocker.
* Client replica trace (flag 2) still requires multiplayer; server trace still requires the local gate; no flag
  default, rate, net, physics or Lua change. CI registers `BotMoveTraceEligibility`; the test covers the positive PS
  case, IA, client, disabled flag, unsupported mode and both guard directions.

## Notes / conditions

1. The unit test is predicate-only; the engine mapping and `CanRunLocalIA` are not unit-tested. The first PS run must
   show a real `MOVE TRACE ... swim=... waterZ=...` line; that line is the wiring proof, not the unit test.
2. Server PS still needs the existing local gate (`sv_bind=127.0.0.1`, `sv_lanonly=1`, gate open, mode
   `PowerStruggle`); a public or non-local PS server keeps the trace off by design.
3. The trace is diagnostic only; it must not be used as a substitute for the native water/door/capture observations.
   The old `00e58632` binary must not run; wait for exact CI green.
4. No new `wb_*` flag, default, rate, net or Lua change; `BotMoveTrace` output remains behind `wb_move_trace`.

Verdict: `review f66406d29f91e1950710907b02ad67aa9ec3825a: OK`. The predicate and the adapter mapping preserve the
old guards and open only the intended PS diagnostic path. No BLOCK.
