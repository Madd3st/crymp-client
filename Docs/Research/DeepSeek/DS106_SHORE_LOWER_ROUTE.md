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
| `US-low90` + one `ladder` link, 545 -> 99.768 | **path found, 5 corners, 1 ladder link, Reachable=true** |
| Same, 545 -> 103.25 | still `other_component` in the subset; the 99.75 -> 103.34 ramp is not in this exact subset |

The candidate link used the baked lower floor and the nearest baked upper-deck poly next to the shaft opening:
`2116.100 1880.600 90.168 -> 2117.500 1880.600 99.768 1 ladder`. The 1.4 m horizontal offset from the shaft
centre is the ladder opening in the deck, not an invented shortcut. This is a real native ladder, and the
connector is a data row, not a teleport.

## 3. The two human routes and why only one is minimal now

* **Life 2 (ladder, preferred):** lower deck -> native long shaft `z 90.31 -> 99.54` at 1.00 m/s -> 99.75 deck ->
  walkable ramp to `103.34` -> existing upper Strength link -> ring -> sea. The only missing lower piece is the
  ladder link plus its executor; the upper route is already the accepted E223/C149 geometry.
* **Life 1 (bilge dive, deferred):** lower deck -> local bilge surface `89.37` -> swim to `83.66` under the hull ->
  outside `100.11`. This needs real 3-D swim/dive intent. `BotMovement::Move` zeroes `direction.z`
  (`BotMovement.cpp:91-92`) and `BotLocomotion` refuses jumps while swimming (`:111`), so the current surface-water
  policy cannot follow it. Do not fake it with a lower `zmin` or a teleport; it is a separate scope.

## 4. Minimal coherent fix (data + spawn + bounded native ladder actuator)

**A. Data (one new full-map bake, no forced resample).**
`--zmin 90.0 --water 99.5` on the full `Shore.crop.obj.gz` (not the US subset), keeping the C145 capsule/params.
This includes the dry lower deck and the shaft but deliberately excludes the bilge water at `89.37` and the pool
floor at `83.66`. The water mask already refuses a 99.5 water quad under a solid above-water deck
(`recast_bake.cpp` `AddWaterSurface`: a triangle above `waterZ` blocks the cell), so the dry hull interior does not
gain a fake sea surface. Add exactly one manual link row at the validated ladder; do not add links through the hull.

**B. Spawn layer validation (pure helper + engine call).**
After the PS group spawn is chosen (`BotManager.cpp:390-405`) and before the position is used, require either:
* dry: `Project(pos, snapped, 1.5, 1.0, 3.0)` and `|snapped.z - pos.z| <= 2.5`; or
* water: `Project(..., waterOnly=true)` with the existing water bounds.
On failure, ask the game for another spawn with the rejected id in `skipId`, bounded to 8 tries, then log and refuse.
Never accept 530's +12 m projection and never silently fall back to an off-mesh point. The frozen C150 package keeps
its `z>=99.5` readiness rule; a new package should use "projects on the mesh or is genuine water" instead.

**C. Bounded native ladder actuator (new small controller, default OFF).**
The native API is server-capable and already exists:
`CPlayer::IsLadderUsable` (`Player.cpp:7126`), `RequestGrabOnLadder` (`:7226`), `GrabOnLadder` (`:7242`),
`RequestLeaveLadder` (`:7311`), and `CPlayerMovement::ProcessMovementOnLadder` (`PlayerMovement.cpp:1568`). Bots
have no `PlayerInput`, and the automatic top exit is inside `player.IsClient()` (`PlayerMovement.cpp:1612-1635`),
so a server-only bot needs a tiny explicit controller:

1. **Approach** the link base with the normal path; face the real ladder line (not the nav link's diagonal).
2. **Grab** only when `IsLadderUsable()` is true (look ray hits `mat_ladder` within 2 m), then call
   `RequestGrabOnLadder(eLAT_Use)`. On server this goes straight to `GrabOnLadder`; log the native entry snap and
   fail if it moves the body more than 1.5 m from the approach point.
3. **Climb** while `isOnLadder`: keep a horizontal move target `feet + ladderOrientation * 2.0` so the native
   `desiredVelocity.y` stays positive; native movement owns the 1 m/s vertical motion. Do not inject z into
   `BotMovement` and do not time-detach.
4. **Exit** only when `feet.z >= ladderTop.z - 0.5`: call `RequestLeaveLadder(eLAT_ReachedEnd)` (not
   `eLAT_ExitTop`, which waits for a client animation and would leave a server bot in `isExitingLadder`). Verify
   `isOnLadder==false` and the body is on the deck before declaring done; otherwise fail and ban the link.
5. **Cleanup/timeout:** 15 s total, 1 s grab window, always leave the ladder on stop/death/level end, restore the
   collider mode (native `LeaveLadder` does), and never keep a pending climb across a reset. A failed link is banned
   for the level through the existing `NavMeshCore::BanLink`/`NoteLinkFailure`.
6. **Gates:** ladder kind is enabled only when this executor is enabled (`wb_bot_ladder=1`, default OFF) and the
   existing jump-link mask is on. Do not enable the ladder bit while the executor is absent: today
   `BotNavMesh.cpp:109-111` enables only drop/jump/strength for exactly that reason. Net-zero flag rule applies to
   the batch (retire/promote one old flag or name the diagnostic exit date).

## 5. RED/GREEN oracles for the batch

* **RED (already measured):** E223 545 `Project=false`; 530 snaps `+12.331 m`; `US-low90` 545 -> upper
  `other_component`; `BotNavMesh.cpp:109-111` never enables the ladder kind.
* **GREEN (data):** full-map bake with `zmin 90` and the one real ladder row: all 12 lower spawns project with
  `|snap| <= 0.25 m`, 545 -> 99.768 returns a path with exactly one `ladder` link, and 530's false projection is
  rejected by the layer check.
* **GREEN (executor, engine-free unit):** fake ladder API feeds approach/grab/climb/exit; assert no grab without
  `IsLadderUsable`, no exit before `top-0.5`, no timeout detach, one ban after a failed exit. RED on 5ef: no ladder
  kind/executor at all.
* **Runtime acceptance later (not this item):** one lower-spawn bot, no `start_off_mesh`, `LADDER` events, deck at
  `~99.7`, then the existing upper route; no client/visual/full-Shore claim until that run.

## 6. What this does not prove

The subset does not contain the 99.75 -> 103.34 ramp or the full upper graph, so 545 -> 103.25/sea/captures is not
proven by the 5-corner result. A full-map bake must re-check the ramp and the existing upper links, and the native
ladder grab/exit/collision behaviour still needs the runtime acceptance. The bilge dive route is explicitly
deferred. No new gameplay, teleport, gate weakening or upper resampling is proposed.

## 7. Reproducible recipe

`ds106_lower_route_recipe.ps1` (own branch) contains the exact baker and probe commands. The probe source is
`proto/ds106_nav_probe.cpp`; it links the same `BotNavMeshCore.cpp` + Detour sources as CI and prints
`Project`/`FindPath`/`FindPathLinks`. The private run used the existing `recast_bake-final.exe` and
`Shore.US.exact-subset.obj`; no QA or game directory was written.
