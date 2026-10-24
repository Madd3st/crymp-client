# WarsBots research status (Steelmill) — 2026-10-24 14:50 MSK

One page, current truth. Evidence: **src** source · **unit** tests · **server** server logs · **client** owner in
the client / video · **graph** real BotNav export analysed offline. History lives in git and the journal below.

## Known

| Topic | Fact | Evidence |
| --- | --- | --- |
| Spawn 195 (1247.50, 1319.75) | bot revives freeze 19/19 across 825 revives in 10 logs; spawn 218 18 m away 0/11; body: groundHeight invalid, bStuck 3/4; zero revive offset still 9/9 frozen; humans spawn fine there (owner, years of play) | server + client |
| Graph shape | 506 SCCs; outer terrain ring (1714 nodes) and factory (1536) have **0 edges** between them; 51 sinks = 25 % of nodes; 13/36 spawns outside both | graph |
| Missing terrain classes | metal stairs/catwalks with pipe railings, strength-jump ledges (+7 m), roofs with 3–10 m drops; owner spends >50 % of time on parts the graph treats as traps/islands; 1/28 of his component changes is reproducible by the graph | client + graph |
| Route failures | `no_route` 90–92 % of NAV fallbacks; most start or end in sinks/islands; A* budget explains none | server + graph |
| Watchdog (`wb_trap_recovery`) | 15/16 episodes in non-main components; levels 1–3 free a bot only in the main component; 45 s self-kill is the only exit at 195 (owner: keep, D-001) | server |
| Weapon filter B (`wb_reachable_goals`) | lowers pickups; of 255 skips 51 % projection errors (fixed in 1db9a47), 49 % "unreachable" only because the graph is wrong → parked until graph repair | server + graph + level |
| Sink escape C (`wb_sink_escape`) | no measurable effect yet | server |
| Pace | owner median 4.3 m/s, >4 m/s 53 % of moving time, sprint (Shift) almost always, circle/wall jumps; bots 2.5 m/s, >4 m/s 4–10 %, sprint 7–17 % | client + server |
| Map data | 36 spawns, 78 map-placed weapons, minimap 2048² with bounds; CryAI nav in level.pak absent | level files |

## Open, in order

1. **Spawn 195 root cause** — native revive path (`spawn:Spawned`, `UpdateAreas`), human `wb_revive_at 195 me` (C-002).
2. **Edge audit** — which `Walkable` test cuts flat pairs; 40 candidates (C-006).
3. **MOVE TRACE** — exact jump heights, sprint/suit timing for the owner and bots.
4. **Pace / sprint** — target >4 m/s 35–55 % of moving time (roadmap step 2).
5. **Graph repair** — railings/stairs false cuts → drop links → gated strength-jump links. Then re-test B.
6. Decision stability (CHASING↔MOVING_TO_WEAPON), later decision layer, cover, circle/wall jumps, Power Struggle.
7. Bot-code refactor after step 1 closes — audit in `Docs/Research/SteelmillTriage/REFACTOR_AUDIT.md`.

## Where things are

| What | Where |
| --- | --- |
| How to work (roles, loop, rules) | `Coordination/HANDOFF.md` |
| Talking to Codex | `Coordination/MAILBOX/` (`TO_CODEX.md`, `PROTOCOL.md`, `DECISIONS.md`); Codex replies in `Docs/Coordination/TO_CLAUDE.md` on weapon-goals |
| Branch SHAs, component table | `Coordination/PROJECT_COORDINATION.md` |
| Tools | `Tools/SteelmillTriage` (`summary`, `analyze`, `graph`, `movement`, `mem`, `level`, `episodes`) |
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
