# PS measurement (DS-050 #3)

PS runs previously had no IA-style summary. `wb_ps_summary` (`BotManager.cpp:157`, `:835`) logs a parser-friendly
line per bot and per team:

```
[WarsBots] PS SUMMARY points=7 team1_held=2 team2_held=3.
[WarsBots] PS SUMMARY bot=3 id=131003 team=2 pp=350 rank=2 buys=2.
```

- `points` = capturable `AlienEnergyPoint`/`Factory` entities from the PS-1 cache; `teamN_held` counts their current
  owner (`CGameRules::GetTeam`), i.e. map control, not a capture-event counter.
- per bot: team, PP (`GetPlayerPP`), rank/CP order (`GetPlayerRank`) via the rules script table, and `buys`
  (`BotBrain::PSBuys()`, incremented for every successful primary/ammo purchase in PS-2).
- Works only on PowerStruggle; on IA it logs `skipped`.
- The line is level 0 and goes to the `.bots.log` next to `MOVE TRACE`/`PROFILE`, so the existing PS run tools can
  grade team, PP, map control and spend the same way IA uses kills/pickups.

Limits: no per-bot capture-event counter yet (map control is the proxy); PP/rank are Lua reads at the moment of the
command, so call it at a fixed time (e.g. 30/120/240 s) for a comparable series. Not run in the engine here.
