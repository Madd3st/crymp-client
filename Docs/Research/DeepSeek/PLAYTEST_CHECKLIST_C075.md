# Owner playtest C-075 evidence (91a01bbc, 8 bots, ~1674 s)

Source: `build/owner-playtests/Sprint-91a01bbc-20261025-194805/` server bots.log + wbdemo/2. Log-only unless
marked demo. This is not the C-078 corners batch; `session-final.json` says `corners_effective:0` (91a predates it).

| Item | Evidence | Verdict |
| --- | --- | --- |
| Laser | `WEAPON HANDLING` logs `IsLamAttached`, **not activation**: 589 weapon samples, 24 `lam=1`, 565 `lam=0` (`BotWeapons::Handle`). No line proves the beam was on. | **unconfirmed**; add an activation field/log. |
| Zoom | same log prints `zoomDistance` (threshold 20/15), not `IsZoomed`: SCAR 102/97 FY71/23 SMG samples at threshold. Actual zoom is demo `FRAME.flags bit4`. | **unconfirmed** from log; demo parser needed. |
| Names | all `CreateActor name=` lines carry `[Skill/Style]`, e.g. `WarsBot_03 [Veteran/Marksman]`. | **PASS**. |
| Snipers on roofs | `VANTAGE 246 sniper spots` load only; no `VANTAGE travel`/`relocate`; no bot prone. | **AGAINST/unconfirmed** (no claim evidence). |
| Prone | demo FRAME: 0 stance=2 for all 16 bot ids; only the owner (130997) has 761 prone frames. | **AGAINST for bots**. |
| Spectator ammo | demo INV: all 16 bot ids have entries with clip/reserve >0 (max 500/500). Server-side replication confirmed; client HUD not logged. | **PASS at data level**, HUD unverified. |
| Aim lag | needs BRAIN aim vs FRAME view; not measured here (parser not run). | **no evidence**. |
| Angle hold | `HOLD angle from` 228 lines. | **PASS** (active). |
| Jump-strafe | `INTENT WHY` + `JUMP TAKEOFF`: 691 normal jumps with `state=ATTACKING`, 9 Strength. | **PASS**. |
| Loot | `LOOT item=` 107; `REAL PICKUP acquired=1` 336. | **PASS**. |
| Strength links | `LINK start kind=strength` 84, `done` 37, `failed` 43 (44 % done); jump links 99/87, drop 56/31. | **AGAINST** (high failure). |
| Stuck spots | 31 `TRAP RECOVERY`; clusters (2 m/3D): 12 at 1291.5,1342.3; 7 at 1199.7,1298.3; 4 at 1306.1,1354.0; 3 at 1360.9,1349.0. | **AGAINST** (top cluster is not the old stair one). |

Notes: server log loads the navmesh (9333 polys/1133 links) even though the demo header says `"navmesh":"none"` — the
header field is unreliable, do not use it for gate evidence. The `WEAPON HANDLING` log is one line per weapon entity
(static dedup), so 24/589 is not a time share.
