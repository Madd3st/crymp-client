# Owner reference video, 2026-10-24 14:08 (5 min, Steelmill) — first pass

Source: `Desktop_2026_10_24_-_14_08_18_02.mp4` (1080p30, 295 s), owner's PC. The owner's play is the **reference**
for bot movement. Positions: `r_displayInfo` CamPos OCR'd once per second (178/295 frames, glitches removed →
171 samples; camera position = eyes, ~1.7 m above feet). Audio could not be transcribed in this workspace
(speech models are not reachable); visual only. Evidence type: **client** (video) + **graph**.

## Numbers

* Horizontal speed while moving (1 s samples, n = 98): median **4.3 m/s**, p75 6.1, p90 9.0;
  **>4 m/s 53 %**, >6 m/s 26 %. Bots: median 2.5 m/s, >4 m/s 4–10 %. Matches the earlier LOS-channel estimate
  (owner 41–54 %).
* Time by BotNav component under the owner's feet: main-B (street) 43 %, sinks 22 %, small pieces 22 %,
  isolated 11 %, no-entry 4 %, main-A (upper) 2 %. **The owner spends >50 % of the time on parts of the map the
  bot graph treats as traps or islands.**
* Of 28 component changes along his path, the graph can reproduce **1**. After removing OCR glitches the real
  ones cluster in three kinds of places (below).

## Where the graph disagrees with a human (client + graph)

1. **Metal stairs and catwalks with pipe railings** (t 141–159 s, (1310–1317, 1323–1358), z 147 → 160; also
   t 283 s next to pit B): he walks up in steps of ~3 m height per second; the graph has these as separate sinks
   and small SCCs. Spawn 195's clearance hit (knee height, D) is the same kind of rail. Suspect `Walkable`'s
   centre-line rays at 0.5 / 1.2 / 1.75 m hitting railings, and stair geometry failing the ground-sample test.
   Sheet: `evidence/owner-video-stairs-126-159.jpg`.
2. **Strength jumps onto containers / walls** (t 272–282 s, (1344–1361, 1315–1334)): climbs a two-high tank
   container frame, then **+7 m in one second** up the blue wall at (1361, 1333) z 142 → 149 (next to pit B).
   BotNav allows rises up to 1.9 m and has no jump links at all. Sheet: `evidence/owner-video-strength-272-283.jpg`.
3. **Roof running and drops** (t 200–221 s, (1196–1231, 1306–1346), z 145–148; t 178–180 s drop 160 → 146 m):
   moves across corrugated roofs and drops down 3–10 m; roofs are isolated/no-entry islands in the graph.
   Sheet: `evidence/owner-video-roofs-200-233.jpg`.

The area around spawn 195 (t 70–84 s, (1243–1247, 1309–1319)): he walks and turns freely along the sidewalk
and onto the road; the railing is waist-high bollards with a rail — passable at the gaps.

## What this means

* The graph split is not a few bad edges: three whole classes of terrain (stairs/catwalks, strength-jump
  ledges, roofs) are missing. Fix order: (a) stairs/railings — `Walkable` false cuts (C-006 measures which
  test); (b) drop links (down up to ~10 m, cheap, one-way); (c) strength-jump links (up to ~7 m, verified by a
  reachability test at build time, gated). Bots that use (c) also need suit Strength before the jump.
* Pace target for step 2 stays: >4 m/s share while moving 35–55 % (owner 53 %).

## Next (needs MOVE TRACE)

Exact jump heights/lengths, sprint/suit timing and landing positions need `MOVE TRACE` on the owner's client;
a 5-minute repeat of the same route with it on is enough.
