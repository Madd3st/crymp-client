# Probe v1: five runnable Steelmill scenarios (DS-011 #1)

For the Codex runner: one fresh server per probe, `+map multiplayer/ia/steelmill +wb_probe_local_test 1`,
`sv_bind=127.0.0.1`, `sv_lanonly=1`, no humans. Coordinates are world metres, Crysis Z-up, taken from the
Steelmill navmesh/spots/links and the c070/c071 logs. All assertions use **existing log lines only**.

Common cvars: `wb_log 2`, `wb_debug 1`, `wb_trap_recovery 1`, `wb_revive_guard 1`, `wb_spawn_exclude195 1`,
`wb_bot_navmesh 1`, `wb_bot_jump_links 1`, `wb_bot_pace 1`, `wb_bot_suit_policy 1`, `wb_bot_corners 0`
(corners gets its own A/B run), `wb_bot_tactics 0`, `wb_bot_vantage 0`, `wb_bot_weapon_handling 0`,
`wb_qa_socom_only 0`, `wb_skill 3`. For probes 1–4 append `wb_probe_goal <goal>` once that hook exists (below).

| # | Probe | Start pose → goal | Commands (after start) | Pass/fail from logs |
| --- | --- | --- | --- | --- |
| 1 | stair descend 1306,1353 | `(1306.20,1356.40,146.00)` → `(1312.00,1353.00,136.00)` | `wb_probe_spawn 1306.20 1356.40 146.00`, `wb_bot_start`, wait 25 s | PASS: `AI telemetry ... pos=(x,y,z)` reaches z<140 and x>1310 within 20 s; `TRAP RECOVERY` count 0. FAIL: any `TRAP RECOVERY actor=<id>` or z>142 after 20 s. |
| 2 | spawn 176 exit | `(1204.60,1363.50,136.07)` → `(1215.00,1363.00,136.00)` | `wb_probe_spawn 1204.60 1363.50 136.07`, `wb_bot_start`, wait 15 s | PASS: telemetry x>1210 within 10 s; no `TRAP RECOVERY` at that actor. FAIL: any `TRAP RECOVERY` (level 1/2) at pos 1204.6,1363.5, or speed=0 for 8 s. |
| 3 | Gauss tower Strength climb | `(1279.64,1502.85,125.24)` → `(1289.57,1511.63,146.64)` | `wb_probe_spawn 1279.64 1502.85 125.24`, `wb_bot_start`, wait 70 s | PASS: ≥3 `JUMP TAKEOFF: strength=1` for the actor, final telemetry z>146.0, no `LINK failed`/`TRAP RECOVERY`. FAIL: fewer than 3 Strength takeoffs or z<144 after 60 s. |
| 4 | DSG1 stairs | `(1303.50,1364.70,136.00)` → `(1303.47,1364.74,149.74)` | `wb_probe_spawn 1303.50 1364.70 136.00`, `wb_bot_start`, wait 35 s | PASS: telemetry z>149.0 within 30 s, `TRAP RECOVERY` 0. FAIL: no z progress ≥+10 m in 30 s or any trap. |
| 5 | 1v1 duel at 30 m | A `(1290.12,1362.38,136.12)`, B `(1319.90,1362.38,136.10)` | `wb_probe_spawn` A, `wb_bot_start`, `wb_probe_spawn` B, `wb_select 2`, `wb_bot_start`; wait 40 s | PASS: `TARGET CHOICE` within 3 s, first `combat kill shooter=` within 30 s, `crashes 0`; FAIL: no target/first shot or both stuck (`TRAP RECOVERY`/`blocked=1`). |

## Missing hook (blocker for 1–4)

There is no “go to x y z” command today: `wb_bot_gait` only sets `pos + forward*25 m` (`BotBrain::AbilityCommand`),
and the AI otherwise roams/seeks weapons. Probes 1–4 need a one-shot forced goal, e.g.
`wb_probe_goal <x> <y> <z>` (selected bot) that fills `m_testGoal`/a probe goal and keeps `m_testUntil` until reached;
without it the scenarios are not runnable as written. Codex can implement the runner, but Claude must add this hook.
Probe 5 needs no goal: the two bots acquire each other.

## Notes for the runner

- Read telemetry with regex `AI telemetry EntityId=(\d+).*pos=\(([-\d.]+), ([-\d.]+), ([-\d.]+)\)`; time from the
  line prefix. These lines are emitted at `wb_debug 1`, ~2 s.
- `wb_probe_spawn` adds a no-AI bot and selects it; `wb_bot_start` starts AI on the selected bot. For probe 5,
  after the first `wb_bot_start`, the second `wb_probe_spawn` selects the new bot, so `wb_select 2` is optional.
- Never enable `wb_qa_time_scale` (`×8` changes behaviour, `STATUS.md:28`); run real time.
- `wb_bot_corners` is a separate A/B on probes 1/2: same poses with `0` and `1`; the gate is PASS only if the
  corners=1 run has no new `TRAP RECOVERY` and no >20 % drop in telemetry speed.
- No `wb_seed` exists yet; determinism comes from the fixed spawn poses and a single bot per probe.

