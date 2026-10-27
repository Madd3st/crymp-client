# PS doors (DS-052 #3, updated by DS-053 #1)

Shore (`ps-Shore.xml`): **31 `Door`**, 17 on `Base_NK`; all have `fUseDistance=5`, `bLocked=0`, `bActivatePortal=0`.
No `AutomaticDoor1Piece`/`AnimDoor` entity is placed on Shore, so **none open by themselves**; every door opens only
with a use (F), as the owner rule (DS-052a) requires. The flow graph can also call `Event_Open`/`Event_Close`.

Stock path (`GameData.pak:Scripts/Entities/Doors/Door.lua`):
- state: `self.action`, `DOOR_OPEN=1`/`DOOR_CLOSE=2` (`:84-86`), reset to close in `Door:ResetAction` (`:191-192`);
- player use -> `Door:OnUsed(user)` (`:342`) -> `self.server:SvRequestOpen(user.id, self.action~=DOOR_OPEN)` (`:356`);
- `Door.Server:SvRequestOpen(userId, open)` (`:410`) -> `Door:Open(user, mode)` (`:524`, comment `:523`: safe only from
  the server); use distance `Door:IsUsable` (`:315-334`) with `Properties.fUseDistance` (5 m here).

Patch (`wb_ps_doors 0`, default OFF): `BotBrain::UpdatePSInteract` finds a `Door` within 5 m, **skips it if
`action == DOOR_OPEN`**, and calls the stock server handler `Door:Open(user, DOOR_OPEN)` directly (`Door.lua:524`,
comment `:523` "only safe to be called from server"; `allClients` broadcast to clients `:565/:572`). `Door:OnUsed` is the
client/RMI path (`:356`) and fails on a server bot, so it is not used (DS-054). Thus a bot opens a closed door and never
closes one; no per-client RMI and no direct state write. Per-door cooldown 2 s (`m_doorUsed`). The door must lie on the
current path segment (within 3 m of the bot -> next corner, at least 10% along it); without a navmesh path the facing
cone `dot >= 0.2` is the fallback. Log line: `PS DOOR open entity=... state=...`.

Locked doors are not filtered because stock `OnUsed` has the lock check commented out (`:344-354`); Shore doors are
unlocked anyway.

Limits: no closing (owner rule), no pathing through the doorway or door-aware stuck recovery yet. See
`DS053_DOORS.md` (state/cooldown/path) and `DS054_DOORS_RMI.md` (why `OnUsed` is replaced by `Open`).
