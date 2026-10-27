# Playtest 5: Veteran K/D vs Normal (DS-046 #3)
Owner window 549.5-1075.8 s; 2 bots/skill (N,N,V,V,E,E,B,B); kills/deaths from combat kill/death observed.
| Bot | Skill | K | D | K/D |
| --- | --- | ---: | ---: | ---: |
| 131007 | Normal | 18 | 10 | 1.80 |
| 131028 | Normal | 12 | 13 | 0.92 |
| 131012 | Veteran | 3 | 13 | 0.23 |
| 131033 | Veteran | 15 | 10 | 1.50 |
Aggregate N 30/26 (1.15) vs V 18/26 (0.69); within-pair spread N 0.92-1.80, V 0.23-1.50.
One Veteran (131012) and one Normal (131007) are the outliers: with 2 bots each this is per-bot noise, not a 12-kill skill effect.
Veteran parameters: Normal accuracy 35.19 %, hit p50/p90 10.59/16.98 m, first shot 0.629 s, first primary 3.34 s, SOCOM armed 16.32 %; Veteran 26.36 %, 14.81/24.23 m, 0.402 s, 6.32 s, 21.73 %.
`BotDifficulty`: Normal reaction 0.50 / aim 1.2 deg / holdScale 1.0 / rangeScale 1.0; Veteran 0.28 / 0.6 / 1.3 / 1.4.
Veteran walks 1.3x farther and fires at 1.4x range -> lower accuracy at 15-24 m, but this does not explain K/D 0.23 vs 1.50 inside the skill.
Do not retune Veteran on 2 bots; re-run >=6/skill or pair the same style and inspect 131012 separately.
