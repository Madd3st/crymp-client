# Probe runner wiring (DS-047 #3)

`BotProbeRunner.h` was a header-only core with no engine caller (C-120b: "BotProbeRunner NOT WIRED"). This patch
adds the host behind `wb_probe_runner 0`; the core stays engine-free and the same file is still unit-tested.

## Flow

1. `wb_probe_runner 1` (cvar, `BotConfig.cpp`; default 0) enables the host. All commands below refuse when it is 0.
2. `wb_probe_load <scenario.json>` reads and parses the file (`BotManager.cpp`, `ProbeLoadCommand`). `assert` is
   accepted as an alias of `assertions` (`BotProbeRunner.h`, `ParseScenario`), so `Tools/Probes/*.json` load as-is.
3. `wb_probe_start` executes `Probe::BuildSetupCommands` on the console, skipping the trailing `wb_probe_run`,
   then starts the metrics window. `wb_probe_spawn` now accepts the scenario form
   `x y z [skill] [style] [0|1]`; the old three-argument form is unchanged.
4. Every line written by `WarsBots::Log` is passed to the runner (`BotLog.cpp` -> `ProbeObserveLine` ->
   `BotManager::ObserveProbeLine`). Only a running scenario parses it; there is no recursion (the parser never logs).
5. `BotManager::UpdateProbe` (called from `OnPostUpdate`) checks the `wb_probe_goal` marker at 10 Hz, logs
   `PROBE GOAL reached`, and when the duration expires evaluates the assertions, writes
   `%LOCALAPPDATA%/WarsBots/Logs/probe-result-<id>-<yyyymmdd-hhmmss>.json` and logs
   `PROBE RESULT id=... pass=... assertions=n/m file=...`.
6. `wb_probe_run <id> <seconds>` starts the window without a file for an external setup script; the loaded
   scenario's assertions are kept if one is loaded.
7. `wb_probe_runner_status` prints flag/loaded/active/id/remaining and live shots/hits/kills/deaths/pickups/revives.
8. `wb_probe_stop` stops without writing a result.

## Status and limits

- Unsupported setup verbs are refused with `PROBE UNSUPPORTED <command>` and counted in `PROBE START ... unsupported=N`:
  `wb_probe_dummy`, `wb_probe_burst`, `wb_probe_peek`, `wb_probe_cloak`. The scenario still runs with the supported
  subset (cvars, `wb_probe_spawn`, `wb_probe_item`, `wb_probe_goal`), and the refusal is explicit in the log rather
  than silently changing semantics. Implementing those four needs actor/weapon plumbing that does not exist yet.
- The result JSON is schema 1: `id`, `map`, `started_at`, `duration_s`, `elapsed_s`, `pass`, `metrics` (23 values),
  `assertions` (`metric`, `op`, `expected`, `actual`, `pass`).
- `revive` counts both `revive result ... alive=1` and the new `PS REVIVE SEEN` (DS-047 #1), so the PS wave is not
  miscounted as zero.

## Verification

- `Tests/BotProbeRunnerTests.cpp`: existing scenario/command/metric tests plus `assert` alias, `PS REVIVE SEEN`,
  runtime load/start/due/finish, pass=true and pass=false JSON. MSVC `/std:c++17 /permissive-`: all pass.
- `BotConfig.cpp`, `BotLog.cpp`, `BotInstance.cpp`, `BotManager.cpp` compile with `WARSBOTS_EXPERIMENTAL_SPAWNER=ON`
  (Ninja, MSVC 14.44) in the `d6a9a521` worktree.
- Two disjoint patches: `ds047_ps_revive.patch` (BotInstance.h/.cpp) and `ds047_probe_wiring.patch` (host + core +
  test). Each applies alone on `d6a9a521`, and both apply together in either order.
- Not run: an actual engine probe with a scenario file (needs a DLL copied into the game). The patch is default-OFF,
  so no existing run changes.
