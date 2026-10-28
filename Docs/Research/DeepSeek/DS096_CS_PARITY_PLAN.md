# DS-096: plan to reach "CS-bot maturity" - evidence-based gaps and coherent batches

Base: source `b8b444d6` (claude-sprint) only. Evidence used: C149 archive, C150 frozen package, `Docs/Research/STATUS.md`,
the new wiki (C151), old batch reports in `STATUS_HISTORY_2026-10-28.md` / X-reports, and the code itself. No games,
builds or feature changes by this review.

**Коротко по-русски:** до «зрелости CS-ботов» не хватает не одной фичи, а общего цикла матча и его измерения.
Код навигации/воды/PS уже есть и частично проверен (C149), но нет ни одного полного матча на текущем SHA, не
подтверждён захват, не проверены клиент, двери, техника и нижние спавны. План — 6 пакетов: (1) общий измерительный
прогон и IA-база, (2) завершение навигации Shore (нижние 12 спавнов, 41 дыра), (3) цикл захвата PS, (4) глубина боя
(укрытие/гранаты — доказать или удалить), (5) замер восприятия/прицела, (6) качество клиента. Сначала Steelmill IA,
потом вторая IA-карта; Shore PS — после навигации и захвата.

## 0. What "CS maturity" means here (and what cannot be claimed)

Counter-Strike bots are mature because a **whole match loop** works and is measured: buy/arm, take/hold/retake an
objective, rotate, use utility, fight as a team, respawn, keep score, and the client stays smooth. We have **no common
evaluation with CS** (different game, no shared scenario), so this plan does not claim parity or superiority. It
defines the minimum loop we can prove on our own metrics and closes the gaps that block it.

Minimum loops to accept:

* **IA (Steelmill)**: join -> spawn -> arm -> engage -> die/revive -> repeat for a whole match, 8 bots, no traps, one
  client, stable frame time.
* **PS (Shore)**: spawn on own base -> buy -> route to an objective -> **capture event** -> PP/economy -> respawn ->
  repeat; vehicles/boats only after the infantry loop is accepted.

## 1. Gap matrix: code present vs verified at `b8b444d6`

| Area | Code present | Verified evidence | Gap to the minimum loop |
| --- | --- | --- | --- |
| Navigation | `BotNavMesh.cpp` (Detour, manifest, link mask), `BotNavMeshCore.*` (paths, links, bounded water start `:264`), `BotJumpLink.h`, `BotCorners.h`, `BotMovement.cpp`, `BotRoamFallback.h`; `BotNav.cpp` only fallback | C149: one US2, upper Strength link done 5.48 s, sea, dry shore 31.2 m from 654, 0 NAV fallback, HP100; offline: 1095/1136 native water starts, water ray 1083/1095 | Lower **12 US spawns** absent from the mesh (zmin 99.5); **41 water holes** (38 at the x~2134 shore void, 3 open-water); capture **344** other_component, **316** 256-poly prefix 31 m; no full-map route; Steelmill 8-bot nav not re-run at b8 (C150 packaging only) |
| Battle decisions | `BotGoalArbiter.h:12` (Roam/Hunt/Weapon/Fight + commitment), `BotDecisionCommit.h`, `BotBrain::UpdateCombat`, withdraw path `BotBrain.cpp:1088-1104`, `BotThreatScan.h`, `BotTactics.h`, `BotVantage` | Old batches only (pre-b8): 287 kills/32 stalls/0 traps (`d6a9a521`); night 302/27, 287/29, 293/30; `withdraw_cover` showed **no gain** (69 % deaths); `finish_lock` never fired | **No grenades/utility at all** (no `grenade` reference in `Code/CryGame/Bots`); no cover-based firefight (vantage is sniper-only); no suppression/area denial; no focus fire; withdraw flag unproven |
| Weapon pursuit | `BotWeapons.cpp`, `BotWeaponRank.h`, `BotUpgradeGate.h`, `BotSniperReach.h`, first-primary gate | first primary p50 33.7 -> 5.7 s (D-032 era); pickups 260-291 vs 223/238 (D-016 era) | Snipers never picked with defaults (`sniperReach=0`; DSG1 20.3 m away, 0/8 bots ever held DSG1/Gauss); no ammo/utility pickup policy; PS buy is style-based (`BotPSBuy.h`), no role/round logic |
| Perception + aim | `BotPerception.h` (110 deg cone, hearing), `BotAimModel.h`, `BotAimInterp.h`, `BotAimControl.h`, turn cap | Only old owner playtests; no hit-rate/TTK measurement at b8 | No measured accuracy by range, reaction or TTK on the current SHA; no utility/flash awareness (nothing to be aware of yet) |
| Team + objective | `BotTeamPlan.h` (PS only, `:1-19`), `BotGameMode.h`, `BotMatch.cpp`, `BotPSCapture*` (Area destination `:216-219`), `BotPSBuy.h`, `BotCaptureArea.h`, `BotBoatDriver.h` | Buy measured (C142: primary+LAM); doors open request only (C140, no passage proof); capture **route** reached 31.2 m (C149) with **no capture event**; vehicles implemented, **never run**; IA has no teams/objectives (deathmatch) | Capture completion, PP/economy, wave respawn, doors passage, vehicles/boats, lower spawns; no rotations/retake/hold orders; IA team play is by design absent |
| Client stability | `BotNetCompat.h` (`net_ext`), `BotReplicaSmooth.h` + `Player.cpp:5071-5090`, spectator aim (`Player.cpp:1996`), 20 Hz demo | **No client run at b8**; C149 was server-side; C150 is the first planned client session (static review only) | Unknown stutter/desync/spectator quality in PS; owner-PC FPS; demo playback quality |

## 2. Batches (6), each with locations, dependencies, oracle, acceptance, abort

Targets below are **proposed thresholds**, not measurements.

**B1 - Common evaluation + IA Steelmill whole-match baseline** (do first)
* Files: `Tools/` harness (C150 `Run.ps1` profile), `BotProbeRunner.h` metrics, `BotManager` summary, `BotMoveTrace`.
* Depends on: nothing; enables acceptance for B2/B4/B5.
* Oracle: existing probe contract tests + a metrics JSON schema test; one 8-bot 300 s run on the C150 Steelmill profile.
* Acceptance (target): kills >= 60, stalls <= 20, traps 0, errors 0, health/revive loop works, client FPS report, all
  metrics present in the archive. Abort: readiness/hash/review-gate failure, any error line, no metric.
* Why: without this, "maturity" is not measurable and every later claim is anecdotal.

**B2 - Shore navigation completion** (data + core, no new subsystem)
* Files: bake tooling (`Tools/RecastBake`), local sub-99.5 interior bake for the lower 12 spawns, shoreline/water-mask
  fix for the 41 holes, `BotNavMeshCore` (no global up-widen).
* Depends on: B1 oracle harness; C149 evidence.
* Oracle (actual core, offline): all 124 spawns project, lower 12 on-mesh and connected; >= 95 % of the 1136 C147 swim
  rows pathable; the 5 capture destinations reachable from US and NK; **no outside poly below the water plane**
  (seabed control), 344/316 fixed or explicitly listed.
* Acceptance (target): >= 95 % water starts, 0 new holes introduced, 344 reachable, 316 full route. Abort: any
  seabed/through-hull link, any spawn left off-mesh, any regression on Steelmill/other maps.

**B3 - PS infantry match loop on Shore** (capture first, then economy)
* Files: `BotPSCapture.cpp` (Area destination/hold), `BotBrain::UpdatePSCapture`, PS event hooks, `BotPSBuy`, respawn.
* Depends on: B2 (route to the Area), B1 (metrics).
* Oracle: engine-free planner tests (destination inside Area, hold on membership) + actual-core path from a real spawn
  to the Area; event contract (capture start/step/captured).
* Acceptance (target): one shared run reaches the capture Area and logs a GameRules capture event + PP change;
  respawn works. Abort: destination outside Area, no event, PP unchanged, or a captured "fake" state.

**B4 - Combat depth: prove-or-delete cover/withdraw; minimal grenade utility**
* Files: `BotBrain.cpp:1088-1104` (withdraw), `BotConfig.h` flags (D-021 "flags die"), new small utility policy only if
  a red oracle exists; native grenade item, teammate guard (D-023).
* Depends on: B1.
* Oracle: engine-free policy test (no throw when a teammate is near the blast, no grenade without target), plus a probe
  scenario for cover use; `withdraw_cover` A/B.
* Acceptance (target): either withdraw shows a deaths decrease (target -10 %) or the flag is deleted; grenade
  friendly-fire accidents <= 1 per batch. Abort: no measurable effect after two paired batches -> delete, do not keep.

**B5 - Perception/aim measurement, no aimbot**
* Files: `BotPerception.h`, `BotAimModel.h`, `BotAimInterp.h`, `BotWeapons.cpp`; probe metrics.
* Depends on: B1.
* Oracle: metrics extraction test (hit rate by range bucket, TTK, reaction), owner playtest for feel.
* Acceptance (target): measured table at b8 + one worst range fixed (e.g., 20-30 m hit rate), no increase in
  artificial headshot rate. Abort: if the probe cannot measure it, stop and fix instrumentation, do not tune blindly.

**B6 - Client quality for the showcase and beyond**
* Files: `BotNetCompat.h`, `BotReplicaSmooth.h`, `Player.cpp:5071-5090`, spectator aim, demo recorder.
* Depends on: C150 client session; parallel to B2-B5.
* Oracle: A/B `net_ext`/`client_smooth` with the same server seed; demo + owner video.
* Acceptance (target): no visible stutter/desync in a 5-minute 8-bot session, spectator aim consistent with tracers.
  Abort: any desync/crash -> report, do not "fix" by disabling the net path silently.

## 3. Order: IA first, then second map, then PS

1. **IA Steelmill** (B1 + B4/B5 in parallel, B6): the minimum whole-match loop is deathmatch with respawns; what blocks
   it today is not code but **evidence**: no b8 8-bot run with a client, no metrics, unproven withdraw/finish.
2. **Second IA map** (Quarry or Outpost, data already canonical): same B1 harness + per-map bake/manifest/spots
   acceptance; one map at a time.
3. **Shore PS** (B2 -> B3, then vehicles/boats, doors): priority is capture completion, not more route polish. The
   minimum PS loop (spawn -> buy -> objective -> capture -> PP -> respawn) is blocked by: lower 12 spawns, 41 holes,
   capture not reached, doors/vehicles untested, client unverified.
4. **Vehicles/boats and doors** only after the infantry loop is accepted; boats are the owner's stated US exit, but a
   boat run must not be the first evidence of the capture loop.

## 4. Codex / DeepSeek split and cost

* **Codex**: integration, CI, data bakes, game runs/QA receipts, harness changes, owner-facing packaging. Cost: CI
  20-90 min per push; one shared run <= 300 s per acceptance; bake minutes.
* **DeepSeek**: evidence and gap analysis, engine-free policies + oracles, patch candidates, independent review of
  every exact SHA before a run. Cost: offline minutes-hours; no builds/games.
* **Owner**: playtest feel and default decisions (D-002/D-031); one client session per acceptance batch.
* Risks: data holes vs code fixes (B2), client quality unknown (B6), flag debt (B4), scope creep into utility/vehicles,
  and the temptation to mask gaps with stronger aim - explicitly out of scope.

## 5. Real defects seen while preparing this plan (not a wishlist)

1. **Heard water goal** (`BotBrain.cpp:894-905`): a sound from a swimmer is probed with the dry goal box; same class of
   bug as DS-090b, left open (no blind lift).
2. **Water-agnostic `Reachable` in vantage/weapon scans** (`BotBrain.cpp:1031/1103`, `BotWeapons.cpp:135`): raw
   swimmer feet + dry goal box -> false "unreachable" in water (harmless today, wrong if a goal is in water).
3. **Capture graph holes**: 344 other_component and 316's 256-poly prefix (DS-084) - a code or data limit, not a bot
   decision; must be resolved before claiming objective play.
4. **Sniper pickup**: default `sniperReach=0` means DSG1/Gauss are never chosen even when reachable (old 0/8 bots).
5. **Flag debt**: `withdraw_cover` measured no gain and `finish_lock` never fired - keep-or-delete (D-021), do not stack
   more flags on top.
6. **Untested paths presented as features**: doors (open request only), vehicles (never run), lower spawns (not baked).
   The C150 package correctly labels these as unproven; the plan keeps that boundary.

No game/build/install or source change by me. Next step after this plan: Codex picks the batch order, and the first
concrete SHA + CI + review gate starts with B1.
