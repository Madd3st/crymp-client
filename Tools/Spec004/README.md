# SPEC-004: offline QA preparation

Files are scenario recipes, not an engine runner. No game/server starts from this directory.
Plan: (1) preserve the pinned failed runtime evidence; (2) test/implement strict trace parsing; (3) freeze recipes; (4) hand off unresolved run gates. No CI/game build is needed for pure Python.

`python -B Tests/test_spec004_trace.py` runs synthetic trace tests.
`python -B Tools/Spec004/trace.py <single trace.bots.log> --unstick 1 --level Multiplayer/PS/Shore` prints JSON, exit 0 only for a valid final level-end with p95<=2 ms; otherwise exit2.
Requires Python standard library only. Does not launch processes, mutate input logs, set environment variables or write live Memory/Navmesh.

The new owner request requires **level-end**. A last level report is reported as NOT_MEASURED; earlier 06.11 runs used the previously authorized last-level fallback. Do not relabel those as strict PASS. Window reports cannot override the final level-end; a later level means the current level is incomplete. Require mode/map selection, n>0, finite ordered percentiles, dropped0 and virtual filter. A high max alone does not fail a p95 gate.

UNSTICK step/exit/failed/abort are event-line counts. MEMORY is excluded. An exit event is not proof of physical arrival. Parse one complete trace, not main+trace together. Truncated final lines are ignored. This parser does not grade stalls, route diversity, solo-memory/combat conditions, kills or animations; use source-aware scenario analysis for those. Original stops.awk has no EOF flush and can undercount censored episodes.

Recipes keep 6x300 per phase. The memory recipe reuses only its own namespace between paired phases; OFF never consumes learned unstick memory as a baseline. NAV fallback<=26/12min requires a dedicated 720s observation, not scaling 300s. Steelmill's phrase "within noise" still needs a numerical tolerance frozen by the owner before PASS/FAIL. No launch is authorized by JSON: exact CI pair, source review, unchanged data hashes, free game (D-044), and owner-approved recipe remain prerequisites. Current pin 3ba5119c is functionally FAIL and launch_allowed=false.
