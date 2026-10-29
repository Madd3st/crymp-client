# DS112 · C154 V2 native ladder attaches but never climbs

Status: diagnosis + proposed minimal native-path fix + actual-caller RED/GREEN fixture. No engine
patch was applied; Codex owns integration. No game/build/QA/data writes.

## Archive facts (exact 3cfd, V5, CI40855444355)

`QA/build/c154-route-3cfd8596-v2`:

- `session.json`: `phase=archived_restored`, `client_pid=null`, server ended; `runtime-result.json` FAIL.
- `C154-3cfd8596-v2.bots.log:96`: `#2 LADDER start a=(2116.12,1880.62,90.16) b=(2117.50,1880.60,99.76)`.
- `:111`: `phase=2 grab=1 ... nativeOn=1 exiting=0 grounded=1 pos=(2115.54,1879.85,90.12)`.
- `:112`: `phase=3 ... climb=1 nativeOn=1 exiting=0 grounded=1 pos=(2116.09,1880.65,90.25)`.
- `:113` onward: repeated `MOVE TRACE ... pos=(2116.091,1880.648,90.250) vel=(0,0,0) ... grounded=1 ... flying=1`; `MOTION INTENT requestedBase=1.30 actual=0.00 ... blocked=0`.
- `:487`: `27.03 ... LADDER failed reason=timeout nativeOn=1 exiting=0 grounded=1 pos=(2116.09,1880.65,90.25)`.
- `runtime-result` has 107 trace rows, `lower_grounded` t=1.06, `ladder_start` 92, `native_attached` 107, no `ladder_done`/`strength_done`/water.
- V1 is a separate preserved INVALID_DATA_PACKAGING archive (0-byte demo); V2 is the route failure, physical attempt 2. Current explicit MSIX data namespace Memory4/Nav16 restored exact; no client.

## Actual source path

1. `BotMovement::Move` (`Code/CryGame/Bots/BotMovement.cpp:87-145`) zeroes `direction.z`, sets a 2 m move
   target and desired speed, then `RequestMovement`. This is the ordinary horizontal target path; on a ladder the
   native code consumes the **local desiredVelocity.y** as the climb input, not world z.
2. `CPlayerMovementController::Update` (`PlayerMovementController.cpp:720-722`) returns early with default frame
   params if `GetCharacter(0)` is null. Otherwise `:801-862` projects `moveTarget - playerPos` (z zeroed) through
   `desiredSpeed/stanceSpeed` into local `params.desiredVelocity`; no ladder-specific zero gate was found.
3. `CPlayerMovement::Process` (`PlayerMovement.cpp:89-90`) dispatches to `ProcessMovementOnLadder` while
   `isOnLadder`; `:1699` computes native `move` from `m_movement.desiredVelocity.y`, and `:1732-1744` reaches:
   `if (!player.UpdateLadderAnimation(eLS_Climb,...)) return;` **before** setting
   `m_request.type=eCMT_Fly; m_request.velocity=move;`.
4. `CPlayer::UpdateLadderAnimation` (`Player.cpp:7444-7484`): for `eLS_Climb` it requires
   `GetEntity()->GetCharacter(0)` and a FIFO animation whose flags contain `CA_MANUAL_UPDATE`; otherwise it returns
   false. A headless/dedicated server can satisfy neither, so the actual climb velocity is discarded.
5. `CPlayerMovement::Commit` (`PlayerMovement.cpp:150-154`) then records `m_lastRequestedVelocity`; the observed
   `BODY LIVING requested=(0,0,0)` and `MOVE TRACE vel=(0,0,0)` are consistent with the early return.

## Confirmed vs unproven

**Confirmed from source + actual caller fixture:** the animation gate returns before `eCMT_Fly`; with no FIFO the
real block produces no native request, even with `desiredVelocity.y=1.0`.

**Not yet runtime-proven:** whether the runtime animation FIFO was absent (no animation telemetry), whether
`m_movement.desiredVelocity.y` was positive at the stall (not logged; `MOTION INTENT requestedBase` is only bot
intent), whether `m_actions` contained `ACTION_MOVE`, whether `m_linkStats.CanMove()` was true, and whether a
`eCMT_Fly` request would pass the physics/`AlwaysPhysics` path. The proposed input fallback below covers the
headless-no-character/zero-input case defensively; it is not proof that this was the runtime input value.

## Candidate 272 (accepted) and the input-fallback decision

Candidate `272a430604fd67d01362e771faacbeff23157a56` implements only the animation-gate bypass for the owned gated
bot; it deliberately does **not** add a speculative zero-input/`ACTION_MOVE` fallback. That is the right scope:
V2's same actor physically walked ~5 m before the attach, the character was present then, and `GrabOnLadder`
does not delete the body character; zero input was not logged. A fallback could hide a bad controller projection or
stale intent. Zero and down intent are preserved by the candidate's controls.

```cpp
const bool animationReady = player.UpdateLadderAnimation(CPlayer::eLS_Climb, eLDir, animTime);
bool authoritativeClimb = false;
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
authoritativeClimb = WarsBotLadderServer(player) && player.GetHealth() > 0 &&
                     !player.IsFrozen() && !m_stats.isRagDoll;
#endif
if (!animationReady && !authoritativeClimb)
    return; // humans/clients/flag-OFF/non-owned unchanged
m_request.type = eCMT_Fly;
m_request.velocity = move;
```

- Scoped by the existing `WarsBotLadderServer` (owned server, `wb_bot_ladder==1`); human/client/non-owner retain
  the exact old animation gate.
- No `SetPos`, `SetWorldTM` bot logic, impulse, direct `eLAT_ExitTop`, timer completion or manifest bypass; native
  `AdjustPlayerPositionOnLadder`, top ray, axis/speed and strict exit handling remain.
- `WB_LOG(2)` records native input/computed velocity/animationReady and exit graph/output/motionFlags/pos for the
  next exact run; no new flag.
- If the new run still has zero `desiredVelocity.y`, that is a separate input failure. Do not add a fallback or
  claim the animation gate is the sole cause without a reachable actual caller state and a bounded intent guard.

## Actual-caller fixture

`Docs/Research/DeepSeek/ds112_ladder_climb_fixture.py` extracts from exact 3cfd:

- the real `ProcessMovementOnLadder` climb block `move *= ...` through `m_request.velocity = move;`;
- the real `CPlayer::UpdateLadderAnimation` body;
- the real `WarsBotLadderServer` helper.

It compiles an engine-free mock caller and tests:

| case | expected |
| --- | --- |
| old, no character/FIFO manual animation | RED: no `eCMT_Fly` |
| old, FIFO with `CA_MANUAL_UPDATE` | GREEN control: native request set |
| proposed, owned, no FIFO, desired input 1.0 | GREEN |
| proposed, owned, no FIFO, desired input 0 + `ACTION_MOVE` | hypothetical fallback only; **not in 272** |
| proposed, non-owned, no FIFO | RED (old gate preserved) |

Local MSVC run: `DS112 actual climb fixture: old RED, manual-FIFO control, proposed GREEN, non-owned preserved`
(exit 0). The fixture uses a no-op `AdjustPlayerPositionOnLadder` and does not execute the controller projection or
physics; it is a gate/request proof, not a whole-function or runtime proof. Compile failure is not counted as RED.

## Limits

- No engine patch applied; Codex owns the new local SHA/CI.
- The fixture does not prove the runtime FIFO was absent or that `desiredVelocity.y` was positive; the next exact
  run needs the existing verbose log at the branch.
- No client visual/full Shore/full capture claim. V1 remains invalid; V2 remains a route FAIL; no retry was run.
