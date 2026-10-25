# T-1 writer inventory at `716f8903` (DS-011 #4)

Source: `Code/CryGame/Bots/BotBrain.cpp` at `716f8903`; `m_intent=BotIntent()` resets every tick (`:314`), `ResetLife`
resets it too (`:231`). `WBI(field,value)` (`:23`) and `WBI_MOVETO` (`:24`) record `why.*` but are still ordinary writes.

## By channel

- **move=true**: `WBI_MOVETO` at 413, 456, 533, 617, 662, 709, 767, 783, 812, 836, 934; direct at 1055
  (link executor). `MoveTo` sets `destination` too.
- **move=false**: 430, 452, 460, 464, 528, 663, 721, 769.
- **aim**: `WBI_MOVETO` at the same 11 lines; `WBI(aim)` at 316, 663, 721, 769, 1112 (look policy).
- **destination**: only `MoveTo` + direct at 1056 (link executor).
- **gait**: 88, 92, 95, 413, 662, 713, 767, 857, 861, 935, 975, 981, 1057, 1058.
- **suitMode**: 89, 97, 104, 415, 416, 711, 713, 827, 858, 861, 935, 949, 961, 965, 967, 969, 971, 976, 980,
  1061, 1062, 1246-1249 (suit policy) — 22 writes.
- **jump**: 104, 414, 817, 827, 936, 939, 960, 1059, 1251 (suit policy downgrade).
- **stance**: 663, 722 (vantage hold/fight, prone).
- **combat**: 710, 713, 723, 770, 784, 820, 842, 933.
- **strafe**: 413, 710, 767, 785, 813, 935.
- **linkJump**: 951 (false), 1060 (true).
- **meshPath**: 148 (Steer, not a decision write).

## Observations for the control-frame refactor

- The dominant writers are `suitMode` (22) and `gait` (14); the same `TravelGait` lines 88-97 write both. `aim`/`move`
  are written together by `WBI_MOVETO`, except the look policy (1112) and hold/aim branches (663, 721, 769).
- `stance` and `combat` are still direct writes (2 and 8) and must become channels too (`Action`/`Aim-Fire`).
- `m_intent.why` covers only suit/jump/gait/aim/move (`BotIntent.h`), not stance/combat/strafe; add those before T-1
  starts deleting writers.
- `m_pathIndex`/`m_link` still call `m_intent.destination` directly (1055-1060); the link executor is a separate
  producer and must keep exact take-off semantics.

