# C146 owner native route on Shore - offline analysis (measured, not inferred)

Source: owner session `build/owner-playtests/C146-native-Shore-b06681ea-20261028-151609/` (session.json
`phase=archived_memory_restored`, demo `Multiplayer_PS_Shore-20261028-151611-1.wbdemo` 252 007 B sha256 `6BBF0254...`,
memory snapshot + `memory-restored.json`, no screen video). Server log: `RULES INFO class=PowerStruggle`,
`bots=0/1`, `wb_bot_navmesh=0`, `wb_bot_jump_links=0`, 0 `[Error]`/Lua lines on both sides. Client trace: 999
`MOVE TRACE` rows `t=31.696..300.149` at 20 Hz, `side=client kind=human`; server trace 1003 rows at 4 Hz.
Two `wb_mark route` marks at t=37.96 `(2118.77,1884.45,86.75)` and t=41.12 `(2114.75,1880.58,84.60)`.

This is the first human-route evidence for C145/DS-084. It answers the DS-084 decision question directly.

## Life 1 (t=31.7..113.4): flooded bilge swim-out, then boat

| t | fact |
| --- | --- |
| 31.91 | spawn **lower deck** `(2124.50,1880.63,90.12)`, HP 100, `mode=3` (Defense) |
| 33.4-35.9 | walks off the deck `89.58` at `(2123.6,1884.0)`, drops in |
| 36.15 | **swim=1** at `(2120.47,1884.38,87.81)` - bilge is flooded below the dry deck (~89.6); the local water plane at the entry is `waterZ=89.37` (`rel=1.50` standing, `0.61` when dropping in) |
| 42.15 | deepest `83.66` at `(2112.90,1876.23)`; deep rows report the sea plane `waterZ=99.5` with `relativeWater=-5.00` (sentinel), `underwater` timer 0->8.8 s, **HP stays 100** (no drowning damage in 8.8 s) |
| 45.14 | swim ends **outside at sea** `(2092.57,1871.14,100.11)`, `relativeWater=+1.80` |
| 47-75 | `speed=0` while position moves 9-19 m/s at `z~100.9` - **boat** (player attached, own velocity 0) |
| 77-91 | shore near `(1942,1593)` -> `(1958,1538)`, `z~101.1`, walking, 0.5-0.8 m jumps off ledges |
| 113.4-132.9 | **no trace rows** (server gap 122.8-142.1, ~19 s), no error/death line; then respawn below |

Water plane in the open sea is `waterZ = 99.37..99.67` (median 99.5): the bake's 99.5 is **correct**. The bilge has
its own surface at `89.37`, and once submerged the deep rows report the global 99.5 with the sentinel
`relativeWater=-5.00` (no valid local plane) - this is the DS-078 "interior dry sentinel": a sub-water-plane
coordinate inside the hull is not evidence of a baked swimmable floor.

## Life 2 (t=132.9..300.1): shaft ladder -> deck -> Strength jump -> ring -> water -> boat

| t | fact |
| --- | --- |
| 132.9 | respawn **lower deck** `(2113.66,1880.38,90.37)` |
| 139.39-148.64 | **native vertical climb**: `x=2116.125, y=1880.625` (fixed), `z 90.31 -> 99.54`, `vz=+1.08`, `speed=0`, `grounded=0`, 9.25 s = **1.00 m/s** - the long-shaft ladder from DS-083 (`2116.125,1880.125, 90.25..100.29`) |
| 148.6-154.6 | deck `99.73-99.75`, walks `(2116..2130, ~1881..1884)` |
| 154.9-155.4 | **walkable ramp** `z 99.75 -> 103.34` at `x 2117.7 -> 2114.8`, `vz=+4.66/+4.48`, `grounded=1` |
| 155.6-156.6 | walks `103.13` west along `y~1883.3` to `(2107.37,1883.29)` |
| **156.64** | take-off `(2107.373,1883.286,103.125)`, `mode=1` (**Strength**), energy `172.5` -> `105.1` after the jump, `sprint=0` |
| 157.39 | apex `z=106.682` (`peakZ`), i.e. **+3.557 m** above take-off |
| 158.15 | lands `(2102.23,1882.35,106.02)` -> **rise 2.892 m**, horizontal **5.22 m**, flight **1.50 s**, first-step `vh 3.75 m/s` (avg 3.48) |
| 158.4-159.4 | walks the **ring 106.0-106.1** (`mode` back to 3/Speed) |
| 159.65 | second (normal) jump `vz=+3.17`, `peakZ=106.98`, then falls |
| 161.40 | lands in the sea `(2120.06,1871.78,100.36)`, **HP 100->80** (fall damage), regenerates |
| 163-236 | boat west/north along the shore, then shore walking/jumping `z 101-115` |
| 240-300 | hills `z 124-128`, ends static `(2264.5,1299.5,124.35)` |

## What this settles

1. **The human exit is a Strength jump, not a ladder.** The upper `forwardhatch_B` ladders (103.25 -> 106.87) were
   **not used**: the human jumped from the 103.125 deck onto the ring. So the one-Strength-link candidate models a
   real human move, and the DS-083 ladder actuator is not required for this exit.
2. **The measured jump matches the candidate link's rise exactly**: human rise **2.892** vs link rise **2.890**
   (`a 2110.56,1885.58,103.18 -> b 2113.98,1885.51,106.07`). Human take-off is 3.93 m from `a`, landing 1.42 m
   from the west ring point `(2103.5,1883,106.05)` - i.e. the human used the **west** counterpart of the same
   deck->ring edge that the candidate models on the **east** side.
3. **Apex is 3.557 m, not 4.7 m.** The DS-084 "stock jumpHeight1 x mult4.7 -> apex 4.7" model overestimates the
   take-off velocity by ~15 % (`v0 = sqrt(2*g*3.557) = 8.35` vs 9.60). The DS-046 `maxStrengthRise 3.72` and the
   conservative audit apex 3.62 are the realistic numbers; 4.7 is not what the game did here.
4. **Take-off horizontal speed is 3.5-3.75 m/s, with `sprint=0`.** The executor forces Sprint in `Act`
   (`BotBrain.cpp:1465`, ~6.75 m/s). At the real apex 3.56 that arc carries `6.75 * 1.22 = 8.2 m` from `a`, i.e.
   ~4.8 m past `b`, beyond `landRadius 3.0` (`BotJumpLink.h:160`); the conservative arc audit also reported
   `vh=6.75 blocked=1` at `t=0.44`. Human-consistent `vh` is ~3.5-3.75.
5. The held patch `3ec91e44` caps to `clamp(needed*1.1, 2.5, 4.0)`; with the measured apex the east link needs
   `3.421/1.22 = 2.81 m/s` -> cap 3.09, and with its own 4.7 model 2.16 -> cap 2.5. Both land within 0.6 m of
   `b`, and both sit at/below the human's 3.5-3.75 m/s. The band is right; the 4.7 term is the only wrong input.

## What this does not settle

* The bot's own execution: whether it fails as `missed_landing` (overshoot) or `no_takeoff`/blocked (geometry),
  and whether it then uses the ring -> sea part of the graph. The human fell 5.7 m off the ring with 18 damage,
  which is a physical drop, not a route the bot has been shown to take.
* Whether the **east** link (candidate) or a **west** link matching the human
  (`2107.37,1883.29,103.13 -> 2103.5,1883,106.05`, rise 2.92, horizontal 3.9) is the better bake. The human's
  take-off spot is inside the bot's observed dry-loop bounds (`x 2105.0..2121.4, y 1882.6..1886.2`), so a west
  link would match the human path more closely - but that is a new data row and needs its own bake/review.
* Nothing here proves the bot reaches water, swims, captures or plays; it is a measured human route.

## Next (narrow)

One bot run of the already-audited DS-084 recipe (candidate E2235993 + one-row links file, `wb_bot_jump_links 1`,
one owned US2, <=120 s) to record `LINK start kind=strength` and the actual failure/success reason. Apply
`3ec91e44` only if the link actually fails; keep the 4.7 term out of the acceptance argument, and consider the
west link only as a separate data task if the east arc is blocked for geometry reasons.
