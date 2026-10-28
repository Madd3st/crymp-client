# DS-098: Shore lower-deck spawns and route inventory (offline, archive + C149 mesh)

Inputs (read-only): failed owner session `C:/Users/Magic8/WarsBotsBuild/WarsBots-Showcase-b8b444d6/Shore/Sessions/
S150S-20261028-222902` (source `b8b444d6`), its `Shore.navmesh` = E2235993 (C149 candidate, 9,762,176 bytes), native
export `build/c072-ps-export/a943e070/Shore.{spawns,geometry}.json` + `crop/Shore.crop.obj.gz`, and the C149/C146 evidence.
No game, build, install, feature edit or launcher change. Code lines are from `git show b8b444d6:<path>`.

**Коротко по-русски:** игра не падала. Бот №2 получил спавн **внутри подлодки** (z=90.575), а кандидатная сетка Shore
обрезана по плоскости воды 99.5 — там нет ни одного полигона ниже 99.5 (проверено офлайн на её же файле). Бот оказался
`start_off_mesh`, и лаунчер правильно не поднял клиента (fail-closed) и всё восстановил. Ослаблять порог нельзя:
чинить надо выбор спавна (код) и, отдельно и позже, запекание внутренностей корпуса. Цели 64/316/579/654 достижимы от
верхнего US-спавна, 344 — в другом компоненте; нижняя палуба недостижима без линка-лестницы, которого у ботов нет.

## 1. What actually happened (abort, not a crash)

| Evidence | Value |
| --- | --- |
| `session.json` | source `b8b444d6…`, `client_pid: null`, `phase: archived_restored`, server PID 21588 |
| `S150S-…log` | `#2 PS initial spawn team=2 group=537 spawn=545` then `BODY STATE … spawn=545 entity=(2114.500,1887.875,90.575)` |
| same log | t=0.808 `MOVE TRACE … pos=(2114.607,1887.817,90.101) grounded=1 swim=0 waterZ=-1000000.00 relativeWater=5.00 bottomDepth=-1000090.12` |
| same log | `NAV fallback start=(2114.50,1887.88,90.49) goal=(2121.88,1883.00,99.88) … reason=start_off_mesh from=-1 to=-1` |
| `nav-before/` vs `nav-after/` | Shore mesh absent before, installed during, `nav-restored.json`/`memory-restored.json` confirm removal/restore |
| `Multiplayer_PS_Shore-…wbdemo` | 0 bytes (recording started, no client ever joined) |

So the server ran normally, the bot landed on the hull floor (90.10, grounded), the navmesh could not accept its start,
and the launcher's readiness rule (`initial z >= 99.5`, frozen C150 package) refused to start the client. Correct
fail-closed behaviour; nothing to loosen and no random retry.

## 2. Offline inventory on the same C149 mesh (actual core, real Detour)

Built `NavMeshCore` + Detour from `BotNavMeshCore.cpp` (no engine) and queried the archived `Shore.navmesh` and
`Shore.links.txt`:

* `mesh polys=46615 components=3803 links=1` (link mask all kinds 0x3E).
* **Spawns (124 native, `Shore.spawns.json`):** 113 project, **11 off-mesh**. All **12 below z=99.5** (z=90.38) are in
  the submarine hull: 11 fail `Project` even with `down=20 m`; **spawn 530 falsely projects 12.34 m up** onto the upper
  deck (102.71) — a stacked-floor hit, the dangerous half of the same defect.
* **Inside the hull volume** (x 2106–2136, y 1876–1892): `Project` finds **0 polygons at z=90.4 and z=95.0**;
  at z=99.4 everything snaps to 99.51–103.41 (water surface / upper deck).
* **Whole map** (3074 samples, step 20 m): `minSnappedZ=99.51`, **0 samples below 99.5** → this bake is clipped at the
  water plane. Consistent with the C149 bake note (`eed85fb1 water99.5 …`) using a seabed `--zmin` at the water level
  (the tool documents `--zmin` as “sea bed”, README of `Tools/RecastBake`).
* **Geometry is not the problem:** the crop OBJ has 65,890 vertices inside the hull footprint, 44,142 of them below
  z=96 (z range 72.06–106.13). The interior exists in the input and is lost in the bake.
* **Routes from the retained upper US spawn** (`spawn 510`, BODY 2110.500,1882.500,103.450) to the five native capture
  destinations the b8 log itself prints (`PS CAPTURE dest id=… pos=…`):

  | dest | position | path | points | links |
  | --- | --- | --- | --- | --- |
  | 64 | 1960.98, 1387.64, 110.31 | OK | 42 | 1 |
  | 316 | 2082.12, 1284.04, 124.56 | OK | 49 | 1 |
  | 344 | 2133.21, 1482.51, 111.11 | **NO — other_component** | – | – |
  | 579 | 2216.32, 1334.99, 124.51 | OK | 47 | 1 |
  | 654 | 1794.54, 1690.64, 101.53 | OK | 21 | 1 |

  `316` is reachable from the US spawn on this mesh (the old “256-poly prefix” note was from a different start); `344`
  remains the one graph gap among the five. From the lower spawn (545) every path, including to 510, fails
  `start_off_mesh` — the interior is a separate, unbaked island (and there is no link into it).

## 3. Water vs inside-hull (why the 99.5 plane is the wrong classifier)

`MOVE TRACE` prints `SPlayerStats.worldWaterLevel` (`BotMoveTrace.cpp:77`). Inside the hull it is the engine default
sentinel `-1e6`, `bottomDepth -1e6`, `swim=0` → **no water volume at the bot**; the hull is dry even though z=90.1 is
7.4 m below the global sea plane. That is exactly why the frozen launcher rule (`z >= 99.5`) and the bake's seabed clip
are approximations that work at sea and fail inside the hull. They are not wrong for what they were built for; they are
simply not a water test.

## 4. Code gaps at b8 (exact hook points)

| Gap | Where | Why it matters |
| --- | --- | --- |
| Group spawn is not validated against the navmesh or water | `BotManager.cpp:390-401` (C-136 block), `:405-408` position use | `GetSpawnLocation(… group …)` may return any free spawn of group 537, including the 12 below-deck ones; nothing rejects an off-mesh or “false-projected” point |
| IA fallback picker validates only native safety | `BotManager.cpp:300-324` (`IsSpawnLocationSafe`, `TestSpawnLocationWithEnvironment`) | same risk whenever a map has an unmeshed pocket |
| Spawn is created at the raw position | `BotInstance.cpp:40` (`Spawn(position)`) | no post-validation/projection guard; the bot can be born in an unbaked volume |
| Water-agnostic queries | `BotBrain.cpp:899-907` (heard goal: water start bounds but a dry goal box), `:936-944` (roam hotspot/spawn goals, plain `Reachable`), `:1031`, `:1103`, `:1123` (vantage spots/shots), `BotWeapons.cpp:135` (pickup filter) | not the cause here (`start_off_mesh` dominates), but these are the callers that must become water-aware if a spawn can be in water (DS096 items 1-2) |
| No connector for the interior | `BotJumpLink.h:21-22` (`LinkMove` = Drop/Jump/Strength only) | even a perfect interior bake leaves the hull isolated — a ladder/hatch executor is a separate feature, not a spawn fix |

## 5. Minimal candidate fix (proposal; not implemented here)

**A. Spawn-side validation (recommended for the next batch, smallest change, no new subsystem).**
After `groupSpawn` is chosen (`BotManager.cpp:398`) and before `where` is built, validate the spawn entity position with
the loaded `NavMeshCore` (only when a mesh is ready):
1. dry: `Project(pos, snapped, 1.5, 1.0, 3.0)` succeeds **and** `|snapped.z - pos.z| <= 2.5 m` (rejects the 530
   false-projection through the deck);
2. water: otherwise `Project(pos, snapped, WaterQueryBounds().side/up/down, waterOnly=true)` succeeds (a genuine
   swimmer spawn, uses the existing bake water-area check);
3. on failure, ask the engine again with the rejected spawn as `skipId` (`GameRules.h:361` exposes `skipId`), bounded to
   e.g. 8 tries, then log and use the mesh-validated IA list (`ChooseSpawnLocation`) or refuse — never silently accept an
   off-mesh point.
The rule is a pure helper (booleans + z tolerance) with unit tests; the engine call stays in `BotManager`.

**B. Data-side (not for the showcase).** Bake the hull interior (`--zmin` below the hull floor, `--water 99.5` kept for
open water, interior accepted only inside the hull footprint) **and** provide a way out (ladder/hatch executor). Without
the connector the interior is an isolated component; with it, this is a map-play feature, not a spawn fix.

**C. Readiness wording (future).** Replace “initial z >= 99.5” with “spawn projects on the mesh or is a genuine water
start” in the next package generation. The frozen C150 package must stay as it is; do not patch it now.

## 6. Red/green oracle for the batch

* **RED (today, measured):** on E2235993, 11/124 native spawns are off-mesh, 1 false-projects +12.34 m; the live roster
  picked spawn 545 → `start_off_mesh`, no client. No code change needed to reproduce: the archived mesh + spawns are
  enough (the probe in §2).
* **GREEN (target):** with A, the same roster preflight on the same mesh logs a retained spawn whose position projects
  within 2.5 m (upper deck), the first 5 s contain no `NAV fallback … start_off_mesh`, and the offline oracle accepts the
  upper US spawns while rejecting {506, 517, 524, 525, 527, 530, 534, 536, 539, 543, 545, 562}.
* No additional Shore game is requested by this item; the first shared gameplay acceptance stays the C152 batch 1
  weapon/goal run on a new exact green SHA. DS097 is unaffected (the aborted session never reached combat).

## 7. Limits

One archived mesh (E2235993), one live pick, no demo (0 bytes) and no client → no visual/capture/door/vehicle evidence;
`344` stays `other_component`; the lower deck stays unplayable without B; the numbers describe the C149 candidate, not a
new bake.
