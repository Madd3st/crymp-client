# Doors: call the stock server handler, not `OnUsed` (DS-054, fixes DS-053)

Base `ba4d5f4e`. C-138 X-190: with `wb_ps_doors 1` the DS-053 hunk produced two Lua/RMI errors at
`Door.lua:356` (`self.server:SvRequestOpen`): *"RMI via client ... not a client"*.

## Why `OnUsed` is wrong from C++

Stock `GameData.pak:Scripts/Entities/Doors/Door.lua`:

- `Door:OnUsed(user)` (`:342-357`) is the **client use path**: it ends with
  `self.server:SvRequestOpen(user.id, self.action~=DOOR_OPEN)` (`:356`). `self.server` is the client -> server RMI
  proxy; a bot is a server actor with `IsPlayer=false` and no channel, so the RMI dispatcher rejects the call.
- The server side is `Door.Server:SvRequestOpen(userId, open)` (`:410-420`): it maps `open` to `DOOR_OPEN`/`DOOR_CLOSE`
  (`:412-416`), resolves `System.GetEntity(userId)` and calls `self:Open(user, mode)` (`:418-419`).
- `Door:Open(user, mode)` (`:524`) carries the comment **"this is only safe to be called from server"** (`:523`). It
  sets `self.action` (`:525-535`), rotates/slides the door (`:541-573`), broadcasts to the clients through
  `self.allClients:ClRotate(...)` / `ClSlide(...)` (`:565`, `:572`), updates the AI smart-object state and fires
  `Open`/`Close` (`:575-582`), returns 1 (`:585`).

`Door:Event_Open(sender)` (`:588-592`) is the flow-graph path to `self:Open(sender, DOOR_OPEN)`; it shows the intended
server-side entry into `Open`.

## Patch (`ds054_doors.patch`)

`BotBrain::UpdatePSInteract` now calls the server handler directly:

```
Script::CallMethod(dt, "Open", actor.GetEntity()->GetScriptTable(), 1);   // mode 1 == DOOR_OPEN
```

with `dt = door->GetScriptTable()`, `self = door table` (the method is a normal `Door:` function) and the actor script
table as `user` (for `bRelativeToUser`, `:546-558`). No `OnUsed`, no `self.server` RMI; `Open` itself does the
`allClients` broadcast. `SvRequestOpen` was not used because reaching the class `Server` subtable from the entity table
needs a global-table lookup and a pushed `self` for no behavioural gain: `Open` is exactly the function it calls.

Kept from DS-053:

- state check `action == DOOR_OPEN (1)` -> skip (`Door.lua:84-86`, `:191-192`);
- 2 s per-door cooldown (`m_doorUsed`, cleared on respawn) and 0.5 s scan throttle;
- open only: `Open` with `DOOR_OPEN`; never `DOOR_CLOSE`, `Event_Close` or a close write;
- path-aware: door origin within 3 m of the current segment (bot -> `m_path[m_pathIndex]`), `t > 0.1`; facing
  `dot >= 0.2` only as the no-mesh fallback;
- log `PS DOOR open entity=... actor=... state=...`.

## Verified

- `ds054_doors.patch` applies clean on `ba4d5f4e`; BotBrain/BotConfig compile with MSVC (experimental spawner) in a
  clean patched worktree.
- 43 engine-free suites pass in the author tree (2 engine suites skipped); Lua not executed, no door playtest.
