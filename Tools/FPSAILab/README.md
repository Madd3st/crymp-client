# FPS AI Lab

Engine-free prototype of a tactical decision layer for WarsBots: utility goal selection with
commitment, a bounded symbolic sequencer, navigation-verified actions and per-bot failure memory.
Original code; see `Docs/Research/FPSAILab/FPS_AI_COMPARISON.md` for sources and licences.

```
fpsai/core.h      Observation (bot-perceivable only), Facts, DeriveFacts
fpsai/planner.h   ActionDef, bounded forward planner (depth 4, 256 nodes)
fpsai/brain.h     goals, Scorer interface, commitment, executor, decision log
fpsai/legacy.h    decision-level model of current BotBrain rules, for comparison
fpsai/sim.h       crude closed-loop scenario world + metrics (not game results)
fpsai_bench.cpp   scenario table + 15-bot cost stress -> Markdown
```

```
g++ -std=c++17 -O2 Tests/FPSAILab/test_fpsai.cpp -o build/test_fpsai && build/test_fpsai
g++ -std=c++17 -O2 Tools/FPSAILab/fpsai_bench.cpp -o build/fpsai_bench && build/fpsai_bench --timeline
cl /std:c++17 /EHsc /O2 Tests\FPSAILab\test_fpsai.cpp        (MSVC)
```

Docs: `Docs/Research/FPSAILab/` — FEAR_AI_ARCHITECTURE, FPS_AI_COMPARISON, WARSBOTS_AI_GAPS,
TACTICAL_PLANNER_DESIGN (decision, results, integration plan, A/B contract), ML_INTEGRATION_OPTIONS,
BENCH_RESULTS.
