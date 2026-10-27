# DS-070 addendum: swim stance + engine-free step oracle

The pushed `ds070_water_policy.patch` already carries the single gate, native oxygen, CI registration and the
jump-link stop. Two narrow gaps remain against DS-070's wording ("swim stance/movement use native handling; no
jump/sprint while swimming"; "engine-free negative/positive tests ... wall, drycliff, deepwater"), plus one review
note not in DS-071.

Base: `2e0a6641` -> `ds070_water_policy.patch` -> `ds070b_stance.patch` -> `ds070c_step_oracle.patch`.
All three apply clean in that order (`git apply --check`, no build).

## ds070b_stance.patch (BotLocomotion.cpp)

The pushed patch forces `strafe=false` and refuses jump, but still passes `intent.stance` to `GetStanceMaxSpeed`,
`Move`, `SetWarsBotActions` and `CMovementRequest::SetStance`. In gated water that is a land stance (run/crouch) and
`sprint` is only suppressed indirectly by `grounded=false`. 070b uses
`stance = waterMovement ? STANCE_SWIM : intent.stance` for all four, and `sprint = !waterMovement && eligible && ...`.
No other behaviour changes; dry movement is byte-identical.

## ds070c_step_oracle.patch (BotWaterPolicy.h / BotMovement.cpp / tests)

`SafeStep` currently returns early on a body ray and `if (waterMovement) return true;` for the ground ray. 070c moves
the decision into the pure `WaterStepAllowed(waterMovement, bodyBlocked, groundHit)` next to `BotWaterAllowed`:
body/cliff rays always win, gated water skips only the downward ray, dry ground keeps the old ground requirement.
`SafeStep` computes `bodyBlocked` (nine rays) and `groundHit` (only when not blocked) and returns the helper. Tests in
the already-registered `BotWaterPolicyTests.cpp` add the six-row table: dry void false, dry cliff/body false, dry land
true, water wall false, deep water without ground true, deep water with ground true. This is the engine-free
negative/positive oracle DS-070 asked for; it exercises the same inline function the movement uses, not a copy.

## Review note not in DS-071

`BotNavMeshCore.cpp:325-334` (game code, unchanged) still accepts a buffer-full partial path as success: it fails only
when `polys[n-1] != b && npolys < 256`, then snaps to the last poly and returns true. The revised tool
(`eed85fb1:857`) now reports `partial=true`. Before the tool's `path` JSON is used as Shore route/capture evidence,
the runtime must reject the same `DT_PARTIAL_RESULT | DT_BUFFER_TOO_SMALL | DT_OUT_OF_NODES` details or at least log
`partial`; otherwise a bot can follow a truncated route while the tool says partial. This is a game-code follow-up,
not a tool review block.

No build, bake, game or install here. The runtime plan (PS owned bot, 30 m water, land-cliff negative, submerged
native-oxygen observation) stays as in `DS070_WATER_POLICY.md`.
