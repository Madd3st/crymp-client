# Difficulty spec: Beginner / Normal / Veteran / Elite (DS-014)

Owner: "difficulties feel the same; Elite = honest head aimbot." Evidence: run speed 4.4–4.6 m/s for all; aim error
median 2.2/1.2/1.1/0.9°, p95 45–70° for all; kills 17/39/39/59 (Beginner…Elite). So the current ladder is mostly
reaction/aim multipliers; p95 is dominated by flicks for every skill. Target build `5b90bfb6`.

## Current values that vary by skill

| Parameter | Beginner / Normal / Veteran / Elite | Code |
| --- | --- | --- |
| reaction (s) | 0.75 / 0.35 / 0.25 / 0.22 | `BotProfile.h:121`; used by `BotCombat.cpp:264`, `BotPerception.h:63` |
| aim error multiplier | 2.0 / 1.0 / 0.7 / 0.45 | `BotProfile.h:122,138` |
| aim error floor (m) | .10 / .07 / .05 / .035 | `BotProfile.h:139` |
| tracking lag (s) | .22 / .16 / .13 / .11 | `BotProfile.h:140` |
| head preference | .02 / .06 / .10 / .16 (capped 0.3 chance) | `BotProfile.h:141`, `BotAimControl.h:16-20` |
| burst gap multiplier | 1.35 / 1.0 / 0.9 / 0.85 | `BotProfile.h:123,143` |
| range adaptation | 0 / .35 / .75 / 1 | `BotProfile.h:125,131` |
| retreat HP | 0 / 22 / 35 / 45 | `BotProfile.h:126,131` |
| target judgement | 0 / .35 / .75 / 1 | `BotProfile.h:127,131` |
| switch margin / hold | .45/.35/.28/.25; 3/2/1.6/1.3 | `BotProfile.h:128-129` |
| strafe rhythm | 1.6 / 1.0 / 0.9 / 0.8 | `BotProfile.h:130,134` |
| dodge chance | 0 / .30 / .45 / .60 | `BotProfile.h:120,136` |
| blocked chase patience | 4 / 2 / 1.5 / 1.2 | `BotProfile.h:132` |
| use Speed suit | false / true / true / true | `BotProfile.h:133` |
| turn cap | 400 / 720 / 800 / 900 °/s | `BotBrain.cpp:1154-1161` |
| aim model flick/track/noise/maxTurn | .18/.20/.1.2/400 … .08/.07/.30/900 | `BotAimModel.h:34-36` |
| jump trait | .10 / .30 / .50 / .70 | `BotTraits.h:25-27` |
| prone | only elite marksman | `BotTraits.h:37` |

Perception cone 110°/sight 80 m/cloak 12 m is **constant** (`BotPerception.h:20-29`); fire gate is 0.96 dot ≈16° for
all (`BotCombat.cpp:260`); enemy scan 80 m for all (`BotCombat.cpp:174`).

## Proposed table (new `wb_bot_difficulty=1`; default 0 = current)

| Row | B / N / V / E | Code place | cvar |
| --- | --- | --- | --- |
| reaction after LOS | 0.90 / 0.50 / 0.28 / **0.08** s | `BotProfile.h:121`, `BotPerception.h:63` | `wb_diff_reaction` |
| turn cap | 250 / 500 / 850 / **1200** °/s | `BotBrain.cpp:1154` | `wb_diff_turn` |
| tracking error at 30 m | 4.0 / 1.5 / 0.7 / **0.10**° | `BotAimModel.h:34-36`, `BotProfile.h:122,139` | `wb_diff_aim` |
| aim point | body / chest / chest+head 40 % / **head 95 %** | `BotCombat.cpp:95-123`, `BotAimControl.h:16-20` | `wb_diff_head` |
| burst / gap | 0.5/1.0 · 0.3/0.7 · 0.18/0.45 · **0.08/0.25** s | `BotProfile.h:84-85,123`, `BotFireControl.h` | `wb_diff_burst` |
| fire-before-on-target | 25° / 15° / 8° / **2°** (and no fire in saccade) | `BotCombat.cpp:260`, `BotAimModel.h:54` | `wb_diff_firetol` |
| engagement (rifle) | 10 / 20 / 35 / **55 m** | `BotProfile.h:16-22` | `wb_diff_range` |
| strafe / jump in fight | none / strafe 1.2 s / strafe+jump 20 % / **strafe+jump 60 %** | `BotBrain.cpp:812-817`, `BotProfile.h:120,130` | `wb_diff_move` |
| suit use | Armor / +Speed travel / +Strength jumps / **all + cloak** | `BotSuitPolicy.h:55-87`, `BotProfile.h:133` | `wb_diff_suit` |
| weapon preference | pistol/SMG / SCAR / SCAR+DSG1 / **best+head** | `BotWeapons.cpp:216`, `BotWeaponRank.h` | `wb_diff_weapon` |
| sight / FOV / hearing | 45/90/30 · 60/100/45 · 75/110/60 · **90/120/80** | `BotPerception.h:20-29` | `wb_diff_perception` |

Elite rule: after a clear LOS ray it reacts in 80 ms, tracks with ≤0.1° error, aims at the head if the head ray is
clear, and fires only when `BotAimModel::onTarget`/angular tolerance 2°; no target through walls, no prefire at an
unseen position. Head aim must be honest: `BotCombat::Observe` already requires a clear head ray (`BotCombat.cpp:122`).

## Implementation notes

- Add `wb_bot_difficulty` master (0 = current values, 1 = this table) + the row cvars; all default 0. Mapping above.
- `BotProfile::Make` currently multiplies global values; add skill dimensions instead of only style presets. Keep
  `wb_skill` as the per-bot default selector.
- `BotAimControl::PreferHead` caps chance at 0.3; for Elite remove the cap only when the head ray is clear, otherwise
  chest. `BotAimModel` must expose `onTarget` and `inSaccade` to the fire gate.
- `BotCombat::Attack` dot 0.96 -> `cos(fireTol)`; do not fire during `inSaccade`.
- `BotPerception` needs per-skill cone/sight/hear; `SetReactionTime` already exists.
- `BotSuitPolicy` needs the skill input (currently only `canUseSpeed` from the profile).

## Tests (unit + probe)

- `BotProfileTests`: monotonic per skill; no NaN; default 0 reproduces current table.
- `BotAimModelTests`: at 30 m, p95 error B>2°, N>1°, V<0.8°, E<0.2°; Elite head preference >0.9 with clear ray.
- `BotCombatTests`: fire tolerance per skill; no fire in saccade; target switch hysteresis.
- Probe `duel_30m` per skill: time-to-first-shot, first-hit, headshot share, TTK; expected ladder B≫N>V>E; Elite first
  hit <0.5 s after LOS, headshot share >0.7, no wall hits.
- Probe `perception_40m`: time-to-notice B>1 s, E<0.3 s with the same LOS; no noticing through the wall.
