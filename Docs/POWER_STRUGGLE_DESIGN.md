# Power Struggle architecture — design only, 2026-10-24

Status: source review and staged design, **no PS runtime support implemented or tested**.
Current playable source remains `5e8daa65`; development remains on
`warsbots/feature/weapon-goals`. Individual combat/navigation on Steelmill is still
first priority. Goal: two autonomous teams playing a full native PS match from
capture through legitimate enemy-HQ destruction. No change to IA gate in this task.

## Evidence and current boundaries

Read selected functions, not the entire SDK. Repository sources:

- [PowerStruggle.lua](../Pak/CryMP/Scripts/GameRules/PowerStruggle.lua): inherits
  TeamInstantAction; `TEAM_SPAWN_LOCATIONS=true`, `USE_SPAWN_GROUPS=true`; uses
  `reviveQueue[playerId]`, `inBuyZone[playerId]`, PP/CP/rank keys. `RequestRevive`,
  `QueueRevive`, `UpdateReviveQueue`, `RevivePlayerInQueue`, `CommitRevivePurchases`
  own wave timing, eligibility, queued equipment and rank-related PP floors.
- [InstantAction.lua](../Pak/CryMP/Scripts/GameRules/InstantAction.lua): inherited
  `RequestSpawnGroup` validates team, vehicle condition and seat capacity;
  `CanRevive` requires a selected group when groups are enabled. Group choice is
  entity state, not just an arbitrary spawn coordinate. TeamInstantAction additionally
  requires a nonzero team. Do not reuse the IA five-second bot revive loop in PS.
- [PowerStruggleAlert.lua](../Pak/CryMP/Scripts/GameRules/PowerStruggleAlert.lua):
  `OnCapture` forwards objective events; `OnHQDestroyed` calls native `OnGameEnd`.
- [CGameRules](../Code/CryGame/GameRules.cpp): `SetTeam` maintains entity/team lists
  but also objective RMI, voice and `m_channelteams[channelId]` state. Multiple
  channel-0 actors cannot safely be treated as separate channel-team entries.
- [ScriptBind_GameRules](../Code/CryGame/ScriptBind_GameRules.cpp)::GetPlayers already
  distinguishes match participants from connection-only queries. Its bot recognition
  and enumeration must be audited for PS; existing IA success does not establish PS.
- [BotMatch](../Code/CryGame/Bots/BotMatch.cpp)::Register explicitly rejects spawn
  groups and attaches IA-only hooks. `BotInstance::Revive` directly chooses an IA
  spawn and team 0. `BotManager::CanRunLocalIA` is intentionally IA-only.
- [BotTargeting](../Code/CryGame/Bots/BotTargeting.cpp) already recognizes owned
  channel-0 actors and equal nonzero teams as allies. Vehicle occupants currently
  remain excluded; team-0 FFA behavior must not leak into unassigned PS actors.

Dependencies inspected read-only inside the installed original Crysis archives at
`E:/Steam/steamapps/common/Crysis/Game/`. These are game assets, not files to copy
into Git. CryMP's tracked scripts override some files; others are loaded from PAKs.
Before implementation verify actual engine load precedence and script versions.
Observed dependency candidates (patch replaces same-named base entries):

| Asset | Source inspected | Evidence |
|---|---|---|
| PowerStruggleBuying.lua | ZPatch1.pak | `SvBuy` validates team, frozen/alive state, item availability, buy zone and PP; then BuyItem/BuyVehicle. `AwardPPCount` sends ClPP to actor channel. |
| PowerStruggleRank.lua | GameData.pak | entity-keyed rank/CP; UpdateTeamRanks sorts team participants. |
| PowerStruggleUtils.lua | GameData.pak | MakeCapturable tracks area occupants by EntityId; UpdateCapture checks living/spectator/team state and native requirements/contestation. |
| Factory.lua | ZPatch1.pak | Buy -> Queue; production/cancellation callbacks target actor channel; capture can register a power point. |
| HQ.lua | GameData.pak | OnHit accepts enemy/neutral `explosion && type=="tac"` damage, then native destruction callback. Ordinary rifle DPS is not a base-win strategy. |

SHA256 of inspected dependency contents, for reproduction:
- Buying: `de1c7b236607be7ea7ae6ad4a5c10470a4009e885fb7a2dadee63617769c3d2a`.
- Utils: `fe85d8e5f398677c464cb8c6f38dcc46fe5615e58792062594606136b80f13d0`.
- Factory: `f94cc5a33ca3558f29377624d4b488818417ea38067f1e48ee82f96fdac04453`.
- HQ: `413bd3ed6bf4eff7fe243aa68057cbc6676f6539720ee82d6f088e5b13b89107`.

Buying's `BuyVehicle` checks factory and limits, queues production, deducts PP and
team energy and abandons the previous owned vehicle. `SetTeamPower` clamps native
power to 0..100, using synchronized global values; `PowerTick` accrues power from
owned registered points. Preserve native costs, income scales, ranks, queue capacity,
refunds, cooldowns and weapon damage. Calling BuyItem alone would bypass outer
SvBuy validation and is **not** an acceptable bot purchase implementation.

## Proposed boundary, not an implemented interface

Retain shared BotProfile (skill/style), BotCombat/Targeting (LOS and utility),
BotMovement, BotNav/map memory, native weapon/suit execution and bounded telemetry.
Steelmill is an evaluation map, not a class or hardcoded coordinate source. Cache
navigation/experience by actual map/version; objective entry points come from native
entities and checked geometry. No automatic reuse of Steelmill memory on PS maps.

Introduce the smallest mode boundary only when PS integration is implemented:

```cpp
// Sketch; types and signatures deliberately not added to the build yet.
struct IBotGameMode {
    virtual ~IBotGameMode() = default;
    virtual ModeCapabilities Capabilities() const = 0;
    virtual ModeResult RegisterOwnedBot(EntityId bot, int team) = 0;
    virtual void UnregisterOwnedBot(EntityId bot) = 0;
    virtual Relation Classify(EntityId self, EntityId other) const = 0;
    virtual ObjectiveView ReadObjectives(EntityId bot) const = 0;
    virtual ModeResult RequestSpawn(EntityId bot, EntityId group) = 0;
    virtual ModeResult RequestPurchase(EntityId bot, ItemId item) = 0;
    virtual void OnBotDeath(EntityId bot) = 0;
    virtual void OnLevelEnd() = 0;
};
```

IA adapter preserves current registration/equipment/free-for-all respawn behavior;
PS adapter owns team participation, authoritative validated transactions and native
spawn-wave requests. ModeResult must distinguish accepted/pending/rejected and give
reason/transaction ID: queue acceptance is not production completion. Own-actor
identity, mode, server opt-in and current-level generation are checked on every
request. Removal/death/map change cancel pending bot intentions safely; native
purchase/refund state remains authoritative. Do not perform refactoring just to
create this header while PS support remains blocked.

`PSCommander`: one server-side instance per team, strategic decisions at about 1 Hz,
not per physics tick. Consumes only team-authorized objective/energy/budget state,
owned squad status and expiring shared sightings. No global enemy position/HP feed.
Produces assignments `{objectiveId, role, priority, expiry, reason}`: capture spawn,
defend spawn, capture power/factory, resupply, support/escort, eventual TAC assault.
Reserve limited squad slots/vehicle budget to prevent every bot selecting one goal;
commit reservations only on native success and release on failure/death/timeout.

Individual brain at existing 10 Hz decides local safety, combat distance and temporary
withdrawal. It can interrupt an assignment to survive, then resume/replan; weapon
pickup cannot replace a strategic goal indefinitely. Commander cannot teleport,
set HP/energy/PP, fabricate kills or spawn vehicles. Objective roles and strategy
are separate from difficulty and personal Assault/Marksman style. Strength roof
links, cover/flank planner and driving are separate verified capabilities, not
assumed dependencies of the first infantry capture milestone.

## channelId=0: required safety work before any PS opt-in

1. Keep EntityId + owned-registry identity as the participant key. channelId is only
   a real transport endpoint; never invent a channel, OnClientConnect, global
   IsPlayer=true, or a GetActorByChannelId(0) lookup for bot identity.
2. Audit CGameRules::SetTeam and unregister/reset symmetry. Entity/team membership
   and replicated ownership must work, while connection-team, objective channel
   RMIs and voice handling stay restricted to actual positive connected channels.
   Do not simply skip all SetTeam logic for bots or globally reinterpret channel 0.
3. Audit all inherited Lua paths, not only entry points: QueueRevive writes
   channelSpectatorMode[0]; UpdateReviveQueue sends ClReviveCycle/ClSpawnGroupInvalid;
   buying awards/confirmations/zones and Factory production target actor channels.
   Their gameplay state must remain entity-keyed; bot feedback becomes a local
   result/event. Validate real positive channel before client-targeted notifications;
   preserve broadcasts/state replication to genuine clients. Do not assume channel
   0 is harmless or that every engine RPC dereferences a channel: prove each case.
4. Native queue is already keyed by EntityId, but every eligibility, spectator,
   equipment and revive callback must be made safe. Do not run both native PS wave
   queue and IA bot auto-respawn. Failed spawn remains pending/rejected with a reason.
5. Route bot buy intent through the same complete native validation/transaction
   sequence; never call a client RPC wrapper unmodified or grant free equipment.
   Separate bot local feedback from human HUD notifications, without relaxing price,
   rank, team, buy-zone, production or energy requirements.
6. Audit capture/reward/rank loops and IsPlayer checks (e.g. PS ProcessScores),
   ordinary players and connection-only counts. Shared GetPlayers changes are not
   proof that capture, score rewards, team balancing and rank already include bots.
7. Network binding remains native actor creation. Test team/model/objective changes,
   death/revive and late join with matching real clients. Source review alone cannot
   establish any of those network/visual outcomes in PS.

## Stages and acceptance gates

1. Finish Steelmill core: owner feedback, actual stall diagnosis, balanced repeated
   difficulty evaluation. Keep progress experiment OFF until evidence supports it.
2. Isolated PS lifecycle prototype (separate default-OFF opt-in): one owned infantry
   bot per native team, ordinary human teammate, no commander/economy shortcuts.
   Verify teams/hostility/Tab and capture-zone recognition, removal and level change.
3. Native spawn groups + revive waves: deaths queue exactly once, correct team group,
   lost/captured/destroyed group causes valid replanning, no cross-team/channel-0
   collision, no bot-driven spectator/HUD effects on humans. Two real clients and
   late join required before claiming replication works.
4. Infantry capture/defend: minimal PSCommander goals, native area occupancy and
   ownership events, pressure-aware replanning. Test contested/re-captured objectives
   and reachable entrances on a real PS map, not invented Steelmill coordinates.
5. Economy/equipment: native PP/CP/rank, buy zones and respawn purchases, insufficient
   funds/rank, failure/refund and removal during pending orders. Reconcile native
   transactions exactly; no negative balances or bot-only income multipliers.
6. Factories/energy and vehicles: queue/claim/cancel/refund first; vehicle control,
   seat ownership, path planning and combat separately. Human owners/queues protected.
7. End-to-end win: accumulate native energy, legally buy/produce required TAC-capable
   equipment, deliver native allowed TAC damage to enemy HQ, receive native game end.
   Then repeated side-swapped full matches, resets and human join/leave regressions.

Record build/script hashes, map, teams/rosters/seed if available, costs, objective
changes, assignment failures, queue latency, spawn failures, vehicle transactions,
CPU and match outcome. Compare repeated side-swapped matches; do not optimize only
K/D. Offline adapter/transaction tests precede headless PS tests; visuals, driving
and fair team knowledge require live verification. No claim of a fully autonomous
PS match until both teams actually complete this chain without operator commands.
