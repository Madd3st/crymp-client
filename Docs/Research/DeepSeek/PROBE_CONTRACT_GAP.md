# DS-059: native probe contract gap (base 532e4497)

C143/codex oracle: `Docs/Coordination/C143_NATIVE_CONTRACT.json`, raw `build/coordinator-c143/result.log`: 13
unsupported occurrences over the five real fixtures. This patch adds only the missing diagnostic names/operators;
`wb_probe_runner` stays 0 and no gameplay decision, weapon, HP or spawn path changes.

## Metrics and truthful producers

| Metric | Existing/added log line | Semantics |
| --- | --- | --- |
| `kills:<botN>` | `#N combat kill shooter=... target=...` (`BotManager.cpp:280`) | per-shooter kill count; total `kills` unchanged |
| `pickup:<class>` | `REAL PICKUP actor=... item=... acquired=1 class=<Class>` (`BotWeapons.cpp:193`) | count of **acquired** pickups of that class; `acquired=0` ignored |
| `link:<kind>:done` | `LINK done kind=<kind> at=(x,y,z) actor=...` (`BotBrain.cpp:1408`) | count per `drop`/`jump`/`strength`/`lift`/`ladder` |
| `landing_z` | same `LINK done` line | z of the last completed link; 0 when no link (then `ge` fails, not passes) |
| `link:failed` | `LINK failed kind=... reason=...` (`BotBrain.cpp:1416`) | all failed links |
| `unreachable` | `GOAL SKIP unreachable ...` (`BotWeapons.cpp:171`) or `weapon %u unreachable; skipping it` (`BotBrain.cpp:766`) | count of rejected unreachable weapon goals |

`LINK done` is `WB_LOG(2)`, but `BotLog.cpp:47-60` writes every level to the trace file and feeds
`ProbeObserveLine`; no `wb_log` cvar change is needed.

## Operators

* `ge_ratio_of`: parses the fixture's `"other":"kills:2"`. `actual = left / other`; denominator 0 or missing is
  **fail**, never a pass and never `0/0`; a missing left metric is fail.
* `le_seconds`: time of the **first** event, currently `pickup:<class>` (`firstPickupByClass`); no event is NaN ->
  fail. The count remains available as `pickup:<class>` for `eq`/`ge`; this keeps count vs first-time semantics
  separate.
* Unknown operator is now rejected instead of falling through to equality.

## Minimal instrumentation required

Only `pickup:<class>` had no truthful producer: `REAL PICKUP` carried item id, not class. The patch adds
`class=%s` from the item entity class to that existing level-2 line. It changes no pickup behavior. If that log
field is rejected, `pickup:<class>` must stay explicitly unsupported; aliasing it to the global pickup count or to
`WEAPON RANK select` would fabricate evidence and weaken the fixtures.

`unreachable` uses two existing lines. DS-051 removes the graph fallback eventually; if both lines disappear, the
metric has no truthful producer and must be re-instrumented, not mapped to 0.

## Regression tests (RED on 532e4497, GREEN after)

`Tests/BotProbeRunnerTests.cpp` adds:
* `other` parse and `ge_ratio_of` pass/fail (3:1 passes, 4:1 fails);
* per-class acquired pickup counts and first-time `le_seconds` (2 s passes 2.5, fails 1.5; missing class fails);
* `LINK done` kind/z and `LINK failed`; `unreachable`;
* an unknown operator rejects;
* a loop over the five `Tools/Probes/*.json`: every assertion metric returns non-NaN and every operator is
  supported on an empty `Metrics`.

RED baseline is the C143 result above (`kills:1`, `pickup:DSG1`, `link:jump:done`, `landing_z`, `link:failed`,
`unreachable` are NaN; `ge_ratio_of`/`le_seconds` fall through). No build/run was performed here per DS-059; Codex
CI must confirm the test compiles and passes.

Patch: `Docs/Research/DeepSeek/patches/ds059_probe_contract.patch`, applies clean on `532e4497`.
