# Bot code refactor audit (read-only), 2026-10-24

Scope: `Code/CryGame/Bots` + `Tests` on `warsbots/feature/weapon-goals` @ `fb578d52` (src). 4,566 lines, 40 files,
164 functions, 9 C++ test files + 2 Python tests. No TODO/FIXME markers. Nothing here is to be done before
roadmap step 1 (spawn 195) closes; each item is one Codex commit with a paired soak and an unchanged
`summary.json` (behaviour must not move).

## Findings, by payoff

| # | Finding | Where | Proposal | Risk |
| --- | --- | --- | --- | --- |
| R1 | **`BotBrain::Update` is 361 lines** — the whole FSM (10 Hz tick, weapon seeking, roaming, combat, trap recovery, test hooks) in one function; next-longest function is 67 lines | `BotBrain.cpp:202–563` | split by state into `UpdateSeeking/UpdateRoaming/UpdateCombat/UpdateRecovery` with the same order of calls; keep `Change()` the only state writer. Precondition for the decision layer (roadmap 6) and for fixing CHASING↔MOVING_TO_WEAPON flips | medium: pure move, verify with paired soak |
| R2 | **`BotManager.cpp` 911 lines, 25 console commands** + lifecycle + spawn choice + IA participants | `BotManager.cpp:84–142`, `*Command` | move commands into `BotConsole.cpp` (thin parsers calling BotManager), keep BotManager = lifecycle/registry | low |
| R3 | **Cvars have two sources of truth**: registered into globals (`g_trapRecovery`, `g_reachableGoals`, …) but read in hot paths via `GetCVar("wb_…")` string lookups | `BotBrain.cpp:287,565,579,584`, `BotNav.cpp:402,420,443–444`, `BotInstance.cpp:193,288`, `BotSuit.cpp:10` | one `BotConfig` struct registered once, read by reference; removes per-tick string lookups and rename bugs | low |
| R4 | **Experiment and QA flags accumulate** (all default 0): `wb_progress_experiment`, `wb_qa_socom_only`, `wb_revive_z_test`, `wb_revive_hooks_test`, `wb_hotspot_all_nodes`, `wb_reachable_goals`, `wb_sink_escape`, `wb_bot_cloak_experimental` | `BotManager.cpp:96–112` | decide per flag after its experiment: promote (owner, D-002), keep as diagnostic, or delete with its code. Candidates to delete once C-002 closes: `wb_revive_z_test` (0 offset did not help), `wb_revive_hooks_test` (fold into the real revive path if it fixes 195). `wb_reachable_goals` parked until graph repair | low per flag |
| R5 | **Legacy single-bot commands** `wb_select`, `wb_probe_*` (4), `wb_bot_*` (9) duplicate `wb_add`/`wb_list`/`wb_revive_at`/`wb_teleport` | `BotManager.cpp:129–142` | keep `wb_bot_jump/strafe/gait/suit` (ability tests), merge `wb_probe_*` into `wb_revive_at`/`wb_add`; mark the rest deprecated for one release, then delete | low; check QA scripts first (`Tools/*.ps1`) |
| R6 | **Mixed code style**: newer code is dense (`const int next=age>=45.f?4:…;`, no spaces, one-line classes), older code is the CryEngine style | `BotTrapWatchdog.h`, `BotReachability.h`, `BotNav.cpp:400+`, `BotWeapons.cpp:84+` | one formatter pass (`.clang-format` from the older files), separate commit, no logic | none (whitespace only) |
| R7 | **Logging is spread**: 66 `WB_LOG` calls in BotManager, 28 BotBrain, 26 BotInstance; line formats differ (`k=v.` vs `k=v` vs free text); Triage parses them by regex | all | a small `BotEvent` helper that always writes `[WarsBots] EVENT k=v k=v` (+ actor); add new events only through it; keep old formats until Triage is switched | low; coordinate with Triage |
| R8 | **Revive path lives in two places**: automatic `BotInstance::Revive` (60 lines) and `ReviveAtCommand` (67 lines) | `BotInstance.cpp:259`, `BotManager.cpp:714` | after C-002: one `ReviveAt(spawn, actor, options)` used by both | medium: touches the 195 fix |
| R9 | Test coverage is good for pure helpers (aim, fire, projection, hotspot, physics probe, reachability, spectator, watchdog) and absent for the FSM, BotNav build/Walkable and BotCombat | `Tests/` | after R1: table tests for state transitions on recorded blackboard inputs; BotNav: graph build on a tiny synthetic physics world is not possible offline → rely on export + Triage | — |

## Order

R6 (format) → R3 (config) → R2 (console split) → R5 (legacy commands) → R4 (flag cleanup, as experiments close)
→ R1 (Update split, before the decision layer) → R8 (after C-002) → R7 (with Triage). Each step: build, unit tests,
one paired 15-bot 660 s soak, `summary.json` within noise of the baseline.
