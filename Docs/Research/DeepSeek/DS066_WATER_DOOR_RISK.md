# DS-066 / C145: water + door pre-batch risk audit (base claude-sprint 2e0a6641)

Independent audit of DS-062 (water) and DS-063 (door order) before the coherent Shore package. Code read at
`2e0a66414305b163848c6d5f18bb5812df4462a9` (game) and triage `c137b949` (tool); no game, no build, no bake.
`git apply --check` results below are the only "executed" evidence here; everything else is a code path or a
Codex-measured result explicitly marked as such.

## 1. Proven (code, apply checks, or a reproduced oracle)

1. **Patches still fit the current heads.** `ds063_door_priority.patch` on `2e0a6641` = clean; `ds062_water_runtime.patch`
   on `2e0a6641` = clean (BotBrain/BotNavMeshCore unchanged since `532e4497`, `git diff --stat` empty); `ds062_water_baker.patch`
   on triage `c137b949` = clean. `Tools/RecastBake/` does not exist on claude-sprint, so the tool has no code-branch conflict.
2. **The door cause is a scheduling bug, not an RMI bug.** `BotBrain.cpp:829` returns from `UpdatePSCapture` before
   `:830` reaches `UpdatePSInteract`; the 31.50 s `PS DOOR open` appears only after the capture target changes
   (`C140` log, DS-063). The server call is `Door:Open(user,1)` at `:600`, no `OnUsed`, no close.
3. **Reordering is only free while vehicles stay off.** `UpdatePSInteract` returns **true** for vehicle exit (`:558`)
   and vehicle enter (`:574`). After the DS-063 swap, `wb_ps_vehicles 1` makes cars preempt capture every tick,
   which is a behaviour change the door oracle does not test. With the C140 recipe (`wb_ps_doors 1`,
   `wb_ps_vehicles 0`) the only body is the door scan and it returns false (`:605`), so capture still moves in the
   same tick. Acceptance must pin `vehicles 0` or add a separate vehicle case; do not ship both flags "on" unmeasured.
4. **Door scan cadence changes.** `m_nextDoorUse` is advanced only after a successful open (`:601`); every decision
   tick near a door now runs a `QueryProximity` box 5x5x3 (`:580`) where the old capture-first path usually returned
   before it. NK has 17 doors (`DECISIONS/DS-052`), 10 bots at 10 Hz -> up to ~100 queries/s. Not a bug, but it must be
   bounded in the same patch (advance the cooldown after a scan attempt and log the rejection reason at `WB_LOG(2)`).
5. **Water mask is unsafe as submitted.** Codex reproduced the C145 `mask-oracle` (tiny 0.4 m island at
   x20.1..20.5 / y4.1..4.5 / z100 inside one 2 m cell, one water quad overlaps it) and the selftest defects:
   `"obj": "%s"` prints Windows backslashes (invalid JSON), no `--points` so `items_reachable` cannot be asserted,
   roof fixture really has **3** components (the roof itself), and `area_cost: 4.0` is a printed constant, not a
   query result. All four are code-confirmed; DS-067a/b fix them.
6. **Water traversal is not implemented and currently cannot work.** `BotBlackboard.cpp:20-22` sets `canMove=false`
   while `CPlayer::ShouldSwim()` (`Player.cpp:2830-2853`, deep water >=1.3 m bottom depth); `BotLocomotion.cpp:103`
   then issues `Stop` + `Refused("movement_state")` every tick. Even without that gate, `BotMovement::Move` requires
   `SafeStep` (`BotMovement.cpp:55-76,118-129`), whose downward ray needs ground within 3.6 m; in the bay it hits
   nothing. In Instant Action the same state also applies stock AI drown damage, 30 HP/s (`Player.cpp:2958-2970`,
   `!isClient && !gEnv->bMultiplayer`), so a bot stands still and dies in ~3.3 s. **A water polygon path is therefore
   a freeze+drown path today, not a route.** In a dedicated MP server the damage branch is skipped, but `canMove`
   is still false, so the bot freezes instead of crossing.
7. **Graph reachability ignores area cost.** `BotNavMeshCore::BuildComponents` / `Reachable` (`:81-160,232-239`) filter
   on poly flags only; `FindPath` uses `m_filter` with area costs. So the bake's `components`/`items_reachable` and the
   runtime `Reachable()` are **topological**; cost 4 only affects the actual `findPath`. Any acceptance based on
   `items_reachable` alone does not prove a preferred land route.
8. **No-water compatibility is measured, not argued.** Codex: Steelmill no-water mesh SHA identical; the DS-067 mask
   adds no bytes to a no-water bake (the code path is guarded by `c.water`, and the `pm->flags` test is unchanged in
   effect). Do not replace this with a byte-identity claim from this document.
9. **Capture hold is not the capture area.** `BotPSCapture.h:36-69` picks the nearest point by straight-line distance
   (no route, no cost, no `captureRequirement`) and sets `hold` as soon as the 3D distance is <=6 m; `UpdatePSCapture`
   then stops movement (`BotBrain.cpp:630-641`). For 344 and 579 the entity origin is already just outside its
   `AreaShape` (`SHORE_CAPTURE_ELIGIBILITY.md:19-21`), so "hold" can stop the bot outside the area. Eligibility is
   rule-level yes (DS-064), but participation is still unobserved; the only proof is a native `capture_start`.

## 2. Assumptions / not proven (do not present as results)

* **Door opens by 20 s after the swap.** The starvation is proven; whether the 20 s position/heading passes the
  `ahead` gate (`BotBrain.cpp:588-596`) is not. The current log only prints on success, so a failed scan is invisible.
* **Physical door passage.** `post=1` is script state (`Door.action`), not collision; C140 has no post-state. Only a
  MOVE TRACE crossing through the aperture proves it.
* **Shore water mask at production scale.** No bake: 7.76 M triangles, `allTris` plus two `TriGrid`s plus a 2 m grid
  over full map bounds; memory/time and the actual `blocked_cells` are unknown. Whole-cell SAT is heavier than the
  centre test. This is a real risk for the 163.5 MB release; record bake RSS/time.
* **Coastline connectivity.** The DS-067 rule accepts a shoreline triangle only if it is walkable and rises at most
  `climb` inside the cell. That admits water at the waterline (so land/water polygons can meet) without a walkable
  water tunnel (water under >`climb` of terrain has <1.8 m ceiling and Recast drops the span). It still leaves a
  one-cell gap on shores steeper than `atan(climb/waterCell)` = 14 deg at 2 m, 26.6 deg at 1 m. `shore-gentle.obj`
  tests the admitted case; a steep-shore fixture must be a *negative*, not a connectivity claim.
* **Water level 99.5** is prior evidence (`Docs/Research/DeepSeek/BOATS_SHORE.md:5`: `leveldata.xml`), not re-read here.
* **Capture event for a bot.** No `capture_start` in any run; DS-064 is rule-level only.

## 3. DS-067 revised patch (tool only, applies on triage `c137b949` + DS-062)

Two separately reviewable patches; both verified with `git apply --check` in sequence (no compile, no run):

* `patches/ds067a_water_mask.patch` — `blocked_cells`, whole-cell triangle/rect SAT (`TriOverlapsRect`), shoreline
  acceptance (`TriSlopeOk` + `MaxTriYInRect`), `--water-cell`, `--water-dump` (emitted quads in Crysis XY), JSON path
  escaping, `blocked_cells`/`cell` in the water report. Fixes the C145 RED without a global plane.
* `patches/ds067b_water_oracle.patch` — `--water-cost`, `--path-from/--path-to` real Detour `findPath` oracle
  (`found`, `polys`, `water_polys`, `length_m`, `water_cost`), `two-shores.points`, `tiny-island.obj` (C145 footprint),
  `shore-gentle.obj` + points, `detour.obj`, rewritten `water_selftest.ps1` (footprint assertions, roof = 3, cost 4 vs 1).

Expected selftest facts: dry two-shores 2 components / item unreachable; water 1 component / item reachable; roof
water 0 / 3 components; void 0 cells; island and tiny-island dumps never intersect the solid footprint; gentle shore
1 component / item reachable; detour `water_polys 0` at cost 4 and `>0` at cost 1. If the gentle-shore expectation is
red on the first compile, the fix is an explicit off-mesh link or `--water-cell 1`, **not** a global plane.

Both patches are unverified by compilation (no build here); Codex owns the standalone MSVC build and the real bake.

## 4. D-034 acceptance for the package

Separate the four evidence classes; a pass in one is not a pass in another.

| Gate | Pass criterion (evidence) | Not proof |
| --- | --- | --- |
| A. Patch hygiene | Each patch applies on the exact parent; no Lua/RMI/default/new-`wb_*` flag; net-zero flag rule; BOM/LF checked | "applies" is not behaviour |
| B. Tool offline | MSVC build of the standalone baker; `water_selftest.ps1` green incl. tiny-island footprint, roof 3, points, cost 4 vs 1; Steelmill no-water SHA identical; JSON finite/valid for all cases incl. no `--points`; exact fixture/tool SHAs and bake time/RSS | green selftest is not Shore |
| C. Bake plan | Release `a943` hash verified; bake with Steelmill settings + `--water 99.5`; report cells/blocked_cells/components; per-capture-point `--path-from/to` water_polys at cost 4; hash manifest; unreachable subjects preserved | graph route is not swimming |
| D. Runtime planning | With the baked mesh: every selected capture route is logged with cost/water_polys; no route into deep water while swim is off; no drown deaths; else fail-closed | planner choice is not movement |
| E. Runtime scenario | One planned run, exact SHA + green CI + independent review: door `open` by 20 s, `pre=2/post=1`, MOVE TRACE crosses the `Door48` aperture within 2 s, 0 Lua/RMI errors; water leg only if a swim policy exists (speed >0.5 m/s, exit on land, 0 water deaths); native `capture_start/step/captured` for a bot inside the area + team flip; stalls/traps/kills/pickups vs `532e4497` within the agreed band | one PASS is not a soak |
| F. Promotion | D032 owner answer; defaults stay OFF; a water route is not advertised as playable until D/E pass | status text without SHA |

Hard stops: no 15-bot run while `ShouldSwim -> canMove=false -> Stop` and the IA drown branch are unchanged (D/E would
measure a known freeze/death, not the patch); no re-run of C140 until the door patch is inside the exact SHA; no
water mesh install for a capture run before gate D.

## 5. Smallest next evidence (in order)

1. Codex: compile + run DS-067a/b selftest; publish `fixture-results.json` and the two patch SHAs.
2. If gentle-shore is red: choose off-mesh links vs `--water-cell 1` from the measured component report.
3. Bake Shore offline; publish components/blocked_cells and per-point route costs; do not install.
4. Door: one C140-style replay with the DS-063 patch, adding a rejection log, to prove (1) the 20 s open and (2) the
   physical crossing. This is the named missing observation; no other run is justified before it.
5. Water traversal stays a separate decision (swim policy: `canMove` split, no `SafeStep` ground ray in water, a
   WarsBots-only drown exception in `Player.cpp:2958`, shoreline exit). Not part of C145 and not a new flag until the
   owner/Codex accept the scope; without it, keep water polygons out of bot paths (fail-closed) rather than ship a
   freeze/drown route.
