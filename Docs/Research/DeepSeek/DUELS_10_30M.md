# Why bots lose fights at 10–30 m (DS-035)

Date: 2026-10-26. Read-only at `claude-sprint` `2ffc95fa`. Primary log: C-108 `on15` (15 bots, 660 s,
`build/c108-on15-2ffc95fa`), compared with C-091 owner session and the C-091 demo numbers from DS-028.

## Verdict

The ladder is **not monotonic** and Elite is **not dominant at 10–30 m**. In C-108 Veteran beats Elite on kills,
hits, and hits at 20–30 m; Elite only wins on deaths, headshot share and a slightly shorter first shot. The reason is
structural: **style decides the fighting distance, not skill** (`preferredRange`/`styleScale`), and the difficulty
table scales only `engageRange/holdRange/fireRange` (`BotDifficulty.h:58`), not the range the bot actually walks to.
Elite closes to 8–13 m like everyone else, where its 0.1° aim error is swallowed by SCAR hip spread and strafe, so its
advantage disappears.

## Data

### C-108 per skill (combat-by-skill.json + log)

| Skill | Kills | Deaths | Shots | Hits | Hit rate | Headshot | Hit dist p50 / p90 | Hits > 40 m | Aim error p50 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| Beginner | 25 | 86 | 1,331 | 304 | 22.8 % | 5.6 % | 10.96 / 15.9 | 1 | 3.56° |
| Normal | 87 | 75 | 2,862 | 998 | 34.8 % | 3.5 % | 11.03 / 16.8 | 3 | 1.95° |
| Veteran | **93** | 66 | 3,554 | **1,098** | 30.9 % | 5.6 % | **13.62 / 21.0** | **10** | **1.25°** |
| Elite | 70 | **48** | 2,319 | 753 | 32.5 % | **8.4 %** | 11.56 / 18.0 | 7 | 1.31° |

Veteran has more kills, more hits, more 20–30 m hits and the best aim error. Elite survives better (48 deaths) and has
the best headshot share, but not the best duel record.

### Hits by distance band (`FIRE HIT distance`, reliable)

| Skill | 0–10 m | 10–20 m | 20–30 m | 30–50 m | 50+ m | 20 m+ share |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Beginner | 112 | 180 | 10 | 2 | 0 | 3.9 % |
| Normal | 368 | 581 | 43 | 6 | 0 | 4.9 % |
| Veteran | 163 | 800 | **111** | 22 | 2 | **13.9 %** |
| Elite | 246 | 456 | 29 | 19 | 3 | 6.8 % |

The 10–30 m band is where the owner lives: C-091 owner kills median **12.9 m**, p90 **32.5 m**, max 36.7 m, 22 kills
and **0 deaths**; bots in the same session kill each other at median 11.8 m, p90 17.9 m. The owner uses 20–35 m and
the bots do not.

### First-shot delay and burst

`TARGET CHOICE (visible=1) → first FIRE SHOT at that target` (median; 1,052 choices):

| Skill | Delay | Burst median | Notes |
| --- | ---: | ---: | --- |
| Beginner | 1.04 s | 1 | table says 0.90 |
| Normal | 0.66 s | 2 | table 0.50 |
| Veteran | 0.41 s | 2 | table 0.28 |
| Elite | 0.38 s | 3 | table **0.08** |

Elite’s measured delay is **4.75×** its configured reaction; Beginner’s is only 1.16×. The top of the ladder is
compressed; the difference between Veteran and Elite is 0.03 s, not 0.20 s.

## What stops bots at 20–30 m

1. **Style chooses the range, skill does not.** `BotProfile::WeaponRange` (`BotProfile.h:59-74`) returns
   `preferredRange + (range*styleScale - preferredRange)*rangeAdapt*0.65`. `preferredRange` is 8 for Assault, 20 for
   Marksman, 10–15 for others; `styleScale` 0.7–1.2. The auto roster gives Veteran Marksman and Elite Assault/Flanker
   in C-108, so Veteran holds 20 m and Elite closes to 8–10 m. The comparison is style vs style, not skill vs skill.
2. **`ApplyDifficulty` does not scale the range the bot walks to.** It multiplies `engageRange`, `holdRange`,
   `fireRange` (`BotDifficulty.h:58`) but leaves `preferredRange` untouched. `UpdateAttacking` moves toward
   `desiredRange = WeaponRange(...)` (`BotBrain.cpp:725,828`), so every skill converges to the same 8–20 m by style.
   Elite’s `rangeScale 1.45` only lets it *start* attacking at 26 m; it still walks to 13 m.
3. **Elite fires while closing, with movement spread.** `UpdateChasing` fires at a visible target within `fireRange`
   (`BotBrain.cpp:873-876`) while sprinting/closing; SCAR hip spread is 1–2.25°, which at 20–30 m is 0.5–1.2 m, larger
   than Elite’s 0.1° aim error. At 20–30 m Elite fired 408 shots for 29 hits (7.1 %); Veteran 392 for 111 (28.3 %) —
   Veteran is holding the band, Elite is running through it.
4. **Double reaction gate.** The fire intent requires
   `m_now-m_stateTime>=reaction && m_now-m_combat.SelectedAt()>=reaction` (`BotBrain.cpp:752,854,876`). The later of
   state entry and target selection starts the clock, so the first shot is up to **2× reaction** plus the 10 Hz tick.
   Measured: 1.04/0.66/0.41/0.38 vs 0.90/0.50/0.28/0.08. Elite’s 0.08 s cannot be expressed at 10 Hz; the floor is
   ~0.1 s, and the double gate pushes it to ~0.38.
5. **Zoom is tied to “fighting”, not to range.** `BotWeapons::Handle` is called only when
   `m_intent.combat != None && m_combat.Visible()` (`BotBrain.cpp:1043-1046`); at 20–30 m a chasing bot sets `Aim` and
   may zoom, but if it is closing/sprinting the zoom timing is late. Elite’s `zoomScale 0.4` only helps if it actually
   zooms before the burst.
6. **Aim zone interval 1.5 s.** `AIM ZONE` re-chooses head/body every 1.5 s (`BotCombat.cpp:110-118`); a moving target
   changes the head ray in that window. `headPref 0.95` therefore yields 8.4 % headshots, not 95 %.
7. **Weapon at range.** At 20+ m the shots are SCAR 791, FY71 419, SOCOM 298, Hurricane 26, SMG 21, Shotgun 14
   (band assignment from shot positions; approximate). SOCOM at 20–30 m means some bots fight without a primary after
   a death; `weaponRank` does not guarantee a primary before the next contact.

## Fix (D-027: Elite dominant 10–30, nobody lowered elsewhere, ladder at every band)

### 1. Scale the hold range by skill

In `ApplyDifficulty` (`BotDifficulty.h:58`) also scale `preferredRange` (and therefore `WeaponRange`):

| Skill | Scale | Universal SCAR hold | Marksman hold | Assault hold |
| --- | ---: | ---: | ---: | ---: |
| Beginner | 0.8 | 10 | 16 | 6 |
| Normal | 1.0 | 13 | 20 | 8 |
| Veteran | 1.3 | 17 | 26 | 10 |
| Elite | 1.6 | **21** | **32** | **13** |

This is the single change that makes Elite fight at 20–30 m instead of 10–13. Keep the style floor/ceiling so Assault
does not become a sniper.

### 2. One reaction clock

Replace the double gate with the later of state entry and target selection:

```
const float readyAt = max(m_stateTime, m_combat.SelectedAt()) + m_profile.reaction;
m_intent.combat = m_now >= readyAt ? Fire : Aim;
```

Then measured delay should approach `reaction + 0–0.1 s`: B 0.9–1.0, N 0.5–0.6, V 0.28–0.38, E 0.08–0.18. If 0.08
feels inhuman, cap Elite at 0.15 (still the fastest) and keep the ladder monotonic.

### 3. Engage/hold/fire by skill

| Skill | engage | hold | fireRange |
| --- | ---: | ---: | ---: |
| Beginner | 16 | 20 | 35 |
| Normal | 20 | 24 | 45 |
| Veteran | 26 | 30 | 55 |
| Elite | **32** | **38** | **60** |

Today Universal is 18/22/40 and `rangeScale` makes Elite 26/32/58; the new table makes the intent explicit and
prevents style from overriding it.

### 4. Hold the band, do not run through it

In `UpdateAttacking` (`BotBrain.cpp:804-846`), if `distance` is within `[preferredRange-3, holdRange]` and the bot is
Veteran/Elite, strafe/plant instead of closing. Only close when `distance > holdRange`; only retreat below
`retreatEnter`. This is where the 20–30 m hits come from.

### 5. Zoom before the burst

Make `WantZoom` threshold `max(12, preferredRange*0.7)` and call `BotWeapons::Handle` whenever a target is visible and
`distance > 12`, not only when `m_intent.combat != None`. Elite at 20–30 m should be zoomed for the first burst, not
after it.

### 6. Aim zone faster

Re-evaluate head/body every 0.5 s (or on each new burst) and only prefer the head when the head ray is clear. Do not
promise 95 % headshots; with SCAR hip spread the realistic Elite headshot share is 20–35 %.

### 7. Decouple style from skill in tests

Run a fixed style per skill (all Universal, then all Marksman) to measure the ladder; otherwise Veteran Marksman vs
Elite Assault confounds every number. The owner’s “Elite strong” may be true for a specific style pairing, not for the
skill row.

## Acceptance

| Metric | C-108 | Target after |
| --- | ---: | ---: |
| Elite kills vs Veteran | 70 vs 93 | Elite > Veteran |
| Elite deaths vs Veteran | 48 vs 66 | Elite < Veteran |
| Elite hits at 20–30 m | 29 | > Veteran’s 111 |
| Hit rate order | B 22.8 < V 30.9 < E 32.5 < N 34.8 | B < N < V < E |
| First-shot delay | 1.04 / 0.66 / 0.41 / 0.38 | monotonic, Elite ≤ 0.18 |
| Elite headshot share | 8.4 % | 20–35 % |

## Caveats

- Band assignment for **shots** uses the last `TACTIC SAMPLE`/`MOVE TRACE` distance and is approximate; the reliable
  band metric is `FIRE HIT distance` (used in the hits table). Do not quote shot-band hit rates as exact.
- C-108 is one 11-minute, 15-bot run; C-091 is the owner’s 8-bot session. Re-run a fixed-style pair before/after.
- The 10 Hz decision loop is a hard floor for reaction; a 20 Hz combat tick or a continuous `SelectedAt` gate is
  needed if Elite’s 0.08 s is to mean anything.
