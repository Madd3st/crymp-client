# DS-078: C145 observation recipe review (actorless `wb_debug2` failure) — OK with corrected checks

Read-only source review of `f66406d2` (code unchanged) plus the proposed C145 recipe. No game, build or install.

## Verdict

The preflight failure is a **recipe** bug, not a code bug: `wb_debug` is a console **command** that accepts only
`0|1` (`BotManager.cpp:887-894`; usage line `:891`). `wb_debug 2` prints `Usage: wb_debug 0|1.` and changes nothing,
so an actorless preflight records no effective value. Do **not** patch the command to accept 2.

## Verified source facts

| claim | source |
| --- | --- |
| `wb_debug` is a command, 0/1 only | `BotManager.cpp:142` registration, `:887-894`; `m_defaultVerbose` applies to existing bots and to later `AddWith` (`:416`) |
| confirmation is `wb_status`, not a CVar | `StatusCommand` prints `defaultVerbose=%d` (`BotManager.cpp:876-885`); there is no `wb_debug` CVar entry |
| `wb_log` is a real CVar (0/1/2) | `BotConfig.cpp:61` (`wb_log` -> `logLevel`) |
| telemetry always reaches the file | `BotLog.cpp`: every level is written to `<log>.bots.log`; console/main log only when `level <= logLevel` |
| `wb_move_trace` is a real CVar (1/2, default 0) | `BotConfig.cpp:30`; `BotMoveTrace` samples server/bot rows at 4 Hz, `moveTraceHz` only affects the client human (`BotMoveTrace.cpp:20-22`) |
| PS now reaches `Visit` | `f66406d2` eligibility (`BotMatch::IsPowerStruggle`/`IsBotMode`); DS-077 OK |
| `wb_add 2` gives NK #1 and US #2 | `BotTeamPlan.h:9-12` odd slot -> team 1 (tan/NK), even -> team 2 (black/US); `wb_remove <botNumber>` is by number, not EntityId (`BotManager.cpp:727-735`) |
| native spawn, no teleport | `AddWith` uses `GetTeamDefaultSpawnGroup`/`GetSpawnLocation` (`BotManager.cpp:386-408`); `wb_teleport` is a separate manual command and must not be used |

## Corrected recipe checks (replace `wb_debug2` everywhere)

1. Local PS server, existing gate (`wb_probe_local_test=1`), `wb_bot_navmesh 1`, diagnostic candidate with manifest,
   byte hashes and full restore; no promotion.
2. `wb_debug 1` (command, exactly one argument).
3. `wb_status` -> require `defaultVerbose=1` (never grep a `wb_debug` CVar; it does not exist).
4. `wb_log 2` (optional, CVar) so `WB_LOG(2)` telemetry also appears in the main log; `.bots.log` already has it.
5. `wb_move_trace 1` (CVar). Server/bot rows are 4 Hz; `wb_move_trace_hz` is irrelevant for them.
6. `wb_add 2`; `wb_list` must show #1 tan/NK and #2 black/US; `wb_remove 1`; `wb_list` must still show #2 US.
7. Keep the retained US #2 for <=180 s; no teleport, no respawn trick, no second run without a new named question.
8. Evidence: `MOVE TRACE ... actor=<#2> kind=bot ... swim= waterZ= relativeWater= bottomDepth= underwater=` rows in
   `.bots.log` (and main log with `wb_log 2`), plus `wb_status`; missing fields are UNKNOWN, not PASS.

## Unknowns (not covered by this recipe)

* Upper/lower US spawn coverage, door passage and native capture membership are separate observations; this run only
  captures the water/depth fields for one US bot.
* The trace reports native swim state, not proof of ascent or a full route.
* Candidate `653f5e1e` install/restore manifest and hashes are QA-owned; not verified from this branch.

No code patch is needed for DS-078; the recipe correction above is the deliverable.
