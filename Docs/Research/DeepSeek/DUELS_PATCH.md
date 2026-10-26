# DS-035 duel patch (for 06a6e79b)

Exact unified diff: `Docs/Research/DeepSeek/duels_035.patch` (20 KB, 4 files). Apply with
`git apply duels_035.patch` on top of `06a6e79b`. Engine-free test `Tests/BotDifficultyTests.cpp` was built with
MSVC 19.44 and passes (`BotDifficultyTests OK`); the game was not built.

## What it changes

1. **`BotDifficulty.h`: skill owns the band.**
   - `DifficultyRow` gets `holdScale`, `engageCap`, `holdCap`, `fireCap`.
   - Rows:
     | Skill | rangeScale | holdScale | engageCap | holdCap | fireCap |
     | --- | ---: | ---: | ---: | ---: | ---: |
     | Beginner | 0.85 | 0.8 | 16 | 20 | 24 |
     | Normal | 1.0 | 1.0 | 20 | 24 | 45 |
     | Veteran | 1.4 | 1.3 | 30 | 36 | 55 |
     | Elite | 1.75 | 1.6 | 38 | 44 | 60 |
   - `ApplyDifficulty` now clamps `engageRange/holdRange/fireRange` and scales `preferredRange`:
     `preferredRange = max(6, preferredRange * holdScale)`. This is the range `UpdateAttacking` actually walks to;
     before, it stayed at the style value (Assault 8, Marksman 20) for every skill.
   - Result for Universal: B 15/19/24, N 18/22/40, V 25/31/55, E 32/38/60. Marksman Elite caps at 38/44/60 instead
     of 49/60/114.
2. **One reaction clock** (`BotBrain.cpp:756,861,883`): `m_now >= max(m_stateTime, SelectedAt()) + reaction`.
   Removes the 2× reaction from the old `&&` and makes the measured first shot approach 1× reaction.
3. **Zoom at >12 m while chasing** (`BotBrain.cpp:1047-1055`): `Handle` is called when a target is visible and
   `dist > 12`, not only when `m_intent.combat != None`. Elite/Veteran now zoom before the 20-30 m burst instead of
   running through it with hip spread.
4. **AIM ZONE 1.5 s → 0.5 s** (`BotCombat.cpp:116-117`): the head/body choice follows a moving target; log says
   `interval=0.5`.

## Test update

`Tests/BotDifficultyTests.cpp` replaces the old `fireRange == raw.fireRange * 1.45` assertion with:

```cpp
assert(std::fabs(m.fireRange - 60.f) < 1e-3f);      // Marksman Elite cap
assert(std::fabs(m.holdRange - 44.f) < 1e-3f);      // 34 * 1.75 = 59.5 -> cap
assert(std::fabs(m.preferredRange - 32.f) < 1e-3f); // 20 * 1.6
```

`BotDifficultyTests` passes. `BotJumpLinkTests` is unchanged from 06a6e79b.

## Acceptance (C-111 A/B)

With `wb_bot_difficulty=1` and a fixed style per skill (all Universal, then all Marksman):

| Metric | C-108 baseline | Target |
| --- | ---: | ---: |
| Elite kills vs Veteran | 70 vs 93 | Elite > Veteran |
| Elite deaths vs Veteran | 48 vs 66 | Elite < Veteran |
| Elite hits 20-30 m | 29 | > Veteran 111 |
| Hit rate order | B 22.8 < V 30.9 < E 32.5 < N 34.8 | B < N < V < E |
| First-shot delay | 1.04 / 0.66 / 0.41 / 0.38 | monotonic, Elite ≤ 0.18 |
| Elite headshot share | 8.4 % | 20-35 % |

Run `Tools/PlaytestReport/duels.ps1` on the log and compare `per_skill`.

## Risks / notes

- `rangeScale` changed for Beginner/Veteran/Elite; any old test or tuning that assumed 0.55/1.15/1.45 must be updated
  (`Tests/BotDifficultyTests.cpp` was the only exact assertion).
- A Marksman Elite at 38 m engage / 44 hold is intentional; if it feels too far, lower `holdScale` to 1.5 before
  lowering the caps.
- The patch does not touch `ApplySuitPolicy`, links, or navmesh; it is independent of 06a6e79b.
- The 10 Hz decision loop still floors Elite reaction at ~0.1 s; `reaction=0.08` is aspirational until a 20 Hz combat
  tick. The measured delay should still drop from 0.38 to ~0.12-0.18.
