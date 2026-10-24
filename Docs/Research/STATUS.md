# WarsBots research status — 2026-10-24 18:35 MSK

One page, current truth. Evidence: **src** source · **unit** tests · **server** server logs · **client** owner in
the client / video · **graph** real BotNav export analysed offline. History lives in git and the journal below.

## Known

| Topic | Fact | Evidence |
| --- | --- | --- |
| Spawn 195 workaround (C-014, 6147cf8/3bda447) | one 15-bot 660 s run: 202 revives, 0 frozen (after Triage fix: frozen = never > 0.3 m from revive point), 0 at 195, traps 0, stalls 104 vs 111, self-kills 0 → PASS; defaults ON (f5dc17b) | server |
| Spawn 195 (1247.50, 1319.75) | human `wb_revive_at 195 me` does **not** freeze (owner, 15:45); bot revives freeze 19/19 across 825 revives in 10 logs; spawn 218 18 m away 0/11; body: groundHeight invalid, bStuck 3/4; zero revive offset still 9/9 frozen; humans spawn fine there (owner, years of play) | server + client |
| Graph shape | 506 SCCs; outer terrain ring (1714 nodes) and factory (1536) have **0 edges** between them; 51 sinks = 25 % of nodes; 13/36 spawns outside both | graph |
| Missing terrain classes | metal stairs/catwalks with pipe railings, strength-jump ledges (+7 m), roofs with 3–10 m drops; owner spends >50 % of time on parts the graph treats as traps/islands; 1/28 of his component changes is reproducible by the graph | client + graph |
| Route failures | `no_route` 90–92 % of NAV fallbacks; most start or end in sinks/islands; A* budget explains none | server + graph |
| Watchdog (`wb_trap_recovery`) | 15/16 episodes in non-main components; levels 1–3 free a bot only in the main component; 45 s self-kill is the only exit at 195 (owner: keep, D-001) | server |
| Weapon filter B (`wb_reachable_goals`) | lowers pickups; of 255 skips 51 % projection errors (fixed in 1db9a47), 49 % "unreachable" only because the graph is wrong → parked until graph repair | server + graph + level |
| Sink escape C (`wb_sink_escape`) | no measurable effect yet | server |
| Owner playtest OwnerPace7 (19:17, 7 bots, pace ON) | bots feel livelier and stronger; aim wanders/indecision; pistol fights too eager (wants SCAR first); "teleports" — server trace clean, Codex MSVC builds at 100 % CPU during the session → client starvation (rule: no builds during owner play). Live: median 4.49 m/s, >4 m/s 76 %, sprint 31 %, 45 revives 0 frozen | client + server |
| Pace ON (`wb_bot_pace`, C-018 ef25395e, 2+2 paired runs) | median 2.49→4.48 m/s, >4 m/s 4→76 %, sprint 8→22 % (gap), stalls 134/147→76/89, pickups 91/75→117/124, frozen 0, traps 0 → win, no regression; default ON = D-007 after owner playtest | server |
| Pace (owner MOVE TRACE) | owner moving median 5.0–5.7 m/s, >4 m/s 70–77 %, sprint 46–54 %, Armor 84–87 %, Speed-mode bursts to 13 m/s, Strength ledge climbs +4–5 m per jump; bots 2.5 m/s, >4 m/s 4–10 %, sprint 7–17 % | client trace + server |
| Player movement (src, OpenCode OC-001 @ 981bd134, spot-checked) | MP stance run `maxSpeed 4.5` m/s, sprint ×1.5 = 6.75, Speed mode up to ~10–14.7 m/s short bursts; jump 1.0 m, Strength jump 4.7 m (70 energy); energy 200, regen 10/s, sprint drain 50/s; air control 0.9; wall jump impulse 600. **Bots are capped at 2.5 m/s — slower than a human running without sprint** | src |
| Decision flips (OpenCode O-004 @ 5e8d5638, spot-checked) | CHASING_ENEMY ↔ MOVING_TO_WEAPON flips come from the weapon-upgrade gate using instantaneous `!Visible()` without a primary (`BotBrain.cpp:334`) vs chase on visible-but-far, no hysteresis/commitment timer | src |
| Map data | 36 spawns, 78 map-placed weapons, minimap 2048² with bounds; CryAI nav in level.pak absent | level files |

## Now (18:35)

- Codex: C-018 pace — code `ef25395e` (flag `wb_bot_pace`, default 0), 4 paired 15-bot 660 s runs with MOVE TRACE
  running until ~19:10, then STOP. Next: C-019 geometry export (D-006).
- OpenCode: OC-008/009/010 done (reviews of 994aeab/6147cf8/3bda447; mail loop works; WALL_STARE audit). Now
  OC-011 (engine geometry-export recipe for C-019), then OC-012 (review `ef25395e`).
- Claude: terrain-only navmesh rehearsal from level files (Steelmill + 3 IA maps) with a voxel walkability model,
  scored with the graph metrics; at ~19:10 pace/summary verdict on the 4 C-018 runs.
- Finding (OC-010): with no route `Steer` returns the raw goal → bots walk/aim into walls; wall staring is largely
  a navigation symptom (no_route ~90 %) — another reason for D-006.

## Open, in order (D-006, 18:19: all maps, navmesh)

Goal: bots play correctly on **every IA map** (Steelmill, Quarry, Armada, Outpost; PS later). Map-specific fixes
only as data (e.g. a bad-spawn list), never code. Acceptance on ≥ 2 maps from now on.

1. ~~Spawn 195 workaround~~ — PASS (C-014/C-017). Generic revive guard stays; the 195 exclusion is Steelmill data.
2. **Pace / sprint** (C-013 → C-018, Codex now) — targets from the owner trace; map-independent.
3. **Navmesh (D-006)**: `wb_geom_export` on 4 IA maps (C-019, prep OC-011) → Claude builds Recast navmesh offline
   → compare with BotNav graph (traps, spawns outside the main part, owner route reproducibility, no_route) →
   if it wins, bots switch to Detour paths. Vertical play is first-class: auto-generated ballistic jump links
   (normal 1.0 m apex, Strength 4.7 m apex / 70 energy, arc clearance) + owner MOVE TRACE jumps as off-mesh links;
   Triage `navmesh` reports walk / +jump / +Strength so the value of jumps is measured per map.
   Graph repair (C-006/C-011), filter B, sink escape C, most trap-watchdog levels → parked, removed in refactor.
4. **Wall staring** (owner video) — OC-010 audit → MOVE TRACE aimYaw/wallDist → fix.
5. Decision stability (CHASING↔MOVING_TO_WEAPON) — `PATCH_DECISION_COMMIT.md`; later utility-AI decision layer.
6. ML: parameter tuning from paired runs first (after 2–3), decision learning from `episodes.jsonl` later,
   imitation of the owner last.
7. **In-game demos** (owner 19:26/19:45): format `SteelmillTriage/DEMO_FORMAT.md` (wbdemo/1, view yaw/pitch 20 Hz +
   bot brain track + events); recording C-025 right after geometry export, before sprint/weapon/aim fixes (they are
   measured with demos: aim wander °/s, reaction time, flips, time to primary — Triage `demo`); in-game playback C-026
   after C-023. Log-based 2D replay (`replay`, artifact) stays as a quick viewer.
8. Weapon priority (SCAR first, avoid pistol fights) C-022; aim/decision commit C-023.
9. Bot-code refactor (REFACTOR_AUDIT) — remove graph-patch flags after the navmesh switch.

## Lessons (2026-10-24)

- We patched symptoms of a broken navigation approach for a day (graph repair, filters, escape, watchdog) because
  every patch helped a little. Check each fix against "does this work on another map?".
- Spawn 195: a whole day of diagnosis by three agents; the workaround took one hour. Rule: > 2 h → workaround.

## Where things are

| What | Where |
| --- | --- |
| How to work (roles, loop, rules) | `Coordination/HANDOFF.md` |
| Talking to Codex | `Coordination/MAILBOX/` (`TO_CODEX.md`, `PROTOCOL.md`, `DECISIONS.md`); Codex replies in `Docs/Coordination/TO_CLAUDE.md` on weapon-goals |
| Branch SHAs, component table | `Coordination/PROJECT_COORDINATION.md` |
| Tools | `Tools/SteelmillTriage` (`summary`, `analyze`, `graph`, `movement`, `pace` (MOVE TRACE + C-013 check), `mem`, `level`, `episodes`) |
| Contracts | `SteelmillTriage/SUMMARY_CONTRACT.md` (run verdict), Lab branch `BOTNAV_EXPORT_CONTRACT.md` (graph export) |
| Owner map | Steelmill Nav Atlas (private artifact, link in HANDOFF) |

## Journal (dated, newest first)

| Date | File | What |
| --- | --- | --- |
| 10-24 14:20 | `SteelmillTriage/OWNER-VIDEO-2026-10-24.md` | owner reference video: pace, stairs, strength jumps, roofs |
| 10-24 13:00 | `SteelmillTriage/REVIEW-2026-10-24-1300-codex-ABCD.md` | Codex A–D review, B regression |
| 10-24 05:15 | `SteelmillTriage/PLAYTEST-2026-10-24-0458.md` (+ addenda 11:30, 11:40) | first real graph, Recovery6, r3 soak, spawn 195 point-specific |
| 10-24 05:15 | `SteelmillTriage/CODEX_MESSAGE-2026-10-24-0515.md` | requests A–D (superseded by the mailbox) |
| 10-24 03:00 | `SteelmillTriage/FINDINGS-2026-10-24.md` | F1–F5 from older owner logs |
| 10-24 04:30 | `SteelmillTriage/MOVEMENT_BASELINE.md` | pace profile, MOVE TRACE spec |
