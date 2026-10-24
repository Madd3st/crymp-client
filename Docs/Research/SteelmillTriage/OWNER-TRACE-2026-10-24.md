# Owner MOVE TRACE, 2026-10-24 15:43 and 15:51 — the human reference

Source: `OwnerTrace-20261024-154328-Client.bots.log` (1261 samples, ~5 min) and `...-155135-Client.bots.log`
(2084 samples), `MOVE TRACE kind=human side=client`, 4 Hz, build 8b9236f4 (Codex launcher). Evidence: **client
trace** (exact positions/velocities), video for context. Codex's log root is the Store-app sandbox:
`C:\Users\Magic8\AppData\Local\Packages\OpenAI.Codex_2p2nqsd0c76g0\LocalCache\Local\WarsBots\{Logs,Memory}`
(connected as its own folder). Human revive at spawn 195 (`wb_revive_at 195 me`): **did not freeze** (owner).

## Pace (grounded, moving > 0.5 m/s)

| | Session 1 | Session 2 | Bots today |
| --- | --- | --- | --- |
| median horizontal speed | **5.0 m/s** | **5.7 m/s** | 2.5 |
| > 4 m/s share | **70 %** | **77 %** | 4–10 % |
| > 6 m/s share | 35 % | 38 % | ~0 |
| sprint while moving | **46 %** | **54 %** | 7–17 % |
| suit mode while moving | Armor 84 %, Speed 10 %, Strength 6 % | Armor 87 %, Strength 8 %, Speed 5 % | Armor 72–100 % |
| Armor run / Armor sprint (median) | 3.2 / 6.0 m/s | 3.7 / 6.1 m/s | 2.5 / 3.75 (src) |
| Speed mode sprint (median / max) | 10.2 / 13.3 m/s | 9.9 / 12.8 m/s | ≤ 5.5 (src) |

The video-based estimate earlier (53 % > 4 m/s) was low because of 1 s sampling; the trace is the reference.
Armor + Shift is the default; Speed mode is used in short bursts; Strength for jumps.

## Jumps

* Strength jumps (mode 1): typical rise 0.8–0.9 m on flat ground (hops), **single-jump ledge climbs up to
  +3.9…+5.2 m** with 6–8 m horizontal travel; a two-jump wall climb at (1367, 1334) gained **+7.8 m** in 2 s
  (the +7 m wall in the first video). Native Strength jump height is 4.7 m (src, OC-001) — consistent.
* Armor jumps: climbs up to +2.9 m (ledge grabs on crates); flat hops ~0.
* Speed-mode sprint jumps cover 12–18 m horizontally.
* Places of the big climbs: (1367, 1334) blue wall at pit B, (1313, 1358) stairs/catwalk at pit C,
  (1300, 1374) and (1249, 1308) containers. These are the first candidates for strength-jump graph links.

## Bot vs player (src, OpenCode OC-002 @ 798ebb08)

Bot base speed is clamped to 2.5 m/s in `SetWarsBotMotion` (Player.cpp:4972) vs 4.5 native; bots never get
Speed-mode ×1.3/×1.5 because `AdjustMovementForEnvironment` runs only for `IsPlayer()`
(PlayerMovement.cpp:1070); bot sprint needs energy > 45 plus profile gates; Strength jump gate 90 vs 70;
3 s jump cooldown vs none; pseudoSpeed 0.3/0.5 vs 1/2/3. Full table: movement-params branch,
`Docs/Research/MovementParams/BOT_VS_PLAYER.md`.
