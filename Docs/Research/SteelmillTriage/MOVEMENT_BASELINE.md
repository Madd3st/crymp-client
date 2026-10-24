# Movement pace: bots vs the owner (2026-10-24)

Owner report: bots move "like invalids", not on Shift, slow, never break out of cover.
Tool: `python Tools/SteelmillTriage/steelmill_triage.py movement <run.bots.log>...`.
Evidence type: server trace logs only. No client or visual check here.

## Bots, by state (owner vs 11 Elite, build 5e8daa65, moving samples only)

| State | Requested sprint | Native sprint on | Median speed | Suit mode | Median energy /200 |
| --- | --- | --- | --- | --- | --- |
| MOVING_TO_WEAPON | 27% | 12% | 2.40 m/s | Armor 72%, Speed 26% | 117 |
| SEARCHING_ENEMY | 17% | 15% | 2.42 m/s | Armor 82%, Speed 16% | 100 |
| CHASING_ENEMY | 10% | 7% | 2.49 m/s | Armor 90%, Speed 9% | 89 |
| ATTACKING | 0% | 0% | 2.45 m/s | Armor 100% | 39 |

Grounded ground speed by native flags (all states): sprint on in Speed **4.06 m/s** median, p90 5.39
(n=1423); sprint off in Armor 2.48, p90 2.50 (n=9964). Play6 session gives the same picture.
Refusals in the 48-minute match: `speed_energy` 330, `strength_energy` 147, jump landing check 165,
Strength mode/energy 116.

## Like-for-like with the human

Only one channel records the owner's position: `LOS blocked target=<id> ... chest=(x,y,z) sample=t`,
logged when a bot checks line of sight to a target. Measuring bots through the same channel:

| Session | Who | n | Median | p75 | p90 | Faster than 4 m/s |
| --- | --- | --- | --- | --- | --- | --- |
| Play11Elite | owner (Nomad) | 26 | 4.11 | 4.77 | 6.79 | 54% |
| Play11Elite | bots | 258 | 2.07 | 2.43 | 2.61 | 1.9% |
| Play6 | owner | 29 | 2.62 | 5.22 | 6.55 | 41% |
| Play6 | bots | 23 | 1.61 | 2.40 | 2.49 | 0% |

Small human samples, biased to moments a bot was looking at the owner. The gap is still large and
consistent: the owner spends 40–55% of observed time above 4 m/s, bots almost never.

## Cause (code + data)

1. Bots reach human pace only while native sprint is on, and it is on 0–15% of moving time.
   Gates: travel sprint needs energy > 120 and > 15 m to go (`BotBrain.cpp` `TravelGait`); chase sprint
   needs > 100 energy beyond `sprintChaseRange`; combat never sprints; median energy is 39–117.
2. Without sprint the request is fixed at 2.5 m/s with a half-speed animation hint:
   `BotLocomotion.cpp:121` (1.3 / 2.5), `BotMovement.cpp:107-108` (`SetDesiredSpeed`, `SetPseudoSpeed(0.5)`),
   `Player.cpp:4964` (`clamp(speed, 0, 2.5)`). Whether 2.5 equals a human's non-sprint run is
   **unknown**; no human non-sprint sample can be separated from these logs.
3. No action leaves cover or peeks: there is no cover concept (WARSBOTS_AI_GAPS).

## What would settle it: human trace (spec for Codex)

One additive server line, local IA only, behind `wb_trace_move 0|1` (default 0), 4 Hz per actor, for
humans *and* bots so both are sampled identically:

```
MOVE TRACE actor=<EntityId> human=<0|1> pos=(x,y,z) speed2d=<m/s> vz=<m/s> grounded=<0|1>
  sprint=<0|1> mode=<ENanoMode> energy=<0..200> health=<hp> stance=<EStance> ladder=<0|1>
  weapon=<class> firing=<0|1>
```

Fields come from what `ABILITY PHYSICS` already reads (`SPlayerStats`, NanoSuit, physics
dynamics). Cost: 4 lines/s/actor at level 2, file only. Then one 10-minute owner session on
Steelmill with 6 bots gives: human sprint share by situation, non-sprint run speed, mode use,
energy at fight start, jump frequency.

## Targets for the pace change (until the human trace exists)

- Share of moving time above 4 m/s while CHASING/SEARCHING/MOVING_TO_WEAPON: from ~10% to 35–55%.
- Energy at the first shot of an engagement: not lower than today's median (39) by more than 10.
- No increase in deaths per bot-minute, pinned/trap %, or `speed_energy` refusals per minute.
- Measured with this tool on ≥ 2 paired 10-minute runs, then the owner plays against both builds.
