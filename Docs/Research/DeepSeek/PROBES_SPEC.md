# Scenario probe spec (DS-001 · T-4) — JSON schema + 8 Steelmill probes

Goal: 30–90 s deterministic probes with pass/fail, cheaper than the 15-bot soak. This spec is
read-only; the runner is Codex (tooling), engine hooks are Claude. Steelmill coordinates come from
`Tools/RecastBake/meshes/Steelmill.{spots,links}.txt` and `warsbots/research/steelmill-triage`
`evidence/recast/Steelmill.json` (spawn/item points).

## JSON schema (`probe` v1)

```json
{
  "schema": 1,
  "id": "weapon_pickup",
  "map": "multiplayer/ia/steelmill",
  "seed": 12345,
  "duration_s": 40,
  "bots": [ { "skill": "normal", "style": "universal", "spawn": [1290.12, 1362.38, 136.12], "ai": true } ],
  "dummies": [ { "pos": [0,0,0], "hp": 100, "team": 0, "shoot_every_s": 0 } ],
  "items": [ { "class": "SCAR", "pos": [1365.2, 1332.49, 139.75] } ],
  "cvars": { "wb_bot_navmesh": 1, "wb_bot_jump_links": 1, "wb_bot_pace": 1,
             "wb_trap_recovery": 1, "wb_qa_socom_only": 0 },
  "assertions": [
    { "metric": "pickup", "op": "lt", "value": 8.0 },
    { "metric": "stuck_events", "op": "eq", "value": 0 }
  ]
}
```

Metrics read from the existing `*.bots.log` / `wbdemo/2` / `summary.json` (see
`Docs/Research/SteelmillTriage/DEMO_FORMAT.md`, `SUMMARY_CONTRACT.md`): `pickup`, `revive`, `kills`,
`hits`, `stalls`, `stuck_events`, `trap_level_max`, `nav_fallback`, `decision_flips`, `time_to_first_shot`,
`link_attempts`, `link_bans`, `vantage_holds`, `net_pos_error_m`, `net_yaw_error_deg`.

## The 8 probes (coordinates)

| id | setup | assertions |
| --- | --- | --- |
| `weapon_pickup` | bot spawn (1290.12,1362.38,136.12); SCAR at (1365.2,1332.49,139.75); no enemies | pickup < 8 s; stuck == 0 |
| `stairs_roof` | spawn (1290.12,1362.38,136.12); goal sniper spot (1315.18,1388.57,141.64) | reaches within 20 s; `trap_level_max` ≤ 2 |
| `jump_link` | spawn near a=(1282.72,1513.17,143.74); goal b=(1289.57,1511.63,146.64) over the Strength link | `link_attempts` ≥ 1; `link_bans` == 0; lands ≤ 3 m of b |
| `duel_dummy` | bot vs passive dummy (2nd `wb_probe_spawn`, no AI) 15 m apart; dummy hp 100 | `time_to_first_shot` < 0.5 s; `hits` > 0; no friendly-fire |
| `cover_under_fire` | bot at cover spot (1281.84,1393.73,134.37); scripted shooter 20 m on the open side | exposure (moving in open) < 5 s total; not standing still |
| `vantage_hold` | Marksman + DSG1, spawn near sniper spot (1315.18,1388.57,141.64) | `vantage_holds` ≥ 1; hold > 5 s; leaves after damage |
| `crowd_corridor` | 8 bots spaced 2 m along a straight stretch | no deadlock: all move ≥ 20 m in 30 s; `stuck_events` == 0 |
| `net_sync` | **server + client** (not headless-only), 1 bot | `net_pos_error_m` < 0.5; `net_yaw_error_deg` < 5; ammo equal |

`ps_capture` is deferred until PS data exists (T-6/T-10); schema slot reserved (`map` allows a PS map,
`dummies[].team`, `items[].team`).

## Determinism limits (must be stated in the runner)

- `wb_seed` seeds only the bot LCG (`BotBrain::Configure`, `BotBrain.cpp:155`). World randomness
  (weapon drops, native spawn picks, physics) is **not** seeded. So probes pin items/positions via the
  `items`/`dummies`/`spawn` fields and `wb_qa_socom_only` where relevant; cross-variant comparison uses
  the same seed per slot, not bit-reproducibility.
- No `SetTimeScale`: ×8 already changes behaviour (`Docs/Research/STATUS.md:28`). Use real time.

## Missing engine hooks (Claude)

1. `wb_probe_item <class> <x y z>` — place a fixed item (needed by `weapon_pickup`); today only
   `BotWeapons::GiveTestSCAR`/`FindNearby` exist (`Code/CryGame/Bots/BotWeapons.*`).
2. `wb_probe_dummy <x y z> [hp] [team] [shoot_every_s]` — inert/scripted target for `duel_dummy` and
   `cover_under_fire`; today `wb_probe_spawn` only adds a no-AI bot (`BotManager.cpp:112,876`).
3. A `wb_probe_run <id>` command + a JSON loader that sets seed/spawns/items/cvars, waits the duration,
   then dumps metrics to `summary.json` and exits (or `wb_probe_done`).
4. `wb_net_stats` (see NET_POSE.md) for `net_sync`; this probe also needs the client harness.

## Runner responsibilities (Codex)

- One probe = one fresh server process on a free port, `+map multiplayer/ia/steelmill
  +wb_probe_local_test 1`, apply the probe JSON, wait `duration_s`, collect `*.bots.log` + `summary.json`.
- Exit code 0/1 = all assertions pass/fail; never overwrite existing evidence; tag runs with the probe
  `id` + `seed` + git SHA (mirror `Tools/Test-WarsBotsTactics.ps1` hygiene).
- Run each probe on ≥ 2 maps (Steelmill + one of Quarry/Outpost/Armada) before a feature merge; 15-bot
  paired runs stay as the integration gate, probes are the fast gate.
- CMake/ctest target for `Tests/*.cpp` (T-4) lives with this, so pure-module regressions run in CI.
