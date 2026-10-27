# PS doors (DS-052 #3)

Shore (`ps-Shore.xml`): **31 `Door`**, 17 on `Base_NK`; all have `fUseDistance=5`, `bLocked=0`, `bActivatePortal=0`.
No `AutomaticDoor1Piece`/`AnimDoor` entity is placed on Shore, so **none open by themselves**; they need a use
(the flow graph can also call `Event_Open`/`Event_Close`).

Stock path (`GameData.pak:Scripts/Entities/Doors/Door.lua`):
- player use button -> `Door:OnUsed(user)` (`:342`) -> `self.server:SvRequestOpen(user.id, self.action~=DOOR_OPEN)`
  (`:356`);
- `Door.Server:SvRequestOpen(userId, open)` (`:410`) -> `Door:Open(user, mode)` (`:524`, comment `:523`: safe only
  from the server); the use distance is `Door:IsUsable` (`:315-334`) with `Properties.fUseDistance` (5 m here).

Patch (`wb_ps_doors 0`, default OFF): `BotBrain::UpdatePSInteract` finds a `Door` within 5 m in front of a walking
PS bot and calls the stock `OnUsed(actor)` on the door script table (`Script::CallMethod`). `OnUsed` then takes the
normal `SvRequestOpen` -> `Open` server path; no per-client RMI, no direct door state write. One use per second per
bot. Locked doors are not filtered because stock `OnUsed` has the lock check commented out (`:344-354`); the map's
doors are unlocked anyway.

Limits: no pathing through the doorway or door-aware stuck recovery yet; the bot opens a door it is already facing.
