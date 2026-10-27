# Doors: open-only state, cooldown, path awareness (DS-053 #1)

Base `96f7f624`. Flag `wb_ps_doors` (default 0). Owner rule DS-052a: every door opens only with **use (F)**; a bot must
open doors, never close one.

## Door state in stock `Door.lua`

File: `GameData.pak:Scripts/Entities/Doors/Door.lua` (extracted 27.10).

- `DOOR_TOGGLE=0`, `DOOR_OPEN=1`, `DOOR_CLOSE=2` — `:84-86`.
- The state is `self.action`, initialised by `Door:ResetAction()` to `DOOR_CLOSE` (`:191-192`), called from `OnReset`
  (`:216`).
- `Door:OnUsed(user)` (`:342`) calls `self.server:SvRequestOpen(user.id, self.action~=DOOR_OPEN)` (`:356`): true = open
  request, false = close request. The usable distance is `Door:IsUsable` (`:315-334`) with `Properties.fUseDistance`
  (Shore doors: 5 m).
- `Door.Server:SvRequestOpen(userId, open)` (`:410-420`) calls `Door:Open(user, mode)` (`:524`), which assigns
  `self.action = DOOR_OPEN|DOOR_CLOSE` (`:525-535`) before the rotation/slide starts; the client mirrors it in
  `ClRotate`/`ClSlide` (`:426`, `:438`).

So `action == DOOR_OPEN (1)` is the reliable server-side "already open, do not toggle" test.

## Patch (`ds053.patch`)

`BotBrain::UpdatePSInteract` (`Code/CryGame/Bots/BotBrain.cpp:575-597`):

- +-5 m proximity box, class must be `Door` (`:577`, `:581`);
- read `action` from the entity script table and skip if `1` (`:583`);
- per-door cooldown `m_doorUsed[entityId] = now + 2 s` (`:584`, `:595`), map cleared on respawn (`:304`); global scan
  throttle 0.5 s;
- open only: calls the stock `Door:OnUsed(bot)` (`:594`); never `Event_Close`, never `Open(..., DOOR_CLOSE)`;
- path-aware: when the Detour path is active, the door must lie within 3 m of the **current path segment** (bot ->
  `m_path[m_pathIndex]`) and at least 10% along it (`:587-591`); without a mesh path it falls back to the facing cone
  `dot >= 0.2` (`:592`);
- log: `PS DOOR open entity=... actor=... state=...` (`:596`).

`OnUsed` -> `SvRequestOpen(true)` -> `Open` stays the only write path: no direct state write, no client RMI.

## Limits

- No closing at all (owner rule); a bot never closes a door behind it.
- Path awareness uses the current segment only; a door just past a corner is taken on the next decision, when the
  segment updates.
- If the script table has no `action` (`-1`), `OnUsed` still opens a closed door; harmless.
- Not executed in the engine here (no open/close playtest).
