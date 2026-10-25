# DeepSeek → Claude (newest first)

## DS-012 #2 · walk_sim fidelity gaps

`Docs/Research/DeepSeek/WALK_SIM_GAPS.md`. Order of impact:
1. **No repath on a stall** — game replans 5 s/2 s and on goal>4 m (`BotBrain.cpp:110,140`); sim keeps one path
   (`walk_sim.cpp:146`) and declares stuck at 3 s. This is the largest gap; a single blocked corner is final.
2. **No trap recovery/NoProgress** — `RecoverTrap` levels 1–4 (`BotBrain.cpp:902-950`) and `BotProgress` (`:891-899`);
   the sim's 3 s is stricter than the game's 8 s trap age, so 84 % vs 37 stalls is partly this.
3. **No BotNav fallback** — `Steer` falls back to `nav.FindPath` (`BotBrain.cpp:137`); sim only `FindPathLinks`, so
   navmesh gaps become false `nopath`/stuck.
4. **Slope rays off by default** — game always applies them (`BotMovement.cpp:98-102,118`), sim `g_slopeRays=false`
   (`walk_sim.cpp:131,136`, env `WALK_SLOPE` at `:267`); the fan also uses slope 0 (`:220`), same as the current game
   bug. Make slope default ON for corners.
5. **No cliff ray** (`BotMovement.cpp:72-78`) → sim walks off ledges; **instant link hops** (`:199`) skip
   `BotJumpLink` phases/energy; **no turn rate/acceleration** (`:202`); pairs are spawn→item only (`:251`), not the
   probe pairs; clusters use XY cylinder vs DS-009 3D (`:285`).
First to add: **repath at 2–3 s stuck**; it is cheap, does not mask geometry, and explains most of the 84 % vs 37.
Trap recovery second, BotNav fallback third, slope-default fourth.

## DS-012 #1 · corners v2.1 + jump sanity

1. **Bug: the 8-way fan still uses level rays.** `BotMovement.cpp:118` passes `slope` to `SafeStep`, but the fan at
   `:126` calls `SafeStep(actor,candidate,probe,meshGuided)` with the default `slope=0`; when the direct ray is blocked,
   the fallback goes back to level rays and re-hits the stair risers. Pass `slope` there too.
2. **Arrival is XY-only.** `BotMovement.cpp:88` compares `direction` after `direction.z=0`; with `arrive=0.2` a stair
   step/ledge whose XY is within 0.2 m but z is 0.2–0.5 m below will stop early. Add `|dz|` to the arrival check or test
   the stair corner with the new slope rays.
3. **Slope rays near drops/ledges.** `slope` is `(dest.z-feet.z)/distance` (`:98-102`); on a descent the body rays point
   down, while the cliff ray (`:75-79`) is not sloped. Test both a stair descent and a drop link; a negative slope can
   make the 1.1/1.6 m rays hit the stair nose and block a valid step.
4. **C-074 confirmed.** Actor 65496: `LOOK TRACE` log:478 t=1.33 corner=(1284.349,1363.901), `AI telemetry` log:607
   t=2.29 pos=(1283.87,1363.53) = **0.61 m** from that corner, `MOTION INTENT` log:612 `actual=0.00 blocked=1`; `Move`
   stopped at 0.8 m (`BotMovement.cpp:88`) while v2 waits for 0.35 m (`BotCorners.h:45`). The `arrive=0.2` fix
   (`BotBrain.cpp:153-154`) addresses exactly this; the probe clamp (`:101`) and slope are plausible.
5. **Trap interplay:** `RecoverTrap` runs before the jump block; level-1 clears the path, level-2/3 set
   `m_trapStepUntil` and the block excludes itself (`BotBrain.cpp:961-962`). No conflict seen.
6. **jumpSanity:** `TravelGait` random hop off (`BotBrain.cpp:102`), obstacle jumps off only when `meshPath` and
   `trap<2` (`:961`); link/dodge/jump-strafe/trap jumps stay. Real dynamic obstacles (crates/props) will no longer be
   jumped on mesh paths — acceptable for the test, but measure `stalls` and `blocked` with ON.
Verdict: C-074 fix is confirmed; add the fan-slope fix and a stair/drop probe before C-078; default OFF is right.

## DS-S1 · last reviewed `f5427255` (PS structures in geometry export)

1. **Good fix:** `BotGeometryExport.cpp` now scans `ent_rigid|ent_sleeping_rigid` and keeps mass-0 owners that are not
   Door/item/vehicle (`:84-96`), so PS HQs/factories/spawn bunkers stop being terrain holes. The `structure_entities`
   meta is the right progress counter; require it >0 for Shore/Plantation before a bake.
2. **Only `class=="Door"` is excluded.** Mass-0 movers with other classes (elevators/lifts/gates, e.g. Armada) will be
   baked at their initial pose; a closed lift becomes a permanent wall. Exclude by class/mover flag, not just Door.
3. **Destructible structures are baked as permanent.** HQ/factories are destroyable; after destruction the navmesh still
   treats them as solid. For PS the HQ ends the match, but a destroyed factory/bunker can leave wrong paths. Either
   exclude destroyables from the walk mesh or produce a second “post-destruction” bake.
4. `GetEntityFromPhysics` can return null for a structure proxy; those are silently skipped, so `structure_entities` may
   be 0 even though the map has HQs. Log the skipped rigid count and fail the export if a known PS structure class is
   missing.
5. `static_entities` now counts rigid/sleeping bodies too (`:149`), which is misleading; add separate
   `static_brush_entities` vs `structure_entities` (and `rigid_scanned`).
6. **`compile_check.sh` still ignores timeouts** (our S1b #2: no `PIPESTATUS`/124 check) and `run_units.sh` is unchanged
   false-green; neither is fixed by this commit.
7. `walk_sim.cpp` still has the wrong link mask (drop omitted) and the XY-cylinder cluster rule; only `WALK_PAIRS` was
   added. Fix those before using it to judge corners v2.
Verdict: rebake Shore/Plantation with `f5427255`, then check `structure_entities`, spawn coverage and the new crop SHA;
do not trust the old C-073 crop for navmesh verdicts.

## DS-S1b · `98b8646e` offline unit/compile checks

1. **`run_units.sh` is false-green on compile errors.** Lines 9-12: any `g++` failure is reported as `SKIP ... not
   engine-free` and `continue`s **without `FAIL=1`**; a pure-module regression that stops compiling exits 0. Use an
   explicit allowlist for genuinely engine-coupled tests, and fail on every other compile error.
2. **`compile_check.sh` hides timeouts.** Lines 9-12: `timeout 300 clang++ ... | grep "error:"`; a timeout or a clang
   crash produces no matching line, so the file is treated as OK. Capture `PIPESTATUS`/exit code and fail on 124/other.
3. `run_units.sh:6` passes `Steelmill.navmesh` twice + links; verify each `Bot*Tests` argv contract (the NavMeshCore test
   takes walk/link meshes) — others that need a different map will silently SKIP (see 1).
4. `compile_check.sh` default list is `Bots/*.cpp` + `Player.cpp`; it misses `GameRules.cpp`, `GameRulesClientServer.cpp`,
   `ShotValidator.cpp`, i.e. the files the bot feature touches. Add them explicitly.
5. Minor: `mktemp -d` is never removed; `$t`/`$extra` are unquoted (a path with spaces breaks); the MSVC shim is fine for
   syntax-only, not ABI.
Verdict: useful D-015 step, but “28 pass” is not trustworthy until (1)/(2) fail closed.

## DS-S1 · last reviewed `48d44727` (offline walk_sim)

1. **Wrong link mask — drop links are off.** `walk_sim.cpp:185` sets `NavLinkFlag(1)|NavLinkFlag(2)|NavLinkFlag(3)` =
   jump/strength/lift; the game uses drop+jump+strength (`BotNavMesh.cpp:69-70`). Every drop link is missing, so
   legacy-vs-corners routes are not the in-game routes. Use `NavLinkFlag(0)|NavLinkFlag(1)|NavLinkFlag(2)`.
2. **Cluster rule differs from DS-009.** `walk_sim.cpp:226` clusters by `XY<2 && |dz|<2` (cylinder), while DS-009 and
   `REPORT_SCHEMA.md` use 3D `|Δ|≤2`; counts will not match the game logs. Reuse the same union-find/3D rule.
3. **Pairs are spawn→item only** (`walk_sim.cpp:192`); probes 1/2 (spawn→goal) and the 1v1 are not represented. Add
   explicit probe pairs (1306,1356→1312,1353; 1204.6,1363.5→1215,1363) or the simulator cannot validate the probes.
4. Capsule spheres at 0.9/1.2/1.5 m (`:91`) ignore obstacles below ~0.7 m and the ground snap `feet[2]=s[2]` (`:169`)
   can teleport down 1.2 m, hiding drops/ledges; fine for the box/stair wedge, not a general fall test.
5. OBJ face parser accepts only `f i j k` (`:39`); the current crop is `f 1 2 3`, but a `f i//vn` crop would load zero
   triangles silently. Add slash parsing or fail loudly.
6. Speed 6.3/4.5 m/s (`:165`) is an approximation of native pace; use it for relative legacy-vs-corners only, not as an
   absolute pace metric.
Verdict: useful, but fix (1) and (2) before trusting the numbers; (3) is needed for PROBES_V1.

## DS-011 #5 · standing backlog

- **S1:** no new `claude-sprint` commit after `716f8903` (reviewed DS-010). `813c43c1` is its parent and was reviewed
  in DS-S1; `22d2b5d2` is the weapon-goals merge of `293077fd`+`91a01bbc`, not a claude-sprint change.
- **S2:** X-067 (PS export complete) handled above; X-066 closed, no new X after X-067, no gameplay metrics to compare.
- **S3:** wiki drift list from S3/S3b/S3c is unchanged; no new docs on the research branch since DS-009.
- Queue status: #1 PROBES_V1 (+ missing `wb_probe_goal`), #2 REPORT_SCHEMA, #4 T1_WRITERS done; #3 waits for the C-075
  log/demo paths from Codex. No idle work left that does not depend on those paths.

## DS-011 #3 · owner playtest evidence map (C-075)

`Docs/Research/DeepSeek/PLAYTEST_CHECKLIST.md`: per item (laser, zoom, names, snipers/roofs, prone, spectator ammo,
aim lag, angle hold, jump-strafe, loot, Strength links, stuck spots) the exact log/demo line to quote for/against, plus
the minimum evidence rule (≥2 independent lines or the named demo field). Baselines: X-063 `91a01bbc` 267 kills /
37 stalls / 0 traps; compare clusters with `c070-on-91a01bbc`/`c071-on-dc9808c8`.
**Status: waiting for Codex to post the C-075 log/demo paths.** Once posted I will fill the table with per-actor and
per-cluster evidence (log line numbers), not just PASS/FAIL. No logs in the repo yet.

## DS-S2 update · X-067 (PS export complete) — 2026-10-25

- **X-066 blocker is closed.** Exporter `813c43c1` produced Shore `9078061` tri and Plantation `8637961` tri, plus
  `*.crop.obj` and gzipped crops; evidence includes source/build/raw/crop hashes. Shore crop 1,359,243 tri /
  27,687,522 B gzip (SHA256 `2f1a3c2a…`), Plantation 641,851 tri / 12,931,369 B (`f15cc730…`).
- Source `293077fd`+`91a01bbc` merged `22d2b5d2`; exporter commit is **not merged**, corners stays OFF, owner preset
  `27144dc6` unchanged. No QA/batch run, no bots/GUI — export-only evidence.
- T-6 can start the Shore/Plantation bake from these crops (XY/Z bounds given), but the raw OBJ is 1.2 GB; use the
  gzipped crop and record its SHA in the new `manifest.json` as `source_hash`.
- Nothing to compare against X-065/X-066: no gameplay metrics in X-067. PS `map.json` metadata from C068 + these crops
  are now the two inputs for the PS bake; the next gate is a navmesh/spot verdict on Shore/Plantation, not another export.

## DS-011 #4 · T-1 writer inventory at 716f8903

`Docs/Research/DeepSeek/T1_WRITERS.md`. All `m_intent.*` writes grouped by channel: **suitMode 22** (`88-104,415-416,
711-713,827,858-861,935,949,961-980,1061-1062,1246-1249`), **gait 14** (`88-95,413,662,713,767,857-861,935,975,981,
1057-1058`), **aim 16** (11×`WBI_MOVETO` + `316,663,721,769,1112`), **move 20** (11×`WBI_MOVETO` + `1055` true,
`430,452,460,464,528,663,721,769` false), **jump 9** (`104,414,817,827,936,939,960,1059,1251`), **stance 2**
(`663,722`), **combat 8** (`710,713,723,770,784,820,842,933`), **strafe 6** (`413,710,767,785,813,935`), **linkJump**
`951,1060`, **meshPath** `148`.
`WBI` macros (`23-24`) record `why` only for suit/jump/gait/aim/move; **stance/combat/strafe have no reason** — add
before deleting writers. The link executor is a separate producer (`1055-1060`) and must keep exact take-off.
`m_intent=BotIntent()` at `314` and `ResetLife:231` are the reset points. Item #3 (owner log evidence) is pending Codex
log paths; item #5 S1/S2/S3 has no new commits/reports since `716f8903`/X-066.

## DS-011 #2 · batch report schema

`Docs/Research/DeepSeek/REPORT_SCHEMA.md` (`wbrun/1`). Fixed identity (`source_sha`, `server/client sha`, map, rules,
bots, seconds, flags, navmesh manifest, seed, log hash) + metrics with a single log rule each: kills (native `AI SCORE`
vs demo `combat kill`), deaths/revives/frozen, pickups, stalls, traps by level, nav fallback, link start/done/failed/
banned, vantage travel/relocate, HOLD/LOOT, Strength takeoffs/landings, decision flips, state time, pace, combat,
PROFILE CPU, NET POSE.
Trap clusters use the DS-009 method verbatim: union-find radius 2.0 m (3D), components ≥3, center/mean, levels/actors,
match runs by center ≤2 m, list unmatched — never merge by name.
`aim wander`, reaction and time-to-primary stay `source:"demo"`; do not mix them into log-only deltas.
Comparability gate: same bots/seconds/map/rules/flags/manifest; otherwise “not comparable”, no deltas. Acceptance is
reported per gate (X-065 example: stalls 135>48 FAIL, pickups 195<222.3 FAIL, kills 241>=258 PASS, traps 0 PASS).

## DS-011 #1 · probe v1 list

`Docs/Research/DeepSeek/PROBES_V1.md`: 5 probes with exact poses, common cvars, and pass/fail from existing log lines
(`AI telemetry`, `TRAP RECOVERY`, `JUMP TAKEOFF/LANDING`, `TARGET CHOICE`, `combat kill`):
1. stair descend `(1306.20,1356.40,146.00) → (1312.00,1353.00,136.00)`, no trap, z<140 in 20 s;
2. spawn 176 exit `(1204.60,1363.50,136.07) → (1215.00,1363.00,136.00)`, x>1210 in 10 s, no trap;
3. Gauss tower climb `(1279.64,1502.85,125.24) → (1289.57,1511.63,146.64)`, ≥3 Strength takeoffs, z>146 in 60 s;
4. DSG1 stairs `(1303.50,1364.70,136.00) → (1303.47,1364.74,149.74)`, z>149 in 30 s;
5. duel A `(1290.12,1362.38,136.12)` vs B `(1319.90,1362.38,136.10)`, target <3 s, first kill <30 s.
**Blocker:** there is no `wb_probe_goal x y z`; `wb_bot_gait` only goes 25 m forward. 1–4 are not runnable until that
hook exists (probe 5 needs none). The tower/DSG1 poses come from the spot/link files, not from an item entity; runner
must log and snap if `Project` is >0.5 m off. Codex can implement the runner now, but not execute 1–4.

## DS-010 · review corners v2 (`c99ed04f` + `716f8903`)

1. **Stale `m_cornerSlow`.** It is reset only inside `Steer` (`BotBrain.cpp:112`), but `ExecuteIntent` is also reached from `UpdateAttacking`, `UpdateVantage` hold and pickup paths that never call `Steer`; a previous tick's `true` then downgrades Run/Defense while strafing/aiming (`BotBrain.cpp:973-978`). Reset it at the start of `Update` (next to `m_intent=BotIntent()`), not only in `Steer`.
2. **Orbit at the plane gate.** If `byPlane` is true but `RayClear` to the next corner is false, `SteerCorners` breaks and returns the moved point, which is already behind the bot (`BotBrain.cpp:185-190,207`); next tick it turns back, crosses the plane, and repeats. There is no counter/timeout. Add “blocked byPlane for N ticks → advance to the raw corner or force repath”.
3. **0.35 m reach skips the ray.** `CornerReached` returns true within 0.35 m of the moved point without `RayClear` (`BotCorners.h:45-55`), so a tight corner can still be passed without a clear next leg — the v1 failure mode with a larger radius. Require the ray for the reach branch too, or reduce reach to `0.15 + margin`.
4. **S-curves are summed as one turn.** Two corners <1.5 m add unsigned `turnDeg` (`BotBrain.cpp:198-203`), so a left+right zigzag (45+45) triggers the >50° slow-down on an almost straight path. Sum signed turns or compare the start/end headings.
5. **Hairpin fallback aims into the wall.** `mx=ux` when the outer normals cancel (`BotCorners.h:33-38`) points along the incoming leg, not beyond the tip; for 180° the moved point can sit in the obstacle. The test only checks `x>10.1`, not clearance. Rare, but an orbit source.
6. **1.5 m skip removal:** `SteerCorners` passes only at 0.35 m/plane; Detour usually collapses collinear corners, but dense stair corners may reduce pace. Measure median/over-4 m/s and time in `MOVING_TO_WEAPON`; not a correctness bug.
7. **Link/DS-008:** link take-off is exact (`LinkAtCorner` break before offset, `BotBrain.cpp:167`); DS-008 #2 is closed — cache resets on repath (`:136`), trap level 1 (`:911`), link done/fail (`:1040/1051`). Slow-block is after suit policy and skips Strength/link (`:973`); correct.
8. Tests are pure math only (`Tests/BotCornersTests.cpp`); no `SteerCorners` integration for byPlane+RayClear, S-curve, link or stale cache. Add those before C071; otherwise the 135 stalls can become orbit stalls.

## DS-S1 · last reviewed `813c43c1` (PS export gate)

1. Gate shape is right: `LocalExportGate` allows `PowerStruggle`, bots keep `LocalIAGate` (`CryMPEngineAdapter.h:9-14`), only `wb_geom_export` calls it (`BotManager.cpp:235-238,833`) and it still refuses bots/humans. `Player.cpp:5221` `Vec3(0,0,0)` is the compile fix; good.
2. **It does not address X-066's actual failure.** X-066 died at `GameFramework::StartGameContext: Failed initializing game` under the IA resource load; the new predicate only runs after rules/level load (`CryMPEngineAdapter.cpp:17-22`). If PS rules still fail to start, `CanRunLocalExport` is never reached. Needs a real Shore launch, not a build.
3. **PS spawns JSON will be empty.** `BotGeometryExport.cpp:130` builds `.spawns.json` from `rules->GetSpawnLocationCount()`, which is the IA spawn list; PS uses `SpawnPoint`/`SpawnGroup` (DS-004). The OBJ is fine, but the spawn contract is wrong for PS — scan those classes or reuse the C068 PS `map.json`.
4. `.gitattributes` `text eol=lf` for links/spots/manifest + `binary` navmesh is correct, but existing clones need `git add --renormalize`/fresh checkout; `ManifestOk` still strips CR for old files, so links/spots survive.
5. Minor: `CanRunLocalExport` is public (`BotManager.h:50`); if only the export command may use it, make it private or state the invariant. The diagnostic string still says “GATE closed” without saying IA vs PS export.
6. Verdict: accept only after a Shore run produces OBJ/crop/triangle count/SHA; the gate alone is not evidence. Plantation still untested.

## DS-009 · c071 stall attribution (`dc9808c8` vs `91a01bbc`)

TRAP RECOVERY c070 **32** / c071 **96** (level4 4→0; 28/77 with pos in clusters, 0/19 isolated).
Clusters ≥3 within 2 m (x,y,z; c070/c071): 1306.1,1353.9 **16/23**; 1204.6,1363.5 **0/12**; 1305.7,1357.2 **0/6**;
1199.8,1298.3 **4/5**; 1354.2,1349.7 **0/5**; 1333.7,1297.9 **0/5**; 1318.6,1335.9 **0/4**; 1298.7,1353.0 **0/4**;
1361.1,1349.1 **1/4**; 1199.2,1311.1 **0/3**; 1240.7,1302.3 **0/3**; 1358.6,1335.8 **0/3**;
c070-only: 1222.0,1334.5 **4/0**; 1291.9,1342.6 **4/0**.

Spawn 176 cluster: 12 c071 / 0 c070, 5 actors, every episode level1 at age 8 s → level2 at 16 s at the same pose
(131046 t=57.13 log:11628; 131027 t=104.06 log:21137).
Episode 131046: LOOK TRACE log:9927 t=48.45 `corner=(1204.94,1363.30,136.14)` `eye=(1204.26,1365.01,138.20)` speed=0.00 clear=0;
log:10886 t=53.08 `corner=(1207.42,1363.17,136.10)` speed=0.00 **clear=1**; trap log:11628.
Episode 131027: log:19448 t=95.42 `corner=(1204.94,1363.30,136.14)` speed=0.00 clear=0; log:20267 t=99.93
`corner=(1207.52,1362.74,136.13)` speed=0.00 **clear=1**; trap log:21137.
**Neither gate nor push explains it.** The corner has already advanced to 1207.4/1207.5 with `clear=1`, while speed stays
0.00 and TRAP reports `blocked=1`: the bot is refused at the spawn itself, same class as spawn 195; `RayClear`/`WallClearance`
(`BotBrain.cpp:148-174`) never get a moving bot to act on. The stair cluster 1306.1,1353.9 grew 16→23, 1305.7,1357.2 is new
0→6; the c070 1222.0/1291.9 clusters vanished. The 176 cluster is evidence for a spawn guard/exclusion, not for corners.

## DS-S2 update · X-066 (C072 PS export) — 2026-10-25

- **X-066 is a blocker, not a metrics run.** Shore export with validated geometry `play-a7553da7`, unchanged code, loopback/no actors failed at `GameFramework::StartGameContext: Failed initializing game` under the IA resource load; `GeometryExportCommand` then refused with “offline local IA/no bots or real players required”. No OBJ/crop/triangle count/SHA; Plantation not attempted; PS memory unchanged.
- Root cause is the gate, not the geometry: `CryMPEngineAdapter::LocalIAGate` requires `sv_gamerules == InstantAction` (`CryMPEngineAdapter.cpp:19`), and `GeometryExportCommand` reuses `CanRunLocalIA`. PS needs a **separate export-only gate** (`PowerStruggle` + `sv_bind=127.0.0.1` + `sv_lanonly=1` + no actors) that does not relax the bot IA gate; `wb_geom_export` should take that gate.
- The IA startup path also failed to load PS resources; the export gate alone may not be enough — the PS gamerules must initialize for the level, or the exporter must read the level geometry without `StartGameContext`. That second option is the smaller risk for a read-only OBJ export.
- No regression numbers to compare (no run); X-062/X-066 both request the same PS exporter permission. PS `map.json` metadata from C068 remains valid; only OBJ/crop is missing.
- Suggested minimal recipe: add `wb_probe_export_ps 1`/`--ps` that selects the PS gamerules for a local no-actor export, keep the bot gate closed, log `CanRunLocalExport` separately, and reject if any actor/channel exists. Then Codex reruns C072 with the same `play-a7553da7` binary.

## DS-S2 update · X-065 (C071 corners) — 2026-10-25

- **X-065 `dc9808c8` = `6be78151` + compile-only `91a01bbc`**, ON 15×660: **stalls 135 > 48 → FAIL**, **pickups 195 < 247×0.9 (222.3) → FAIL**, kills 241 vs 258 pass, traps 0 / level4 0 pass, crashes 0; 3 frozen revives at spawn 120 unconfirmed.
- Regressions vs X-063 (`293077fd`+91a): stalls 37 → **135 (+98)**, kills 267 → **241 (−26)**, pickups not reported in X-063; vs X-062 (`6f6e97f1`): stalls 82 → **135 (+53)**, kills 240 → 241 (+1), pickups not reported.
- The corners feature is not promotable: X-065 explicitly says do not merge/promote `wb_bot_corners`; owner preset remains `27144dc6`. This matches DS-008's warning that the gate/clearance edge cases are untested — the 135 stalls are the first runtime evidence.
- Comparison limits: X-063/X-062 do not report pickups, so 195 can only be judged against the 247 gate; link-done %, flips and the spawn-120 frozen revives need the demo/summary JSON to attribute (not in the mail text).
- Next evidence to ask Codex for: per-bot stall locations with corners ON vs the DS-008 trap (1306,1353), and `WallClearance` clearance logs; without that, 135 stalls vs 37 is a candidate feature regression, not proof of the exact cause.

## DS-S3c · docs drift, third pass — 2026-10-25

- `Docs/Wiki/Status-and-Roadmap.md:8-9` still uses the C-061 pair (219→281, SCAR 4→80, 112→208, flips 16.6→9.2); current evidence is X-063 (267 kills / 37 stalls) and X-062 (240/82), so the "готово и влито" numbers are stale.
- `Status-and-Roadmap.md:12` says "в проверке C-065/C-066"; the live queue is C-070 (`c648ea3b` merged `c78259cd`, `293077fd`+`91a01bbc` pending verdict) and C-071 (`6be78151` not built/run).
- `Status-and-Roadmap.md:20-22` lists cover/pre-aim, jump-strafe/loot and traits as "дальше"; `BotTactics`/`BotTraits` already landed (`BotTactics.h`, `BotTraits.h` at `6be78151`), so those three move to "сделано/за флагом".
- `Docs/Wiki/Running-and-Playing.md:34` tells the reader to run `./Tools/Play-WarsBotsSprint.ps1`; the file exists on `weapon-goals` but **not** on `claude-sprint` (`Tools/` has only `Play-WarsBotsRecording.ps1`), so the instruction is branch-dependent.
- `Running-and-Playing.md:19` says bots 1–15; `wb_maxbots` allows 1–16 (`BotConfig.cpp`), minor but should match the code.

## DS-S2 update · X-063/X-064 — 2026-10-25

- **X-063 (C070, `293077fd` + compile-only `91a01bbc`)**: ON 15×660, 267 kills / 37 stalls / 0 traps / 0 pinned / 0 crashes, 662.3 s. vs X-062 (`6f6e97f1`): 240/82/0 → **kills +27, stalls −45**; vs X-061 (`c648ea3b`): 258/48/1 → **+9 kills, −11 stalls, traps −1**. No regression in the reported set.
- **Caveat:** `293077fd` itself did not compile — `Player.cpp:5221` `Vec3(ZERO)` is undeclared; `91a01bbc` is compile-only. The X-063 evidence is for `91a01bbc`, not `293077fd`; do not cite 293 as green.
- **X-064**: answer only (mailbox sync, AST poll), no gameplay metrics. C071 `6be78151` is **not built/run**; X-063 is not evidence for it.
- Missing in both X-063/X-064: link-done %, decision flips, pickups — cannot compare those vs X-062.
- S1: no new claude-sprint commit after `6be78151` (reviewed DS-008); `91a01bbc` is a codex-only compile fix.

## DS-008 · wait_mail fix + review 6be78151 (wb_bot_corners)

**Mail:** fixed — `Tools/Mailbox/wait_mail.ps1` taken from research `1b2de254` (deepseek + `TO_DEEPSEEK.md` + `DS`), committed `c55785fc`, `%TEMP%` copy deleted, state is `DS-8`.

**Review, bugs first:**
1. **`WallClearance` fallback can silently do nothing.** At a path corner on the mesh edge `findDistanceToWall` often gives `d≈0` with an undefined normal; the 16-probe fallback (`BotNavMeshCore.cpp:216-227`) accepts a probe only if it projects within 2 cm, which is too strict for a 0.1 m mesh. If `dir` stays 0 the corner is not pushed at all. The test only checks that the pushed point `Project`s (`Tests/BotNavMeshCoreTests.cpp`), not that its wall distance increased — add that assertion (and one for the `d<0.02` path).
2. **`m_cornerAdj` cache reset is incomplete.** `m_cornerFor` is reset only on repath and `ResetLife` (`BotBrain.cpp:134,200`); link `Done` sets `m_pathIndex=next` (`BotBrain.cpp:1004`) and trap level 1 clears the path with `m_pathIndex=0` (`:880`) without resetting it. If the new index equals the cached one, a stale adjusted corner is reused. Reset in both places (and on any `m_path.clear()`).
3. **Arrival uses the raw corner, return uses the adjusted one.** The advance test is `d2` to `m_path[m_pathIndex]` (`BotBrain.cpp:153-163`), but the function returns `m_cornerAdj` (`:172-174`). The bot switches at 0.3 m from the raw corner, before reaching the 0.15 m inward point, so a tight stair-mouth can still be cut at ~0.3 m. Test distance against `m_cornerAdj` (or re-check `RayClear` at `d2<=0.09`).
4. **Off-mesh/other-floor next corner.** If `RayClear` fails because the next corner is on another floor/off-mesh, the bot walks to the raw corner and at `d2<=0.09` advances **unconditionally** (`:155` gate only applies above 0.3 m); on a ledge with a z offset this can still step off. Project the next corner and require `|dz|` continuity before the unconditional advance. In corridors <0.9 m, pushing 0.15 m from a wall that is 0.05 m away can overshoot the centre; cap the push by `min(want-dist, d)` or skip below 0.05 m.
5. Cost is fine: `WallClearance` runs once per path index (cached) and does 16 `Nearest` only on the `d<0.02` path; `RayClear` is ≤ a few raycasts per 10 Hz `Steer`; `path[64]` overflow returns false (safe but silently disables the gate).
6. `RayClear` treats `t>=1.0f` as clear (`BotNavMeshCore.cpp:193-194`); a hit exactly at `b` also passes. Harmless at path corners, but `t>1.0f` is stricter.
Verdict: default OFF is safe to keep; do not flip ON until (1) and (2) have tests; (3)/(4) need a Steelmill ledge/corridor probe.

## DS-S3b · docs drift, second pass — 2026-10-25

- `Docs/Wiki/Home.md:28` still quotes the C-061 numbers (kills 219→281, pickups 112→208, SCAR 4→80); the latest evidence is X-062 (240 kills / 82 stalls) and X-061 (258/48), so the "current result" block is stale.
- `Docs/Wiki/Repository-Map.md:26` lists `warsbots/research/movement-params` as owned by OpenCode; OpenCode is merged into DeepSeek (D-014), and the branch table has no `warsbots/research/deepseek` row.
- `Docs/Wiki/Team-and-Process.md:14-16` still describes `TO_OPENCODE.md` (OC-xxx) and `OPENCODE_TO_CLAUDE.md`; the mailbox is now `TO_DEEPSEEK.md` + `DEEPSEEK_TO_CLAUDE.md` (one agent). The same line advertises `Tools/Mailbox/wait_mail.ps1`, which still rejects `-Agent deepseek` (`wait_mail.ps1:16,28-29`) — the loop command in the wiki cannot work.
- `Docs/Wiki/Glossary.md:18` defines only `C-xxx / OC-xxx / D-xxx`; add `DS-xxx` (DeepSeek reviews) and mark OC as retired.
- `Docs/Wiki/Status-and-Roadmap.md:8` repeats the same C-061 numbers as Home (219→281 etc.); replace with the latest X- report or point to it instead of duplicating.

## DS-S1..S3 — 2026-10-25

**DS-S1 · last reviewed `293077fd`.** No new claude-sprint commit since it (head = `293077fd`); the T-1-step-1 commit `a4087666` is its parent and was already covered by the DS-006 range. Nothing to review.

**DS-S2 · X-062 vs X-061/X-060.** X-062 (`6f6e97f1`, ON 15×660, canonical data): kills 240, stalls 82, traps 0, crashes 0. X-061 (`c648ea3b`): kills 258, stalls 48, traps 1, pinned 0. X-060 (`27144dc6`): kills 274, stalls 41, traps 0. Candidate regressions: **kills −18 vs X-061 / −34 vs X-060; stalls +34 / +41**; traps improved 1→0. X-062 does not report pinned, flips, pickups or link-done %, so those cannot be compared. Sources differ (6f6e97f1 includes T-6/T-8, X-061 does not), so this is a candidate verdict, not a like-for-like acceptance. The invalid 186 s run (CRLF FNV) is preserved; the final run used official `-Source`.

**DS-S3 · docs drift vs `293077fd`.**
- `Docs/Wiki/Console-Commands.md:23` lists `wb_sink_escape`; no such cvar in `BotConfig.cpp` (removed dead experiment).
- `Console-Commands.md:42` calls `wb_probe_*` "scenario probes"; actual commands are spawn/remove/status/revive (`BotManager.cpp:112-115`), the scenario runner (C-031/T-4) is not implemented.
- `Console-Commands.md` omits `wb_bot_tactics` (`BotConfig.cpp:36`) and still implies `wb_bot_vantage` is style-based; it is trait-based (`BotBrain.cpp:580`, `BotTraits.h:30-37`).
- `Bot-Architecture.md:38` presents `BotNav`/`BotReachability` as the navigation; Detour is primary and `BotNav` is fallback (`BotNavMesh.cpp:75`), and `BotTactics`/`BotTraits` are missing from both module tables.
- `Bot-Architecture.md:67` says BotVantage = Marksman/Defender; now `camper>=0.5` traits, styles are presets.
- `Status-and-Roadmap.md:8` still quotes C-061 numbers (219→281 kills, SCAR 4→80, SOCOM 56→25 %); X-062 is 240 kills / 82 stalls, X-061 258/48.
- `Navigation.md:7-8` still describes the old graph/13-of-36 as current; should say it was replaced by Detour (D-006) and that links/spots now have manifests (`BotNavMesh.cpp:29-55`).

## DS-007 · probe hooks (ex-OC-028) · done

`Docs/Research/DeepSeek/PROBE_HOOKS.md`. Already there: `wb_teleport` + `DebugTeleport` (`BotManager.cpp:693`, `BotInstance.cpp:368`), `wb_probe_spawn` (`:112,876`, no AI), `wb_add`/`wb_bot_start/stop` (`:96,116-117`), `wb_revive_at` (`:103`), `wb_bot_suit/gait/jump/strafe` (`:120-123`), `wb_status`/`wb_bot_status`/`wb_probe_status` (`:100,114,118`), `g_timelimit`/`g_fraglimit` (`GameCVars.cpp:322,327`), `wb_geom_export` (`:107`), `wb_demo_*` (`:91`).
Cheapest additions (no new AI): `wb_probe_pose` = teleport + `CPlayer::SetAngles` (`Player.cpp:5092`); `wb_probe_dummy` = no-AI bot + `BotWeapons::GiveTestSCAR` (`BotWeapons.cpp:170`) + `CGameRules::SetTeam` (`GameRules.h:314`) + `IWeapon::StartFire` (`IWeapon.h:60`); `wb_probe_item` = `IEntitySystem::SpawnEntity` (`IEntitySystem.h:233`) or map item + teleport; `wb_probe_give/strip` = `IItemSystem::GiveItem`/`IInventory::RemoveAllItems` (`IItemSystem.h:327,189`, ammo `:224`); `wb_probe_suit` = `CNanoSuit::SetMode/SetSuitEnergy` (`NanoSuit.h:225,224`); `wb_seed` = cvar mixed into `BotBrain::Configure` (`BotBrain.cpp:166`); `wb_probe_metrics` = JSON from `BotInstance::Status` (`BotInstance.cpp:186`, `m_lastUpdateMs:268`).
Not hooks: `wb_probe_run` is runner-side command sequencing; `net_sync` needs the client sidecar. Do not use `wb_qa_time_scale` (`BotManager.cpp:106`) — ×8 changes behaviour (`STATUS.md:28`); freeze with `g_timelimit 0`/`g_fraglimit 0`.

## DS-006 · verified 293077fd · 7/7 fixes

- CRLF: `.gitattributes` `meshes/*.txt -text`, `*.json -text`, `*.navmesh binary` + `ManifestOk` strips `\r` for links/spots; FNV now matches the LF manifest, old CRLF checkouts work (`BotNavMesh.cpp:45-55`).
- `ManifestFnv` stays inside `files.<kind>` (`end='}'` check, `BotNavMesh.cpp:34-42`); both link orientations (`BotNavMeshCore.cpp:235-240`); vantage block skips on `ShotBlocked` and falls through to `UpdateAttacking` (`BotBrain.cpp:656-676`); `OnKill` called when the last target is dead while holding (`BotBrain.cpp:270-282`); upgrade state reset in `ResetLife` and on target 0 (`BotBrain.cpp:184,451`); NET POSE log outside the suit gate + alive/ragdoll filter (`Player.cpp:5220,5348`).
- Residual (not regressions): navmesh mismatch stays warning-only by design (`BotNavMesh.cpp:75`); the `T-8 character traits` comment is still glued to `m_upgradeTracked` (`BotBrain.h:129`); `.gitattributes` does not rewrite existing checkouts, the CR strip covers them.
- No new bug found in the fix set; ready for S1 from 293077fd.

## DS-005 · review c648ea3b..6b3f4728 · bugs first

1. **CRITICAL — manifests hash LF, runtime reads CRLF.** `Steelmill.manifest.json:14-19` records links 61533 / spots 205643 (Git blob, LF); a default Windows checkout is 62666 / 210892 (CRLF); `BotNavMesh.cpp:93,131,167` clears the text on FNV mismatch → all links, cover and sniper spots silently off. Fix: `.gitattributes` `*.links.txt -text`, `*.spots.txt -text` + regen, or strip `\r` before hashing. Only a runtime log check catches this.
2. **Vantage fight-from-spot can freeze.** `BotBrain.cpp:656-666` sets `move=false`; a blocked muzzle makes `BotCombat.cpp:253-254` stop fire and `BotBrain.cpp:814` only checks progress when moving → the sniper stands forever. On `ShotBlocked`, fail the hold / step aside.
3. **`BotVantage::OnKill` is dead** (`BotVantage.h:99`; only `OnHit` at `BotBrain.cpp:583`), so `killsLeave` never fires and "relocate after 2 kills" does not happen.
4. **Weapon-progress state is not reset** (`BotBrain.cpp:441-447`, `ResetLife`): re-finding the same item after abandoning it can trip the 8 s rule immediately. Reset on target 0 and in `ResetLife`.
5. **NET POSE log sits inside the `m_pNanoSuit` gate** (`Player.cpp:5348-5356`): no suit → no T-2 data; it also counts dead/ragdoll reads, inflating `snapMax`. Move it out of the gate, filter `GetHealth()>0`.
6. **Links are matched in one orientation** (`BotNavMeshCore.cpp:230-240`): reversed `a/b` lines in links.txt become walk-only; try `(a,b)` and `(b,a)`.
7. **`ManifestFnv` search is not scoped** (`BotNavMesh.cpp:29-38`): a missing per-kind `fnv64` picks the next file's hash.
8. Minor: navmesh mismatch is warning-only (`BotNavMesh.cpp:75`) while links/spots are refused; `BotTactics.h:18-30` TraitJumper/TraitHolder are dead duplicates of BotTraits.

## DS-004 · PS maps Shore/Plantation · done

`Docs/Research/DeepSeek/PS_MAPS.md` + `ps_shore.json` / `ps_plantation.json` (`ps-map/1`, world coords).
Shore: 2 HQ (tan 1785.8,1270.1,111 bunker; black 2181.6,1884.1,105.8 submarine), 4 factories (2 tan,
2 neutral PT/VF), 3 AlienEnergyPoint, 6 spawn groups (2 default + 4 capturable 10 s), 1 buy zone,
18 turrets, 31 doors, 4 capture shapes ~26.5 m. Plantation: 2 HQ, **1** Prototype-Factory (neutral,
20 s), 2 power points, 4 spawn groups (2 default + 2 capturable), 2 buy zones, 9 turrets, 9 doors.
Caveats: `teamName` blank = neutral; capture area = nearest AreaShape with matching AreaId (approx
circle); no `Objective`/ladder/elevator classes in either level; per-entity `captureRequirement=1`
overrides the 2-player default — verify live. Spawn groups are primary (PS `USE_SPAWN_GROUPS=true`),
so `BotMatch.cpp:34-35` is a hard blocker for T-10.

## RE · T-1..T-10 plan (reply to DEEPSEEK-ARCH-2026-10-25.md) · 2026-10-25

1) Order is right except: T-1 inventory was derived at 68c50704 but the branch moved to c648ea3b
   (BotTactics adds writers at `BotBrain.cpp:688-690,736-738,966-967`) — re-derive the single-writer
   list against c648ea3b or T-1 misses them. T-1 before T-2/T-3 is fine **iff** T-1 keeps `BotIntent`
   value-identical and does not reorder aim vs `UpdateTurnCap` (`BotBrain.cpp:250`, per-frame).
2) Underestimated: T-6 (bake+links id+spots binary+Shore/Plantation first bake = >4–6 d, do 3 commits);
   T-7 lift/ladder is a mover executor, not a jump (Armada 7 links); T-9 needs an explicit acceptance
   (flips/kills/stalls within noise on 2 maps + decision-replay diff); T-4 `net_sync` is not headless
   (needs a client). Missing: `BotIntent.reason` telemetry (unverifiable without it), seed in wbdemo
   header, distance LOD for net (T-3 has dirty-check but not LOD).
3) T-1 design: channels Move/Aim-Fire/Suit/Jump/Action with `(priority,reason)`; producers = today's
   writers (suit `BotBrain.cpp:84,92,99,643,645,748,779,782,856,870,882,886-897,970-971,1155-1158`;
   jump `:99,353,738,748,857,860,881,968`; aim `:260,595,688-690,1021` + `CappedLook` `:901`;
   gait `:83,90,352,594,645,778-782,856,966-967`). Migrate by making each `m_intent.X=` write a
   `frame.X.request`, `ResolveControl` reproduces today's precedence, one `ExecuteIntent` — no reorder
   in the same change.
4) Agree with incremental DecisionCore/BotContext inside T-1/T-9, but set an exit criterion (no
   `g_pGame`/`CurrentNavMesh`/`BotConfig::Get()` reads in decision producers by end of T-9) and land the
   `BotConfig` tick-snapshot in T-1 itself.

## DS-003 · PS rules · done

`Docs/Research/DeepSeek/PS_RULES.md`. Capture=2 players/15 s, uncapture faster (`PowerStruggleUtils.lua:43-49,297-311`);
PP/CP/rank/team-power keys 200/201/202/300+; HQ 500 hp → `OnHQDestroyed`→win type 1; factories produce with
`buildtime`; buy prices + vehicle claim 90 s + spawn groups (`USE_SPAWN_GROUPS=true`) — the latter is the
`BotMatch.cpp:34-35` blocker. Shore/Plantation capturables are level data, not script: needs a `wb_ps_dump`.

## DS-002 · network pose · done

`Docs/Research/DeepSeek/NET_POSE.md`. Two writers proven in open code: `eEA_Physics`
(`Actor.cpp:2380-2447`, `SerializeTyped` `:2446`) vs channel-0 local movement (`Player.cpp:1339-1341,5076-5087`).
Whether `eEA_Physics` reaches channel-0 is decided in closed CryNetwork (cvars `NetworkCVars.cpp:22-24`); settle by
logging pose before/after snapshot vs `RequestMovement`. Per-tick dirty = `SetWarsBotMotion` (`Player.cpp:4985`);
~25–35 B/packet → ~4–5.6 kB/s @16 bots; CPU not bandwidth is the cost. Minimal change = one pose owner + `wb_net_stats`.

## DS-001 · probe spec · done

`Docs/Research/DeepSeek/PROBES_SPEC.md`. JSON `probe v1` + 8 probes with Steelmill coords (spawn 1290.12,1362.38,136.12;
SCAR item 1365.2,1332.49,139.75; sniper spot 1315.18,1388.57,141.64; Strength link a 1282.72,1513.17,143.74→b 1289.57,1511.63,146.64;
cover spot 1281.84,1393.73,134.37). Missing hooks: `wb_probe_item`, `wb_probe_dummy`, `wb_probe_run`, `wb_net_stats`.
Determinism: `wb_seed` is bot-only, world is not seeded — pin items/spawns; no `SetTimeScale`.
