# withdraw_cover on C-121 (DS-048 #2)

Raw: `build/c121-on15-d6a9a521/WarsBots-Tactics-...bots.log`, `Steelmill.spots.txt`. 189 low-HP events =
**85 `withdraw_abort` + 104 actual withdraws**; 72/104 withdraws die within 2.5 s (69.2 %).

## Distance histogram (nearest cover spot, 3D, from the bot at the event)

| nearest cover | 0-2 m | 2-4 | 4-6 | 6-8 | 8-10 | 10-12 | >12 | median |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| all 189 | 55 | 50 | 43 | 34 | 7 | 0 | 0 | 3.52 m |
| aborts 85 | 19 | 17 | 21 | 21 | 7 | 0 | 0 | 3.5 m |
| withdraws 104 | 36 | 33 | 22 | 13 | 0 | 0 | 0 | 3.4 m |

182/189 events have a cover spot within 8 m; **zero** have their nearest cover beyond 10 m. Enlarging `maxMove`
from 8 m to 12 m would add **2** candidates out of 189. The abort is not a distance problem.

## What the 85 aborts actually miss

Using the last `AI telemetry target=` before the event and that enemy's MOVE TRACE position (the threat), and the
spot `coverMask` sector (k, k+1, k+15):

- **40/85** have no cover spot within 8 m whose mask faces the threat (data gap: nearby cover is on another side).
- **44/85** have a facing spot within 8 m; these are rejected by the LOS/reachability lambda at
  `BotBrain.cpp:842-846` (`!EyeRayClear(spot+1.6, chest)`): the mask says "body covered", the standing eye ray is
  clear (low cover). 50/85 have no **full**-cover facing spot (flag 1); 44/85 have no low-cover one (flag 2).
- `dToward` histogram for events with at least one facing spot: 0-2 18, 2-4 36, 4-6 41, 6-8 44, 8-12 2, >12 0.

## Verdict: neither the radius nor a spots edit is justified by these numbers

`PickCover` already finds cover at a 3.5 m median; the failures are **side/quality**, not distance. A radius change
buys 2/189; adding spots without a new collision bake cannot create threat-facing cover. More important, the
69.2 % is measured over the **104 events that did withdraw** (the 85 aborts are excluded), so fixing the abort rate
would not move that number: those bots reached a spot that broke the LOS ray at selection and still died there
within 2.5 s, i.e. the threat moved or a third bot shot them. Proving which needs the demo kill/shooter/angle per
death, not a radius.

No behavioral patch for #2. Next step if the owner wants < 50 %: (a) per-death shooter/angle from the 20 Hz demo,
(b) a fresh `tactical_spots` bake with closer threats (6-20 m) for the 40 no-facing clusters, (c) optionally require
a 3-sector (67.5 deg) protected arc for withdraw, which needs a run to prove.
