# Probe runner core (DS-045 #3)

Engine-free, header-only: `Code/CryGame/Bots/BotProbeRunner.h`. Tests: `Tests/BotProbeRunnerTests.cpp`. Patch:
`patches/ds045_probe_runner.patch` on `6e2713f9`; CI list gets `BotProbeRunner`.

## API

- `ParseJson(text, Json&, error)` — minimal probe-schema JSON parser (objects, arrays, strings, numbers, bool, null).
- `ParseScenario(text, Scenario&, error)` — reads `id`, `map`, `seed`, `duration_s`, `bots`, `dummies`, `items`,
  `bursts`, `peeks`, `cloaks`, `cvars`, `goal`, `goal_radius`, `assertions`.
- `BuildSetupCommands(scenario)` — returns the console lines in deterministic order: cvars, spawns, dummies, items,
  goal, `wb_probe_run <id> <duration>`.
- `Metrics` — `Begin(t)` and `ObserveLine(line)` over `*.bots.log`; tracks shots/hits/kills/deaths/pickups/revives,
  first-shot delay, goal time, stalls, stuck, trap level, link attempts, vantage holds, glances, heard, target/hit
  distances, headshots, accuracy, percentiles.
- `Evaluate` / `EvaluateAll` — `metric` + `op` (`lt/le/gt/ge/eq/ne`) + `value` from the probe JSON.

## Hook commands emitted

| Command | Arguments | Meaning |
| --- | --- | --- |
| `wb_probe_spawn` | `x y z [skill] [style] [ai]` | spawn one probe bot |
| `wb_probe_dummy` | `x y z [hp] [team] [shoot_every_s] [weapon]` | inert/scripted no-AI actor |
| `wb_probe_item` | `class x y z` | place a fixed item |
| `wb_probe_goal` | `x y z [radius]` | nav goal; logs `PROBE GOAL reached` |
| `wb_probe_burst` | `actor shots interval` | scripted burst (bait probe) |
| `wb_probe_peek` | `actor x y z expose_s hide_s` | scripted peek (corner probe) |
| `wb_probe_cloak` | `actor 0|1` | force cloak on a dummy/human |
| `wb_probe_run` | `id duration_s` | run the scenario, write `summary.json` |

`summary.json` / assertions use the metrics in `Metrics::Value`: `shots`, `hits`, `kills`, `deaths`, `pickup`,
`revive`, `time_to_first_shot`, `goal_time`, `stalls`, `stuck_events`, `trap_level_max`, `link_attempts`,
`vantage_holds`, `glances`, `heard`, `detected_at_m`, `exposed_s`, `accuracy`, `headshots`, `hit_distance_p50/p90`,
`target_distance_p50/p90`.

## Tests

`BotProbeRunnerTests` parses an inline scenario, checks every setup command, feeds synthetic log lines (including the
duplicate `FIRE HIT phase=after-Lua`), checks metrics/assertions/malformed JSON and parses
`Tools/Probes/exploit_height_camp.json` when run from the repo root. Built with MSVC 19.44 (`/std:c++17 /permissive-`):
all pass. GCC is not installed on this PC; the header uses only standard C++17 (no MSVC extensions), so the same source
is GCC-clean by construction.
