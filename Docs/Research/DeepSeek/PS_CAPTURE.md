# PS-1 capture goal (DS-048 #3)

PS-0 passes (C-121: PS wave QUEUE 4 / REVIVE true 4). This adds the first objective behaviour behind
`wb_ps_capture 0` (default OFF), using the DS-028 order: power points first, then the nearest capturable factory /
proto, value-weighted by `nCaptureIndex` (100/200/300 PP, 15/22.5/30 s on shipped maps).

## Planner (`Code/CryGame/Bots/BotPSCapture.h`, engine-free)

`PsCapturePoint { id, ownerTeam, value, captureSeconds, capturable, power, x/y/z }` and
`BotPSCapture::Update(points, n, team, bot, holdRadius=6, switchMargin=15)`:

- ignore own points and non-capturable ones; score = distance - 8*value - 6 for `AlienEnergyPoint`;
- nearest best wins, but a valid current target is kept unless the alternative is > 15 m better (no oscillation);
- `hold` when the bot is within 6 m (inside the map capture shape, ~26.5 m diameter) - the engine stops movement
  there; when `GetTeam(point) == team` the point is skipped and the next one is chosen automatically.

## Engine adapter

- `BotPSCapture.cpp` caches `AlienEnergyPoint` and `Factory` entities once per level via `GetScriptTable()` ->
  `Properties` (`bCapturable`, `nCaptureIndex`, `captureTime`), refreshes only `ownerTeam` with
  `CGameRules::GetTeam(id)` every 2 s (`PSCaptureSnapshot`). PS Shore has 3 energy points + 4 factories; Plantation
  2 + 1 (verified from the mission XML).
- `BotBrain.cpp:715` calls `UpdatePSCapture` from `UpdateRoam` (no visible enemy, after the weapon/upgrade goals),
  steers to the point, or stops and looks at it inside 6 m. Logs `PS CAPTURE target=<id> pos=... team=... hold=...`
  once per target. `ResetLife` clears the commitment.
- `BotConfig.h:56` / `BotConfig.cpp:16`: `wb_ps_capture` (`0` OFF, `1` ON).

## Tests

`Tests/BotPSCaptureTests.cpp` (CI list `BotPSCapture`): own points ignored, nearest choice, value/power weighting,
hysteresis (10 m better keeps, 19 m better switches), captured target -> next, no team/points -> invalid.
MSVC `/std:c++17 /permissive-`: pass.

## Limits

- Only `AlienEnergyPoint`/`Factory`; `HQ`/`Flag` are not capture targets here.
- The 6 m hold is inside the capture shape but the game still needs 1+ player and 15-30 s; the planner does not
  read `captureRequirement` (shipped maps = 1).
- Not run in the engine; the patch is default OFF and `BotBrain.cpp`/`BotPSCapture.cpp` compile with
  `WARSBOTS_EXPERIMENTAL_SPAWNER=ON`.
