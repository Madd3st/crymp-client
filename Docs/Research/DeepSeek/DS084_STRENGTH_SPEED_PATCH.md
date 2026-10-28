# DS-084 - revised Strength take-off speed patch (replaces ds083_strength_speed.patch)

The DS083 patch `26dac8d7` is **not accepted** and must not be integrated: it only changed the gait (Pace1 Run still
used stance max, so no real cap), `TakeoffGait()` also returned Run for **normal** jumps (legacy sprint
regression), and the policy kept the audit's hardcoded apex 3.62/g 9.81. This patch replaces it.

## Confirmed graph input (X233 / weapon-goals 276a39b5)

Full unchanged canonical bake, candidate `E2235993CE72...` (data only, not installed): exactly **one** Strength
link `2110.56,1885.58,103.18 -> 2113.98,1885.51,106.07` (the reverse of the legitimate drop, rise 2.89 < 3.72)
connects upper US to both sea probes and the graph reaches capture 64/316/579/654 + NK (11 corners/1 Strength,
direct US->sea). 344's chosen point stays `other_component`; 316's path is a 256-poly rolling prefix 31.02 m
short - no full route/Playable claim. The other 10 autolinks are not needed.

## What the patch does (existing executor only)

1. **Actual numeric cap, propagated**: `BotIntent.speedCap` -> `BotLocomotion::Execute` uses
   `min(base, speedCap)` for the movement request and **disables sprint** while capped. Pace1 Run no longer
   passes stance max for a capped link.
2. **No normal/drop/long-Strength regression**: the Act gait line is `speedCap>0 ? Run : Sprint`. Only a
   steep-short upward Strength link with a valid cap runs; normal jumps, drops and long Strength links keep the
   legacy Sprint path. `TakeoffGait()` is gone.
3. **Actual actor params, no audit constants**: `StrengthSpeedCapFor(a,b,gravity,jumpHeight,energyOk,airTime)`
   uses the native formula `v0 = sqrt(2*g*jumpHeight*4.7) - 0.5*g*inAir` (`PlayerMovement.cpp:1130+`) from
   `SPlayerParams.jumpHeight`, `SPlayerStats.gravity`, `inAir`, suit mode and energy >=70. No 3.62/9.81
   hardcode. Invalid physics (g<=0, jumpHeight<=0, v0<=0, disc<=0) or missing suit energy -> no cap, legacy
   gates still apply (TooHigh/NoEnergy unchanged).
4. **Bounded and general**: only `dz > 0.5` (steep upward) and `needed <= 3.6` (inside the audited clear band)
   are capped: `cap = min(4.0, max(2.5, needed*1.1))`. Long/flat/downward links return no cap. No map
   coordinates, no new flag, no impulse, no teleport, no ray bypass.
5. **Arrival semantics untouched**: `BotJumpLink` Done still requires `landRadius` and `|dz| < landDz`; the cap
   only limits the desired speed during Approach/RunUp/Act/Air, including the 2.5 m run-up and 0.6 m take-off
   window. No false Done at a wrong height.

The C145 link with actual defaults (jumpHeight 1, g 9.81) needs 2.157 m/s and caps at 2.5 m/s (audited clear);
the old forced sprint 6.75 is blocked at 0.44 s. A measured-rise/closed-loop refinement is possible later from
`BotLocomotion`'s observed apex, but is not needed for this bounded patch.

## Files

* `Code/CryGame/Bots/BotJumpLink.h` - `StrengthSpeedCap`, `StrengthSpeedCapFor`, `BotJumpLink::SpeedCap`.
* `Code/CryGame/Bots/BotIntent.h` - `speedCap` field.
* `Code/CryGame/Bots/BotLocomotion.cpp` - capped desired speed, sprint disabled when capped.
* `Code/CryGame/Bots/BotBrain.cpp` - per-tick cap from live params; Act gait legacy unless capped.
* `Tests/BotJumpLinkTests.cpp` - engine-free red/green: C145 actual-param cap, invalid g/jumpHeight/energy/inAir,
  long/flat/downward/above-arc no cap (legacy), `SpeedCap` through the link object.

Gate: existing `wb_bot_jump_links` (default OFF); no new flag, no default change. `BotJumpLink` is already in the
CI unit list, so the new cases run without a workflow change. B1 `0b39bdb6` is untouched.

## Verification order

1. Codex: apply `patches/ds084_strength_speed.patch` on `b06681ea`, compile, run the `BotJumpLink` unit (CI) and
   the existing client/server CI; independent review of this exact SHA.
2. Only then one named native jump/exit observation on the candidate (exact green+review), not a Playable claim.
