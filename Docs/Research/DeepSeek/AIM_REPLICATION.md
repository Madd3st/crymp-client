# Aim replication: why spectator 1st person looks late/crooked (DS-013)

Target build `5b90bfb6`. Path: server `BotBrain` → `BotMovement::Move/Look` → `CPlayer::SetWarsBotMotion/SetWarsBotAim`
→ optional `WarsBotMotion` group in `eEA_GameServerDynamic` → client `UpdateWarsBotMotion` → FP spectator
`PlayerView` / `Weapon`.

## What is serialized

- `SetWarsBotMotion` writes `m_warsBotAim` as an **absolute world point** and marks the aspect dirty
  (`Player.cpp:4974-4985`); the group sends `warsBotAim` (`'wrld'`), direction, speed, strafe, viewLimited, etc.
  (`Player.cpp:5298-5310`), at the brain rate (10 Hz).
- `SetWarsBotAim` (capped look, every frame when turn cap is on) writes the same `m_warsBotAim` and applies the view
  (`Player.cpp:5005-5011`); `UpdateTurnCap` calls it every frame via `BotMovement::Look` (`BotBrain.cpp:1165-1185`,
  `BotMovement.cpp:42-44`).
- On the next decision tick `BotMovement::Move` calls `SetWarsBotMotion` with the **raw intent aim** again
  (`BotMovement.cpp:119`/`SetWarsBotMotion`), overwriting the capped value. With `wb_bot_turn_cap=1` the network aim
  therefore alternates capped/raw at 10 Hz.

## Client / spectator

- `UpdateWarsBotMotion` smooths direction/aim with `BotReplicaSmooth` (`Player.cpp:5034-5043`; aimTau 50 ms,
  `BotReplicaSmooth.h:15-16`) and sets the body/view (`Player.cpp:5050-5052`). The body yaw is clamped to 3 rad/s
  (172°/s), independent of skill/turn cap.
- `WarsBotViewAim()` returns the smoothed `m_warsBotAim` only when `wb_bot_client_smooth=1` (`Player.cpp:5022-5027`).
- FP spectator: `UpdateFirstPersonSpectator` builds `m_netAimDir` from `WarsBotViewAim()` minus the **camera eye**
  (`Player.cpp:1931-1932`), then `Interpolate`s it into `m_netAimDirSmooth` with `mp_netAimLerpFactorCrymp` (default
  42/s, `GameCVars.cpp:721`; `Player.cpp:1970-1975`). `PlayerView.cpp:186-191` sets the spectator view from
  `m_netAimDirSmooth`.
- **Tracer/weapon uses raw `m_netAimDir`, not the smooth one:** `Weapon.cpp:3099-3104` (`GetNetAimDir()`).
  So during a turn the crosshair (camera) uses the smoothed direction and the tracer uses the raw direction.

## Most likely causes, in order

1. **Crosshair vs tracer divergence:** camera `m_netAimDirSmooth` vs tracer `m_netAimDir` (`Weapon.cpp:3103`); this is
   exactly “aim looks crooked” while the shot/tracer is ahead.
2. **Double smoothing + 10 Hz absolute point:** `BotReplicaSmooth` (50 ms) then `mp_netAimLerpFactorCrymp` (42/s), with
   `warsBotAim` refreshed only at 10 Hz; expected lag 80–150 ms, worse during flicks.
3. **Raw/capped overwrite:** `SetWarsBotMotion` overwrites `SetWarsBotAim` with the raw target at 10 Hz when turn cap is
   on; the replicated aim sawtooths, so the camera converges to two different points.
4. Absolute `'wrld'` aim + two pose sources (DS-002) add angular error at 50–80 m; 0.1 m at 50 m is 0.11°, not enough
   alone, but it stacks.
5. Body turn clamp 3 rad/s (`Player.cpp:5050`) matters for third-person/body, not the FP camera; do not “fix” it first.

## Minimal fix

1. Make camera and tracer use the same direction: `Weapon.cpp:3103` use `GetNetAimDirSmooth()` for FP spectator, or
   use raw `m_netAimDir` for both while firing (`mp_netAimLerp` already has a skipSmooth path for `mp_crymp`).
2. Remove one smoothing layer: either let `mp_netAimLerpFactorCrymp` be the only aim filter (do not also run
   `BotReplicaSmooth` for aim), or send yaw/pitch at 20 Hz and keep only `BotReplicaSmooth`.
3. In `SetWarsBotMotion`, do not overwrite `m_warsBotAim` when `m_warsBotViewLimited`; keep the capped `SetWarsBotAim`
   value as the single writer for the view/aim channel.
4. Send `fire`/`zoom` bits and use the same dir for the muzzle; add a client-side debug line comparing
   `m_netAimDir`, `m_netAimDirSmooth`, camera yaw and tracer yaw once per second.

## C-083 numbers that confirm

- Demo/client sidecar: `BRAIN.aim` vs `FRAME` yaw/pitch for the bot, median/p95 during turns; >2× server aim error
  (~1°) and a sawtooth/bimodal distribution confirms raw/capped or smoothing.
- At each `EVENT shot`, angle between camera view (`FRAME`/sidecar) and tracer/shot direction; a persistent 1–5° offset
  confirms the camera/tracer mismatch, not a server aim error.
- Toggle `wb_bot_client_smooth` 0/1 and `mp_netAimLerpFactorCrymp` 42/∞: if lag drops with smoothing off (steps grow),
  double smoothing is confirmed; if it remains, the 10 Hz raw/capped overwrite is the cause.
