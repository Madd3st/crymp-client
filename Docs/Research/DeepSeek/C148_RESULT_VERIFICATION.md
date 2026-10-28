# C148 result: independent verification of the one shared run (`def8a65d`)

Archive `build/c148-route-def8a65d-v1` (harness sha `9f14e9a7...`, source `def8a65d`, CI 40703800474 green, data
E2235993, one US2, doors/vehicles 0, 300 s cap, actual wall 85.0 s). Offline check of the archived trace/log/summary;
no game/build.

## Verdict: PASS on the trajectory claim (upper exit -> sea -> dry shore near 654); capture/full map NOT tested

| fact | measured |
| --- | --- |
| LINK | `start 0.27` a=(2110.56,1885.58,103.21) b=(2113.98,1885.51,106.04); native jump; **`done 3.10`** at (2114.85,1885.48,105.92) |
| sea entry | `11.52` (2054.84,1880.623,97.855), `swim=1`, `waterZ 99.43` |
| swim / wade legs | swim 11.5-25.5 s; then `swim=0` at z 98.2 with water 99.48 (≈1.3 m wading on a shallow bar) to 54.8 s; swim 55.0-76.3 s; walk 76.5-82.5 s |
| dry endpoint | `82.546` (1822.856,1685.845,100.967), `swim=0`, `relativeWater +2.92` (dry), `grounded=1`, health 100 |
| target 654 | 28.7 m away by 2-D; harness early-stop condition (dry, >100 m from the sub, <40 m from the target) met |
| remaining route | 3 corners / 0 links, `end_distance 0.001` to the target on the candidate mesh |
| health | never below 100 in 330 samples |
| nav fallbacks | 1 (`no_route`), no `start_unprojected`/`start_not_water` burst |
| archive/restore | demo 1642 frames sha `fd409447...`, logs archived, current memory+nav exact names/hashes restored |

Independent mesh probe (`area-query16384` on E2235993): endpoint -> on-mesh, snapped `101.081`, **land component**
(`fromNK=1`); target 654 -> on-mesh `101.531`, same component. Sea entry -> water `99.511` (`fromNK=0`, the known
water-component/buffer artifact). So the endpoint is real dry land connected to the target, not a dock/islet.

Wading note: `swim=0` while ~1.3 m under `waterZ` is the native shallow-water state (feet on ground); the endpoint
itself is 2.9 m above the water. Not a seabed walk; the patch did not create land.

## The DS090b B1 BLOCK: latent here, not refuted

This run never exercised the local water roam: the capture goal stayed selectable (`PS CAPTURE target=654` at 0.27 s,
no `hasDestination` loss), the mesh path succeeded, and only 1 nav fallback occurred. So the false-negative in
`BotBrain.cpp:959-970` (water goal sampled at feet z, dry goal box) did not fire. It remains a real defect for any
run where the bot falls back to the local water leg (e.g., a goal dropped inside one of the 41 holes), and the
one-line fix (`if (waterStart) p.z = m_blackboard.waterZ;` at `:965`) is still worth landing before such a run.

## Not claimed

No capture completion, no full-map traversal, no Playable claim, no claim that the shore->capture 28.7 m is
physically walkable beyond the 3-corner mesh path + the measured dry endpoint. The upper Strength exit, the water
start and the swim path are now **observed native** for one run.
