# Probe hooks: cheapest existing engine/server path (DS-007, ex-OC-028)

Read-only review of `claude-sprint` @ `293077fd`. For each missing hook in `PROBES_SPEC.md`: the cheapest
existing path, and the minimal `wb_*` command to add if none exists. No build/game run here.

## Already available (no code)

| Need | Existing path |
| --- | --- |
| put a bot at a pose | `wb_teleport <bot> x y z` → `BotManager::TeleportCommand` `BotManager.cpp:693`, `BotInstance::DebugTeleport` `BotInstance.cpp:368` (stops/restarts AI, sets TM) |
| spawn a passive actor | `wb_probe_spawn x y z` → `BotManager.cpp:112,876` (`Add(..., startAI=false)`); `wb_probe_remove` `:114` |
| spawn an AI opponent | `wb_add [n] [skill] [style]` `BotManager.cpp:96`; `wb_bot_start/stop` `:116-117` |
| revive at a spawn | `wb_revive_at <spawnId> [bot|me]` `BotManager.cpp:103` |
| suit mode (test) | `wb_bot_suit defense|speed|strength|cloak` → `BotBrain::AbilityCommand` (`BotBrain.cpp:1235`), `CNanoSuit::SetMode` `NanoSuit.h:225` |
| gait/jump/strafe | `wb_bot_gait`, `wb_bot_jump`, `wb_bot_strafe` `BotManager.cpp:120-122` |
| per-bot state/cost | `wb_status` `:100`, `wb_bot_status` `:118`, `wb_probe_status` `:114`, `BotInstance::Status` `BotInstance.cpp:186` (avg/max ms), `wb_list` `:99` |
| round limits | `g_timelimit` `GameCVars.cpp:322`, `g_fraglimit` `:327`, `g_fraglead` `:328` (0 = off) |
| demo/metrics | `wb_demo_record/start/stop/mark` `BotManager.cpp:91`, `wbdemo/2` `DEMO_FORMAT.md` |
| map/item coords | `wb_geom_export` `BotManager.cpp:107` (spawn/item JSON), `wb_nav_export` `:108` |
| verbose | `wb_debug 1`, `wb_bot_verbose 1` `BotManager.cpp:101,119` |

## Missing hooks → cheapest implementation

1. **Pose + view (`wb_probe_pose <bot> x y z [yaw] [pitch]`)** — `DebugTeleport` already moves; add
   `CPlayer::SetAngles(Ang3(pitch,yaw,0))` (`Player.cpp:5092`) or `SetWarsBotAim` (`Player.cpp:5005`) after it.
   ~10 lines in `BotManager::TeleportCommand` (optional args) or a new command.
2. **Shooting dummy (`wb_probe_dummy x y z [team] [hp] [every] [targetBot]`)** — spawn a no-AI bot
   (`BotManager::Add(..., false)`), `BotWeapons::GiveTestSCAR` (`BotWeapons.cpp:170`, already gives 180 reserve),
   `CGameRules::SetTeam` (`GameRules.h:314`), then a 10 Hz timer: `SetWarsBotAim(target)` + `IWeapon::StartFire`
   (`IWeapon.h:60`; same call as `BotCombat.cpp:263`). ~30 lines; no new AI.
3. **Item at a pose (`wb_probe_item <class> x y z [clip] [reserve]`)** — `IEntitySystem::SpawnEntity`
   (`IEntitySystem.h:233`) with the item class; or `IItemSystem::GiveItem` (`IItemSystem.h:327`) to a dummy then
   `IItem::Drop` (`IItem.h:78`). For a walk-and-pickup probe the cheapest is no code: use a real map item from
   `wb_geom_export` and `wb_teleport` the bot 8 m away.
4. **Give/strip items (`wb_probe_give <bot> <class>`, `wb_probe_strip <bot>`)** — `IItemSystem::GiveItem`
   (`IItemSystem.h:327`) / `IInventory::RemoveAllItems` (`IItemSystem.h:189`), ammo `SetAmmoCount`
   (`IItemSystem.h:224`, used in `BotWeapons.cpp:183`). ~10 lines.
5. **Suit/energy (`wb_probe_suit <bot> <mode> <energy>`)** — `CNanoSuit::SetMode` (`NanoSuit.h:225`),
   `SetSuitEnergy` (`NanoSuit.h:224`), `Activate` (`NanoSuit.h:204`); existing `wb_bot_suit` only sets a test mode
   for 8 s and cannot set energy. ~10 lines.
6. **Seed (`wb_seed <n>`)** — int cvar; `BotBrain::Configure` currently `m_rng = 0x9E3779B9u*(number+1)`
   (`BotBrain.cpp:166`); mix the cvar instead. ~5 lines. Seed still does not cover world randomness (weapon drops,
   native spawn picks) — probes must pin items/positions.
7. **Machine-readable metrics (`wb_probe_metrics [path]`)** — `BotInstance::Status` (`BotInstance.cpp:186`) prints
   text; add JSON: number, entity, state (`Brain().StateName()`), health, ammo, pos, target, `m_lastUpdateMs`
   (`BotInstance.cpp:268`), `m_maxUpdateMs`. ~20 lines; runner parses it into `summary.json`.
8. **Scenario orchestration** — no engine hook needed: the runner sends the ordered `+wb_*` commands, waits
   `duration_s`, then calls `wb_probe_metrics`/`wb_status` and collects `*.bots.log` + summary. `wb_probe_run <json>`
   is a Codex-side convenience, not a game hook.

## Notes

- Do **not** use `wb_qa_time_scale` (`BotManager.cpp:106`) for probes: ×8 changes behaviour (`STATUS.md:28`).
- `g_timelimit 0`/`g_fraglimit 0` are enough to freeze the round end; no gamerules hook.
- Freeze the bot itself with `wb_bot_stop` (AI off, actor alive) when a passive target is needed; `wb_probe_spawn`
  already creates it without AI.
- For `net_sync` the client must run the same `wbdemo/2` sidecar; that is a runner/Codex task, not a server hook.
