# Boats on Shore (DS-031)

Date: 2026-10-26. Read-only at `claude-sprint` `2ffc95fa`. Source of entity data:
`Docs/Research/DeepSeek/ps_shore.json` (DS-004), `PowerStruggleBench` factory table, `leveldata.xml`.
Water level on Shore: **99.5** (verified in `leveldata.xml`).

## 1. What is on Shore

### Black HQ and the submarine

- `HQ12` (black) at **(2181.6, 1884.1, 105.75)**, model `…/Submarine/…/tower.cgf`, `hqType=submarine`.
- Black spawn group `SpawnGroup_US5` at **(2109.25, 1884.25, 104.4)** with ~60 spawn points inside/around the hull
  (z 90.4–103.3). This is the only black spawn area on Shore.
- `BuyZone_US13` at (2128.5, 1883.8, 100.5) — inside the submarine.

### Placed boats (all neutral, `kind=vehicle`)

| Class | Count | Positions (x, y, z) |
| --- | ---: | --- |
| `US_smallboat` | 4 | 2140.1, 1871.6, 101.1 · 2081.8, 1871.6, 99.9 · 2102.0, 1871.7, 100.4 · 2120.9, 1871.8, 101.1 |
| `US_hovercraft` | 4 | 2103.2, 1895.8, 99.8 · 2082.5, 1895.8, 99.8 · 2145.5, 1895.8, 99.8 · 2125.4, 1895.8, 99.8 |
| `Civ_speedboat` | 3 | 2310.9, 1349.2, 98.9 · 2306.9, 1636.2, 98.9 · 2303.3, 1349.2, 99.4 |

All are at or just above the 99.5 water line, i.e. in the bay south of the submarine and along the east shore.
They are **neutral** (`team=null`), so `CanEnterVehicle` (`PowerStruggle.lua:571-584`) accepts any team until a PS
owner claims one (90 s claim window).

### Naval factory

- `Naval_Factory_NK` (tan) at **(1758.5, 1343.2, 108.3)**; builds `usboat, nkboat, nkgaussboat, ushovercraft, nkapc`.
- **Black has no naval factory on Shore.** Factories on Shore: `Naval_Factory_NK` (tan), `Small_Vehicle_Factory_NK`
  (tan), `Vehicle_Factory_Neutral`, `Prototype-Factory6` (neutral). So black either uses the placed boats or
  captures the neutral vehicle factory (no boats) / the tan naval factory. This asymmetry is the core of the boat
  meta on Shore.

### Landing points (from spawn clusters and capture zones)

| Name | Point (x, y, z) | Why |
| --- | --- | --- |
| West dock | **1968, 1536, 100.7** | 20 neutral spawn points at z 100.7–101.1; capture zone centre 1949.8, 1527.5 (r 26.5); alien point 1799.4, 1686.3 is 180 m inland |
| East dock | **2292, 1534, 101.2** | 18 neutral spawn points at z 101.2–101.6; capture zone centre 2254, 1572.75 (r 26.5) |
| Submarine bay | **2100, 1871, 100.0** | boat row + black spawn group; the start/return point |
| Tan naval dock | **1758, 1343, 108.3** | factory itself is 8.8 m above water; a ramp/beach must exist west of it (verify in game) |
| High points | 1925.8, 1309.9, 117.1 · 2110.7, 1294.3, 123.1 | 15–24 m above water — **not** boat landings; reach via the dock ramps |

Distances: submarine bay → west dock ≈ **360 m**; → east dock ≈ **390 m**; tan naval factory → west dock ≈ 280 m.

## 2. Boat behaviour

### 2.1 Goal selection

The bot only considers a boat when the objective is on the other side of water and the land route is > 1.6× the
water route. Candidate boats: `US_smallboat`, `US_hovercraft`, `Civ_speedboat` within **30 m**, `GetSeatForPassenger`
empty, `CanEnterVehicle` true. Prefer smallboat (fast, narrow) over hovercraft (wide, can beach).

### 2.2 Enter

```
v = vehicleSystem->GetVehicle(boatId);
seat = v->GetSeatById(1);                 // driver; fall back to GetSeatForPassenger
if (seat && !seat->GetPassenger() && mode->CanEnterVehicle(v, botId))
    seat->Enter(botId, false);            // no transition, server snap
```

After entry: `GetLinkedVehicle()==v`, `GetChannelId()==0` unchanged, and **do not** call `SetWarsBotMotion` or the
normal `RequestMovement` — the vehicle owns the pose (`Player.cpp:1787` guard). Log `BOAT enter id= seat=`.

### 2.3 Drive (DS-028 contract)

- **Route**: straight legs between sea waypoints (no navmesh on water). Lookahead 12 m, corridor 4 m.
- **Throttle**: target speed 10–14 m/s for smallboat, 8–10 for hovercraft; slow to 4 m/s 20 m before a waypoint with
  a turn > 45°; brake at 8 m from the landing.
- **Steer**: `IVehicle::OnAction(eVAI_TurnLeft/Right, eAAM_OnPress, 1, botId)` at 10 Hz; release when heading error
  < 5°. `eVAI_MoveForward` held while target speed > current; `eVAI_Brake` when above.
- **Stuck**: `speed < 1 m/s` for 2 s with throttle → `eVAI_MoveBack` 1 s, then turn 30°; 3 failures → abort to the
  nearest shore and replan on land.
- **Shallow/edge**: raycast down 3 m ahead; if water depth < 1.2 m or land hit within 6 m, steer away; never beach a
  smallboat on purpose (hovercraft may beach, then exit).
- **Landing**: stop within 6 m of the dock waypoint, speed < 2 m/s, `seat->Exit(false,false)`; raycast down at the
  exit: land z > 100.5 and slope < 30° → accept, else try the next landing.
- **Combat**: driver never fires (boats have no mounted gun except the APC/AAA, which are not boats). If the bot is
  shot from shore, increase speed and hug the far side of the bay; do not stop in the open.

### 2.4 Exit and hand-off

After exit: verify `GetLinkedVehicle()==nullptr`, clear the `BOAT` goal, run `ChooseSpawnLocation`-independent land
pathing from the exit position. If the exit failed (still linked), retry once with
`GetExitPositionForActor`, then abandon the boat.

## 3. Water route generation

### 3.1 Runtime mask (recommended)

No navmesh exists on water, and the heightmap file is a container (header + material table + several blocks; see
§3.2). Build the mask at level start:

1. Bounding box of navigable water: x 1700–2400, y 1100–1960 (the bay + east channel).
2. Grid 4 m (176×216 = 38k cells). For each cell centre, `RayWorldIntersection` from z=160 downward, `ent_all`,
   `rwi_stop_at_pierceable`.
3. Cell = **water** if the first hit is the water surface or terrain z < 99.0; **land** if terrain z > 100.5;
   **unknown** otherwise. Water depth = 99.5 − terrain z.
4. Keep cells with depth ≥ 1.2 m and distance to land ≥ 6 m (erode the mask by 2 cells).
5. A* 8-neighbour, cost = length × (1 + 2/(depth+0.5)), so the route prefers the middle of the channel; then
   string-pull with a water-only line-of-sight check and resample every 10 m.

This costs ~38k rays (one-time, < 0.5 s on the server) and survives map edits. Cache by level name + water level.

### 3.2 Offline heightmap (optional)

`terrain/heightmap.dat` header is verified: version 22 at offset 0, file size at 4, `HeightmapSize=2048` at 8,
`HeightmapUnitSize=2` at 12, sector 64 at 16/20, `scale=0.015625` at 24, `water=99.5` at 28. The height grid is
**not** at offset 32: the file is 15,911,862 bytes, while 2048²×2 + 32 = 8,388,640, and offset 32 starts a material
string table. So an offline tool must locate the grid by validating known points (west dock 1968,1536 → 100.7; east
dock 2292,1534 → 101.2; submarine 2100,1871 → 100.0) before using it. Until then, use §3.1.

### 3.3 First routes

```
Bay → West dock:   (2100,1871) → (2060,1770) → (2010,1660) → (1975,1580) → (1968,1545)
Bay → East dock:   (2100,1871) → (2160,1780) → (2230,1680) → (2275,1600) → (2292,1545)
Tan naval → West:  (1758,1343) → (1810,1410) → (1880,1480) → (1940,1520) → (1968,1545)
```

These are straight-line proposals, not verified water; §3.1 must validate each leg before the first run.

## 4. Probes

| Probe | Setup | Pass |
| --- | --- | --- |
| `boat_shore_west` | bot at black spawn (2109,1884,104), objective west dock, boat at 2100,1871 | `BOAT enter`, 360 m in < 60 s, `BOAT exit` on land z > 100.5, no stuck > 2 s |
| `boat_shore_east` | same, objective east dock | 390 m in < 70 s, exit on land |
| `boat_naval_factory` | tan bot at naval factory (1758,1343), buy/enter `nkboat`, objective west dock | `CanEnterVehicle` true, exit at west dock |
| `boat_stuck_reverse` | boat placed against the submarine hull, objective 50 m behind | reverse within 2 s, no > 3 s push, no damage |
| `boat_no_route` | objective with no water path (high point 1925,1309,117) | no boat goal; land route chosen |
| `boat_owner_claim` | black bot claims a boat, tan bot tries to enter within 90 s | tan denied by `CanEnterVehicle`; after 90 s allowed |

New log lines needed: `BOAT enter actor= vehicle= seat=`, `BOAT waypoint i= pos= speed=`,
`BOAT stuck n=`, `BOAT exit actor= pos= onLand=`, `BOAT abort reason=`. Existing `MOVE TRACE` describes the actor,
not the boat; add `VEHICLE TRACE actor= vehicle= pos= speed= heading=` at 2 Hz for probes.

## 5. Risks

- **Water kills.** Crysis infantry drowns in deep water; the exit raycast is mandatory, not cosmetic.
- **Hovercraft on land.** It can drive on land, but the route planner assumes water; if it beaches, switch to the
  land controller only after a confirmed exit.
- **PS claim.** A bought boat is owned for 90 s (`PowerStruggle.lua:350-370`); `CanEnterVehicle` must be the
  authority, not the bot's own team check.
- **No boat on black's side after a wipe.** If all 4 smallboats are destroyed, black must capture the tan naval
  factory or use the east dock by land; do not loop `EnterBoat` on a missing boat.
- **`sv_gamerules`/gate.** Boats are only useful after DS-030 opens PS; the current `LocalIAGate` refuses it.
