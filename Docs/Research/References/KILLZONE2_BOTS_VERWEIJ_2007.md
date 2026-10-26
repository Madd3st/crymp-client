# Killzone 2 multiplayer bots (Verweij, VU Amsterdam / Guerrilla, 2007) — what we take for Power Struggle

Source: https://www.guerrilla-games.com/media/News/Files/VUA07_Verweij_Hierarchically-Layered-MP-Bot_System.pdf
(read 26.10 by Claude; the fetch covered ch. 1–5.1, evaluation chapter not read).

## Key facts
- Three layers: **faction leader (commander) → squad leader → bot**; messages only between adjacent layers; an order stays
  active until completed or overwritten. Mode: Capture and Hold (immobile capture areas, like PS points).
- Commander uses **only information a human has** (score, area owners, spawns) — no cheating.
- Update rates: bot planner ~5 Hz, squad ~2 Hz, **commander every 6⅔ s**, the two commanders staggered by 3⅓ s —
  slow strategy = no flip-flopping.
- Commander is plain C++ heuristics; squads are HTN domains (planner where logic decomposes, code where it is arithmetic).
- Desired number of areas = half + 1, corrected by score lead, time left, expected kill points. Test bug: commander
  aimed for 1 area → bots clustered → fewer kill points; the prediction was fixed.
- Target order: keep already-targeted areas (stability) → neutral → enemy; nearer own spawn preferred; failed-attack
  counts change the ranking only when the difference > 10 (hysteresis).
- Split: scouts to untargeted areas, the rest evenly over targets (8 bots, 3 areas, 2 targets → 4-3-1). Squads ≤ 4.
  Commander picks each squad's spawn near its target. Existing squads are kept to avoid churn.
- Squad behaviours: **regroup at a covered spot before an assault so members arrive together**, distinct roles
  (flank), **focus fire on the most pressing threat**, **split watch directions when no threat**, no-go zones
  (enemy territory) passed down. Squads report summaries + failures up; bots report sightings/lost tracks/kills.
- Individual bots: single-player AI made more aggressive, **spread inside an area instead of clumping**, aim tuned to an
  average skilled human.
- Killzone 1 lesson: a script bug never assigned defenders → chaotic play. Log and test role assignment.

## For WarsBots (D-018/D-019: PS, 3–4 bots per team, scaling)
1. T-10 orders layer = exactly this shape: commander (slow, ~5–7 s, honest info) → squad (2 Hz) → bot (our 10 Hz brain).
   With 3–4 bots per team one squad per team is enough at first; the layer still pays off when we scale.
2. Commander: area count + priority + hysteresis rules above, plus PS extras (prestige spending, vehicle use).
3. Squad v1: regroup-then-assault, focus fire, watch-direction split, spread on the capture point.
4. Stability everywhere: slow strategic tick, keep current targets, thresholds before switching (our arbiter lessons).
5. Test role assignment explicitly (probe: 2 teams × 3 bots on Shore, count attackers/defenders per minute).
