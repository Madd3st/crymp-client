# walk_sim fidelity vs BotBrain/BotMovement at `d9c518b7` (DS-012 #2)

The sim now models `arrive` (0.2/0.8), a 3×2 body-ray `SafeStep`, the 8-way fan, the 2 s sample/avoid timer, and
drop/jump/strength links. Still missing, in order of impact:

| # | Missing | Game ref | Sim ref | Impact / fix |
| --- | --- | --- | --- | --- |
| 1 | **No repath on a stall** | `BotBrain::Steer` repaths after 5 s (success) / 2 s (failure) and when the goal moves >4 m, `BotBrain.cpp:110,140` | one path at `walk_sim.cpp:146`, then a 3 s stuck verdict | largest: a single blocked corner is final in the sim, while the game re-routes. Add a repath at 2–3 s stuck (clear `idx`/replan) before trap recovery. |
| 2 | **No trap recovery/NoProgress** | `RecoverTrap` levels 1–4, `BotBrain.cpp:902-950`; `BotProgress` `:891-899` | none | the sim's 3 s “stuck” is stricter than the game's 8 s trap age; 84 % vs 37 stalls is partly this. After (1), add level 1 (clear path) + level 2/3 (step/jump) and the 8 s age. |
| 3 | **No BotNav fallback** | `Steer` falls back to `nav.FindPath` when the navmesh has no path, `BotBrain.cpp:137` | only `FindPathLinks` | a navmesh gap becomes `nopath`/stuck in the sim even though the game routes via the legacy graph. Add the same fallback or mark those pairs `nopath` separately. |
| 4 | **Slope rays off by default** | game always passes `slope` when `meshGuided && arrive<0.5`, `BotMovement.cpp:98-102,118` | `g_slopeRays=false` `walk_sim.cpp:131,136`; only `WALK_SLOPE=1` at `:267` | sim does not reproduce the d9c stair fix unless the env var is set. Make slope default ON for corners; pass it to the fan (`:220` currently 0, same bug as the game). |
| 5 | **No cliff ray** | `SafeStep` down-ray at `BotMovement.cpp:72-78` | absent | sim walks off ledges/drops; add the same `-3.6 m` ground check. |
| 6 | **Links are instant hops** | `BotJumpLink` approach/prepare/run-up/air/land, energy gate | `walk_sim.cpp:199` teleports to the next corner | Strength-tower/drop probes are not modelled; add at least a time+speed + energy fail for Strength. |
| 7 | **No turn rate/acceleration** | body turns ≤3 rad/s (`Player.cpp`), movement controller desired speed | constant 6.3/4.5 m/s toward the aim (`:202`) | affects overshoot/corner cutting; medium. Add a yaw rate and acceleration if timing is compared. |
| 8 | **Pairs are spawn→item only** | probes are spawn→goal/duel | `walk_sim.cpp:251` | probes 1/2/5 absent. Add explicit probe pairs (PROBES_V1) and the DS-009 3D cluster rule (`:285` uses XY cylinder). |

First to add: **repath at 2–3 s stuck** — cheap, does not mask geometry, and removes the largest single
game/sim difference. Trap recovery next; then BotNav fallback and slope-default.
