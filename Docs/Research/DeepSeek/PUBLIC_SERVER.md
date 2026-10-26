# Public WarsBots server with bots for vanilla CryMP clients (DS-029)

Date: 2026-10-26. Read-only analysis at `claude-sprint` head `2ffc95fa` + `CryCommon/CryAction` headers.
Goal: an ordinary CryMP client joins a WarsBots server and sees bots as normal players (aim + animation,
not sliding statues).

**Verdict.** The idea (server writes the standard `INPUT_ASPECT` for a channel-0 bot) is right in principle but
**not sufficient as-is**. There are two independent blockers:

1. the server has **no `IPlayerInput` object** for a channel-0 actor, so the aspect is written with a
   default `SSerializedPlayerInput` (look forward, stance NULL, zero movement);
2. a client creates **no `CNetPlayerInput`** for an actor without a channel, so even a correct payload is
   parsed and discarded. The bot then remains a physics-only replica: position updates, no aim, no animation.

A vanilla client cannot be changed, so the only standard path is to give each bot a **server-side channel**
(not a fake client connection) and to suppress the `CNetPlayerInput` the engine would create for it. Details below.

## 1. Who writes `INPUT_ASPECT` today

- `INPUT_ASPECT = eEA_GameClientDynamic` (`IPlayerInput.h:58`); the nanosuit uses the same bit
  (`Player.h:375`).
- The **client** writes its own input: `CPlayerInput` calls `ChangedNetworkState(INPUT_ASPECT)` on every input
  change (`PlayerInput.cpp:188,691,991,1314`).
- The **server** receives it into `CNetPlayerInput::SetState` → `DoSetState`, which calls
  `ChangedNetworkState(INPUT_ASPECT)` again (`NetPlayerInput.cpp:102-107,131-134`) so the server re-broadcasts
  the input to the other clients. That is exactly how remote players animate today.
- The server also marks the aspect on stance/profile changes (`Actor.cpp:2351`).
- Serialization: `CPlayer::NetSerialize` (`Player.cpp:5368-5387`):
  - writing: `m_pPlayerInput->GetState(serializedInput)` (`:5371-5372`), then `Serialize_Default` or
    `Serialize_CryMP` (`:5376-5379`);
  - reading: `m_pPlayerInput->SetState(serializedInput)` (`:5381-5383`).
- Authority: there is **no `DelegateAuthority` for player entities**; only held/picked items delegate to a
  channel (`Actor.cpp:327,3634`). A channel-0 entity is server-owned by definition, so the server may mark and
  write the aspect. It already does: `Actor.cpp:2351` fires for every actor, including bots, and the nanosuit
  block (`Player.cpp:5326-5327`, `NanoSuit.cpp:1300-1370`) serializes on the same bit.

**So the server is allowed, but the payload is empty**: `m_pPlayerInput` is created only when
`GetGameObject()->GetChannelId() != 0` (`Player.cpp:984-993`), or in demo playback (`:995-997`). For a
channel-0 bot it stays `nullptr`, `NetSerialize` skips `GetState`, and `serializedInput` remains
default-constructed (`IPlayerInput.h:24-33`: `lookDirection=FORWARD`, `deltaMovement=0`, `stance=STANCE_NULL`).
That is why a client that did read it would still see a statue facing world +Y.

## 2. What a vanilla client does with a channel-0 actor

- Same creation gate on the client: `m_pPlayerInput` is created only for `GetChannelId()!=0`
  (`Player.cpp:984-993`). For a channel-0 bot it is **nullptr**; the client does not create an alternative
  class.
- The read path is guarded: `if (m_pPlayerInput && ser.IsReading()) m_pPlayerInput->SetState(...)`
  (`Player.cpp:5381-5383`). The payload is deserialized into a local variable and thrown away.
- Consequence: the bot replica is driven by the `eEA_Physics` snapshots only (`CActor::NetSerialize`,
  `Actor.cpp:2380-2447`); the mover has no input, so no aim direction, no gait, no stance, no animation.
  This matches the owner's "slide with no aim/animation" observation with `wb_net_ext 0`.
- Important distinction: a **CryMP client** has the `mp_crymp` cvar and the `dir3` format; an **original
  1.1.1.6156 client** does not. "Vanilla CryMP" below means CryMP client without WarsBots; an original EA
  client is a separate, harder target.

## 3. Other things a vanilla client chokes on

| Item | Where | Effect / action |
| --- | --- | --- |
| `warsBotActor` in spawn info | `Player.cpp:4964-4983` (`GetSpawnInfo`, `SerializeSpawnInfo`) | Extra bool before any aspect → malformed stream. Must be off per vanilla channel (DS-027). |
| `WarsBotSuitActive` / `WarsBotMotion` optional groups | `Player.cpp:5317-5323,5330-5349` | Same; `wb_net_ext 0` removes them. `WarsBotMotion` is the only thing that animates bots on our client, so vanilla gets nothing. |
| `mp_crymp` layout | `IPlayerInput.h:35-53` (`dir0` vs `dir3`), `GameCVars.cpp:602` (`OPTIONAL_SYNC`) | Server must run `mp_crymp 0` for original clients; a CryMP client follows the synced value. Mixed original/CryMP clients are impossible with one server value unless the format is per channel. |
| `ClMoveTo` | `Actor.h:746`, `BotInstance.cpp:378` | Vanilla has the RMI, but it teleports the actor; combining it with an input stream produces visible snapping. Do not use for normal driving once input is replicated. |
| PS `SvRequest*` RMIs | `PowerStruggle.lua:201-207` (`SvRequestPP`, `SvBuy`, `RequestRevive`, `RequestSpawnGroup`) | Client→server only. A vanilla client never sends them for a bot, so the server must perform buy/revive/spawn-group directly for bots (DS-030). |
| `GameRules` per-client RMIs to channel 0 | `PowerStruggle.lua:346,350,780,806`; `ClPP/ClRank/ClTeamPower/ClHQHit` | Channel 0 is the server channel, not a client. Sending there either drops or asserts. Guard with `channel>0` (already done in `PowerStruggleAlert.lua:797-812` for `ClHQHit`) or use `eRMI_ToAllClients` for bot events. |
| `m_channelteams[0]` | `GameRules.cpp:1764-1775`, `GetChannelTeam` `:1814-1821` | All channel-0 actors share one map key; the last `SetTeam` wins. `GetTeam(entityId)` is per entity (`:1804-1811`) and is safe, but anything using `GetChannelTeam(0)` is not. |
| `PlayerCountOk` / `g_minteamlimit` | `TeamInstantAction.lua:446-457` → `GetTeamChannelCount` (`GameRules.cpp:1668-1681`) | Counts channels only; bots are invisible → PS never leaves PreGame. `InstantAction.lua:295-302` has the same issue via `GetPlayerCount`. |
| `wb_probe_local_test` gate | `CryMPEngineAdapter.cpp:17-20`; `BotManager.cpp:312` (passes `allowPowerStruggle=false`) | Requires loopback + `sv_lanonly=1` + InstantAction. Public and/or PS server is refused before any of the above matters. |
| `IsPlayer()` gates | PS `ProcessScores` `PowerStruggle.lua:1134`; `GameRules.cpp:1381` already extends `IsMatchParticipant` to bots | Bots earn no PP/CP and may be skipped by score/assist code. Extend the PS gate to `botPlayers[id]`. |

## 4. Handshake for mixed clients

Current `WarsBotsWire()` is a **global** decision: server = `wb_net_ext` cvar (`Player.cpp:4949-4952`), client =
server address heuristic (`:4953-4961`). That cannot serve a mixed server.

Minimal per-channel design:

1. **Hello (client→server), before the first spawn/aspect write.** A tiny reliable RMI
   `WarsBotsHello(protocolVersion, featureBits)` sent once when the client reaches `eCVS_InGame`. Vanilla
   clients never send it.
2. **Server map** `channelId -> featureBits`, default `0` (vanilla). `WarsBotsWireFor(channel)` replaces the
   global `WarsBotsWire()`. The spawn info and optional groups are gated on that channel, not on a cvar.
3. **Our client** sends the hello and uses `WarsBotsWireFor(serverChannel)`; the address heuristic remains only
   as a fallback for old builds.
4. **No mid-connection flips**: CryNetwork caches aspect layouts; `wb_net_ext` becomes a startup override and
   is ignored after the first spawn.
5. **Original EA clients** need `mp_crymp 0` on the server; a per-channel format would require a second
   `Serialize_*` variant selected by the hello, not by a synced cvar.

## 5. Minimal plan (ordered)

1. **Server defaults to vanilla** for public: `wb_net_ext 0` + `mp_crymp 0`; confirm a vanilla client joins
   and sees bots (they will slide; that is the baseline).
2. **Per-channel hello** + `WarsBotsWireFor(channel)`; keep `wb_net_ext` as an override. Test: our client still
   gets `WarsBotMotion`, vanilla gets the standard stream.
3. **Bot replication channel.** Allocate one server-side channel for bots (not a client connection), set
   `IGameObject::SetChannelId(botChannel)` on each bot, and make sure `CPlayer::PostInit` does **not** create a
   `CNetPlayerInput` for a bot (guard `IsWarsBotActor()` in `Player.cpp:984`), otherwise the server will apply
   zero input to the bot every tick and fight the brain.
4. **Write real input** for the bot: a small `IBotPlayerInput : IPlayerInput` that returns the brain's
   `SSerializedPlayerInput` (look = aim direction, deltaMovement = intent, stance/sprint/lean) and marks the
   aspect dirty only when it changes (10–20 Hz, not every frame). `CNetPlayerInput` on the client then animates
   the bot exactly like a remote player.
5. **Guard the channel side effects**: `m_channelteams` must not be written for the bot channel (or must be
   per-bot), `PlayerCountOk` must not count the bot channel as a real player (or must count bots deliberately),
   and scoreboard/voice/chat must ignore it.
6. **Test on two clients** (below), then repeat with `sv_gamerules PowerStruggle` after DS-030.

Risks:

- One channel for many bots means the client sees one "connection" but many actors; `sv_input_timeout` will
  zero the shared input state unless the server writes it every tick for every bot (the input is per entity,
  so this is fine, but the timeout path in `NetPlayerInput.cpp:88-95` must not run for bots).
- If the engine routes RMIs by entity channel, a shared bot channel receives `Cl*` RMIs addressed to bots; a
  vanilla client may execute them for the wrong actor. Use `eRMI_ToAllClients` + entity id checks.
- `BindToNetwork`/`SetChannelId` on an already-bound actor may re-create the entity on clients; do it before
  the first `Revive`/spawn, never mid-round.
- If a real client later connects with the same channel id, the server must not reuse a live bot channel.

**If the channel path is rejected**, the honest fallback is: vanilla clients see bots as sliding actors with
correct hitboxes and names; only WarsBots clients see aim/animation. Do not claim "looks like a normal player"
for vanilla until the channel + input provider is in.

## 6. Ten-minute local test (a person, two clients)

1. Server: `CryMP-Server64.exe +exec server.cfg` with `sv_gamerules InstantAction`, `sv_lanonly 0`,
   `sv_bind 0.0.0.0`, `wb_probe_local_test 1`, `wb_net_ext 0`, `mp_crymp 0`, `wb_add 2` (or `wb_fill 2`).
2. Client A: CryMP client **without** WarsBots (the vanilla target). Client B: WarsBots client.
3. On A: join, spectate/observe a bot for 20 s. Record: does A stay connected (no "Malformed packet"), do the
   bots move smoothly, do they aim/turn, do they animate (legs/arms), does the scoreboard show them, does A's
   `Client.log` contain a serialization error.
4. On B: same observation; B should show aim/animation via `WarsBotMotion` even with `wb_net_ext 0`.
5. Repeat with `wb_net_ext 1` on the server: A must fail (expected); this proves the gate works.
6. Save both `Client.log` + `Server.log` and the demo; the acceptance is A connected + bots move + no
   malformed packet, and (after step 3 of §5) bots aim/animate on A.

## 7. What to measure before/after

- `Client.log`: "Malformed packet", "Failed to read", desync/disconnect lines; count per join.
- Server: `wb_net_ext`, `mp_crymp`, per-channel hello bits, bot channel id, `PlayerCountOk`.
- Gameplay: bot yaw/pitch on A vs server `MOVE TRACE` (should match within quantization); animation state
  (idle/run/crouch) vs `MOTION INTENT`; no rubber-banding (position delta per snapshot).
- Regression: our client must still pass the DS-027 wire tests and C-104 probes with `wb_net_ext 1`.
