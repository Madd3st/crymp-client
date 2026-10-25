# Network: one or two pose sources? (DS-002 · review risk #1 · T-2)

Scope: `warsbots/feature/claude-sprint` @ `c648ea3b`, read-only. Claims cite open code only; the
decisive send/apply decision is in the closed CryNetwork DLL and is called out explicitly.

## Verdict

Two writers exist in open code. Whether the first actually reaches a channel-0 replica is decided in
CryNetwork (closed); I prove both code paths exist, show they would conflict, and give the one
measurement that settles it.

## Writer A — server-authoritative physics snapshot

- `CActor::NetSerialize` serializes `eEA_Physics` by writing the physics proxy snapshot:
  `Actor.cpp:2380-2447`, `pEPP->SerializeTyped(ser, type, pflags)` at `Actor.cpp:2446`.
- `CActor::Init` marks the aspect for remote update for every actor, bots included:
  `RequestRemoteUpdate(eEA_Physics | ...)` at `Actor.cpp:944` (also `1435/1440/1444`).
- A client-side physics smoothing path exists: `net_phys_pingsmooth` / `net_phys_lagsmooth` /
  `net_phys_debug` are registered in `Code/CryAction/NetworkCVars.cpp:22-24`; the code that consumes
  them lives in CryNetwork (not in this tree). So "clients apply/smooth remote physics" is real, but
  we cannot see the per-entity routing from here.

## Writer B — client local movement from intent

- `CPlayer::ShouldUsePhysicsMovement()` returns `true` exactly for `m_warsBotPose && GetChannelId()==0`
  (`Player.cpp:1339-1341`), and sets `adpm = eADPM_Never` for the same actor (`Player.cpp:836`).
- `UpdateWarsBotMotion()` runs on channel-0 replicas and issues `RequestMovement(...)` every frame:
  `Player.cpp:5029-5088`, the request at `Player.cpp:5076-5087`; it also rotates the entity
  (`m_baseQuat/m_viewQuat`, `Player.cpp:5050-5052`) and overwrites `m_stats.jumped` (`Player.cpp:5075`).
- Intent is produced server-side and marked dirty every decision tick: `SetWarsBotMotion`
  `Player.cpp:4974-4985`; the client receives it through the optional group `WarsBotMotion` in
  `CPlayer::NetSerialize` `Player.cpp:5270-5288` (fields `5277-5288`).

## Conflict if both reach the replica

The replica would, per frame: (1) receive a physics snapshot and set pose via the physics proxy, and
(2) independently request movement and re-derive pose/rotation from the replicated intent. There is no
reconciliation between the two in `UpdateWarsBotMotion`; only rotation is filtered later by
`BotReplicaSmooth.h`. Symptom would be jitter / rubber-banding, not just CPU (owner's earlier
"teleports" were attributed to CPU, so this has not been ruled out).

## What settles it (measurement, not more reading)

Add a read-path log in `CPlayer::NetSerialize` when `aspect==eEA_Physics && IsWarsBotActor()` and one
at the end of `UpdateWarsBotMotion`; on a client with `wb_move_trace 2` compare, per frame:
- `entity->GetWorldPos()` before/after the physics snapshot apply;
- `entity->GetWorldPos()` after `RequestMovement`.

If the first does not move the actor (no `eEA_Physics` for channel 0), Writer B is the sole owner and
the fix is to keep intent + add a low-rate authoritative pose. If it does, Writer B must stop issuing
`RequestMovement` and animate only.

## ChangedNetworkState inventory for bots

All in `Code/CryGame/Actors/Player/Player.cpp`:
- `4966` `SetWarsBotActions` (sprint/move change) — event-driven.
- `4972` `PublishWarsBotJump` — per jump.
- `4985` `SetWarsBotMotion` — **every decision tick (10 Hz), even if values unchanged.**
- `5011` `SetWarsBotAim` — called from `BotMovement::Look`/`Aim`; per tick when turn cap/look run.
- `5058` `UpdateWarsBotMotion` — only when `airborne` flips.
- non-bot: `2394` (INPUT), `4162`, `4671` (health), `5141-5169` (current item), `6563` (health), `7972` (frozen).

## Bytes/bot/s estimate (to be confirmed by `wb_net_stats`)

`WarsBotMotion` payload = `dir0` (2 B) + speed (float? or quantized) + `wrld` aim (3 floats ~12 B or
quantized) + 6 bools + `stnc` (2 bits) + `ui8` + 2×`ui16` ≈ **25–35 B/packet**. At the current 10 Hz
dirty rate that is ~250–350 B/s per bot; 16 bots ≈ 4–5.6 kB/s per client, 32 ≈ 8–11 kB/s. This is
small next to the demo recorder (≈20 KB/s at 16 actors) and the physics scans; the real cost is CPU,
not bandwidth. The dominant fix is the per-tick dirty (`4985`) and the absolute `wrld` aim.

## Minimal change proposal (one owner of the pose)

1. Decide the owner from the measurement above; if both, disable `RequestMovement` on replicas and
   animate from `eEA_Physics` velocity + intent gait/jump.
2. `wb_net_stats` fields: `bytes/bot/s`, `updates/s`, `dirty_vs_sent`, `max|serverPos-clientPos|`,
   `max|yaw/pitch error|`, `eEA_Physics_delivered(bool)`.
3. Then T-3: dirty-check `SetWarsBotMotion`/`SetWarsBotAim` before `ChangedNetworkState`, and switch
   the aim field from world position to yaw/pitch (+ distance) + a `fire` bit.
