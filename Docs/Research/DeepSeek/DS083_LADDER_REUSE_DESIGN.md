# DS-083 - C145 upper native-ladder route verification and bounded reuse design

> SUPERSEDED FACTS (DS-083 correction, `DS083_UPPER_STRENGTH_CORRECTION.md`): the ring **is** baked on the full
> candidate (east 106.0386 / west 106.0496, on mesh, `fromUS=0`). The "ring is not baked" row in section 2 is
> wrong - those probes were inside the cockpit. The ladder subsystem is deferred until the existing Strength +
> drop route is ruled out. The native-mechanics and design sections below stay as the superseded design.

Corrects DS-082 (`1e9fc9c3`): the 9.4 m shaft is **not** the only exit. Codex X-231 / weapon-goals `9e5fef54`
(`Docs/Coordination/C145_NATIVE_LADDER_ASSETS.json`, raw `Shore.sub-ladder-placements-validated.json`) places
**two `sub_ladder_forwardhatch_b` ladders on the upper cockpit edges** plus the long shaft. This note verifies the
placements against the canonical triangles, maps the upper route, and proposes the smallest native-ladder reuse.
Design only: no actuator, link enable, flag flip, build, game or GUI run.

## 1. Placement/material facts and independent check

Source: Codex brush.lst decode (76 material records, 1069 objects, 5543x76 placements, objectIndex at 8, Matrix34
at 24, all finite, det 0.9999999..1.000000000000014). Assets are `objects/library/architecture/submarine/props/
ladders/*.cgf`; `sub_ladders.mtl` submaterial 3 = `ladderproxy` -> `mat_ladder`; the CGFs contain the ladderproxy
node and material strings (sha256 in the JSON). `Player.cpp:7126-7223` uses the **collision surface** `mat_ladder`,
not a mission entity, so the missing Ladder entity in the mission XML is not evidence.

Independent triangle check (`Shore.US.exact-subset.obj`, exact JSON world bboxes, +/-0.01 m):

| Placement | record/object | translation | world top | OBJ vertices in bbox | Codex near-count |
| --- | --- | --- | --- | --- | --- |
| forwardhatch_B east | 3369 / 797 | 2113.5, 1885.5, 103.25 | 106.866 | **204** | 204 |
| forwardhatch_B west | 4827 / 797 | 2104.75, 1883.0, 103.25 | 106.866 | **204** | 204 |
| longshaft bottom | 3538 / 816 | 2116.125, 1880.125, 90.25 | 91.25 | **54** | 54 |
| longshaft (whole) | 3688 / 835 | 2116.125, 1880.125, 90.25 | 100.291 | 553 | 565 |
| longshaft middle | 934 / 412 | 2116.125, 1880.125, 91.25 | 95.25 | 72 | 85 |
| longshaft top | 2478 / 682 | 2116.125, 1880.125, 95.25 | 99.75 | 369 | 435 |

The two upper ladders match 204 exactly; the shaft segments match within the "near bbox" tolerance of the Codex
count (same asset family, same coordinates). Rotation determinants are 1.0 (0.99999988 on the west ladder, i.e.
the 90-degree yaw matrix). This independently confirms the placements against native collision geometry.

## 2. Upper route as built (geometry facts)

* Cockpit/spawn: floor **103.13** (spawns 103.25), x 2104.5..2113.5, y 1882..1886.5, **open above**; the walls
  form a coaming ring at z **105.94..106.13** around the opening:
  * west ring x 2102.5..2104.5 (~2 m wide), full y 1881..1887;
  * east ring x 2114..2115 (~1 m), north/south strips y 1881 / 1887 (~0.5 m);
  * the ladder proxy tops (106.866) are the top rung/handrail, 0.75 m above the ring.
* Upper ladders: east base (2113.5,1885.5,103.25) on the cockpit east wall, top at the east ring; west base
  (2104.75,1883.0,103.25) on the west wall, top at the west ring. Both are `forwardhatch_b`, i.e. hatch ladders
  from the cockpit floor to the coaming/hatch top.
* Shaft ladder: (2116.125,1880.125), bottom 90.25, top 100.291, through the corridor-floor opening at
  x2115.5..2116.5 / y1880.5..1881 (DS-082 found the same hole from the triangle side). This is the **lower-deck**
  route, not the only exit.
* Exterior from the ring: west/north ring edge -> hull top at y1879-1880 z~103.0-105.0 (a ~2.5-3 m drop); south
  edge y1888 z105.6-106.1 -> y1889 z104.2 -> y1890 z103.3 -> hull slope -> water 99.5 at y1892-1893. The drops
  are physical, not walkable steps.

Nav check on the loaded bake (`Shore.eed85fb1.candidate.navmesh`):

| Point | Result |
| --- | --- |
| cockpit 2109-2113, 1883-1885, z103.3 | on mesh, z103.211, `fromUS=1` |
| ring 2110,1884 / 2105,1883 / 2113,1885 at z106.3 | projects to **103.211** (ring is **not baked**) |
| hull deck 2110,1878 (z101.45, `fromUS=0`) / 1879 off mesh; south 1889-1891, water 1876-1877/1894 | off mesh or `fromUS=0` |

So the upper route is **physically real but not represented**: the ring is a narrow coaming that recast does not
bake, and the deck/water are other parts. A ladder alone does not connect the bot to the map.

## 3. Native ladder mechanics (facts, exact gates)

* `PlayerInput.cpp:1606-1615`: ACTION_USE while on a ladder -> `RequestLeaveLadder(eLAT_Use)`; otherwise if
  `IsLadderUsable()` -> `RequestGrabOnLadder(eLAT_Use)`. Bots have **no ACTION_USE path** today.
* `Player.cpp:7126-7223`: `IsLadderUsable` needs a **look-at ray (2.0 m)** hitting `surface_idx == mat_ladder`,
  then reads the static object AABB and sets `ladderTop/ladderBottom/orientation/upDir`.
* `Player.cpp:7226-7240`: server -> `GrabOnLadder` directly; client -> `SvRequestGrabOnLadder` RMI.
* `GrabOnLadder` 7242-7300: sets `isOnLadder`, moves the actor to the native entry point (in front of the ladder),
  collider mode "pushes players only", `ClGrabOnLadder` to remotes.
* `PlayerMovement.cpp:1568-1720` `ProcessMovementOnLadder`: climb from `desiredVelocity.y`; **top/bottom exit checks
  are `player.IsClient()` only**; `isExitingLadder` completion is gated `gEnv->bClient`.
* `LeaveLadder` 7320-7376: `eLAT_ExitTop` plays the exit animation; other reasons clear `isOnLadder`; server RMI.
* `BotBlackboard.cpp:25`: `canMove` is false while `stats.isOnLadder`.
* `BotNavMeshCore.h:20-23`: `NavLinkLadder` is declared; `BotNavMesh.cpp:111` enables only drop/jump/strength.

## 4. Proposed bounded reuse design (no code enable now)

**Goal**: reuse the native grab/climb/leave instead of writing a ladder controller. No fake client, no direct
teleport, no hull link, no default change.

**Files**
1. `Code/CryGame/Bots/BotLadderPolicy.h` (new, engine-free): state machine Idle/Approach/Grab/Climb/ExitTop/
   ExitBottom/Cancel/Failed; inputs (dist base/top, look alignment, grounded, onLadder, progress, blocked,
   damage/ragdoll/vehicle, time); outputs (look target, move intent forward/back, grab/leave request, timeout,
   ban). Unit tests + CI.
2. `Code/CryGame/Bots/BotLadderAdapter.h/.cpp` (new, engine-facing, bot-owned actor only): calls
   `IsLadderUsable`, `RequestGrabOnLadder(eLAT_Use)`, `RequestLeaveLadder(eLAT_ExitTop/None)`, sets the look
   through `BotMovement::Look`, drives the climb through `Move`/`Look`, reads `m_stats.isOnLadder`,
   `ladderTop/Bottom`, and reports progress to the policy.
3. `Code/CryGame/Bots/BotBrain.cpp`: one `UpdateLadder` branch before roam/capture, active only when a ladder
   hint is the current route and the gate is on.
4. `Code/CryGame/Bots/BotNavMesh.cpp:111`: enable `NavLinkFlag(NavLinkLadder)` **only** when the prototype gate is
   on (not now).
5. Data (proposal, not installed): `Shore.links.txt` rows - west ladder cockpit (2104.9,1883,103.25) -> west ring
   (2103.5,1883,106.1); east ladder (2113.1,1885.5,103.25) -> east ring (2114.5,1885.5,106.1); shaft
   (2116.1,1880.2,99.75) -> lower deck (2116.1,1880.2,90.4); drop west ring -> hull deck; drop deck -> water.
   The west ladder is the prototype (2 m ring vs 1 m east ring; recast radius 0.3 leaves 1.4 m walkable).
6. `Tests/BotLadderPolicyTests.cpp` + CI registration.

**Gate**: reuse the existing `wb_bot_jump_links` (off-mesh executor family, default OFF) for the prototype. No new
flag is needed for a dormant prototype; if it is ever promoted separately, a new `wb_bot_ladders` + one retired
flag (net-zero rule) + owner D-002 decision. No default flip now.

**Native physical exit / headless hazard**: because the top/bottom exits are `IsClient`-only and `isExitingLadder`
completion is `gEnv->bClient`-gated, the adapter must:
* at the top: check forward clearance with the same static/terrain ray as the client, then
  `RequestLeaveLadder(eLAT_ExitTop)`;
* if `isExitingLadder` does not complete headless within 2 s, fall back to `LeaveLadder(eLAT_ReachedEnd)` (clears
  the state without moving the actor), then let normal movement step onto the ring;
* at the bottom: same with `eLAT_None`.
This is a **real completion risk**, not a 3-line fix; it needs a unit-tested policy and a bounded runtime probe
later. No client emulation, no RMI spoof, no `SetWorldTM`.

**Cancel/timeouts/safety**: grab attempt 1.5 s; no climb progress 2 s; whole link 15 s; cancel on damage,
ragdoll, vehicle, water/zero-g, health<=0, or a human within 2 m of the base/top; ban that ladder for the level on
failure; restore normal movement. The native `GrabOnLadder` entry move is the only actor displacement and is the
normal game path.

**Replication**: the bot is server-owned; `GrabOnLadder` sends the native `ClGrabOnLadder` to remote clients and
`Player.cpp:5627-5630` already serializes ladderTop/Bottom/orientation/upDir. No new RMI, net layout or client
version change; no fake channel.

**Route proof / acceptance**
* Offline: policy unit tests; nav query cockpit -> ring only after the ring is baked/linked; route query
  cockpit -> ring -> deck -> water; exact link rows and triangle counts (this note).
* Runtime later (owner scope, exact green+review, one bot, west ladder, <=60 s): z 103.25 -> 106.1 monotonic,
  `isOnLadder` true then false, grounded on the ring/deck, health 100, no stuck, no human interference. Not a
  Playable claim.
* The ring bake/link and the exterior drop rows are prerequisites; without them the ladder ends on a coaming the
  navmesh does not know.

## 5. Scope proposal to the owner (D-031 decision)

The upper route is **physically valid and shorter** than the shaft, and the native ladder APIs exist server-side.
But it is not a 3-line fix: it needs (a) the native ladder adapter + headless exit completion, (b) a nav/bake
representation of the 2 m west ring, (c) two drop links (ring->deck, deck->water), and (d) a bounded runtime probe.
Option A (recommended prototype): west forwardhatch_B only, ladder link + drop rows, gate `wb_bot_jump_links`,
default OFF, ~3 code files + data + tests, no default change. Option B: lower shaft only, still blocked on the
lower-deck water fact. Option C: park the exit until the owner scopes a ladder subsystem. No option starts without
the owner's decision.

## 6. B1 roam guard

Codex `0b39bdb6` already passed the independent BotRoamFallback unit, actual BotBrain C++20 syntax and the
four-goal core RED->GREEN; no rewrite and no B1 game. My earlier inline proposal (`patches/ds082_roam_guard.patch`,
already on this branch) stays a proposal and is superseded by `0b39bdb6`; the engine-free helper draft was not
pushed. The separate ladder batch stays reviewable on its own.

## 7. Non-goals

No actuator/links/flag enable, no fake client, no direct bot teleport or hull link, no default promotion, no
game/GUI/build/install, no triage writes, no Playable claim.
