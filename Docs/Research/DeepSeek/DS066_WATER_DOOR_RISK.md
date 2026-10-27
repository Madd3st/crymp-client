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
   a freeze path today, not a route.** PS runs in multiplayer (`gEnv->bMultiplayer` true), where that damage branch
   is skipped and native oxygen is `IsPlayer`-gated, so the PS blocker is the freeze, not drowning; IA bots keep the
   stock damage and are out of scope for this repair.
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

Hard stops: no 15-bot run while `ShouldSwim -> canMove=false -> Stop` is unchanged (D/E would measure a known freeze,
not the patch); no re-run of C140 until the door patch is inside the exact SHA; no water mesh install for a capture
run before gate D; no IA run with the Shore water mesh (stock IA AI water damage is out of scope).

## 5. Smallest next evidence (in order)

1. Codex: apply 067c on 067a/b, rebuild, rerun the selftest; publish `fixture-results.json` and the patch SHAs.
2. If gentle-shore is red: choose off-mesh links vs `--water-cell 1` from the measured component report.
3. Bake Shore offline; publish components/blocked_cells and per-point route costs; do not install.
4. Door: one C140-style replay with 067d (not ds063) to prove (1) the 20 s open, (2) the physical crossing, and (3)
   vehicles-on does not preempt capture. This is the named missing observation.
5. Water traversal stays a separate decision (swim policy below). Not part of C145 and not a new flag until the
   owner/Codex accept the scope; without it, keep water polygons out of bot paths (fail-closed) rather than ship a
   freeze/drown route.

## 6. Narrow water-traversal adaptation (proposal, not shipped)

The goal is to reuse the movement controller, not add a swim controller: stock `CPlayer` already swims, the bot path
is already a horizontal destination, and the only blockers are a state gate, a ground probe and the stock AI drown
branch. Four small existing-file changes, each separately reviewable:

1. `BotBlackboard.cpp:20-22` — add `bool swimming = actor.ShouldSwim();` and remove `!actor.ShouldSwim()` from
   `canMove` (keep every other guard: health, frozen, ragdoll, ladder, freefall, spectator, vehicle). `canMove` then
   answers "the actor can act", not "the actor is on ground".
2. `BotMovement::SafeStep` (`BotMovement.cpp:55-76`) — take `bool swimming`; when true, keep all nine horizontal body
   rays (`:67-71`, cliffs, walls, held item) and skip **only** the downward ground-hit return at `:73-75`. Never skip
   collision rays, never teleport, never change `ent_*` flags globally.
3. `BotLocomotion::Execute` (`:100-141`) — pass `state.swimming` to `Move`; while swimming refuse jump/strength
   (`:107-121`) and sprint, and do not request a land stance (the stock movement controller already selects swim from
   the actor stats; verify whether `STANCE_SWIM` is accepted before sending it). On shore (`ShouldSwim()` false) the
   old path is unchanged.
4. `Player.cpp` — **unchanged** (Codex correction 28.10): the 30 HP/s branch is singleplayer-only
   (`!gEnv->bMultiplayer`), and the native oxygen path is `IsPlayer`-gated; PS water repair must not touch either.
   In IA the stock 30 HP/s remains for bots; the Shore water mesh is not installed for IA tests.

Shore entry/exit caveats: enter only where `relativeBottomDepth` crosses 1.3 m; exit only where the navmesh projects a
landing within `climb` (`NavMeshCore::Project`, side 1.5, up 1.0, down 3.0) and the body rays are clear. A quay higher
than `climb` is a cliff and must stay refused. The existing trap watchdog (`BotBrain.cpp:1228-1270`) now sees
`canMove=true` in water, so "no progress while swimming" can reach level 4 (self-kill); that must be measured, not
assumed benign.

Engine-free oracle for the decision (pure helper, no physics): `WaterStep(swimming, groundHit, wallHit, depth)`.
Required table: land + ground + not swimming -> normal; land + no ground + not swimming -> refuse (cliff/void
negative); swimming + wallHit -> refuse (land-cliff negative); swimming + no wall + depth>=1.3 -> allow; depth<1.3 ->
ground path (shallow wade). Plus a land-cliff MOVE TRACE (speed ~0, no fall) and a real water positive (>=30 m leg,
speed >0.5 m/s, HP unchanged, exit on land) as runtime evidence.

Flag discipline: no new `wb_*` for this. Either it rides the water-mesh package while that is default-OFF, or it
promotes/deletes a flag in the same item (net-zero rule). Owner decision; not part of DS-067.

## 7. Query consistency (tool vs runtime)

* Tool points/components filter at `recast_bake.cpp:608` keeps default area costs 1.0; `items_reachable`/`components`
  are therefore **not** route-preference evidence. Only `--path-from/--path-to` (DS-067b) applies `--water-cost`
  (default 4, same as `BotNavMeshCore.cpp:60`).
* Runtime `NavMeshCore::FindPath` uses `m_filter` with `setAreaCost(4,4)`; `Reachable`/`BuildComponents`
  (`:81-160,232-239`) use flags only. A runtime route log should therefore print the chosen path's water polygon count
  and the filter cost at `WB_LOG(2)` (diagnostic, no flag), so a run can be compared 1:1 with the tool's `path` JSON.
* `waterCost` must stay one constant in both places; the DS-067 docs carry the pair, and the selftest asserts 4 vs 1
  behaviour rather than the printed number.

## 8. DS-067 follow-up after the C145 selftest RED

### 8.1 Mask: boundary contact is not coverage (`ds067c_water_mask_clip.patch`)

The first 067a revision used SAT with inclusive contact, so a shore edge lying exactly on a cell border counted as
coverage: the two-shores shore triangle at `x=10` blocked the water cell `x=10..12`, every edge cell was removed and the
positive case disconnected (Codex: selftest RED at positive two-shores). The fix replaces both helpers with exact
Sutherland-Hodgman clipping of the triangle against the cell (`TriClipToRect`): the overlap polygon must have area
> 1 cm^2 (a shared border is 0), and `minY`/`maxY` are taken over the *clipped* polygon instead of the whole-triangle
bbox or sampled corners. Consequences: boundary-only contact is ignored, a tiny island still has positive area and
blocks its cell, and a large triangle is classified by its local height inside the cell. Expected: two-shores
water `1 component / item reachable`; roof `0` water / 3 components; tiny-island footprint untouched; gentle shore
`1` component; detour cost 4 -> `water_polys 0`, cost 1 -> `>0`. Applies after 067a+067b; no interface change.
Not compiled here; this is the exact RED fix Codex asked for.

### 8.2 Door-only ordering (`ds067d_door_only_order.patch`, base `2e0a6641`, supersedes `ds063_door_priority.patch`)

The combined `UpdatePSInteract` returns true for vehicles, so moving the whole function before capture gives vehicles
priority over capture. 067d splits it: `UpdatePSDoors` (door block, always returns false) is called before
`UpdatePSCapture`; `UpdatePSInteract` keeps only the vehicle enter/exit and stays **after** capture, exactly as today.
`UpdateRoam` order becomes doors -> capture -> vehicles (`BotBrain.cpp:845-847` in the patched file). It also bounds
the 10 Hz proximity scan (`m_nextDoorUse=m_now+0.25f` at scan start), keeps the 2 s per-door cooldown, and folds in
the DS-063 `pre/post/dist/positions` log plus a `WB_LOG(2) ... reason=ahead` rejection line. Do **not** apply it on
top of ds063 (same hunks); use it as the door patch for C145. Vehicles-on priority is unchanged, so a `wb_ps_vehicles 1`
run remains a separate acceptance case.

### 8.3 Native swim (`ds067e_native_swim.patch`, base `2e0a6641`, applies after 067d)

**Correction (Codex, 28.10):** the 30 HP/s AI branch at `Player.cpp:2961` is guarded by `!gEnv->bMultiplayer`, so it
does **not** run in a PS multiplayer server; the earlier DS-066 wording conflated singleplayer AI with MP. In PS the
water blocker is only the movement chain: `ShouldSwim()` -> `canMove=false` (`BotBlackboard.cpp:20-22`) -> `Stop` every
tick (`BotLocomotion.cpp:103`), plus `SafeStep`'s ground ray (`BotMovement.cpp:73-75`). Native oxygen
(`UpdateUWBreathing`) is `IsPlayer`-gated (`Player.cpp:3060`) and stays unchanged: a bot neither takes the IA AI
damage in MP nor uses the human oxygen timer. **No Player.cpp edit is part of this repair.** In IA the stock AI damage
still applies; the Shore water mesh is PS-only and must not be installed for an IA test.

067e instead routes the bot through the native path:
* `BotBlackboard`: add `swimming=ShouldSwim()`; remove only `!ShouldSwim` from `canMove` (health/frozen/ragdoll/
  ladder/freefall/grab/spectator/vehicle guards stay).
* `BotMovement::SafeStep`: `swimming` skips **only** the downward ground ray at `:73-75`; the nine body/cliff rays at
  `:67-71` still return false, so a dry cliff or a quay wall is still refused. `Move` forwards the flag.
* `BotLocomotion`: passes `state.swimming`, refuses `jump_swimming`; sprint already requires `grounded` (false in water).
* **No `Player.cpp` change.** PS MP has no AI instant drown (`!gEnv->bMultiplayer`), and the native oxygen path is
  `IsPlayer`-gated; editing it to exempt or enable bots would either be unnecessary (MP) or an invulnerability/HP
  policy change (IA). The repair is scoped to the movement policy only, as Codex requested. Consequence: in PS a bot
  in water takes no drown damage from either path; that is existing stock behaviour, not a new grant, and the
  acceptance must not claim drowning as a positive test. IA bots keep the stock 30 HP/s and are out of scope.
* `BotBrain::RecoverTrap`: the trap watchdog sees `canMove` true in water; pass `canMove && !swimming` so it cannot
  escalate to level 4 (self-kill) while swimming.

Entry/exit: entry only when `ShouldSwim()` turns true (bottom depth >= 1.3 m); shallow water keeps the old ground path.
Exit needs a navmesh landing within `climb` (`NavMeshCore::Project`, side 1.5, up 1.0, down 3.0) and clear body rays;
a quay higher than `climb` remains a cliff. Residual risks: combat/suit behaviour while swimming is not gated (the
bot may fire from water); a bot that falls into water in a non-PS map now swims instead of freezing (repair, but a
behaviour change).
Flag: no new `wb_*`; active only when the existing default-OFF capture/navmesh path puts a bot in deep water.

Engine-free evidence plan: a pure decision table `WaterStep(swimming, bodyBlocked, groundHit, depth)`:
land+ground -> normal; land+no ground -> refuse (cliff/void); swimming+bodyBlocked -> refuse (dry-cliff negative);
swimming+no body+depth>=1.3 -> allow; depth<1.3 -> wade/ground. Runtime (one planned scenario, after exact-SHA CI +
review): land-cliff MOVE TRACE negative (speed ~0, no fall), >=30 m water leg positive (speed >0.5 m/s, HP unchanged,
exit on land). Do not test or claim drowning: in PS it is stock-disabled for IsPlayer=false, and IA is out of scope.
