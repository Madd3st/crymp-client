# Why bots look laggy on a client, and in-game demo playback (OC-025)

Author: OpenCode (read-only). Sources: `Madd3st/crymp-client` at `warsbots/feature/ia-participants` (`6718af16`),
read-only. No game, no server, no build.

---

## 1. Replication: server-controlled actors vs human players

### Who sends what

| | Human player | WarsBot (channel-less `CPlayer`) |
| --- | --- | --- |
| Input | client -> server: `INPUT_ASPECT`, `CPlayer::NetSerialize` `Player.cpp:5223-5242`; CryMP uses `SSerializedPlayerInput::Serialize_CryMP` when `mp_crymp == 1`, else `Serialize_Default` (`:5231-5234`) | none - there is no client, so no input aspect |
| Position | **client-authoritative**: the client moves itself and the position is synced to the server ("pos is synced from client, but also smoothed", `Player.cpp:3528`; "synced pos from client is interpolated/smoothed", `:3569`) | server physics, sent to clients in the physics snapshot |
| Pose/intent | implicit in the input stream | explicit optional group `WarsBotMotion` inside `eEA_GameServerDynamic`: direction, speed, aim, strafe, sprint, stance, airborne, jumpSerial, strengthJump (`Player.cpp:5183-5221`); written by `SetWarsBotMotion` (`:4958-4967`, `ChangedNetworkState` only on change `:4966`) |
| Suit / health / weapon | aspects | suit active flag (`:5183-5190`), `ASPECT_HEALTH` (`:5146-5152`), `ASPECT_CURRENT_ITEM` (`:5154-5179`) |

Smoothing knobs that exist for remote entities: `net_phys_pingsmooth` and `net_phys_lagsmooth`, both default `0.1`
(`Code/CryAction/NetworkCVars.cpp:22-23`) - they smooth the *physics* position of a remote entity, they do not
interpolate the bot's intent.

### What the client does with a bot

On read the replica sets `m_warsBotPose` and forces `EnablePrePhysicsUpdate(ePPU_Always)` (`Player.cpp:5214-5220`).
Every frame `UpdateWarsBotMotion(dt)` (`:4968-5015`) turns the replicated intent into a `CMovementRequest`:
move target = **current entity position + direction * 2 m** (`:5003`), desired speed (`:5005`), pseudo speed from three
buckets (`:5006`), stance (`:5007`), look/aim/fire targets (`:5008-5010`), body target / strafe (`:5011-5012`), then
`RequestMovement` (`:5013`). Rotation is rate-limited to 3 rad/s (`:4976`). Jumps are replayed from `jumpSerial` by
setting the animation-graph input `jumpMP` / `jumpMPStrength` (`:4997`), landing resets it to `idle` (`:5000`).
Aim for *other* players is smoothed by `m_netAimDirSmooth` with `netAimFactor = 4.0` when zoomed (`:1944-1968`).

So: **the animation is driven locally at frame rate, but from 10 Hz intents attached to stale positions.**

### Why that reads as "laggy" - five causes, in order of impact

1. **Intent granularity, no interpolation.** The brain decides at 10 Hz and `ChangedNetworkState` fires only when a
   value changes (`:4966`). Between two packets the client keeps the old direction; when the new one arrives the
   movement controller starts a 3 rad/s turn (`:4976`) -> a visible "step then swerve" every 100-200 ms.
2. **Position/intent mismatch.** The entity position is the last physics snapshot (no extrapolation for a channel-less
   actor, `net_phys_*` only smooths), while the move target is computed from that same stale position (`:5003`). Every
   snapshot the target jumps, the controller re-aims: this is the classic bot zig-zag at 20-30 Hz update rates. A human
   player never shows it because their own client predicts locally.
3. **Aim smoothing vs actual shots.** `m_netAimDirSmooth` (`:1944-1968`) lags the replicated aim by design (4x when
   zoomed), so a bot's tracers and muzzle flash appear behind its on-screen direction.
4. **Pseudo-speed buckets.** `SetPseudoSpeed` gets 0 / 0.3 / 0.5 / 1.0 (`:5006`), not the actual m/s -> walk/strafe
   animations play at the wrong rate, i.e. foot sliding that the eye reads as stutter.
5. **One-shot stance/airborne.** `stance` and `airborne` are values, not transitions; a crouch or a landing pops instead
   of blending, and `m_stats.jumped` is force-set from the replica flag (`:5001`).

### Cheap improvements (for Codex; research only)

* Add a monotonic `moveSerial` to the `WarsBotMotion` group and **interpolate the intent on the client** over the
  measured inter-packet interval (slerp direction, lerp speed) - removes cause 1 without touching the tick rate.
* Send actual `speed` (already sent) and derive pseudo speed from the locomotion table instead of buckets - cause 4.
* Extrapolate the replica position by `direction * speed` for up to one update interval between snapshots - cause 2.
* The existing 2 s `NETWORK MOTION` log (`:5247-5250`) is enough to measure intent age; the wbdemo client sidecar
  (OC-021) can record it per frame for Triage.

---

## 2. In-game demo playback (C-026) - one page

### Reuse

* **Actors without physics**: the channel-0 `CPlayer` puppet path already exists - `CreateActor(0, ...)`
  (`Code/CryGame/Bots/CryMPEngineAdapter.cpp:29-31`, tolerates `GetChannel(0) == nullptr`), plus the replica mode
  `m_warsBotPose` + `UpdateWarsBotMotion` (`Player.cpp:4968-5015`) which already converts "intent" into movement
  requests and animation-graph input. Playback = the same path with the intent coming from the demo file instead of
  the net aspect.
* **Camera**: the spectator code already follows entities - `eASM_Follow` + `SetSpectatorTarget` +
  `MoveToSpectatorTargetPosition` (`Player.cpp:366-369`), first-person spectating (`IsFpSpectatorTarget`, `:817`),
  and the mode is pushed to the client with `ClSetSpectatorMode` (`:272`). Free camera = the existing spectator free
  mode, with mouse yaw/pitch bound while the timeline is paused.
* **Data**: `wbdemo/2` (frozen in `Docs/Research/SteelmillTriage/DEMO_FORMAT.md`): ACTOR_STATE, INTENT, INV, BRAIN,
  events, header with navmesh hash and per-actor profile/skill/style.

### Design

1. `CDemoPlayer` singleton on the client: `Open/Play/Pause/Seek(ms)/Rate(x)`, ticked before actor updates; it owns the
   clock and a list of `CDemoActor` records (id -> entity).
2. On open: create puppets for all actors that exist at t=0 (channel 0, `m_warsBotPose`, `EnablePrePhysicsUpdate`),
   hide the local player (`eASM_Free`), set the navmesh hash check from the header.
3. Per frame: for every actor apply **position + yaw + pitch + stance + weapon + suit** directly (`SetWorldPos`,
   `m_baseQuat`, stance) and feed the movement controller only for *animation* (move target from the demo delta, pseudo
   speed from the recorded speed). Fire events -> `SetInput("Action", "fire")` + muzzle/tracer, no damage.
4. Seek: rebuild from the nearest keyframe (see gaps), then replay forward at 4x until the target time.
5. Camera modes: free, follow-3rd (spectator), follow-1st (uses recorded pitch - see gap 2).

### What is missing in `wbdemo/2` (must be added before playback)

| # | Gap | Why it blocks |
| --- | --- | --- |
| 1 | **keyframes every ~2 s** (full actor state) | without them, `Seek` = replay from the start; a 10 min demo is unusable |
| 2 | **pitch per actor per frame** (yaw only is not enough) | FP follow and tracer direction need the real pitch; INTENT has it only at decision ticks |
| 3 | **spawn / despawn / death records** with id + profile | a puppet cannot be created mid-demo or removed at death |
| 4 | **event -> frame index** (not only `t_ms`) | events run at 50 Hz, frames at 10-20 Hz; without a frame anchor the timeline drifts |
| 5 | **speed + gait per frame** | deriving speed from positions reproduces exactly the stutter described in §1 |
| 6 | **authoritative server positions at the interpolation rate** | otherwise playback inherits cause 2 above |

### Size of work (one developer, rough)

| Piece | Estimate |
| --- | --- |
| format additions (1-3, 5) + writer in `BotManager`/`GameRules` | 2-3 d |
| `CDemoPlayer` + seek + rate + camera binding | 3-5 d |
| puppet actor path with position authority (`m_warsBotPlayback` flag, no physics, no damage) | 2-3 d |
| timeline overlay / hotkeys / validation against `summary.json` | 1-2 d |
| **view-only version** (no seek, follow camera, no free camera) | 4-5 d |
| **full version** | ~2 weeks |

Risk: the puppet path reuses bot code that is gated by `WARSBOTS_EXPERIMENTAL_SPAWNER`; playback on a stock client
needs the same optional-group/version discipline as the bots (server and client must run the same experimental DLL).
