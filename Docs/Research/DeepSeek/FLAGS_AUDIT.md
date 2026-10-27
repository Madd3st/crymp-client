# Flags audit (DS-050 #1)

52 behaviour cvars (all `wb_*` except `wb_maxbots`). Evidence: **C119** = `6e2713f9` owner playtest receipt,
**C121** = `d6a9a521` 15-bot/PS, **code** = only code/probe evidence. C-119 preset = `Play-WarsBotsC119.ps1`
(`+wb_net_ext 1 +wb_bot_difficulty 1 +wb_bot_finish 1 +wb_bot_spectator_aim 1 +wb_bot_path_progress 0
+wb_bot_pro_fire 1 +wb_bot_high_ground 1 +wb_bot_threat_scan 1 +wb_bot_jump_links 1 +wb_bot_state_commit 1`,
vantage 0, corners/jump_sanity defaults 1, bot_pace 1).

| Flag | Default | Evidence | Exit |
| --- | --- | --- | --- |
| `wb_probe_local_test` | 0 | gate; C101+ every run | keep (security) |
| `wb_log` | 1 | diagnostics | keep |
| `wb_skill` | 1 | wb_add default | keep |
| `wb_demo_record` | 0 | C119/C121 demos | keep diagnostic |
| `wb_demo_hz` | 20 | C121 20 Hz | keep diagnostic |
| `wb_trap_recovery` | 0 | C121 1, 0 traps>=60 s | keep QA |
| `wb_physics_diagnostics` | 0 | C121 | keep diagnostic |
| `wb_move_trace` | 0 | C119/C121 1 | keep diagnostic |
| `wb_nav_knee_fix` | 0 | audit-only | keep diagnostic |
| `wb_spawn_exclude195` | 1 | C119/C121 1; 0 frozen spawns | delete -> hardcode ON |
| `wb_revive_guard` | 1 | C119 1; 0 frozen revives | delete -> hardcode ON |
| `wb_bot_pace` | 1 | C119 1; native pace | delete -> hardcode ON |
| `wb_reachable_goals` | 0 | C121 0 | keep diagnostic |
| `wb_hotspot_all_nodes` | 0 | QA only | keep diagnostic |
| `wb_fire_diagnostics` | 0 | C121 1 | keep diagnostic |
| `wb_qa_socom_only` | 0 | QA only | keep |
| `wb_bot_look` | 0 | C119 preset, C121 1 | keep probation |
| `wb_bot_turn_cap` | 0 | C119 preset | keep probation |
| `wb_bot_high_ground` | 0 | C119 1 (3 starts) | promote 1 |
| `wb_bot_pro_fire` | 0 | C119 1 | promote 1 |
| `wb_bot_state_commit` | 0 | C119 1; mode 2 unused | promote 1; delete >=2 path |
| `wb_net_ext` | -1 | C119 1; wire-compatible | keep (auto) |
| `wb_bot_weapon_rank` | 0 | C119/C121 1 | keep probation |
| `wb_bot_arbiter` | 0 | C119 0 | keep diagnostic |
| `wb_bot_perception` | 0 | C120b/C121 1 | keep probation |
| `wb_bot_aim_model` | 0 | C119 preset | keep probation |
| `wb_bot_suit_policy` | 0 | C119 preset | keep probation |
| `wb_bot_client_smooth` | 0 | C119 1 | promote 1 |
| `wb_bot_navmesh` | 0 | C119/C121 1 | keep probation |
| `wb_bot_jump_links` | 0 | C119 1 | promote 1 |
| `wb_bot_weapon_handling` | 0 | C119 preset | keep probation |
| `wb_bot_name_profile` | 0 | C119 preset | keep |
| `wb_bot_vantage` | 0 | C119 forces **0**; still loads spots for high_ground/threat_scan | keep 0 (not dead) |
| `wb_bot_tactics` | 0 | C119 preset | keep probation |
| `wb_bot_path_progress` | 0 | C119 0; C121 snipers 0 pickups | keep diagnostic |
| `wb_bot_jump_sanity` | 1 | C119 requires default 1 | delete -> hardcode ON |
| `wb_move_trace_hz` | 4 | C119 owner trace | keep diagnostic |
| `wb_bot_threat_scan` | 0 | C119 1 | promote 1 |
| `wb_bot_spectator_aim` | 0 | C119 1 | promote 1 |
| `wb_bot_difficulty` | 0 | C119 1 | promote 1 |
| `wb_bot_finish` | 0 | C119 1 | promote 1 |
| `wb_bot_first_primary` | 0 | C120b/DS-044 only | keep probation |
| `wb_bot_withdraw_cover` | 0 | C121 abort 85/189 | keep probation |
| `wb_bot_finish_lock` | 0 | C121 0 cases in IA, 2 logs in 15-bot | keep diagnostic |
| `wb_bot_sniper_reach` | 0 | C121 0/9; route fixed in DS-048 | keep probation |
| `wb_probe_runner` | 0 | DS-047 wiring only | keep diagnostic |
| `wb_bot_link_failfast` | 0 | C125 gated; DS-048 data fix | keep probation |
| `wb_ps_capture` | 0 | DS-048 planner only | keep probation |
| `wb_bot_withdraw_full` | 0 | DS-049 per-death analysis only | keep probation |
| `wb_ps_buy` | 0 | DS-049 chooser only | keep probation |
| `wb_bot_corners` | 1 | C119 requires default 1 | delete -> hardcode ON |
| `wb_progress_experiment` | 0 | C119 0; only `NoProgress` recovery | **deleted by `ds050.patch`** |

The patch deletes `wb_progress_experiment` (no effect at C-119; the only call returned false at 0). The
hardcode-ON candidates are behaviour-identical with C-119 but touch five modules; they are listed as the next
mechanical change, not mixed into this fundamentals patch.
