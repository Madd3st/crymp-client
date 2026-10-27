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
| `landing_z` | same `LINK done` line | z of the last completed link; **NaN/unknown before any LINK**, never a fake 0; `ge` then fails |
| `link:failed` | `LINK failed kind=... reason=...` (`BotBrain.cpp:1416`) | all failed links |
| `unreachable` | `GOAL SKIP unreachable ...` (`BotWeapons.cpp:171`) or `weapon %u unreachable; skipping it` (`BotBrain.cpp:766`) | count of rejected unreachable weapon goals |

`LINK done` is `WB_LOG(2)`, but `BotLog.cpp:47-60` writes every level to the trace file and feeds
`ProbeObserveLine`; no `wb_log` cvar change is needed.

## Operators

* `ge_ratio_of`: parses the fixture's `"other":"kills:2"`. Preserve legacy
  `Run-WarsBotsProbe.ps1:125`: `lhs > 0 && lhs >= factor*rhs`, so a positive `2:0` passes, `0:0` fails and a
  missing/invalid side fails. `actual` stays finite (ratio when `rhs>0`, `lhs` when `rhs==0`); JSON also carries
  `lhs`/`rhs` so a result can be recomputed. Never emit `inf`/`nan` JSON.
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
* a loop over the five `Tools/Probes/*.json`: every assertion metric is `SupportsMetric` and every operator is
  supported. Missing fixture files **fail** the binary (`CHECK(pf != nullptr)`), so an empty CWD is nonzero and the
  repo root passes.
* `landing_z` is `SupportsMetric` before any link but `Value`/`Evaluate` is NaN/unknown, not a fake 0;
* `link_failed` JSON roundtrip after one real `LINK failed` line is `1`, not null.

RED baseline is the C143 result above (`kills:1`, `pickup:DSG1`, `link:jump:done`, `landing_z`, `link:failed`,
`unreachable` are NaN; `ge_ratio_of`/`le_seconds` fall through). No build/run was performed here per DS-059; Codex
CI must confirm the test compiles and passes.

## DS-061 revision (pre-integration review)

* Ratio: legacy `lhs > 0 && lhs >= factor*rhs`; tests `2:0` PASS, `0:0` FAIL, `3:1` PASS, `2:1` FAIL,
  missing/invalid `other` FAIL; JSON has finite `actual` plus `lhs`/`rhs`, never `inf`/`nan`.
* Export: `link_failed` is an alias of `link:failed` in `Value`; `Runtime::Json` now measures and emits `1` after a
  real failed link (test checks the JSON string, not only the support list).
* Availability: `SupportsMetric` is separate from `Value`; `landing_z` is supported but NaN before a LINK. The
  five-fixture loop uses `SupportsMetric`, and a missing fixture fails closed.
* The patch was regenerated from the reviewed `532e4497`; no gameplay/default/flag changes. The C143 oracle must
  use `SupportsMetric` for name/support checks: `Value("landing_z")` is deliberately NaN until a LINK, so a
  `Value`-NaN check would mislabel a supported metric as unsupported.

Patch: `Docs/Research/DeepSeek/patches/ds059_probe_contract.patch`, applies clean on `532e4497`.
