# DS-083 correction - upper ring baked; existing Strength route + take-off speed policy

Supersedes the "ring not baked" fact in `DS083_LADDER_REUSE_DESIGN.md`. I probed (2110,1884)/(2105,1883)/(2113,1885)
at z106.3 - those are **inside the cockpit opening**, so they projected to the cockpit floor 103.211. The actual
coaming points are baked. The ladder-subsystem scope question is **deferred** until the simpler existing-executor
route below is ruled out; the ladder design stays as superseded facts.

## 1. Ring/nav facts (QA actualCore, independently reproduced on both candidate navmeshes)

`upper-link-candidates.tsv/.pts`; reproduced with `area-query.exe` on `Shore.eed85fb1.candidate.navmesh` and
`Shore.candidate.navmesh` - identical results:

| Point | Projected | on mesh | fromUS |
| --- | --- | --- | --- |
| cockpit 2110.56,1885.58,103.18 | 103.211296 | yes | **1** |
| east ring 2113.98,1885.51,106.07 | **106.038643** | yes | 0 |
| west ring 2103.5,1883,106.1 | **106.049561** | yes | 0 |
| east ring 2114.5,1885.5,106.1 | **106.040489** | yes | 0 |
| hull deck 2110,1878,101.45 | 101.445946 | yes | 0 |
| hull/water 2096.06,1891.12,101.06 / 2093.28,1895.43,99.57 | 101.152649 / 99.511292 | yes | 0 |
| water 2100,1920,99.5 | 99.511292 | yes | 0 |

So the coaming **is baked**; it is simply not reachable from the US part. My DS-083 §2 "ring is not baked" is
withdrawn.

## 2. Existing-executor candidate (geometry audit, Codex `upper-strength-arc`)

Link endpoints: `a=(2110.56,1885.58,103.18)` cockpit floor, `b=(2113.98,1885.51,106.07)` east ring.
`rise=2.890`, `distance=3.421`, native Strength apex 3.62 -> `v0=8.4276`, descending branch `T=1.245`,
needed horizontal speed `2.748 m/s`.

Capsule audit on the exact 369 728 triangles (radius 0.4, 3 lanes, heights 0.05/0.9/1.5):

| vh | result |
| --- | --- |
| 2.500 / 2.748 / 3.000 / 4.000 | clear |
| 6.750 (full sprint) | **blocked at t=0.44 s** |

`US.generated.links.txt` already contains the legitimate one-way **drop ring -> cockpit**
(`2113.98,1885.51,106.07 -> 2110.56,1885.58,103.18 0 drop`). The reverse **Strength** row is a candidate using the
existing executor, not runtime proof. The other existing rows are hull/water drops/jumps
(`2096.06,1891.12,101.06 -> 2093.28,1895.43,99.57 jump`, `2085.85,1888.44,103.83 -> 2088.15,1890.57,101.63 drop`,
the 2119/2120 and 2133 deck/water drops, HQ drops), but they start on the hull/deck part, not on the ring.

**Graph status (honest)**: the reverse Strength bridges cockpit (US part) -> ring; the ring, hull deck and water are
all `fromUS=0`. The supplied TSV does **not** prove a ring -> hull/deck leg (the hull side from the ring is a
~50 deg cliff in the canonical triangles, so it would be a drop, not a walk). Next cheap check: a **link-enabled
route query** on the full candidate with the reverse Strength + `US.generated.links.txt` points (cockpit -> ring ->
hull -> water). If the ring part is separate from the hull part, one ring -> hull drop is still missing and the
"existing drops" alone do not complete the exit. No game needed for this check.

## 3. Speed control finding (the likely immediate defect)

`BotBrain.cpp:1465`:
```
if (m_link.Phase()==LinkPhase::Act && m_link.Move()!=LinkMove::Drop) WBI(gait,BotGait::Sprint); // c066
```
Every Jump/Strength take-off is forced to **sprint (6.75 m/s)**. The audit says that exact speed is **blocked at
0.44 s** for the C145 reverse link while run speeds <=4.0 are clear. The link can therefore collide/overshoot even
though the geometry is valid - a policy error, not a map-data error.

**Bounded general policy (no map hardcodes)**: derive the take-off gait from the link's own endpoints and the
native Strength arc:
* `dz = b.z - a.z`, `dist = |b-a|_2D`, `v0 = sqrt(2*g*3.62)`, `T = (v0 + sqrt(v0^2 - 2*g*dz))/g`,
  `needed = dist / T` (descending branch; `dz >= v0^2/2g` -> infeasible).
* `needed <= runSpeed (4.5)` -> **Run**; `runSpeed < needed <= sprintSpeed (6.75)` -> **Sprint**;
  `needed > sprintSpeed` -> infeasible (do not start, log/ban).
* Jump links keep the current run/sprint choice; drops unchanged. No per-map entries.

Reviewable change (proposal, not pushed as game code): `Docs/Research/DeepSeek/patches/ds083_strength_speed.patch`
(applies clean on `b06681ea`, no build; `BotJumpLink.h` + `BotBrain.cpp` + `BotJumpLinkTests.cpp`).
* `Code/CryGame/Bots/BotJumpLink.h`: add `StrengthNeededSpeed(a,b)`, `TakeoffGait()`, `SpeedFeasible()`,
  `LinkParams::runSpeed/sprintSpeed` (4.5/6.75, matching the baker's `vh` and the player sprint). Engine-free.
* `Code/CryGame/Bots/BotBrain.cpp:1465`: use `m_link.TakeoffGait()` instead of unconditional Sprint; at link
  start, skip+ban a Strength link whose `needed > sprintSpeed`.
* `Tests/BotJumpLinkTests.cpp`: C145 reverse -> `needed=2.748`, **Run** (old code would Sprint); long flat link
  (10 m, dz 0) -> `needed=5.82`, Sprint; short high link -> Run; rise > maxStrengthRise -> TooHigh; infeasible
  speed -> not started. RED = forced sprint, GREEN = geometry gait.
* Gate: existing `wb_bot_jump_links` (default OFF); no new flag, no default change.

## 4. Scope / next

1. **Cheap offline (Codex)**: link-enabled route query with the reverse Strength + existing drops (ring -> hull ->
   water). If it completes, this is the smallest completion - no ladder subsystem and no owner scope question.
2. If the ring part is separate: one ring -> hull drop is the missing row (still existing executor, geometry-audited).
3. Only if both fail: the ladder subsystem scope proposal (DS-083 design) returns; it stays superseded until then.

No game/GUI/build/install/triage. No new actuator or default.
