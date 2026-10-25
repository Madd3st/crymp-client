# Owner playtest checklist — log/demo evidence map (C-075)

Fill after Codex posts the run paths (`*.bots.log`, `*.wbdemo`, client sidecar). Baseline: X-063
(`91a01bbc`) 15×660: 267 kills / 37 stalls / 0 traps. Use `wbrun/1` (`REPORT_SCHEMA.md`) for counts.

| Checklist item | Evidence FOR | Evidence AGAINST |
| --- | --- | --- |
| Laser | `WEAPON HANDLING ... lam=1`, no `lam=0` while not cloaked; `FIRE HIT` from bots with LAM | `lam=0` in a normal (non-cloak) life; log absent with `wb_bot_weapon_handling 1` |
| Zoom | `WEAPON HANDLING ... zoomDistance>0` with `FIRE HIT distance` ≥ threshold; demo `FRAME.flags bit4` | zoom on shotgun/pistol or <8 m; `zoomDistance=0` at long range |
| Names | `wb_list` / `TARGET CHOICE ... name=WarsBot_XX [Skill/Style]`; kill feed lines | default `WarsBot_XX` without profile for bots created after the flag |
| Snipers on roofs | `VANTAGE travel` + `VANTAGE relocate`; telemetry z on a high spot; demo `BRAIN` target/aim from roof | no `VANTAGE` claims (X-059/X-066 symptom), or bot leaves the roof immediately |
| Prone | demo `FRAME.stance=2` on a held spot; `TACTIC SAMPLE`/position stationary | stance stays 0 while holding; prone in the open, not on a spot |
| Spectator ammo | `NETWORK ... REPLICA` ammo fields; demo `INV` clip/reserve non-zero; client HUD | HUD 0/0 while the bot fires; `ammoClip=0` in `INV` |
| Aim lag | demo `FRAME` yaw vs `BRAIN.aim` error, `client_smooth` on; `NET POSE ... snapMax` | view lags >5° behind a firing bot; aim snaps >720°/s in the owner sidecar |
| Angle hold | `HOLD angle from (x,y,z) on (x,y,z)` lines, count per bot-minute | zero `HOLD` with `wb_bot_tactics 1`; hold while the target is visible |
| Jump-strafe | `JUMP TAKEOFF` normal jumps during `ATTACKING`; ratio per duel | no jumps in duels or jumps while prone/link |
| Loot | `LOOT item=...` + `REAL PICKUP ... acquired=1` within 12 s of a kill | `LOOT` never fires; loot target not picked |
| Strength links | `LINK start kind=strength` → `LINK done kind=strength`; `JUMP TAKEOFF: strength=1` | `LINK failed ... reason=missed_landing/no_energy`; done% below X-063 |
| Stuck spots | `TRAP RECOVERY` clusters (DS-009 2 m/≥3), `NAV fallback`, `MOVE TRACE` stalls | new cluster or stalls > X-063 37; spawn 176/195 recurring |

Commands: Triage `summary <log>`, `demo <wbdemo>`, `pace <log>`; `REPORT_SCHEMA.md` rules; cluster compare against
`c070-on-91a01bbc`/`c071-on-dc9808c8` from the owner PC. Do not call a checklist item PASS from a single line; require
≥2 independent lines or the demo field named above.
