# DS-106: Shore lower route — offline geometry audit and minimal connector design

Base: `warsbots/feature/claude-sprint` `5ef9ee22dd2a84896f8ed33cc6c32e6e1261ccb9` (accepted C153 head).
Inputs (read-only): C145 bake receipt/command, `Shore.US.exact-subset.obj`, `US-low90.*`, `US-links.*`,
`Shore.eed85fb1.candidate.navmesh` (E2235993), `Shore.sub-ladder-placements-validated.json`, owner session
`S150S-20261028-222902`, C146 human-route evidence, DS098. No game, install, launcher, feature or QA edit; the
candidate bake/query below ran in a private temp directory with the existing offline baker and an offline
`NavMeshCore` probe.

## 1. Root cause is three separate gaps, not one missing input

1. **Bake clip.** The C145 final command explicitly used `--zmin 99.5 --water 99.5` (`Shore.bake-receipt.json`,
   `bake-shore.py`). The lower dry deck at `90.1-90.4` was intentionally dropped, not absent from the export. The
   geometry is present in `Shore.crop.obj.gz` (DS098: 65,890 vertices in the hull footprint, 44,142 below z=96).
2. **No vertical connector.** Even the diagnostic `zmin 90` bake (`US-low90`) contains the lower deck (12 spawns,
   component 171 m²) but no route to the 99.77 deck or the sea. The real native long-shaft ladder is at
   `(2116.125,1880.125)`, z `90.25..100.29` (`Shore.sub-ladder-placements-validated.json`, asset
   `sub_ladder_longshaft.cgf` with `ladderproxy`/`mat_ladder`); it was never represented in the nav graph.
3. **No spawn layer check.** On the accepted E223 mesh, spawn 545 fails `Project` and spawn 530 false-projects
   `+12.331 m` through the deck. A bare `Project==true` is unsafe; the z/layer displacement must be bounded.

## 2. Re-measured evidence (this session, offline)

| Query | Result |
| --- | --- |
| E223, spawn 545 `(2114.50,1887.88,90.38)` | `Project=false` (same as DS098 `start_off_mesh`) |
| E223, spawn 530 `(2113.62,1880.38,90.38)` | `Project=true`, snapped `z=102.711` -> **+12.331 m** false upper-deck hit |
| `US-low90` (zmin 90), spawns | 23/23 project; 12 lower spawns in the 171 m² lower component; 545 -> ladder base = 4 corners |
| `US-low90`, 545 -> upper deck 99.88 / 103.25 | `other_component` (no connector) |
| `US-low90` + one **UP-only** `ladder` link, 545 -> 99.768 | **path found, 5 corners, 1 ladder link, directed Reachable=true** |
| Same link, 99.768 -> 545 | **other_component** (correct for an UP-only production edge) |
| Same, 545 -> 103.25 | still `other_component` in the subset; the subset does not represent the 99.75 -> 103.34 corridor |
| E223, ladder exit `2116.211,1879.724,99.540` -> takeoff `2107.373,1883.286,103.125` | **walk-only path found, 8 corners, 0 links**; every C146 corridor point projects (99.81 -> 103.21) |
| US-low90, exact C146 32 grounded points, `ch 0.1` | 2 `other_component` gaps: `149.895 (2120.737,1880.972,99.744)` and `155.648 (2113.942,1883.214,103.335)` |
| Same geometry/agent, **only `ch 0.05`** | both gaps fixed, **31/31 adjacent paths**; `cs 0.05` alone does **not** fix them |
| Matched no-retain baseline | bit-identical to archived `US-low90` sha `E1ADC571FA4776EFC402D7C21276AC7A58DB26DBE14B961206F01ECEA3AD7ACD` |

The candidate link used the baked lower floor and the nearest baked upper-deck poly next to the shaft opening:
`2116.100 1880.600 90.168 -> 2116.700 1879.800 99.768 0 ladder` (`bidir=0`, UP only). The horizontal offset from
the shaft centre is the ladder opening in the deck, not an invented shortcut. This is a real native ladder, and the
connector is a data row, not a teleport. The SCC count stays 86 because the edge is one-way; the directed reach is
what the route uses.

## 2b. Full-map V2 candidate (offline, NOT accepted)

Codex `QA/build/c154-bake/Shore-candidate-v2.receipt.json` (46.1 s, `installed=false`, `game_runs=0`):
`--zmin 99.5 --retain-box 2103 1877 90 2138 1892 99.5 --water 99.5 --ch 0.05`, navmesh sha
`b60178d7090fd8403b4ba0f4407d6a722c0057afd559a0024148ef0645930609`, 46,830 polys, 2 links,
**124/124 spawns project** (E223 had 112/124). The two links are exactly the accepted upper Strength row and the
UP-only ladder `2116.125 1880.625 90.250 -> 2117.500 1880.600 99.768 0 ladder`.

Actual-core query on that same data (`full-v2-mask14.log` / `full-v2-mask46.log`):

| Mask | lower 545 -> ladder top / upper / sea / dry shore 654 |
| --- | --- |
| 14 (drop+jump+strength) | FAIL `other_component` at the ladder; lower deck is isolated |
| 46 (+ladder) | PASS: ladder top 7 corners/1 link, upper 13/1, sea 23/2, dry shore 654 **40 corners/2 links** |

31/31 native C146 adjacent trace points pass with `ch 0.05`. The ladder edge ends at `2117.500,1880.600,99.768`
beside the observed native exit; the native root motion lands near `2116.2,1879.7,99.54`, then the bot must safely
settle/walk ~1.5 m to that end. Do not snap. The earlier V1 endpoint at the exact human exit failed Detour's 0.3 m
attach radius and is archived as a data failure; V2's real adjacent floor edge connects.

## 3. The two human routes and why only one is minimal now

* **Life 2 (ladder, preferred):** lower deck -> native long shaft `z 90.31 -> 99.54` at 1.00 m/s -> 99.75 deck ->
  walkable ramp to `103.34` -> existing upper Strength link -> ring -> sea. The only missing lower piece is the
  ladder link plus its executor; the upper route is already the accepted E223/C149 geometry.
* **Life 1 (bilge dive, deferred):** lower deck -> local bilge surface `89.37` -> swim to `83.66` under the hull ->
  outside `100.11`. This needs real 3-D swim/dive intent. `BotMovement::Move` zeroes `direction.z`
  (`BotMovement.cpp:91-92`) and `BotLocomotion` refuses jumps while swimming (`:111`), so the current surface-water
  policy cannot follow it. Do not fake it with a lower `zmin` or a teleport; it is a separate scope.

## 4. Minimal coherent fix (data + spawn + bounded native ladder actuator)

**A. Data (Codex `--retain-box` dry-interior exception, not a global zmin drop).**
A global `--zmin 90` is **rejected**: it would admit the submerged sea floor as walkable. Production keeps the global
`--zmin 99.5 --water 99.5` defaults and all original geometry in the water-mask input. Codex adds an explicit
`--retain-box` dry-interior exception around the hull volume (the dry lower deck/shaft only; not the bilge at
`89.37` and not the pool floor at `83.66`), with exact-loader negative controls and all water fixtures PASS. That
tool now exists as the offline **V2 candidate** (`Shore-candidate-v2.receipt.json`, section 2b), not accepted or
deployed. The vertical voxel must be **`ch 0.05`**, not
`0.1`: the exact C146 32-point corridor has two `other_component` gaps at `ch 0.1` (`149.895`, `155.648`) and both
close at `ch 0.05` with 31/31 adjacent paths, while `cs 0.05` alone does not. The matched no-retain baseline is
bit-identical to archived `US-low90` (sha `E1ADC571…`), so this is a real grid effect, not a loosened capsule. Add
exactly one **UP-only** manual ladder row at the validated shaft; no through-hull links and no extra stair connector.
The full-map bake must keep the existing 99.75 -> 103.34 corridor; E223 already has it (walk-only 8-corner path
below), so the fine vertical mesh is the data fix, not another link.

**B. Spawn layer validation (fail closed, no resampling).**
After the PS group spawn is chosen (`BotManager.cpp:390-405`) and before the position is used, require either:
* dry: `Project(pos, snapped, 1.5, 1.0, 3.0)` and `|snapped.z - pos.z| <= 2.5`; or
* water: `Project(..., waterOnly=true)` with the existing water bounds.
If the chosen spawn fails, **log and refuse the spawn**; do not retry with `skipId`, do not search for a convenient
upper spawn, and do not silently fall back. That is the owner's no-forced-resampling rule. The frozen C150 package
keeps its `z>=99.5` readiness rule; a new package should use the layer rule instead.

**C. Bounded native ladder actuator (default OFF; native exit, no manual detach).**
The native API is server-capable and already exists:
`CPlayer::IsLadderUsable` (`Player.cpp:7126`), `RequestGrabOnLadder` (`:7226`), `GrabOnLadder` (`:7242`),
`RequestLeaveLadder` (`:7311`), and `CPlayerMovement::ProcessMovementOnLadder` (`PlayerMovement.cpp:1568`). Bots
have no `PlayerInput`, and the native top/bottom checks are inside `player.IsClient()` (`PlayerMovement.cpp:1612`),
so the adapter widens **only** the gated owned-WarsBot server path and lets the stock collision/ray/animation checks
decide the exit:

1. **Approach** the link base with the normal path; face the real ladder line (not the nav link's diagonal).
2. **Grab** only when `IsLadderUsable()` is true (look ray hits `mat_ladder` within 2 m), then call
   `RequestGrabOnLadder(eLAT_Use)`. On server this goes straight to `GrabOnLadder`; log the native entry snap.
3. **Climb** while `isOnLadder`: keep a horizontal move target into the ladder (`-ladderOrientation`) so native
   `desiredVelocity.y` stays positive; native movement owns the vertical motion. Do not inject z and do not
   time-detach.
4. **Exit** at the native top: request `eLAT_ExitTop` and wait for the native collision/ray check plus the
   animation/`OnLadder` completion (`!isOnLadder && !isExitingLadder`). A height-only `eLAT_ReachedEnd` at
   `top-0.5` is **rejected**: it can leave the actor in the shaft centre under the ledge. No `SetPos`/`SetWorldTM`
   in bot logic; the native grab alignment is the only allowed snap.
5. **Completion check:** the native horizontal exit is C146 `t=148.644` at `(2116.211,1879.724,99.540)`,
   `grounded=1`; after that the bot must be on the 99.75 corridor, then use the existing ramp/stairs to 103.125. If
   native `isExitingLadder` never clears on a dedicated server, that is a **blocker to report**, not a fake success.
6. **Cleanup/timeout:** 15 s total, 1 s grab window, always leave the ladder on stop/death/level end, restore the
   collider mode (native `LeaveLadder` does), and never keep a pending climb across a reset. A failed link is banned
   for the level through the existing `NavMeshCore::BanLink`/`NoteLinkFailure`; OFF/no match cancels/repaths.
7. **Gates:** ladder kind is enabled only when `wb_bot_ladder=1` (default OFF) and the existing jump-link mask is on;
   union is `jump14 | ladder32 = 46`. Do not enable the ladder bit while the executor is absent. Net-zero flag rule
   applies to the batch.

## 5. RED/GREEN oracles for the batch

* **RED (already measured):** E223 545 `Project=false`; 530 snaps `+12.331 m`; `US-low90` 545 -> upper
  `other_component`; `BotNavMesh.cpp:109-111` never enables the ladder kind.
* **GREEN (data):** full-map bake with the Codex `--retain-box` (global 99.5 unchanged), **`ch 0.05`**, and the one
  UP-only ladder row: all 12 lower spawns project with `|snap| <= 0.25 m`, 545 -> 99.768 returns a path with exactly
  one `ladder` link, 99.768 -> 545 has no route, the C146 corridor is 31/31 adjacent, and 530's false projection is
  rejected by the fail-closed layer check. On V2, mask 14 fails at the ladder while **mask 46** reaches ladder
  top/upper/sea/dry-shore 654 (40 corners, exactly 2 links).
* **GREEN (executor, engine-free unit):** fake ladder API feeds approach/grab/climb/native-exit; assert no grab
  without `IsLadderUsable`, no manual `ReachedEnd`/height-only success, exit only after native
  `!isOnLadder && !isExitingLadder`, timeout is FAIL, one ban after a failed exit. RED on 5ef: no ladder kind/executor
  at all.
* **Runtime acceptance later (not this item):** one lower-spawn bot, no `start_off_mesh`, `LADDER` events, deck at
  `~99.7`, then the existing upper route; no client/visual/full-Shore claim until that run.

## 6. The 99 -> 103 corridor is real geometry, not a missing link

The exact subset does not represent the ramp, but the accepted E223 mesh does. C146 client trace rows 149-156 show
**grounded, jump=0, Speed-suit=0** movement from the ladder exit `(2116.211,1879.724,99.540)` through
`(2119.703,1883.319,99.750)`, `(2115.957,1883.350,102.330)`, `(2113.942,1883.214,103.335)` to
`(2111.212,1883.218,103.125)` and the Strength takeoff `(2107.373,1883.286,103.125)`. On E223 every one of those
points projects (99.81 -> 103.21) and a walk-only path from the ladder exit to the takeoff exists with 8 corners and
0 links. So the corridor/ramp/stairs are **not absent**; the subset merely did not include them. Do not invent a
second link there. The remaining unproven part is the lower ladder edge and its native grab/exit execution, not the
99 -> 103 geometry.

## 7. What this does not prove

The 5-corner subset result does not prove the full 545 -> sea/captures route, because the subset lacks the upper
graph; the full-map bake must re-check the retained corridor and existing upper links. The native ladder
grab/exit/collision/animation completion still needs the runtime acceptance. The bilge dive route is explicitly
deferred. No new gameplay, teleport, gate weakening or upper resampling is proposed.

## 8. Reproducible recipe

`ds106_lower_route_recipe.ps1` (own branch) contains the exact baker and probe commands. The probe source is
`proto/ds106_nav_probe.cpp`; it links the same `BotNavMeshCore.cpp` + Detour sources as CI and prints
`Project`/`FindPath`/`FindPathLinks`. The private run used the existing `recast_bake-final.exe` and
`Shore.US.exact-subset.obj`; no QA or game directory was written. **The recipe's `--zmin 90` is a diagnostic subset
only and is rejected for production**; production uses Codex's `--retain-box` with global `--zmin 99.5 --water 99.5`.
