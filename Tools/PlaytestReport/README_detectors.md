# detectors.py (DS-032 / DS-037 #5)

```
python Tools/PlaytestReport/detectors.py <Server.bots.log> [--out dumbness.json] [--marks marks.json]
python -m unittest Tools/PlaytestReport/test_detectors.py
```

Read-only. Emits `warsbots-dumbness/1` with `events`, `marks`, `summary` (per detector, hit rate, events/10 min).
Detectors: oscillation, flapping, freeze, wall/trap, shooting into cover, ignoring a visible target, standing in the
open under fire, goal stall. Thresholds and false-positive twins are in `Docs/Research/DeepSeek/DUMBNESS_DETECTORS.md`.

Notes:
- `MOTION INTENT` and `AI decision` have no actor id in the current log; the matching detectors use `MOVE TRACE`,
  `TARGET CHOICE`, `FIRE HIT` and `TRAP RECOVERY`. The wall detector therefore uses two trap recoveries ≤ 3 s.
- `detectors.py` needs `FIRE SHOT`/`FIRE HIT`; C-106 has no fire diagnostics, C-108 does.
- `test_detectors.py` is self-contained (synthetic log lines); CI/Codex runs it. This machine has no Python, so it was
  not executed here.
- Marks: `OWNER MARK` lines in the log are joined within ±5 s; `--marks` accepts a JSON list of `{t,text}`.
