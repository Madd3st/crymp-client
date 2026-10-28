# DS-076: C145 PS MOVE TRACE eligibility (narrow diagnostic patch)

Base `90cb4148de477f4a8cadde5d31d7150c67a6e989`; patch `patches/ds076_move_trace_eligibility.patch`. No build, no
game, no install. Triage mailbox not touched in this turn (role switched back to prompt C; Codex owns it).

## Blocker (X-218 / C145_PS_TRACE_PREFLIGHT.json)

`BotMoveTrace.cpp:16-19` enabled the trace only when the rules class name was exactly `"InstantAction"`:

```cpp
!std::strcmp(rules->GetEntity()->GetClass()->GetName(),"InstantAction") &&
(gEnv->bServer ? manager.CanRunLocalIA() : gEnv->bMultiplayer)
```

`BotManager::CanRunLocalIA()` already passes `allowPowerStruggle=true` to `LocalIAGate`, so the extra hardcoded class
comparison is the only reason a PS server never reaches `Visit` and the native water fields (`swim/waterZ/
relativeWater/bottomDepth/underwater`) cannot appear. A PS run on `90cb` therefore cannot answer the water question.

## Fix

* New engine-free `BotMoveTraceEligibility.h`: `MoveTraceMode { Unsupported, InstantAction, PowerStruggle }` and
  `MoveTraceEligible(flag, server, localAllowed, multiplayer, mode)`. It keeps the opt-in `wb_move_trace` 1/2 check
  and the existing `server ? CanRunLocalIA() : bMultiplayer` guard; only the mode test changes.
* `BotMoveTrace.cpp` maps the live rules through the existing policy:
  `BotMatch::IsPowerStruggle(*rules)` -> PowerStruggle, else `BotMatch::IsBotMode(*rules)` -> InstantAction, else
  Unsupported. No new `wb_*` flag, no default change, no new subsystem.
* The server/client iteration, local identity, rate (`moveTraceHz`/4 Hz), 200k line cap and record cap are untouched.

## Regression (`Tests/BotMoveTraceEligibilityTests.cpp`, CI `BotMoveTraceEligibility`)

| case | expected |
| --- | --- |
| PS, flag 1/2, local server allowed | true (the C145 blocker) |
| IA, local server allowed / client multiplayer | true (unchanged) |
| PS client with multiplayer | true (replica capture, flag 2) |
| unsupported rules class, both flags | false |
| flag 0 or 3 | false |
| server without the local gate, both modes | false |
| client without multiplayer, both modes | false |

The test fails on the old behaviour (PS false) and on any accidental widening (unsupported/disabled must stay off).

Apply check: patch applies clean on `90cb4148` (temp worktree, no build). CI list gains one engine-free test.

## Required next (Codex/coordinator)

1. Apply to `claude-sprint`, push a new exact SHA (do not reuse `90cb`).
2. Green CI on that SHA; then request DS-077 independent exact review (supersedes DS-074 for runtime).
3. Only after SHA+CI+OK: the predeclared one-run PS native-water observation on the accepted/installed candidate,
   with hash/restore and the missing-evidence=UNKNOWN rule. No blind PS run on `90cb`.
