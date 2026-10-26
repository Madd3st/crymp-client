# Scenario probes v1 (T-4, D-021 #2)

Minute-long checks instead of 11-minute soaks. Spec: `Docs/Research/DeepSeek/PROBES_SPEC.md` (deepseek branch).
One probe = one fresh local IA server (`+map multiplayer/ia/steelmill +wb_probe_local_test 1`), the `setup` console
lines in order, wait `duration_s`, read `*.bots.log`, evaluate `assert`. Runner: Codex (`Tools/Run-WarsBotsProbe.ps1`).

Engine hooks available: `wb_add [n] [skill] [style]`, `wb_teleport <bot#> x y z`, `wb_probe_item <class> x y z`
(new, 1dc… later), `wb_remove_all`, every `wb_*` cvar. Metrics come from existing log lines (the runner greps):

| metric | log line |
| --- | --- |
| `pickup:<class>` | `picked up` / `WEAPON PICKUP` with the class |
| `unreachable` | `weapon N unreachable; skipping it` |
| `kills:<bot#>` | kill events per killer number |
| `stalls` | `TRAP RECOVERY` lines |
| `glances` | `LOOK TRACE ... reason=glance` (needs `wb_move_trace 1`) |

Probes are deterministic only in setup; the game RNG is per process (no seed API yet) — run each probe 3× and use
the median, as the audit suggests.
