# PS event telemetry (DS-052 #2)

One `[WarsBots] PS EVENT` line per stock PS event, no new flag (diagnostic; level 0, goes to the `.bots.log`).

| Event | Hook (stock file:line) | Line fields |
| --- | --- | --- |
| `capture_start` | `PowerStruggleAlert.lua:638` `Server:OnStartCapture` | entity, class, team |
| `capture_step` | `:648` `Server:OnStepCapture` | entity, class, team, remaining |
| `captured` | `:673` `Server:OnCapture` | entity, class, team |
| `uncaptured` | `:700` `Server:OnUncapture` | entity, class, team, oldTeam |
| `purchase` | `PowerStruggleBuying.lua:1026` after `OnItemBought` | player, item, price, alive |
| `vehicle_enter` | `PowerStruggle.lua:585` `OnEnterVehicleSeat` | vehicle, seat, player |
| `vehicle_exit` | `:619` `OnLeaveVehicleSeat` | vehicle, seat, player, exiting |
| `hq_hit` | `PowerStruggleAlert.lua:798` `Server:OnHQHit` | entity, team, damage, shooter |

Capture start/step/captured/uncaptured are the stock alert callbacks (the same ones the HUD uses); purchases are the
stock `BuyItem` success point; HQ damage is the stock server hit callback; vehicle enter/exit are the PS seat
callbacks. No per-client RMI and no HP/score writes.

Limits: `capture_step` can be frequent (once per step); filter by `entity`/`team` in the report. `hq_hit` is the
stock 5 s-throttled alert path (`PowerStruggleAlert.lua:798`), not every bullet. Not run in the engine here.
