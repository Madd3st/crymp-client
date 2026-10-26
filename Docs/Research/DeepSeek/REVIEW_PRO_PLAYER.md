# Pro-player audit of the 35 BEHAVIOUR_BIBLE situations (DS-042 #1)

Date: 2026-10-27. Reviewer: DeepSeek (read-only; no game code changes).

## Sources and method

| Batch | Build | Bots | Length | Role |
| --- | --- | --- | ---: | --- |
| **c116** (`WarsBots-Tactics-c116-on15-fd12a5eb-candidate-r1-n15`) | `fd12a5eb` | 15 (30 actors with respawns) | 660 s | **graded batch** (night batch; `high_ground=1`, `pro_fire=1`, `threat_scan=1`, `vantage=0`) |
| **c113** (`WarsBots-Tactics-c113-on15-2fee59c2-candidate-r1-n15`) | `2fee59c2` | 15 (30 actors) | 660 s | A/B before `high_ground`/`pro_fire` |
| C-112 owner playtest 4 | `2fee59c2` | 8 | 1,304 s | owner comparison (no `FIRE SHOT`; DS-040) |

Numbers from `Tools/BehaviourAudit.ps1` (counts `FIRE SHOT`, `FIRE HIT phase=before-Lua`, `combat kill`,
`death observed`, `TARGET CHOICE`, `AI telemetry`, `MOVE TRACE`, `BOT SUIT`, `GLANCE`, `HEARD`, `HIGH GROUND`,
`VANTAGE`, `TRAP RECOVERY`, `HOLD angle`, `REAL PICKUP`).

Caveats that affect every row:

- `c116` and `c113` are one 11-minute 15-bot batch each, on different builds; kill/accuracy deltas of 2-3 points are
  within batch noise. Grade the **behaviour**, not the delta.
- `wb_bot_vantage=0` in both batches, so B2/C6 are tested through `HIGH GROUND` + `GLANCE` only.
- `ShotBlocked`, `NOFIRE`, tactical reload and grenade events are not logged in these batches (the DS-040 patch is not
  in `fd12a5eb`), so C5/A3/D2 use proxies and say so.
- `distance` in `FIRE HIT` is the shot distance; `kill class` is the last hit by the killer on the victim within 1.5 s.
- "Height kill" = shooter z minus victim z >= 3 m, joined from `AI telemetry` within 3 s (87/287 kills have both
  positions).

## Batch A/B (c113 → c116)

| Metric | c113 (`2fee59c2`) | c116 (`fd12a5eb`) |
| --- | ---: | ---: |
| kills / deaths | 303 / 303 | 287 / 287 |
| shots / hits / accuracy | 10,193 / 3,318 / **32.6 %** | 10,178 / 3,036 / **29.8 %** |
| shots per kill | 33.6 | 35.5 |
| hit distance p50 / p90 / max | 12.6 / 20.3 / 55.6 m | 12.4 / 21.3 / **59.7 m** |
| hits 40-60 m / >60 m | 14 / 0 | 15 / 0 |
| headshot (all / Elite) | 6.4 % / 9.6 % | 6.2 % / **10.4 %** |
| kills by SOCOM / SCAR / FY71 | 95 / 93 / 57 | 97 / 96 / 48 |
| `GLANCE` | 0 | **522** |
| `HIGH GROUND start` | 0 | **35** |
| high-ground samples (z >= 140) | 19.8 % | 21.1 % |
| height kills / known | 13 / 303 (4.3 %) | 9 / 287 (**3.1 %**) |
| jumps / min (15 bots) | 31.25 | 29.36 |
| link starts / failed | 146 / 43 (29.5 %) | 161 / 49 (30.4 %) |
| withdraws / deaths within 2.5 s | 183 / 156 (85 %) | 168 / 134 (80 %) |
| switch-away from <=25 HP / target choices | 291 / 1,127 (25.8 %) | 236 / 1,053 (22.4 %) |
| loot within 3 s of own kill | 50 / 303 (16.5 %) | 39 / 287 (13.6 %) |
| `TRAP RECOVERY` / blocked motion | 9 / 179 | 7 / 165 |

Read: `high_ground` + `threat_scan` now produce activity (35 high-ground starts, 522 glances) but **do not yet buy
height**: high-ground time +1.3 pp and height kills fell 4.3 → 3.1 %. `pro_fire` did not move accuracy up (29.8 vs
32.6) and switch-away only 25.8 → 22.4 %. The rest of the table grades c116. Flags: `BotConfig.cpp:8`
(`wb_bot_high_ground`) and `:9` (`wb_bot_pro_fire`) in `fd12a5eb`.

## The 35 situations

Severity: **blocks** = not human-like in the owner's sense; **visible** = a player sees it immediately; **minor**.

| # | Situation (Bible) | Grade | Measurement / evidence (c116) | Severity | Fix idea |
| --- | --- | --- | --- | --- | --- |
| A1 | Spawn with pistol, go to a real gun | **PARTIAL** | First primary p50 **33.5 s**, p90 57.7 s. SOCOM is the held weapon in **1,758 / 4,920** TACTIC samples (35.7 %); 97 / 287 kills (34 %) by SOCOM. | visible | Weapon goal from t=0 with pickup value, 20 s cap; log time-to-primary. |
| A2 | Weapon choice by range | **PARTIAL** | SCAR 96 + FY71 48 = 50 % of kills; SOCOM 34 %; SMG 22, Shotgun 18; **DSG1/Gauss 0**; `WEAPON RANK` 12. | visible | DSG1/Gauss only at high ground; `SelectForRange` with range bands and a 0.3 margin (pro_fire). |
| A3 | Ammo / reload | **PARTIAL** | 0 shots at clip 0 (good); telemetry `ammo=0` **143 / 4,920 (2.9 %)**; `TACTICAL RELOAD` **0 events**; 35.5 shots/kill. | visible | Tactical reload outside combat (`BotBrain.cpp:346`); log `TACTICAL RELOAD`. |
| A4 | Finish a nearly dead target | **FAIL** | **236 / 1,053 (22.4 %)** target choices switch away from a target hit to <=25 HP within 1 s. | blocks | Finish window (`BotCombat.cpp:238-241`) + threat check; log `FINISH keep/release`. |
| A5 | Loot after a kill | **PARTIAL** | Loot within 3 s of own kill **39 / 287 (13.6 %)**; deaths within 3 s of a pickup 11 / 335 (3.3 %). | minor | Loot only with no visible enemy; add pickup after kill. |
| B1 | Route through the map | **PASS** | 7 `TRAP RECOVERY` in 660 s (0.04/bot-min), 165 blocked motion (0.5/bot-min), navmesh 1. | — | Keep; watch X-088 (`navmesh=0`). |
| B2 | Roofs and upper floors | **PARTIAL** | `HIGH GROUND start` **35**, `VANTAGE travel` 715, high-ground samples **21.1 %** (owner 51.6-57.9 %); height kills **9 / 287 (3.1 %)**; c113 13 / 303 (4.3 %). | visible | DS-041 rule 1/7: hold the spot 8-15 s and fight from it; the flag currently travels more than it holds. |
| B3 | Strength jump to a roof/ledge | **PARTIAL** | `jump_links=1`; **46 Strength take-offs**, 68 Strength link starts, **49 / 161 links fail (30.4 %)**; `VANTAGE travel` reaches (1301.7,1318.3,**161.5**) (DSG1 roof). | visible | Fix link failure (30 %); log take-off mode and landing; DS-041 rule 5. |
| B4 | Strength wall jump | **FAIL** | No wall-jump code and no field; the owner's "wall jump chains" cannot even be identified in his trace. | visible | New `wall jump` probe + recorder field (jump type, wall contact). |
| B5 | Speed bunny hop | **FAIL** | 323 take-offs / 660 s = 29.4/min over 15 bots (**1.96/bot-min**), max chain **1**, speed p90 6.75. Owner 10.5/min, chain 9, p99 10.5. | visible | DS-041 rule 4: 2-4 hops, 0.30-0.35 s, Speed, not in combat. |
| B6 | Door / stairs / lift | **PARTIAL** | 7 traps; no door/lift events; no evidence of door use or lift waiting. Stair mouth still produces traps. | minor | Add door/lift probe and a stall metric at the DSG1 stair mouth. |
| C1 | Contact at 5 m | **PARTIAL** | Hits < 10 m **892 / 3,036 (29.4 %)**; Shotgun 156 shots (1.5 %), SMG 1,131 (11.1 %); no first-shot delay metric. | visible | Prefer Shotgun/SMG < 8 m, hip-fire, strafe; log first-shot delay. |
| C2 | Contact at 20 m | **PARTIAL** | Hit p50 **12.4**, p90 **21.3**; Elite head 10.4 %; 35.5 shots/kill. Strafe/jump not measured. | visible | Strafe rhythm (DS-041 rule 8) and burst timing; log first-shot delay/TTK. |
| C3 | Contact at 50 m | **FAIL** | Hits 40-60 m **15 / 3,036 (0.5 %)**, > 60 m **0**; max 59.7 m. | visible | 50 m fire only with SCAR single/burst or DSG1; close otherwise. |
| C4 | Contact at 80 m | **FAIL** | 0 DSG1/Gauss kills, 0 hits > 60 m, no sniper hold. | blocks | DSG1 + scope + high spot; `vantage` still 0 in the batch. |
| C5 | Enemy behind cover | **PARTIAL** | `HOLD angle` **205**; no `ShotBlocked` metric; 0 grenades, no logged flank kill. | visible | Step-out timing/log; grenade/flank absent. |
| C6 | Enemy on a roof / sniper | **PARTIAL** | `GLANCE` **522** (c113 0); height kills 3.1 %; "time to acquire roof" not logged. | visible | Scan works, but no kill from height; add threat height state. |
| C7 | Being held by a sniper | **PARTIAL** | 0 DSG1/Gauss deaths (no sniper duels in the batch); 168 withdraws, 80 % die; no "being sniped" state. | visible | Add `SNIPED` threat (last hit from z+3), break LOS, change height. |
| D1 | Low HP | **FAIL** | **134 / 168 withdraws die within 2.5 s (80 %)**; no heal/cover metric; `BotBrain.cpp:743-752` withdraws for 2.5 s. | blocks | Retreat goal with cover, not a 2.5 s straight-line back-off; heal/regen wait. |
| D2 | No ammo | **PARTIAL** | 0 shots at clip 0; telemetry empty 2.9 %; no out-of-ammo retreat; `SelectAvailable` exists. | visible | Empty -> switch/retreat, not stand. |
| D3 | Several enemies | **PARTIAL** | 1,053 target choices = 6.4/bot-min; visible-enemy count not logged. | minor | Log `visible=N`; threat count > 1 -> narrow angle/withdraw. |
| D4 | Heard shots, cannot see | **PARTIAL** | `HEARD` **1,181** (c113 452), `GLANCE` 522; time to investigate and kills from heard not logged. | visible | Add investigate timer and `KILL from heard`. |
| D5 | Enemy in cloak | **FAIL** | Cloak mode 66 samples (0.2 %), 1 cloaked shot, 0 cloak kills; `wb_bot_cloak_experimental=0`. | visible | Cloak detection by sound/reveal; log detection distance. |
| D6 | Target almost dead | **FAIL** | Same as A4: 22.4 % switch away at <=25 HP. | blocks | Same as A4. |
| D7 | After a kill | **PARTIAL** | Loot 13.6 %; death within 3 s of pickup 3.3 %; no post-kill reposition event. | minor | Post-kill reload + sidestep 1-2 m; log. |
| E1 | Armor under fire | **PASS** | **99.1 %** of 3,036 hits land on a target in Armor (`suit=3`); Armor 89.6 % of samples. | — | Keep; check Armor energy drain. |
| E2 | Speed for travel | **PARTIAL** | Speed 7.5 % of samples, sprint 37.1 %, speed p90 6.75; no bhop. Owner Speed 7.4-12.3 %. | visible | DS-041 rule 4/9: Speed on >30 m straight, drop in combat. |
| E3 | Strength for a jump | **PARTIAL** | Strength 1.8 % of samples, but **46 Strength take-offs**; 49 / 161 links fail (30.4 %). | visible | Fix link failure; log take-off mode and land; DS-041 rule 5. |
| E4 | Cloak for a flank | **FAIL** | 66 samples, 1 cloaked shot, experimental OFF. | visible | Gate cloak for Elite flank only; detection/reveal. |
| E5 | Grenades | **FAIL** | 0 grenade events; no grenade logic in `Code/CryGame/Bots`. | visible | Out of scope this phase. |
| F1 | PS teams 3-4 | **FAIL** | IA only, no `SetTeam`, no team spawn. | blocks | `T0_GAMEMODE.md`. |
| F2 | PS capture | **FAIL** | No capture goal/zone. | blocks | `PS_MIN.md`. |
| F3 | PS buy SCAR | **FAIL** | No PP/buy zone. | blocks | `PS_MIN.md`. |
| F4 | PS vehicles | **FAIL** | No `IVehicleSeat::Enter` / `IVehicle::OnAction`. | blocks | `VEHICLES.md`. |
| F5 | Vehicles off | **FAIL** | Consistent with F4: no vehicle goal or enter. | blocks | Keep off until F4. |

Result: **2 PASS, 18 PARTIAL, 15 FAIL** (35 rows). The PASS rows are navigation and Armor; the FAILs cluster in
finishing, retreat, long-range/sniper, cloak/grenades and all of PS. The owner-visible gaps are A1/A2 (pistol time),
A4/D6 (finish), D1 (retreat), B2 (height), B5 (hops), C3/C4 (range).

## Top 10 gaps, ranked

1. **Retreat is a death sentence**: 80 % of withdraws die in 2.5 s (D1). It is the single biggest K/D leak; the
   owner does the opposite (breaks LOS, changes height).
2. **Finish/switch-away**: 22.4 % of target choices leave a <=25 HP target (A4/D6); the 6 s finish window is too
   weak against a threat check.
3. **SOCOM time and kills**: 35.7 % of held time, 34 % of kills (A1/A2); a good player has a primary by 10-15 s, not
   33 s median.
4. **No long-range game**: 0.5 % of hits at 40-60 m, 0 above 60, 0 DSG1/Gauss kills (C3/C4). The owner's p90 hit is
   31 m; bots' is 21 m.
5. **High ground is visited, not used**: 35 starts, 21.1 % time, 3.1 % height kills (B2). The new flag moved travel,
   not fights.
6. **Hop/flank verticality**: 1.96 jumps/bot-min, chain 1 (B5); no wall jump (B4); Strength links exist (46 take-offs) but 30.4 % fail (B3).
7. **Cloak and grenades are absent** (D5/E4/E5); a cloaked human is safe and a grenade is never answered.
8. **Cover play is shallow**: `HOLD angle` 205, but no blocked-shot metric and no flank/grenade (C5).
9. **Being sniped has no state** (C6/C7): 522 glances, but no kill from height and no response to a z+3 shooter.
10. **Post-kill discipline**: loot 13.6 %, no reposition (D7/A5); a second enemy punishes it.

## Measurement caveats

- `c116` is an 11-minute batch; per-row percentages have ±2-4 pp noise. The A/B in the table is directional.
- `kill class` joins the last hit by the killer on the victim within 1.5 s; 287/287 kills matched in c116 and
  303/303 in c113.
- `height kill` needs `AI telemetry` for both actors within 3 s; 87/287 c116 kills have both. Treat 3.1 % as a
  lower bound.
- `ShotBlocked`, `NOFIRE`, `TACTICAL RELOAD`, grenade and door events do not exist in these logs; rows C5/A3/D2/B6
  are graded on proxies and will move when the DS-040/DS-041 recorder fields land.
