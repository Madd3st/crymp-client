# C-126 probe recipes: scenario `setup` + optional `wb_probe_run` seconds (DS-053 #2)

Base `96f7f624`. X-184 blocker: the native scenario parser dropped the repo JSON `setup` array and `wb_probe_run`
required two arguments, so the C-126 recipes could not run as written.

## Where the blocker was

- `Probe::ParseScenario` — `Code/CryGame/Bots/BotProbeRunner.h:192`, parsed id/map/duration/cvars/bots/dummies/items/
  bursts/peeks/cloaks/assert/assertions/goal (through `:296`) but not `setup`.
- `Probe::BuildSetupCommands` — `:298`, emitted cvars and `wb_probe_spawn`/`wb_probe_dummy`/`wb_probe_item`... but no raw
  recipe commands.
- `BotManager::ProbeRunCommand` — `Code/CryGame/Bots/BotManager.cpp:1194`: `GetArgCount() != 3` refused
  `wb_probe_run <id>`; usage line `:1199`.

## Patch

- `Scenario::setup` vector — `BotProbeRunner.h:178`.
- parse the `setup` strings — `:272-274`.
- emit them after the cvars, skipping any `wb_probe_run ...` — `:303-306` (the runner starts the metrics window itself).
- `wb_probe_run <id> [seconds]` — `BotManager.cpp:1198-1207`: 2 or 3 args; with 2 args use the loaded scenario
  `duration`, else 60 s; console help updated at `:162`.
- `wb_probe_start` already executes every `BuildSetupCommands` entry that `ProbeSetupSupported` allows
  (`BotManager.cpp:85-95`, `:1179-1185`), so `wb_add`/`wb_teleport`/`wb_probe_item` now run from the recipe unchanged.
- test `Tests/BotProbeRunnerTests.cpp:111-118` parses a recipe with `setup` and checks both commands reach the built
  list.

`Tools/Probes/*.json` need no edits. A recipe-side workaround (moving the commands into `cvars` or an external pre-run
script) was not needed and would have left the native gap in place.

## Verified

- `ds053.patch` applies clean on `96f7f624`.
- BotBrain/BotManager/BotConfig compile with MSVC (`WARSBOTS_EXPERIMENTAL_SPAWNER`), both in the author tree and in a
  fresh `96f7f624` worktree with the patch applied.
- 43 engine-free suites pass under MSVC; `BotAimControlTests`/`BotProfileTests` need engine headers and were not run.
- Lua not executed; no C-126 probe playtest here.
