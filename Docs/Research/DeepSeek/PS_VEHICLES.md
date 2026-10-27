# PS vehicle entry (DS-052 #4)

Spec follows `VEHICLES.md` §2: seat-level entry is server-safe (`IVehicleSeat::Enter(EntityId, false)`), the PS
`CanEnterVehicle` Lua check stays the authority (`PowerStruggle.lua:647`), and `IVehicle::GetSeatById(1)` is the
driver on `Asian_ltv`/`Civ_car1`.

Patch (`wb_ps_vehicles 0`, default OFF):
- `BotBrain::UpdatePSInteract` (only PS, no visible enemy, not already in a vehicle) scans a +-10 m box for
  `Asian_ltv`/`Civ_car1`; takes `GetIVehicleSystem()->GetVehicle(id)`, seat 1; requires no passenger.
- Calls the stock `CanEnterVehicle(vehicle, botId)` through the rules script table (same check as a player).
- `seat->Enter(id, false)` (no transition animation); logs `PS VEHICLE enter vehicle=... class=... actor=...`.
- Exit after **3 s** via `GetSeatForPassenger(id)->Exit(false,false)`; logs `PS VEHICLE exit`; 5 s cooldown before the
  next try. Death/destroy is already reset by `GameRules`; the bot stops the module if the seat is gone.

No driving, no route, no vehicle weapons. Exit on "arrival" is the 3 s timer for now; the stuck>5 s rule belongs to
the future driver, since a parked bot is not stuck.

Limits: no owned/claimed-vehicle preference beyond `CanEnterVehicle`; no seat animation; the scan is a 10 m
proximity box per decision and stops as soon as a car is entered. Not run in the engine here.

## Shore bake note (DS-052a)

Bots can swim. The US side leaves the submarine either by boat (several passengers per boat) or by swimming, so the
Shore navmesh bake must include the submarine exit, the water surface around it and the beach landing, not only the
island interior. `wb_ps_vehicles` is driver-seat entry only and does not drive a boat; boat driving stays
`BotBoatDriver.h` (`BOATS_SHORE.md`).
