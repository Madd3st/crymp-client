# The owner's moveset → bot behaviour (DS-041)

Date: 2026-10-27. Sources, all on this PC:

| Trace | Session | Owner samples | Duration | Bots |
| --- | --- | ---: | ---: | --- |
| C-091 | `C091-40bd9f60-20261026-115617-Client.bots.log` | 1,424 | 365.7 s | 8 |
| C-106 | `C106-757f3ddc-20261026-162613-Client.bots.log` | 5,074 | 1,304.0 s | 8 |
| C-112 | `C112-2fee59c2-20261026-225932-Client.bots.log` | 2,508 | 646.5 s | 8 |
| **pooled** | 3 traces | **9,006** | **2,316.2 s** | — |

Bot comparison: C-112 server `…-Server.bots.log`, 16 bot actors (8 spawns + replacements), 27,223 samples / 1,304.5 s.
Owner trace is `MOVE TRACE kind=human` at **4 Hz**; bot trace is 4 Hz. Extractor: `Tools/OwnerMoveset.ps1`.
Data: `owner_spots.json` (`schema=warsbots-owner-moveset/2`). Reference notes: `SteelmillTriage/OWNER-VIDEO-2026-10-24.md`,
owner playtest 2 (roofs via Strength + wall-jump chains, sniping from height, bunny hop 9-13 m/s).

High ground is `z >= 140` (Steelmill street floor ~136, so +4 m); `owner_spots.json` also stores `height_above_floor_m = z-136`.
The local floor is not logged, so this is a proxy — see *Missing fields*.

## 1. Speed

Grounded-moving speed (speed > 0.3 m/s), by posture, pooled:

| Posture | Samples | Moving | p50 | p90 | p99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| stand | 8,303 | 5,464 | **4.88** | 6.46 | 10.49 |
| crouch | 39 | 2 | 1.20 | 1.20 | 1.20 |
| prone | 664 | 88 | 0.78 | 1.90 | 3.04 |
| airborne | 1,284 | 1,250 | **6.29** | **11.80** | **17.14** |

* The owner almost never crouches: **39 samples / 0.4 %**. He goes **prone 664 samples / 7.4 %** (45 segments, 138.3 s;
  median 2.2 s, max 17.0 s on the DSG1 roof). Crouch is not part of his moveset.
* Sprint share 29-36 % of all samples; C-112 32 %. Speed mode 7.4 % overall, Strength 3.2 %, Armor 89.1 %, Cloak 0.3 %.
* Per session (grounded moving):

| Session | p50 | p90 | p99 | sprint | air | high | jumps/min | chain | rev/min |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| C-091 | 4.56 | 6.42 | 9.84 | 29.1 % | 12.1 % | 49.1 % | 9.19 | 2 | 4.59 |
| C-106 | 4.94 | 6.38 | 10.34 | 36.2 % | 12.9 % | 49.2 % | 9.34 | 7 | 6.44 |
| C-112 | 4.76 | 6.68 | 10.90 | 32.0 % | 18.2 % | 57.9 % | 13.46 | 9 | 4.18 |
| **pooled** | **4.84** | **6.45** | **10.48** | 33.9 % | 14.3 % | 51.6 % | 10.52 | 9 | 5.52 |

The owner is not faster than the bots in a straight line (p50 4.84 vs bots 4.75, p90 6.45 vs 7.21). The difference is
**air time, height and jump/chains**, not raw ground speed.

## 2. Jumps and chains

* **406 jumps in 2,316 s = 10.5 per minute** (56 / 203 / 145 per session; 9.19 / 9.34 / 13.46 per minute). Max chain **9** (C-112), 7 (C-106), 2 (C-091).
* 359 airborne segments; duration p50 **1.01 s**, p90 2.0-2.25 s. During jumps the horizontal speed is 6.3 m/s median,
  11.8 p90, 17.1 p99 — he keeps momentum.
* Airborne suit mode: **Speed 380 (29.6 %), Armor 758 (59.0 %), Strength 138 (10.7 %), Cloak 8 (0.6 %)**.
  - Speed-air = bunny hop (hop height ~0.85 m: 43 of 254 chains sit in the 0.84-0.88 m bucket, 61 in 0.80-0.90 m).
  - Armor-air = dropping off roofs/ledges (758 samples).
  - Strength-air = a Strength jump (138 samples) — the roof/container ascent.
* Chain z-gain (sum of climbs between landings): 254 chains, **p50 0.87 m, p90 4.33 m, max 10.61 m**; 50 chains > 3 m,
  22 > 5 m. A plain hop chain is ~0.85 m per hop; a roof chain is 3-10 m.
* The longest chains are on roofs and open ground, not stairs: C-112 max 9 hops, C-091 2. Bots never chain more than 3.

## 3. High ground

**51.6 % of the owner's samples are at z >= 140** (19.7 % at z >= 145). Per session 49.1 / 49.2 / 57.9 %.
High-ground runs (grounded, z >= 140) reach **43.7 s** (top 15: mean 23.9 s), i.e. he does not just cross roofs, he lives on them.

Top high spots, stay time, and the route in the 8 s before arrival (full list and all points in `owner_spots.json`):

| # | Label | Spot (x,y,z) | Height | Stay | Route (last 8 s, 1 s spacing) |
| ---: | --- | --- | ---: | ---: | --- |
| 1 | roof_147 | (1295.6, 1339.0, 148.4) | +12.4 m | 8.8 s | (1299.9,1353.3,148.2) → (1299.1,1353.3,148.2) → (1298.3,1350.7,148.2) → (1298.1,1342.0,148.1) |
| 2 | ledge_142 | (1294.4, 1297.4, 143.2) | +7.2 m | 19.0 s | (1311.1,1295.4,143.2) → (1311.3,1295.4,143.2) → (1303.8,1292.0,143.2) → (1292.5,1293.9,143.2) |
| 3 | ledge_142 | (1230.8, 1301.6, 142.0) | +6.0 m | 14.7 s | (1222.1,1302.9,139.5) → (1223.2,1302.6,139.9) → (1227.8,1298.8,141.3) |
| 4 | roof_147 | (1227.8, 1334.2, 148.9) | +12.9 m | 5.7 s | (1223.2,1343.3,145.6) → (1222.4,1345.4,147.0) → (1225.9,1343.0,148.9) |
| 5 | roof_147 | (1300.3, 1367.4, 146.6) | +10.6 m | 15.0 s | (1315.6,1358.5,138.5) → (1307.8,1355.9,138.5) → **(1307.9,1355.9,147.0)** → (1306.8,1358.3,146.9) |
| 6 | ledge_142 | (1240.0, 1292.7, 142.0) | +6.0 m | 16.0 s | (1225.2,1299.3,140.5) → (1227.8,1298.7,141.3) → (1231.3,1292.1,142.7) |
| 7 | **DSG1_roof** | (1300.3, 1322.7, 158.3) | **+22.3 m** | 16.0 s | (1317.5,1334.4,153.0) → (1317.7,1328.6,154.0) → (1317.7,1324.6,156.8) → (1317.3,1321.6,157.9) → (1314.6,1322.3,157.9) → (1310.5,1327.5,157.9) → (1307.9,1329.5,157.9) → (1304.2,1327.8,159.0) → (1300.2,1326.5,158.5) |
| 8 | roof_147 | (1228.3, 1347.9, 147.2) | +11.2 m | 9.0 s | (1219.4,1359.5,143.4) → (1216.1,1355.9,144.1) → (1213.8,1353.2,144.4) → (1223.1,1347.9,147.2) |

Two ascent signatures worth nav links:

* **+8.5 m in 1 s** at `(1307.8,1355.9,138.5) → (1307.9,1355.9,147.0)` (spot 5). This is a Strength/wall ascent, not a
  walkable ramp: xy is unchanged, z jumps 8.5 m.
* **+3.3 m** at spot 4 and **+3.8 m** at spot 8. Both are container/roof climbs with a horizontal move of 2-12 m.

## 4. Where he fights from, how long he holds, strafe rhythm

Dwell/prone segments at z >= 140 with a speed < 0.8 m/s for at least 1.5 s (fight proxy):

* **15 spots**, median hold **10.8 s**, max **18.3 s**, z **140.3-158.5**, 2 prone, 13 standing hold.
* Top holds: 18.3 s at (1295.8, 1338.7, 148.3) — the DSG1 roof spot 1; 15.0 s at (1239.9, 1292.8, 141.9);
  14.5 s at (1230.5, 1302.0, 141.9); 14.5 s at (1300.2, 1367.5, 146.7); 14.5 s prone at (1294.4, 1297.7, 143.2).
* Separate prone analysis: **45 segments, 138.3 s**; max 17.0 s at (1295.8, 1338.8, 148.3), 14.5 s at
  (1294.5, 1297.7, 143.2), 8.2 s at (1227.8, 1347.7, 147.3). He lies down on a roof and holds the angle.
* Yaw span inside a hold: 0-80° for most spots; the 6.5 s spot at (1295.4, 1374.0, 140.3) sweeps 178° (turning to
  cover a different approach), and 7.5 s at (1264.3, 1288.2, 145.0) sweeps 134°.
* Strafe rhythm: overall **4.18 / 4.59 / 6.44 reversals per minute**. In the 20 s around a hold the count is 0-5;
  the DSG1 roof hold has **5 reversals in 22 s (~14/min)**, i.e. he commits to a direction when travelling but
  strafes actively during a roof duel.
* Engagement distance is **not in the trace**. The owner playtest-2 report and the mail put his fights at 15-30 m
  (bots p90 hit 22 m, owner 31 m), but this cannot be re-measured from `MOVE TRACE`. Treat 15-30 m as a rule input,
  not a measured DS-041 number.

## 5. Bots in the same session (C-112)

| Metric | Owner (C-112) | Bots (range over 8 long-lived bots) |
| --- | ---: | ---: |
| speed p50 / p90 | 4.76 / 6.68 | 4.50-6.20 / 6.75-8.72 |
| sprint share | 32.0 % | 46.4 % |
| airborne share | **18.2 %** | 4.2-9.9 % |
| high-ground share (z >= 140) | **57.9 %** | 19.2-31.5 % |
| jumps/min | **13.46** | 1.75-6.27 |
| max jump chain | **9** | 2-3 |
| reversals/min | 4.18 | 5.24-21.59 |
| prone samples | 664 (7.4 %) | **0** |
| air mode Speed / Strength / Armor | 29.6 / 10.7 / 59.0 % | 9.3 / 12.6 / 78.0 % |
| high-ground run (grounded) | 18-44 s | 0.3-1.0 s (cells "held" 2-18 s while orbiting at 2-3 m/s) |

Read: bots are as fast on the ground and sprint more, but they spend half the owner's time in the air, a third of his
time on high ground, jump 2-7× less, never chain more than 3 hops, never go prone, and their "hold" is a 2-3 m/s orbit
inside a cell rather than a still firing position.

## 6. Concrete rules for the bots

Numbers from the tables above; the JSON is the source of the coordinates. Implement in `BotBrain`/`BotVantage`/
`BotLocomotion`; keep each behind the existing experimental gates.

1. **Roam goal = a high spot 30 % of the time (Veteran/Elite).** In `BotBrain::UpdateRoam`, with probability 0.30
   choose the nearest unused spot from `owner_spots.json` (`z >= 140`, `hold_s >= 8`) instead of a spawn point; use
   the stored `entry` route as the last 8 m of the path. Never two consecutive high goals; after a hold of 8-15 s
   (or one kill) relocate. Target: high-ground share 35-50 % of roaming time (owner 51.6 %, bots 26.3 %).
2. **A fight at 15-30 m → step to a ledge within 6 m.** In `UpdateAttacking`, if the target is 15-30 m, a high
   spot/ledge from the JSON is within 6 m, the muzzle is not `ShotBlocked`, and health > 40, move to the ledge and
   fight from there; otherwise fight in place. This is the owner's 15-30 m rule from playtest 2; distance is a rule
   input, not a DS-041 measurement.
3. **Prone on a high spot.** Marksman/Elite marksman holding a high spot for >= 3 s with a target > 25 m → prone
   (spread x0.33). Stand when the target is < 15 m or the bot is hit. Existing code only does this for
   `m_vantage.Phase()==Hold` and `m_traits.prone>0.5` (`BotBrain.cpp:750-757`); extend it to any high spot.
   Target: 5-8 % prone time (owner 7.4 %), 0 % today.
4. **Bunny-hop chains on open ground.** On a flat open run > 15 m with a clear path and no visible enemy, do 2-4 hops
   at 0.30-0.35 s after each landing in Speed mode; do not hop on stairs, in corners (`m_cornerSlow`) or while
   firing (DS-040). Target 9-13 jumps/min, max chain 5-9; bots are at 1.75-6.27/min, chain 2-3.
5. **Strength ascents +3..+8.5 m.** Add nav-link candidates from the routes above, especially
   `(1307.8,1355.9,138.5) → (1307.9,1355.9,147.0)` (+8.5 m), `(1223.2,1343.3,145.6) → (1225.9,1343.0,148.9)` (+3.3 m),
   `(1219.4,1359.5,143.4) → (1223.1,1347.9,147.2)` (+3.8 m). Require Strength energy >= 70 and a link take-off;
   one-way if the drop is > 4 m.
6. **One-way drop links 3-10 m.** The owner drops off roofs in Armor (758 airborne Armor samples). Add cheap one-way
   down links from the high spots; Armor before landing is already in `BotBrain.cpp:1010-1011`.
7. **Hold still, do not orbit.** At a high spot, stop (`speed < 0.8`) and aim for 8-15 s; bots currently circle at
   2-3 m/s inside the cell. Relocate after 8-15 s or one kill (`BotVantage::OnKill` already exists).
8. **Strafe rhythm in a duel.** At 15-30 m with a visible target, reverse the strafe direction every 0.7-1.2 s while
   firing (`m_intent.strafe`); outside combat keep reversals <= 6/min. Owner 4.18-6.44/min overall, 14/min in the DSG1
   roof duel; bots 5.24-21.59/min with jitter.
9. **Suit mix.** Speed 10-12 % (travel/roam, hops), Strength 4-5 % (jumps), Armor ~85 %; airborne Speed ~30 % (hops).
   Bots are at 12 % Speed, 2.6 % Strength, 9.3 % air-Speed: raise Strength via rule 5, raise air-Speed via rule 4.
10. **No crouch.** 39 owner samples (0.4 %) vs 0 bot crouch; use prone for a firing position. Do not add crouch rules.

## 7. Missing fields (Claude adds to the recorder)

Present already: position, velocity, speed, z, suit mode, energy, sprint, grounded/flying, jump/land edges, takeoffs,
peakZ, health, yaw, pitch, stance — at 4 Hz.

Missing, in priority order:

1. **Enemy/target id and distance on the owner's trace.** Without it, "fights from 15-30 m" is a report input, not a
   measurement; every fight spot in `owner_spots.json` has z and hold but no range.
2. **Owner fire/shot/hit/damage events.** The client log has only `MOVE TRACE`; we cannot tell if a hold was a fight,
   a reload, or a lookout.
3. **Jump type**: Normal vs Strength link vs wall jump. The only current proxy is the suit mode before take-off.
4. **Wall contact / wall-jump** (the playtest-2 "wall jump chains" cannot be identified).
5. **Jump/dodge cause** (link, dodge, random, bunny hop) and whether the hop was deliberate.
6. **Local floor height per sample** (or a `groundZ`/`onRoof` flag): z >= 140 is a proxy; a roof at z 142 on a hill is
   not the same as a 6 m ledge.
7. **Sampling rate**: 4 Hz in these traces; C-084 asks >= 20 Hz for circle/wall jumps. At 4 Hz a 0.25 s hop can be
   missed and chain boundaries are coarse.
8. **Stance source** (input toggle vs forced by aim) and **yaw rate** (absolute yaw is logged, not turn speed).
9. **Suit mode is logged** (contrary to the mail's worry), but the **reason** for the mode and the refused mode are not.
10. **Health at fire time and the shooter** are not joined to the trace, so "fights while low" cannot be measured.

## 8. Caveats

* Pooled percentiles are weighted by samples; per-session values are in the table. `jumps/min` pooled = 406 jumps /
  2,316 s, not a sum of overlapping session clocks.
* `high_share` counts airborne samples above z 140; `high_runs` uses grounded samples only.
* Bot rows are server-side 4 Hz; bot "hold" is measured in 3 m cells and includes orbiting, so it is not comparable
  with the owner's speed < 0.8 holds without the per-sample check above.
* The owner C-112 trace has 3 `OWNER MARK` events (t=220.97, 755.38, 756.01) only; there is no reliable per-fight
  annotation.
