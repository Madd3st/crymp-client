# DS-064: Shore capture eligibility audit (native rules, base 532e4497)

Inputs: `C144_SHORE_PREFLIGHT.json`, `Shore.capture-candidates.json`, extracted
`level.pak/mission_mission0.xml` (`ps-Shore.xml`), stock `PowerStruggleUtils.lua` and
`Pak/CryMP/Scripts/GameRules/PowerStruggleAlert.lua`. No game/build; no capture events were observed, so nothing
below is a capture-success claim.

## The five explicit capturables

| Entity | Class | Pos (Crysis) | Req | Time | Index | Area (`Area Id=1`) | XML |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 64 | AlienEnergyPoint | 1965,1387.75,105 | 1 | 15 | 1 | 39 `Shape9`, h38 | `ps-Shore.xml:1625` |
| 316 | AlienEnergyPoint | 2075.4373,1285.3788,126.185 | 1 | 15 | 1 | 291 `Shape9`, h38 | `:2803` |
| 654 | AlienEnergyPoint | 1799.375,1686.25,102.75 | 1 | 15 | 1 | 629 `Shape9`, h38 | `:7966` |
| 344 | Prototype-Factory | 2145.1965,1484.6385,110.031 | 1 | 30 | 3 | 330 `Shape4`, h4 + 323 `Shape5`, h7 | `:8004` |
| 579 | Vehicle_Factory_Neutral | 2236.7585,1325.8352,124.32 | 1 | 30 | 2 | 569 `Shape_VF_neutral`, h6 | `:8838` |

All five have `bCapturable=1`, `captureAreaId=1`, `teamName=""`, and a matching `AreaShape` that lists the entity in
its `Area/Entities`; 64/316/654 are inside their h38 energy shapes. 344's origin lies just outside `Shape4`'s max x
(2144.95 vs 2145.20) and on the `Shape5` boundary; 579's origin is 0.17 m outside `Shape_VF_neutral`'s min y
(1326.01 vs 1325.84). The bot hold radius is 6 m (`BotPSCapture.h:36`), so the hold ring overlaps the area, but the
exact origin event is not proven. **Do not assume captureRequirement=2**: all five are 1; `PS_PLAN.md:17`'s
"2 players" is stale for Shore (the stock `MakeCapturable` default 2 is overridden in every mission property here).

## Does an IsPlayer=false/channel0 bot count?

Rule-level **yes**, with one visual-only gate:

* `PowerStruggleUtils.lua:112-127` inserts the entering `entity.id` into `self.inside` before any callback; this is
  the list `UpdateCapture` reads.
* `UpdateCapture` (`:174-208`) filters only `not player:IsDead()`, `player.actor:GetSpectatorMode()==0` and
  `game:GetTeam(playerId)`; it never calls `IsPlayer`, `GetChannel`, `GetPlayerCount` or
  `IsPlayerActivelyPlaying`. `ok = count>0 && count >= captureRequirement` (`:208`).
* `PowerStruggleAlert.lua:655-657` skips the client `ClEnterCaptureArea` RMI when `entity.actor:IsPlayer()` is
  false - that is the correct bot guard and does **not** remove the entity from `inside`.
* `Capture` awards PP/CP to entries in `inside` with `not IsDead` and spectator 0 (`:412-418`); no IsPlayer gate.
* Alive bots have health 100 and spectator 0 (C140 BODY lines); team is set before spawn and logged as team 1/2.
  Thus the native rules accept a bot as one eligible actor for these five.

The remaining eligibility question is the area event itself: no C140 bot reached a point, and no `PS EVENT
capture_start`/`captured` exists, so "AreaShape delivers OnEnterArea for a bot" is rule-plausible but **not yet
observed**. The missing observation is one bot inside a capture area in a run; until then this is not a proven bug.

## Autonomous selection vs eligibility

`BotPSCapture.cpp:20-60` snapshots every `bCapturable` entity and the planner (`BotPSCapture.h:36-65`) picks the
nearest unowned point; it does not read `captureRequirement` or pair actors. For these five req=1 that is fine.
For any future `req=2` point it would send one bot and native `ok` would stay false - a planner limitation, not a
native eligibility bug. No repair is proposed here because the five Shore points do not need it and no capture event
has run yet.

## Reproducible acceptance recipe

Run the C140 recipe with `wb_ps_capture 1`, `wb_ps_events 1`, `wb_ps_doors 1`, `wb_move_trace 1`; require a bot to
enter a point's `Area Id=1` (start with 64 at `(1965,1387.75)` and 316 at `(2075.44,1285.38)`).

Expected evidence, in order:

1. `PS CAPTURE target=<id> pos=... team=...` (planner selection);
2. `PS EVENT capture_start entity=<id> class=... team=...` (`PowerStruggleAlert.lua:638`);
3. `PS EVENT capture_step entity=<id> ... remaining=<t>` (`:649`);
4. `PS EVENT captured entity=<id> class=... team=<team>` (`:675`), team becomes the bot's team;
5. no Lua/RMI/channel error; the entity's `GetTeam` changes in `PS CAPTURE`.

No `capture_start` with a bot standing inside is the precise missing observation; then log `inside` membership
(area id + entity id) before claiming a native gate. No repair patch is shipped from this audit.

## Verdict

* Native eligibility: **yes at rule level**, all five req=1, area ids present; visual `IsPlayer` gate is harmless.
* Proven capture participation: **no** - no arrival/event in C140; DS-063 door/path is the immediate blocker.
* No fake capture event and no new capture goal/subsystem; autonomy is separate from eligibility.
