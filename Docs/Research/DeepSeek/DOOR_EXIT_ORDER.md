# DS-063: NK door exit blocker — late trigger, not an RMI bug

Base `532e4497`. Raw C140: `build/c140-Shore-545a5022/c140-545a5022-run.bots.log` and
`nk-first20.json`; DS-057 map boundary (`Door48`, entity 104).

## Cause

The door scan is starved by the capture goal. In `BotBrain::UpdateRoam` the order is:

* `:829` `psCapture==1 && ... && UpdatePSCapture(...)` -> `return`;
* `:830` `psDoors==1 && UpdatePSInteract(...)`.

From 0.22 s bot 1 has `PS CAPTURE target=64 goal=(1965,1387.8)` (`run.bots.log:92`) and every capture tick
returns before the door code, although the bot is already within 4.4 m of the door at 20 s. At 31.50 the capture
goal changes to `(2291,1537)` (`:1110`) and the door scan finally runs: `PS DOOR open entity=104 ... state=2`
(`:1109`). The game resets 0.03 s later (`:1116`); after the reset the capture goal is again `target=64` (`:1249`)
and no further door request appears for the rest of the run. This is a scheduling bug, not the old `OnUsed` RMI
error.

`Door:Open(user,1)` set `action` to 1, but the log only had the pre-action (`state=2`); no post-state and no
physical passage were recorded. DS-057's boundary is `Door48` at `(1842.8513,1255.3259,111.27002)`: bot 1's
20 s position `(1840.25,1251.77)` is still inside, and the 31.50 request at `(1840.04,1251.62)` is also inside.

## Patch

`patches/ds063_door_priority.patch`, base `532e4497`:

* run `UpdatePSInteract` before `UpdatePSCapture` in `UpdateRoam`; a door request returns false, so capture
  movement still runs in the same tick;
* log `pre`, `post` (`Door.action` after `Open`: 1=open, 2=closed), `dist` and actor/door positions. `post=1` is
  script state only, not proof of passage; the positions let a replay compute the crossing.
* only existing `wb_ps_doors 0` gates it; no coordinate/Door104 hardcode, no `OnUsed`, no close, no collision or
  teleport change.

Do not claim physical passage from `post=1`. The log now has enough to replay it; the old C140 log did not.

## Oracle / missing observation

Replay (C140 recipe, `wb_ps_doors 1`, `wb_move_trace 1`):

1. `PS DOOR open` appears by 20 s (not 31.5);
2. `pre=2`, `post=1`, `dist<=5`;
3. within 2 s the actor's MOVE TRACE position crosses the `Door48` plane (`y>1255.3259` at the door aperture);
4. zero Lua/RMI/channel errors.

If (1)-(2) pass but (3) does not, the remaining blocker is door animation/collision timing, not the trigger; the
missing observation is a physical passage sample in MOVE TRACE. If (1) still fails, capture still returns before
the scan or the door is outside the 5 m/heading gate. No engine-free unit test is proposed for a call-order change;
the raw replay above is the meaningful oracle.
