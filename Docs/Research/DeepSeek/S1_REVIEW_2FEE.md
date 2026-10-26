# Review: new claude-sprint commits 06a6e79b..2fee59c2 (S1)

Date: 2026-10-26. Commits: `ec46f1a3` (FIRE SHOT distance, wb_mark) and `2fee59c2` (DS-035 patch, filtered links,
wb_rules_info). Read-only. `BotDifficultyTests` and `BotJumpLinkTests` built with MSVC and pass; the game was not
built.

## Findings

1. **`wb_mark` still loses the wall clock/demo event when there is no client actor (medium).**
   `BotManager.cpp:739` returns on `!me` before the new `wall=` line and `Demo().Mark()` at `:745-748`. A server-console
   mark or a spectator without `GetClientActor()` gets the old t-only line and no demo event 10, exactly the case
   DS-022 was meant to cover. Move the wall/demo block above the `if (!me)` branch (the text is already built; use 0
   pos/view for the no-actor case).

2. **`BotJumpLink` total window is shorter than Prepare + Act (medium).**
   `BotJumpLink.h:51` now `prepareMax=6`, `:53` `noTakeoff=3.5`, but `:52` `total=9`. A link that waits 6 s for
   Strength then needs a 3.5 s Act window can hit `Timeout` at 9 s before `NoTakeoff`; the DS-036 fix only moved the
   boundary. Set `total=12` (or `prepareMax + noTakeoff + 1`).

3. **`FIRE SHOT distance` uses the brain’s current target, not the shot’s target (low).**
   `BotManager.cpp:222-223` reads `bot->Brain().Target()`; `WeaponEvent.cpp:56` calls
   `TraceShot(weaponId, shooterId, ammoId)` without a target id, so a shot fired at a target that changed between
   `BotCombat::Attack` and the log gets `distance` to the new target, or `-1` when `Target()==0`. Log the target used
   by the shot (pass it from `WeaponEvent` or use the projectile/hit target) so `duels.ps1` bands are exact.

4. **Mark wall clock is local, not UTC, and has no session/build (low).**
   `strftime(..., localtime(&wall))` at `:746` writes server-local time; DS-022 asked for a UTC anchor plus a session
   id/build. The demo event helps, but the client/server join by `utc` still needs the format.

5. **No regression in the filtered links or difficulty numbers checked.** `Steelmill.links.txt` keeps 34/34 DSG1/Gauss
   per the audit; `BotDifficultyTests` expects Elite fire 100 (D-027) and passes; `BotJumpLinkTests` passes. The
   `prepareMax=6` text has a duplicated old comment on the same line (cosmetic).

## Suggested fix order

1. Move the mark block before the early return (one small patch).
2. `total=12` for links.
3. Pass the shot target into `TraceShot` (log-only).
4. UTC/session/build for marks (DS-022 completion, not a blocker).

## What not to change

- `fireCap=100` for Elite is intentional (D-027); `holdCap=44`/`engageCap=38` keep the 20–30 m band.
- The 78 removed links are flat/zero/duplicate Strength links; unknown links stay unused, DSG1/Gauss remain reachable.
- `wb_rules_info` is read-only; no issue found.
