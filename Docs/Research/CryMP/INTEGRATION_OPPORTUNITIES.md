# Integration opportunities: what WarsBots should take from CryMP

Read-only research, branch `warsbots/research/crymp-native-audit`. **Nothing here is implemented.**
Every proposal names the exact files it would touch, so that Codex (owner of
`warsbots/feature/weapon-goals`, BotMatch/BotBrain/BotCombat/BotNav, GameRules integration and
spectator) can schedule it without conflicting work. Evidence: [CRYMP_CAPABILITIES.md](CRYMP_CAPABILITIES.md),
[POWER_STRUGGLE_NATIVE_FLOW.md](POWER_STRUGGLE_NATIVE_FLOW.md), [CHANNEL_ZERO_AUDIT.md](CHANNEL_ZERO_AUDIT.md).

Principle: CryMP is the foundation. WarsBots should *request* native actions (join, spawn group,
revive, buy, capture by presence) with an EntityId and only own what the engine does not have:
decision-making, navigation memory, and bot-side feedback for notifications that are addressed to
a client channel.

## 1. Ranked proposals

Value: H/M/L for reaching a native PS match. Cost: S (≤1 day), M (days), L (weeks).
"Shared" = touches a file another workstream owns; such items are **proposals for Codex**, not
for this branch.

| # | Proposal | Value | Cost | Shared files | Status of evidence |
|---|---|---|---|---|---|
| P0 | Bot test-server profile (cvars only) | H | S | none (server cfg) | SRC |
| P1 | Join through native `CGameRules::ChangeTeam` | H | S | `Bots/BotMatch.cpp` | SRC |
| P2 | Lua channel-0 RMI shim + bot event sink | H | S–M | new `Pak/Scripts/WarsBots/PowerStruggle.lua` | SRC + UNPROVEN dispatcher |
| P3 | C++ channel-0 guards in GameRules | H | S | `GameRules.cpp/.h`, `ScriptBind_GameRules.cpp` | SRC |
| P4 | PS participant predicate (opt-in) | H | S | `Bots/BotMatch.cpp` | SRC |
| P5 | Native spawn group + wave revive | H | S | `Bots/BotInstance.cpp`, P2 file | SRC |
| P6 | Native buying via `Server:SvBuy` | H | S | P2 file | SRC |
| P7 | Capture by presence, objective model from native entities | H | M | new commander code | SRC |
| P8 | Bot kill PP/CP (economy parity) | M | S | P2 file | SRC, product decision |
| P9 | Turret danger zones from `AutoTurret` entities | M | S | BotNav/brain (Codex) | SRC |
| P10 | GameplayRecorder listener for commander/telemetry | M | M | new listener, `Bots/BotManager.cpp` | SRC |
| P11 | Vehicle driving via native `ProcessAI` | H (late) | L | new code, vehicles | SRC + UNPROVEN net |
| P12 | Gunner seats via `CMovementRequest` forwarding | M | M | brain | SRC |
| P13 | Debug: `net_log_remote_methods`, PersistantDebug routes | M | S | none | SRC |
| ✗ | CryAI pathfinding/cover on PS maps | — | — | — | **NO**: header-only `.bai` on all 5 maps |

### P0. Bot test-server profile (no code)
* `g_minteamlimit 0` — otherwise `TeamInstantAction:PlayerCountOk` (TIA:578) keeps a match with a
  bot-only team in PreGame (`GetTeamChannelCount` counts connections; default 1, `GameCVars.cpp:335`).
* `g_autoteambalance 0` — TIA AutoTeamBalance counts entities and could move bots.
* `g_teamlock 0` — `PowerStruggle:IsTeamLocked` uses connection counts and ignores bots.
* Map: **Plantation** (4 SpawnGroup, 1 Factory, 2 energy points) or **Refinery** (5/2/2).
* Risk: none for production servers (test profile only). It is a configuration, not a bypass.

### P1. Native join
Replace the IA-only team handling for PS with `rules->ChangeTeam(actor, teamId)`
(`GameRules.cpp:1286`): on the server it calls `PowerStruggle.Server:OnChangeTeam` directly, which
kills if alive, queues revive, calls `SetTeam`, selects the team's default spawn group and cancels
factory jobs — the same path a human takes. Interface: `ModeResult RegisterOwnedBot(EntityId, int team)`
from the design doc maps 1:1 onto it. Prerequisite: P2+P3 (side-effects).
Risk: `TEAM_CHANGE_MIN_TIME` and `IsTeamLocked` apply to bots too (desirable); team-change
warning text goes to channel 0 (P2/P3).

### P2. Lua channel-0 RMI shim (one file, no PAK copies)
A new CryMP-owned script, loaded the way `Scripts/WarsBots/InstantAction.lua` is (via
`BotMatch::Register` → `ExecuteFile`, no CMake change because `Pak/*` is globbed at
`CMakeLists.txt:1708`), that at PS attach:
1. wraps `g_gameRules.onClient` (and `onClient` of each `Factory`/capturable entity) with a proxy whose
   methods forward to the original unless the first argument is `0`; for `0` it calls
   `WarsBotsPS:Notify(methodName, ...)` (bot feedback: buy ok/error, PP, revive cycle, spawn group
   invalid, vehicle queued);
2. wraps `PowerStruggle.ProcessScores` only if P8 is accepted.
It changes no PAK script and copies no game code. Covers every C-RMI Lua row of the audit with one
mechanism.
**Unproven:** whether the closed ScriptRMI `onClient` table tolerates being proxied (method
identity/`self`). First experiment: wrap one method (`ClBuyError`) and check
`net_log_remote_methods 1` output. Fallback: per-function `replace_pre` wrappers of the few PS
functions involved (`SvBuy`, `UpdateReviveQueue`, `ResetRevive`, `AwardPPCount`, `RequestSpawnGroup`).

### P3. C++ guards (minimal diff, owned by GameRules integration)
| Site | Proposed guard |
|---|---|
| `CGameRules::SetTeam` `GameRules.cpp:1755` | call `UpdateObjectivesForPlayer` only if `GetChannelId(id) != 0` |
| `CGameRules::SetTeam` `:1764-1775` | skip the `m_channelteams` block when `channelId == 0` (removes phantom C2) |
| `GAMERULES_INVOKE_ON_TEAM(_NOLOCAL)`, `ACTOR_INVOKE_ON_TEAM` `GameRules.h:47-97` | `if (int ch = GetChannelId(*_player))` before `InvokeRMI` |
| `CGameRules::AddTaggedEntity` `GameRules.cpp:924-928` | same channel check per member |
| `CScriptBind_GameRules::SendTextMessage` `ScriptBind_GameRules.cpp:1536-1594` | return early if `to == eRMI_ToClientChannel && channelId == 0` |
| EMP `ScriptBind_GameRules.cpp:2466`, `CGameRules::MovePlayer` `GameRules.cpp:1249` | `IsPlayer()` guard |
Interface: none new; behaviour for humans unchanged (their channel is never 0). Risk: low; must be
built once by the BUILD owner, not in parallel.

### P4. PS participant predicate
`BotMatch::IsBot/Collect` (`Bots/BotMatch.cpp:14,23`) return false outside `InstantAction`.
For a PS opt-in (`wb_ps_prototype`, default 0) accept `PowerStruggle` too.
**Side-effect to handle first:** once `GetPlayers()` includes bots, PS's "dead > 5 s → spectator"
rule (`PowerStruggle.lua:941-947`) starts spectating dead bots, and IA `RequestSpawnGroup` counts
them for vehicle seats (correct). Either keep bots revived within 5 s by the wave, or exclude owned
bots from that rule in P2. Spectator semantics belong to Codex.

### P5. Native spawn group + wave revive
* After join: optionally `g_gameRules.Server.RequestSpawnGroup(g_gameRules, botId, groupId)` to
  choose a forward group; default group is already set by P1.
* After death (`death_time` set, IA:1068): server calls `g_gameRules.Server.RequestRevive(g_gameRules, botId)`
  once > 2.5 s; the wave in `UpdateReviveQueue` revives via native `CGameRules::RevivePlayer`.
* Replace for PS: `BotInstance::Revive` (team 0, IA spawn, `Bots/BotInstance.cpp:209-237`) and the
  IA five-second loop. Keep the IA path unchanged for IA.

### P6. Native buying
`g_gameRules.Server.SvBuy(g_gameRules, botId, itemName)` runs the full native validation (team,
frozen, availability/rank, buy zone or dead-time purchase, PP) and transaction; the result arrives
through P2 as `ClBuyOk/ClBuyError`. Never call `BuyItem`/`BuyVehicle` directly. Replaces for PS:
`BotMatch::EquipLikePlayer`, `BotWeapons::GiveTestSCAR`.

### P7. Capture by presence and a native objective model
No capture code is needed: standing alive, non-spectating, in the capture area is what the native
`UpdateCapture` counts. WarsBots needs: an objective list from `System.GetEntitiesByClass`
(`SpawnGroup`, `Factory`, `AlienEnergyPoint`, `HQ`) with team (`game:GetTeam(id)`), capture
progress fields set by `MakeCapturable`, and the referenced `AreaShape` geometry for a reachable
standing point (via BotNav, not CryAI). Feeds the PSCommander of the design doc.

### P8. Bot kill PP/CP
`PowerStruggle:ProcessScores` awards kill PP/CP only for `IsPlayer()` shooters (`PowerStruggle.lua:1188`).
Without a change, bots only earn PP from captures, vehicle kills and the rank floor at revive →
weak economy, no TAC ever. Proposal: P2 wraps ProcessScores and calls the same native
`AwardKillPP/AwardKillCP` for owned bot shooters. Product decision; no income multipliers.

### P9. Turret danger zones
`CGunTurret::IsTargetHostile` is team-based (`GunTurret.cpp:590-597`); 9–18 turrets per map
guard bases. Enumerate `AutoTurret`/`AutoTurretAA` entities at level start; treat enemy turret
radius as a high-cost region in BotNav; never route an assault into it without a plan.

### P10. GameplayRecorder listener
`CGameRules` already records `eGE_ChangedTeam`, kills, hits (`GameRules.cpp:1800` and hit/kill
paths). A listener gives the commander and telemetry one C++ event stream instead of parsing logs.

### P11–P12. Vehicles
A channel-less driver makes every vehicle movement class run `ProcessAI`
(`VehicleMovementBase.cpp:346-355`), consuming `CMovementRequest` move target and desired speed
(`VehicleMovementStdWheeled.cpp:1421-1520,1714`). Passenger/gunner aim is forwarded by
`CPlayerMovementController::RequestMovement` (`:105-115`). What is missing: routes (no road nav),
unstuck, and a multiplayer authority test for channel-less drivers. Late stage only.

### P13. Debug tooling
* `net_log_remote_methods 1` (`CryAction/ScriptRMI.cpp:35`) — log of every script RMI: the cheapest
  way to observe what channel-0 RMIs actually do during the prototype.
* `PersistantDebug` for objective/route drawing; existing WarsBots log levels.

## 2. Own solutions that CryMP can replace

| WarsBots today | Native replacement | Proposal |
|---|---|---|
| IA registration + team 0 in `BotInstance::Revive` | `ChangeTeam` + PS wave queue | P1, P5 |
| Five-second IA auto-respawn | `RequestRevive` + `UpdateReviveQueue` | P5 |
| `EquipLikePlayer`, `GiveTestSCAR` | PS default loadout + `SvBuy` + revive purchases | P6 |
| (future) custom capture timer | `MakeCapturable/UpdateCapture` occupancy | P7 |
| (future) own vehicle steering | `IVehicleMovement::RequestMovement` → `ProcessAI` | P11 |
| (future) team-mate/enemy rules | `GetTeam`, `IsTeamKill`, turret `IsTargetHostile` | — |

Not replaceable: BotNav (no CryAI data), the sprint/motion replication hook (no PlayerInput for
channel 0), decision-making, combat utility.

## 3. Blockers for Power Struggle

| Blocker | Kind | Unblocked by |
|---|---|---|
| Match does not start with a bot-only team | C-COUNT | P0 (`g_minteamlimit 0`) |
| `BotMatch` is IA-only | design | P4 |
| Per-client RMIs to channel 0 from SetTeam, objectives, tagging, buy, revive, factory | C-RMI, closed dispatcher | P2 + P3, then live test |
| `m_channelteams[0]` phantom | C-KEY | P3 |
| No kill PP for bots | P | P8 |
| No CryAI navigation on PS maps | data | own BotNav per map (Codex) |
| Victory needs TAC: economy + vehicle/launcher + HQ approach | capability | P6, P8, P11, commander |
| Replication of team/model/revive to real clients unverified | test | prototype gates below |

## 4. Smallest safe PS prototype (plan only)

Goal: **one owned bot joins a native PS team, dies and is revived correctly by the native wave,
and captures one objective by presence.** Nothing else (no commander, no buying, no vehicles).

### Preconditions
* Codex's Steelmill/IA work is at a stable checkpoint; the prototype lives behind
  `wb_ps_prototype` (default 0) so IA behaviour is byte-for-byte unchanged when off.
* One BUILD owner builds once; QA uses a separate test server process and **not** the shared
  runner or the user's game session.

### Changes (smallest set, in order)
1. **P3 subset**: SetTeam guards (objectives RMI, `m_channelteams`), `SendTextMessage` guard.
   Files: `Code/CryGame/GameRules.cpp`, `Code/CryGame/ScriptBind_GameRules.cpp`.
2. **P4**: `BotMatch::IsBot/Collect` accept `PowerStruggle` when `wb_ps_prototype 1`;
   `BotMatch::Register` PS branch that does *not* attach IA hooks and calls
   `rules->ChangeTeam(actor, team)` (P1). File: `Code/CryGame/Bots/BotMatch.cpp`.
3. **P2 minimal**: new `Pak/Scripts/WarsBots/PowerStruggle.lua` with the channel-0 proxy for
   `g_gameRules.onClient` (fallback: wrappers for `UpdateReviveQueue`, `ResetRevive`,
   `RequestSpawnGroup`, `AwardPPCount`) and `WarsBotsPS:RequestRevive(botId)`.
4. **P5**: in PS mode the bot's death handler calls `WarsBotsPS:RequestRevive` once after 2.5 s;
   IA respawn path disabled for PS. File: `Code/CryGame/Bots/BotInstance.cpp` (PS branch only).
5. Objective: hard-pick the **nearest neutral `AlienEnergyPoint` or bunker SpawnGroup** from native
   entities; walk there with existing movement (straight line + existing nav fallback); stand in
   the capture area. No commander.

### Test matrix (dedicated test server, P0 cvars, Plantation)
| # | Scenario | Pass criteria |
|---|---|---|
| T1 | 1 human US + 1 bot KPA, start | match leaves PreGame; bot team = KPA on server and on the client (scoreboard/Tab) |
| T2 | bot killed by human | exactly one queue entry; revived at the next wave at a KPA spawn group; no IA respawn |
| T3 | bot's group captured by US | bot revives at another valid KPA group or waits with "overdue" (no crash, no cross-team spawn) |
| T4 | bot stands in neutral energy point | native capture completes for KPA; `OnCapture` broadcast seen on the client |
| T5 | 2 real clients + late join | late client sees bot's team/model/position correctly |
| T6 | `net_log_remote_methods 1` | record every RMI with channel 0; zero crashes/disconnects; list kept as evidence |
| T7 | remove bot / map change | queue entry, factory jobs and team membership cleared; no phantom channel count |
| T8 | `wb_ps_prototype 0` regression | IA on Steelmill unchanged (Codex's QA suite) |

### Rollback
`wb_ps_prototype 0` disables steps 2–5; step 1 guards are no-ops for humans. Revert commits are
isolated per step.

### Out of scope for the prototype
Buying, PP for kills, factories, vehicles, TAC/HQ, commander, more than one bot per team.

## 5. What can be done next without touching Codex's work

* Run the two offline tools on any new script/map version and diff the output (they need only the
  installed game files).
* Extend `level_inventory.py` with area geometry (capture `AreaShape` points) to precompute standing
  positions per objective (still offline).
* Write the P2 Lua shim as a *design draft* inside `Docs/Research/CryMP/` (not under `Pak/`), ready for
  review.
* Prepare the P3 patch as a reviewed diff in `Docs/Research/CryMP/` for the BUILD owner.
* After Codex releases a checkpoint: run T1–T8 on a separate test server, never on the shared
  runner or the user's game.
