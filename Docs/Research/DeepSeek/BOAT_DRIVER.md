# BotBoatDriver.h (DS-039 #3)

Engine-free boat controller + water-route builder in `proto/BotBoatDriver.h`; tests in
`proto/BotBoatDriverTests.cpp` (MSVC 19.44: `boat driver: all tests passed`).

## Driver

Input (`BoatInput`): `pos{x,y}`, `yaw` (rad, forward = cos/sin), `speed` (m/s), `depthAhead`, `depthLeft`,
`depthRight`, `grounded`, `now`.

Output (`BoatOutput`): `BoatAction` (`Forward/Reverse/TurnLeft/TurnRight/Brake/Exit`), `throttle` 0..1,
`steer` -1..1, `brake`, `exit`, `reason`.

Contract (BOATS_SHORE):
- lookahead 12 m along the polyline; steer = heading error / 0.8 rad, ±1;
- brake 8 m before the landing and when speed > cruise 11 m/s; slow to 5 m/s for turns > 1 rad;
- shallow `depthAhead < 1.2 m` → brake and steer to the deeper side; left/right depth steers away;
- speed < 0.5 m/s for 2 s → reverse for 1 s (`stuck_reverse`), then resume;
- within 6 m of the final point at < 2 m/s → `Exit`.

CryEngine mapping: one `Update` per 10 Hz tick; a consumer maps `Forward`/`Reverse` to
`eVAI_MoveForward`/`eVAI_MoveBack`, `steer` sign to `eVAI_TurnLeft/Right`, `brake` to `eVAI_Brake`, `exit` to
`eVAI_Exit`/`IVehicleSeat::Exit`.

## Water route builder

`DepthGrid` (origin, cell, nx, ny, depth metres) + `WaterRouteBuilder::Build(grid, start, goal)`:
- passable = depth ≥ 1.2 m and 6 m clearance from land (outside grid = land);
- A* 8-neighbour, cost = step × (1 + 2/(depth+0.5)), Euclidean heuristic;
- string-pull smoothing with a water-only line check, resampled every 10 m;
- returns `ok/reason/points` (`no_water_route`, `start_or_goal_land`, `off_grid`, `bad_grid`).

## Tests

Straight leg, 90° turn, stuck → reverse, arrive → exit, shallow → brake/steer, open-grid route, wall with a gap,
full wall → `no_water_route`. No engine/game build; the header is pure C++17.

## Integration notes

- Feed `depthAhead/Left/Right` from three down-rays (3 m / 4 m lateral) at 10 Hz, not every frame.
- The driver does not own `IVehicle::OnAction`; the game adapter converts the output as above.
- Use `WaterRouteBuilder` offline from the runtime depth grid (BOATS_SHORE §3.1), cache the route per level/water
  level; re-plan when `no_water_route` or the boat is >15 m off the polyline.
