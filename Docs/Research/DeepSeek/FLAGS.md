# `wb_*` flag inventory — claude-sprint `a890f2ad`

Read-only audit, 2026-10-26. Source of truth: `Code/CryGame/Bots/BotConfig.cpp:7-49` (41 registered flags) and
`BotConfig.h:6-47` (defaults). "First" is the first commit in `BotConfig.cpp` history (`git log -S`), not the commit
where the underlying feature was first written. Evidence is deliberately conservative: **none found** is not proof that
a flag is bad, only that no isolated ON/OFF report exists.

## Status legend

- **promote** — accepted by the owner or already default ON; cleanup can remove the flag and its OFF branch.
- **keep OFF** — diagnostic, QA-only, or failed candidate; code may stay but must not become default.
- **delete** — superseded/failed candidate with no remaining purpose; list the dead paths.
- **pending** — no clean verdict yet; keep OFF, wait for the named run.

## Inventory

| Flag | Default | What it does (≤10 words) | First | Evidence / verdict | Status |
| --- | ---: | --- | --- | --- | --- |
| `wb_demo_record` | 0 | server wbdemo recorder | `6e576693` | diagnostic only; C-025/C-084 | keep OFF |
| `wb_demo_hz` | 20 | demo sample rate | `6e576693` | diagnostic; C-025 | keep OFF |
| `wb_probe_local_test` | 0 | local-IA opt-in gate | `6e576693` | runtime safety; required | keep |
| `wb_bot_cloak_experimental` | 0 | bot cloak | `6e576693` | no clean cloak run; C-033 | keep OFF |
| `wb_hotspot_all_nodes` | 0 | scan all hotspot nodes | `6e576693` | QA until 10 min soak | keep OFF |
| `wb_reachable_goals` | 0 | graph reachability filter | `6e576693` | C-001: pickups 81/200→32/192; failed | **delete** |
| `wb_bot_pace` | 1 | native pace/sprint/energy | `6e576693` | C-018; D-007 owner; default ON | promote |
| `wb_revive_guard` | 1 | 2 s post-revive retry check | `6e576693` | C-014 candidate safety; no OFF verdict | keep ON |
| `wb_spawn_exclude195` | 1 | skip spawn 195 | `6e576693` | C-014/C-001 195 workaround; no fix proof | keep ON |
| `wb_nav_knee_fix` | 0 | export-only knee variant | `6e576693` | export audit only; never normal AI | keep OFF |
| `wb_move_trace` | 0 | MOVE TRACE diagnostics | `6e576693` | C-084; diagnostic | keep OFF |
| `wb_physics_diagnostics` | 0 | create/revive body snapshots | `6e576693` | diagnostic only | keep OFF |
| `wb_trap_recovery` | 0 | staged stuck recovery | `6e576693` | C-071/DS-S1 QA; 10 min soak pending | keep OFF |
| `wb_progress_experiment` | 0 | stationary-combat recovery | `6e576693` | C-027 listed as dead if unread; it is read | keep OFF |
| `wb_qa_socom_only` | 0 | QA SOCOM-only weapon | `6e576693` | local QA only | keep OFF |
| `wb_bot_look` | 0 | look 3 m ahead on path | `005af5c7` | C-050/C-075 all-9 ON; no isolated pair | pending |
| `wb_bot_turn_cap` | 0 | per-frame skill turn cap | `005af5c7` | C-050/C-075 all-9 ON | pending |
| `wb_bot_weapon_rank` | 0 | weapon value by range | `005af5c7` | C-050/C-075 all-9 ON | pending |
| `wb_bot_arbiter` | 0 | fight/hunt/weapon/roam arbiter | `005af5c7` | C-050/C-075 all-9 ON | pending |
| `wb_bot_perception` | 0 | 110° cone, notice delay, cloak | `2683e939` | C-093/X-108 green with perception 1; no isolated pair | pending |
| `wb_bot_aim_model` | 0 | flick/tracking/noise aim | `eb4fcf6f` | C-050/C-075 all-9 ON | pending |
| `wb_bot_suit_policy` | 0 | one suit decision per tick | `6f941cda` | roadmap 12; no isolated verdict | pending |
| `wb_bot_client_smooth` | 0 | client replica smoothing | `1e343d81` | O-025; C-083 partial/no FP pair | pending |
| `wb_bot_navmesh` | 0 | Detour navmesh paths | `fcce76bf` | C-093/X-108 green with navmesh 1; X-088 invalid (`navmesh=0`) | pending |
| `wb_bot_jump_links` | 0 | baked drop/jump/Strength links | `0470fe7e` | C-036; no isolated pair | pending |
| `wb_bot_weapon_handling` | 0 | LAM/zoom at distance | `fe08fb70` | C-075 owner logs, no clean ON/OFF | pending |
| `wb_bot_name_profile` | 0 | skill/style in bot name | `fe08fb70` | C-075 owner session; C-093 names 0 | pending |
| `wb_bot_vantage` | 0 | sniper spots / roof holds | `68c50704` | C-098 red sniper gate; C-104 hold fix pending | pending |
| `wb_bot_tactics` | 0 | hold angle, jump-strafe, loot | `c648ea3b` | C-067/DS-005; no isolated verdict | pending |
| `wb_bot_path_progress` | 0 | path-remaining weapon progress | `aebce3b7` | C-098/X-111 red (stalls 39); DS-S1 false-timeout bug | pending |
| `wb_bot_jump_sanity` | 1 | no random/obstacle jumps on mesh | `b521feeb` | C-078/X-078 + owner; D-016 | promote |
| `wb_move_trace_hz` | 4 | client MOVE TRACE rate | `bb172f34` | C-084; diagnostic | keep OFF |
| `wb_bot_threat_scan` | 0 | glance at high sniper spots | `9ac55001` | C-098/X-111 red: 0 glances, stalls 39; `c6f36c28` ray-budget fix pending | pending |
| `wb_bot_spectator_aim` | 0 | camera uses tracer aim | `1e0f4072` | DS-013/C-093 client visual, no FP verdict | pending |
| `wb_bot_difficulty` | 0 | skill table (reaction/aim/range) | `1e0f4072` | C-093/X-108 green: 276 kills, 19 stalls, Elite 70; DS-018 fix in `7179ec36` | promote |
| `wb_bot_finish` | 0 | keep damaged target / short cover | `1e0f4072` | C-093/X-108 green; DS-018 6 s cap in `7179ec36` | promote |
| `wb_bot_corners` | 1 | navmesh corner v2.1 | `6be78151` | C-078/X-078 + owner; D-016 | promote |
| `wb_fire_diagnostics` | 0 | shot/hit diagnostics | `6e576693` | diagnostic only | keep OFF |
| `wb_log` | 1 | log level | `6e576693` | operational setting | keep |
| `wb_maxbots` | 6 | max simultaneous bots | `6e576693` | operational setting | keep |
| `wb_skill` | 1 | default `wb_add` skill | `6e576693` | operational setting | keep |

X-088 (`5b90bfb6`, default-only 15×660) is **not** corners/navmesh evidence: `navmesh=0` means `wb_bot_corners`
could not execute (`BotBrain.cpp:154`); its 223 kills/101 stalls are not a regression against C-078/X-078. DS-S1
also found a `wb_bot_path_progress` false-timeout risk, so that flag stays pending.

## Promote / delete cleanup paths

- **`wb_bot_corners`** — already default ON (D-016). Remove the `corners==1` gate and the legacy branch in
  `BotBrain::Steer` (`BotBrain.cpp:149-154`); `SteerCorners` runs whenever `m_meshPathActive`; keep the mesh check.
- **`wb_bot_jump_sanity`** — default ON. Remove the `jumpSanity!=1` condition around the random travel hop
  (`BotBrain.cpp:102-107`) and the obstacle-ray condition (`BotBrain.cpp:971-981`); keep link/trap jumps
  (`BotBrain.cpp:954,1078`).
- **`wb_bot_pace`** — default ON, D-007. Delete the flag and the `BotPaceEnabled()` false branches in
  `BotBrain::TravelGait` (`BotBrain.cpp:84-98`), `BotLocomotion::Execute` (`BotLocomotion.cpp:102,123-126`), and the
  sprint/energy fallbacks that only exist for legacy pace; keep the pace policy (`BotPace.h`).
- **`wb_bot_difficulty`** — C-093/X-108 green. Remove the `difficulty==1` gate in `BotManager.cpp:327-329`; always
  apply `ApplyDifficulty`, and remove the `turnRate>0` sentinels in `BotBrain.cpp:1197`, `BotCombat.cpp:286` and
  `BotBrain.cpp:1253`. Keep `BotDifficulty.h` as the only skill table.
- **`wb_bot_finish`** — C-093/X-108 green; DS-018 cap in `7179ec36`. Remove the `finish==1` gate at
  `BotCombat.cpp:232`, making the fixed 6 s finish/1 s blink unconditional; keep `m_dmg*`/`m_finishStart` and the
  `BotCombat::Reset` clear (`:35`).
- **`wb_reachable_goals`** — delete: its only read is `BotNav.cpp:452`, its demo flag is
  `BotDemoRecorder.cpp:122`; C-001 shows the filter regressed pickups and was not adopted. Remove the flag and the
  `flag==1` branch; keep the project-to-nearest logic only if a future test replaces it.
- **`wb_progress_experiment`** — C-027 called it dead if unread; it is read at `BotBrain.cpp:906` (`NoProgress`) and
  guards `BotCombat::AvoidCurrent` at `BotBrain.cpp:889-893`. Keep OFF as QA until a clean 10 min soak; do not delete
  a live recovery path by accident.

## Recommended order

1. **Promote `wb_bot_difficulty` and `wb_bot_finish`** — C-093/X-108 is green (276 kills, 19 stalls, traps 0, Elite 70)
   and DS-018 is fixed in `7179ec36`; keep `spectator_aim`, `path_progress`, `threat_scan` pending.
2. **Promote the three defaults already accepted** (`corners`, `jump_sanity`, `pace`) in one cleanup commit with the
   dead-path list above; keep `revive_guard` ON until its OFF risk is tested, and keep `spawn_exclude195` until spawn
   195 is re-tested.
3. **Delete `wb_reachable_goals`** after C-001's failed result; it has no remaining user.
4. **Keep the diagnostic/QA flags OFF** (`demo*`, `move_trace*`, `physics_diagnostics`, `fire_diagnostics`,
   `nav_knee_fix`, `trap_recovery`, `progress_experiment`, `qa_socom_only`, `hotspot_all_nodes`, `cloak_experimental`,
   `client_smooth` until C-083). They are not gameplay defaults.
5. **Pending core flags** (`look`, `turn_cap`, `weapon_rank`, `arbiter`, `perception`, `aim_model`, `suit_policy`,
   `navmesh`, `jump_links`, `weapon_handling`, `name_profile`, `vantage`, `tactics`) need one isolated pair each or an
   explicit bundle verdict. Their current all-9 evidence is not enough to promote individually.
