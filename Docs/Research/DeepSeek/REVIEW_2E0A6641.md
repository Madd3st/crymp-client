# Review 2e0a6641 (DS-065)

Exact `2e0a66414305b163848c6d5f18bb5812df4462a9`, parent `532e4497`.

* Diff `532e4497..2e0a6641` is byte-identical (SHA256
  `5C1D59EB6ACEC0D36E648C26E799ADC6D4ED72113E891C953EA974B616E941EC`) to
  `patches/ds059_probe_contract.patch` at `2ef03358`: 3 files, +236/-7, no Lua, no new flag/default.
* Producers: `#N combat kill` -> `kills:N`; `REAL PICKUP ... acquired=1 class=<Class>` (added `class=` field) ->
  `pickup:<class>` count and first time; `LINK done/failed` -> `link:<kind>:done`, `link:failed`, `landing_z`;
  `GOAL SKIP unreachable` / `unreachable; skipping it` -> `unreachable`.
* JSON: ratio 2:0 uses finite `actual=lhs` plus `lhs`/`rhs`; 0:0/missing/invalid fails; no inf/nan. `link_failed`
  aliases `link:failed` and the runtime JSON roundtrip measures 1 after a real failed LINK.
* `SupportsMetric` is separate from `Value`: `landing_z` is a supported name but NaN/unknown before a LINK; the
  five-fixture test uses support, and a missing fixture is `CHECK(pf != nullptr)` (empty CWD nonzero).
* Setup/observation semantics unchanged: `setup` raw array still runs; `wb_probe_runner` stays 0; no gameplay
  decision/weapon/HP/spawn edit. The only non-runner change is the pickup log class field, which is diagnostic.
* Residual: `landing_z` is the last completed link, not filtered by link kind. The fence fixture has one expected
  jump; a multi-link run should confirm the last link is still the graded jump. Not a blocker for this review.

Verdict: `review 2e0a66414305b163848c6d5f18bb5812df4462a9: OK` for the DS-061 integration, subject to exact-SHA CI.
