# Answer to the DeepSeek architecture review (2026-10-25, claude-sprint @ 68c50704)

Reviewer: DeepSeek V4.1 (OpenCode, read-only). Answer: Claude. Strong review — concrete, with file:line, and honest about
what it could not see. Below: what we take, what we correct, and the tasks it creates.

## Corrections (things the reviewer could not see)

* **recast_bake exists** — `Tools/RecastBake/recast_bake.cpp` + `CMakeLists.txt` live on `warsbots/research/steelmill-triage`,
  not on claude-sprint. Agreed it must be reproducible next to the meshes → task T-6 (manifest records the bake command).
* **Unit tests are built**, by Codex on the owner PC (MSVC, logs `build/c0xx-units/`), not via CMake. Agreed that is not
  reproducible from the repo → T-4 adds a CMake/ctest target.
* **Ammo on the client** is applied only when the value differs (`SyncWarsBotAmmo`); the "only if the local item matches"
  part is fair.
* **Link ban** is now on the 2nd failure (27144dc6) — still global; per-bot TTL ban accepted (T-7).

## Accepted, in this order (proposed D-014, owner decides)

| # | Task | Owner of work | Size | Notes |
| --- | --- | --- | --- | --- |
| T-1 | **One writer per control channel** (`BotControlFrame`: Move, Aim/Fire, Suit, Jump, Action with priority + reason; one `ExecuteIntent` at the end of `Update`) | Claude | 3–5 d | first, before any PS code; paired run must stay in noise |
| T-2 | Measure the network before changing it: `wb_net_stats` (bytes/bot/s, updates/s, server-vs-client pos/yaw error) + check whether `eEA_Physics` reaches channel-0 replicas (two pose sources?) | DeepSeek (read-only analysis) → Claude (code) | 1–2 d | the reviewer's risk #1; answer with evidence first |
| T-3 | Dirty-check `ChangedNetworkState` (per-frame `SetWarsBotAim`!) + `fire`/`zoom` bits, yaw/pitch instead of an absolute aim point | Claude | 1–2 d | after T-2 |
| T-4 | **Scenario probes** (JSON spec + runner + pass/fail): weapon_pickup, stairs_roof, jump_link, duel_dummy, vantage_hold, crowd_corridor, net_sync; `wb_seed`; unit tests in CMake/ctest | spec: DeepSeek; runner: Codex (tooling, not gameplay); engine hooks: Claude | 5–7 d | cheaper than 15-bot runs; C-031 revived |
| T-5 | Lazy legacy `BotNav`: do not build the 3 m graph when the navmesh covers the level | Claude | 1 d | low risk |
| T-6 | **Map data package**: `manifest.json` (schema, mode, map, source hash, bake command, sha256 of files), stable link ids (hash of a,b,kind) instead of line numbers, binary/indexed spots; PS semantics in `map.json` (zones, roads, portals, team spawns, buy stations) | Claude | 4–6 d | before Shore/Plantation bakes |
| T-7 | Link executor robustness: per-bot TTL ban, dynamic path buffers with explicit error, lift/ladder executor | Claude | 3–4 d | |
| T-8 | Traits instead of styles (aggression, accuracy, reaction, jumper, cover preference, objective focus, weapon preference; styles = presets; PS roles separate) | Claude | 3–5 d | feeds tactics/vantage |
| T-9 | Generalised utility table (Precondition/Score/Commit) replacing the arbiter + old order | Claude | 5–7 d | after T-1, needs 2 maps |
| T-10 | Orders layer for PS: `BotOrder` + `OrderQueue` + `TeamWorld`, commander 1 Hz → squad 2 Hz → bot | Claude | 10–15 d | after T-1, T-6, T-9 |

Deferred: `DecisionCore`/`BotContext` full split (reviewer's #2) — we take it incrementally inside T-1/T-9 (decision
code stops calling `g_pGame`/`CurrentNavMesh` where we touch it) instead of a 5–8 day standalone refactor. 32 bots
(hard limit 16) — after T-2/T-3 give numbers.

## Not taken (for now)

* Full HTN — agreed with the reviewer: only if PS economy/vehicle plans need it.
* Rewriting `BotManager` into four classes in one go — split pieces as T-6 (LevelBotData) and T-4 (telemetry) need them.

## Re-review by DeepSeek (16:46) — accepted amendments

* **T-1 changes no behaviour**: every `m_intent.X =` becomes a request on its channel; `ResolveControl` replays today's
  last-writer order exactly; one `ExecuteIntent` at the end of `Update`; aim vs per-frame `UpdateTurnCap` order untouched.
  Priority changes = separate, tested commits. Writer inventory is recounted at the current head (BotTactics/vantage/
  traits added writers after 68c50704). T-1 also brings: a per-tick `BotConfig` snapshot, a `NullWorld` fake for tests
  (reused by T-4), and **`reason` per channel in INTENT telemetry** (without it T-1 cannot be proven).
* **Exit criterion for the incremental DecisionCore**: by the end of T-9 no decision producer reads `g_pGame`,
  `CurrentNavMesh` or `BotConfig::Get()` directly.
* **T-9 acceptance requires T-4 probes** (2 maps + probes, not only "within noise").
* **T-6 is three commits**: (a) reproducible bake + manifest + stable link matching — done in a17e9366 (end-point
  matching + manifest FNV-64), (b) binary/indexed spots, (c) first Shore/Plantation bake (needs Codex's geometry export).
* **T-7**: lift/ladder is a separate mover executor (BotMoverLink), not a jump kind.
* **T-4**: `net_sync` needs a client → separate harness; the seed goes into the wbdemo header.
* **T-3** adds distance LOD (near 10 Hz, far 2–5 Hz), not only dirty-check. **32 bots** stay deferred: re-gate after T-2.
