# Vehicles in Crysis Wars for a channel-0 bot

Date: 2026-10-26. Read-only spec, no code. This covers: how a player enters/exits a vehicle in CryEngine 2/Crysis Wars,
what the server has to do for a WarsBot (`channelId=0`, `IsPlayer=false`), which vehicles exist on Shore/Plantation,
a `wb_ps_vehicles` switch, and the minimum driving contract. `IVehicleSystem.h` is the engine interface;
`VehicleBase.lua` is the extracted game script (local only, not committed).

## 1. Two entry layers

The engine exposes a seat-level entry and the game exposes a use/script path.

1. **Seat level (server-safe, no channel):** `IVehicleSeat::Enter(EntityId actorId, bool transition)` and
   `IVehicleSeat::Exit(bool transition, bool force)` (`Code/CryCommon/CryAction/IVehicleSystem.h:1167-1168`).
   `IVehicle::GetSeatById(TVehicleSeatId)` (`:645`), `GetSeatForPassenger` (`:651`), `GetDriver` (`:797`) and
   `GetExitPositionForActor` (`:809`) are on `IVehicle` (`:567-817`).
2. **Game/script level:** `VehicleBase:EnterVehicle(passengerId, seatId, animation)` calls
   `self.vehicle:EnterVehicle(...)` (`VehicleBase.lua:627-629`); `LeaveVehicle` calls `self.vehicle:ExitVehicle`
   (`:633-636`). `VehicleBase:CanEnter` calls `g_gameRules:CanEnterVehicle` (`:360-362`); PS implements it at
   `ZPatch1.pak.Scripts.GameRules.PowerStruggle.lua:571-584` (owner, same team, or owner nil).

The **proven channel-0 path is the native revive-in-vehicle path**: `CGameRules::RevivePlayerInVehicle`
(`GameRules.cpp:1055-1106`) calls `NetReviveInVehicle` (`Actor.cpp:3730-3771`), which after `Revive` gets
`IVehicleSystem::GetVehicle(vehicleId)`, `GetSeatById(seatId)` and calls `pSeat->Enter(GetEntityId(), false)`
(`Actor.cpp:3763-3770`). This is the pattern to reuse for a normal bot entry: no fake net channel, no `IsPlayer=true`.

## 2. Bot entry sequence (server)

```
IVehicleSystem* vs = g_pGame->GetIGameFramework()->GetIVehicleSystem();
IVehicle* v = vs->GetVehicle(vehicleEntityId);
if (!v) fail;
IVehicleSeat* seat = v->GetSeatById(seatId);      // driver usually 1
if (!seat) seat = v->GetSeatForPassenger(botId);  // already inside
if (seat && !seat->GetPassenger()) {
    if (mode->CanEnterVehicle(v, botId))         // PS owner/team rule
        seat->Enter(botId, false);               // transition off = server snap
}
```

`seat->Enter` is what `Actor.cpp:3769` already does for a revived actor; it links the actor and initialises
`passenger.vehicleId`/`vehicleSeatId` in the script (`VehicleBase.lua:886-975`). For PS, do not bypass
`CanEnterVehicle`: the native seat call does not know about team/owner/claim rules.

Entry must happen **after** `ApplyTeam` (T0): `CanEnterVehicle` reads `GetTeam(userId)` and `GetTeam(vehicle.id)`
(`PowerStruggle.lua:556-562`). A bot with team 0 is treated as neutral.

For a newly purchased vehicle, the PS claim window is 90 s and the owner is stored on the vehicle
(`PS_RULES.md:77-79`, `PowerStruggle.lua:350-370`, `PowerStruggleBuying.lua:1097-1125`). The bot should only enter an
unowned vehicle or
one whose owner is the bot; the PS `CanEnterVehicle` check is the authority.

### Animation / transition

Use `isTransitionEnabled=false` for the first server implementation: no animation graph edge cases, instant seat
attachment. If the vehicle is moving, use `true` only after the native `Enter` accepts; no fake channel input.
Do not call `SetWarsBotMotion` on the same actor while it is linked; `CPlayer`/vehicle movement owns it
(`Player.cpp:1787` guards vehicle state).

## 3. Exit

```
if (IVehicle* v = player->GetLinkedVehicle()) {
    if (IVehicleSeat* seat = v->GetSeatForPassenger(botId))
        seat->Exit(false, false);      // or vehicle->OnAction(eVAI_Exit, eAAM_OnPress, 1, botId)
}
```

`IVehicle::GetExitPositionForActor` (`IVehicleSystem.h:809`) is the safe fallback when the seat helper is blocked;
`VehicleBase:GetExitPos` implements the script side (`VehicleBase.lua:253-271`). Verify after exit:
`GetLinkedVehicle()==nullptr`, health/position valid, no `vehicleSeatId` left in the script (`VehicleBase.lua:998-999`).
On death/destroy, `GameRules` already resets the seat (`GameRules.cpp:659-663,1005-1008`); the bot must stop its
vehicle controller and clear the vehicle goal.

## 4. Driving a vehicle without a channel

`IVehicle::OnAction(TVehicleActionId, activationMode, value, callerId)` is the server-side input surface
(`IVehicleSystem.h:603`); action ids are `eVAI_MoveForward`, `eVAI_MoveBack`, `eVAI_TurnLeft`, `eVAI_TurnRight`,
`eVAI_Brake`, `eVAI_RotateYaw`, `eVAI_Exit` (`:70-100`). `PlayerInput` reaches it through
`m_pVehicleClient->OnAction` (`PlayerInput.cpp:335`) for a real client; a bot has no client, so the
`BotVehicleController` must call `IVehicle::OnAction` directly with the bot entity id.

Minimum driving contract:

1. `IVehicleMovement* m = v->GetMovement()` (`IVehicleSystem.h:686`); `m->StartEngine(botId)` (`:959`) if the vehicle
   needs an explicit start; `m->ResetInput()` (`:953`) before each command batch and on exit.
2. At the 10 Hz brain tick: compare the vehicle heading/position with the next road/navmesh waypoint; emit
   `MoveForward`/`MoveBack` with a speed value and `TurnLeft`/`TurnRight` with a normalized value. Release stale
   actions with `eAAM_OnRelease`/zero, not by stopping the tick.
3. `Brake` when the distance to the waypoint is below ~10 m, when an obstacle/other vehicle is within the braking
   radius, or when the next turn is > 60 deg; reverse for 1–2 s if blocked, then replan.
4. Do not use `SetWarsBotMotion`/`BotLocomotion` while linked. The vehicle physics owns position; the brain only
   chooses waypoints and actions. Stop the vehicle on exit.
5. For air/boat vehicles, the same actions apply; do not attempt a land navmesh path over water. The first PS v1
   should restrict driving to land vehicles (`US_ltv`, `Asian_truck`, `US_apc`, `Asian_apc`, tanks) and disable
   helicopters/VTOL/boats behind the switch.

`IVehicleMovement` is an `IMovementController`; do not mix its movement request with `CMovementRequest` from
`BotMovement` while the actor is linked. One owner of the pose, as in T-1.

## 5. Vehicle classes on Shore / Plantation

Placed entities in the extracted `ps_shore.json` / `ps_plantation.json` (DS-004) and the PS buy list
(`PowerStruggleBuying.lua:74-99`):

| Class | Shore | Plantation | PS buy id(s) | Role |
| --- | ---: | ---: | --- | --- |
| `US_ltv` | 0 placed (factory) | 0 | `light4wd`, `us4wd`, `usgauss4wd`, `usmoac4wd`, `usmoar4wd` | fast 4WD, driver + gunner |
| `Asian_truck` | 0 placed (factory) | 0 | `nktruck`, `ussupplytruck` | truck / mobile spawn |
| `US_smallboat` | 4 | 0 | `usboat` | small boat |
| `Asian_patrolboat` | 0 | 0 | `nkboat`, `nkgaussboat` | patrol boat |
| `US_hovercraft` | 4 | 0 | `ushovercraft` | hovercraft |
| `Asian_aaa` | 0 | 0 | `nkaaa` | AA vehicle |
| `US_apc` | 0 | 0 | `usapc` | APC |
| `Asian_apc` | 0 | 0 | `nkapc` | APC |
| `US_tank` | 0 | 0 | `ustank`, `usgausstank`, `ussingtank`, `ustactank` | tank / TAC |
| `Asian_tank` | 0 | 0 | `nktank` | light tank |
| `Asian_helicopter` | 0 | 0 | `nkhelicopter` | helicopter |
| `US_vtol` | 0 | 0 | `usvtol` | VTOL |
| `Asian_ltv` | 4 placed | 6 placed | — | map-placed light vehicle |
| `Civ_car1` | 19 placed | 2 placed | — | civilian car, no weapons |
| `Civ_speedboat` | 3 placed | 0 | — | civilian boat |

Shore factories: `Small_Vehicle_Factory_NK` builds `light4wd/us4wd/usgauss4wd/nktruck/ussupplytruck`;
`Naval_Factory_NK` builds `usboat/nkboat/nkgaussboat/ushovercraft/nkapc`; neutral `Vehicle_Factory_Neutral` builds
the APC/tank/AA list; neutral `Prototype-Factory6` builds the TAC/MOAC vehicles (`PS_MAPS.md:32-38`). Plantation has
only `Prototype-Factory-small1` with no vehicle list (`PS_MAPS.md:60-62`), so land vehicles there are map-placed
`Asian_ltv`/`Civ_car1` only; the first PS test should use Shore for a bought vehicle.

The `id → class` mapping is in `PowerStruggleBuying.lua:76-99`; the vehicle implementation XML is loaded by
`VehicleSystem.LoadXML` (`VehicleSystem.lua:48-53`) from `Scripts/Entities/Vehicles/Implementations/Xml/<class>.xml`.
Do not infer a class from the buy id in code.

## 6. Replication / authority

- The **server** owns the seat/passenger relationship. `VehicleBase:OnActorSitDown/OnActorStandUp` updates
  `passenger.vehicleId`/`vehicleSeatId` and broadcasts the vehicle event (`VehicleBase.lua:886-1035`). The bot remains
  `channelId=0`; clients see it as a passenger through the vehicle entity, not through a fake channel.
- `CPlayer::GetLinkedVehicle()` is the client/server view of the link (`Actor.cpp:208-218`); `CPlayer` view/weapon
  code already branches on it (`Player.cpp:1938,2170-2210,2299`).
- Do not call `IVehicleSystem::CreateVehicle(channelId,...)` for a bot; use the existing vehicle entity or the
  factory's built vehicle. The `channelId` argument is for network ownership, not for a bot's fake channel.
- When a passenger leaves/dies, `GameRules` clears the seat (`GameRules.cpp:659-663,1005-1008`); the bot must stop its
  controller and wait for the vehicle entity event before re-entering.

## 7. What can go wrong

1. **Entry before team:** `CanEnterVehicle` returns false for a team-0 bot or an enemy vehicle
   (`PowerStruggle.lua:556-562`). T-0 `ApplyTeam` must precede entry.
2. **Client-only OnUsed:** `VehicleBase:OnUsed` returns immediately when not a client (`VehicleBase.lua:309-312`);
   a server bot cannot rely on the client use key. Use the seat/API path above.
3. **Transition/camera:** `Enter(...,true)` can start an animation that expects a client view; start with `false`.
4. **Exit blocked:** `GetExitPositionForActor` can fail at a wall/water/cliff. Try the seat helper, then the vehicle
   hull, then a safe teleport only after a timeout; never leave the actor linked to a destroyed vehicle.
5. **Pose fight:** `BotMovement`/`SetWarsBotMotion` and vehicle physics both writing the same actor; T-1 says one
   owner. Disable normal locomotion while linked.
6. **Air/boat path:** a land navmesh path will drive into water. Restrict vehicles or use a road/water graph.
7. **Owner/claim:** entering another player's claimed vehicle is refused; a PS bot should use its own purchase or an
   unowned neutral vehicle.
8. **No-vehicle mode:** `wb_ps_vehicles=0` must reject `Enter`, clear vehicle goals and never buy vehicles; test it on
   the same map with the same bots.

## 8. Acceptance tests / probes

- `ps_vehicle_seat`: spawn one bot, place a free `US_ltv` nearby, `Enter(seat 1,false)`; assert
  `GetLinkedVehicle()!=null`, `GetSeatForPassenger(botId)->IsDriver()`, bot remains channel 0, no crash.
- `ps_vehicle_passenger`: seat 2; assert bot is not driver, weapon seat is usable, exit restores position.
- `ps_vehicle_exit`: drive 20 m, `Exit(false,false)`; assert `GetLinkedVehicle()==null`, exit position is on the
  terrain and > 1 m from the hull, seat is free.
- `ps_vehicle_owner`: enemy bot cannot enter a claimed vehicle; team-0 bot cannot enter a team vehicle.
- `ps_vehicle_off`: `wb_ps_vehicles=0`; no vehicle buy/enter action is emitted in a 60 s PS run.
- `ps_vehicle_drive`: road waypoint 50 m; reach within 5 m without rollover or water entry; stop at the end.
- `ps_vehicle_destroyed`: destroy the occupied vehicle; bot is unlinked, not stuck, and does not re-enter a wreck.

## 9. Out of scope for the entry spec

Helicopter/VTOL flight, boat navigation, vehicle combat, passenger weapon aiming and squad vehicle orders are later.
The first implementation is land driver/passenger entry/exit plus a straight/road waypoint drive; helicopters and boats
stay behind `wb_ps_vehicles=0` until the road/water graph exists.
