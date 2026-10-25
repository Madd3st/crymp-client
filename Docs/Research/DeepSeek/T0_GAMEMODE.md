# T-0: `IBotGameMode`, teams, team spawn and PS registration

Date: 2026-10-26. Read-only spec; no code. Scope: the minimum mode abstraction that lets the existing channel-0 bot
become a participant of InstantAction (IA), Team InstantAction (TIA) and Power Struggle (PS), with 3–4 bots per team
and a team-safe spawn. Vehicles are specified separately in `VEHICLES.md`; PS behaviour in `PS_MIN.md`; the combined
contract is `PS_T0_SPEC.md`.

## 1. The interface

Keep the interface engine-free (plain ids, floats, strings), so it can be unit-tested without `gEnv`/`CGameRules`:

```cpp
#include <cstdint>
#include <vector>

namespace WarsBots {

using BotEntityId = std::uint32_t;
using BotTeamId = int;

struct BotSlot {
    int number = 0;
    BotSkill skill = BotSkill::Normal;
    BotStyle style = BotStyle::Universal;
    BotTeamId team = 0;       // 0 = FFA / unassigned
};

struct BotSpawn {
    BotEntityId location = 0;
    std::uint32_t group = 0;   // EntityId by value in the adapter
    float pos[3] = {0,0,0};
    float rot[3] = {0,0,0};
};

struct BotScoreKeys { int kills=0, deaths=0, headshots=0, ping=0; };

class IBotGameMode {
public:
    virtual ~IBotGameMode() = default;
    virtual const char* Name() const = 0;          // "InstantAction", "TeamInstantAction", "PowerStruggle"
    virtual int TeamCount() const = 0;             // 0/1 = FFA, 2 for PS
    virtual bool IsTeamBased() const = 0;
    virtual bool IsPowerStruggle() const = 0;
    virtual bool SupportsVehicles() const = 0;
    virtual int DesiredBotsPerTeam(int requested) const = 0; // 3 or 4; 0 for FFA

    // Slot/team before the actor exists; must be deterministic and balance-aware.
    virtual BotTeamId AllocateTeam(int number, const int* assignedPerTeam, int teamCount) const = 0;

    // Engine-facing adapter (implemented next to CGameRules, not in the interface header).
    virtual bool IsParticipant(BotEntityId id) const = 0;
    virtual void CollectBots(std::vector<BotEntityId>& out) const = 0;
    virtual bool RegisterParticipant(BotEntityId id, const BotSlot& slot) = 0;
    virtual void UnregisterParticipant(BotEntityId id) = 0;
    virtual bool ApplyTeam(BotEntityId id, BotTeamId team) = 0;
    virtual BotTeamId TeamOf(BotEntityId id) const = 0;
    virtual BotSpawn FirstLocation(BotTeamId team) const = 0;
    virtual BotSpawn ChooseSpawn(BotEntityId id, BotTeamId team, BotEntityId avoid) = 0;
    virtual bool PlaceAtSpawn(BotEntityId id, const BotSpawn& spawn) = 0;
    virtual void Equip(BotEntityId id, const BotSlot& slot) = 0;
    virtual const char* AttachScript() const = 0;
    virtual bool CanInspect() const = 0;
    virtual BotScoreKeys ScoreKeys() const = 0;
};
}
```

The adapter owns the `CGameRules`/`IActorSystem` calls; the interface stays testable. `BotMatch` and `BotManager` must
take `IBotGameMode&` instead of constructing a mode from a string on every call.

### Implementations

| Mode | TeamCount | Allocation | Registration | Spawn | Score keys |
| --- | ---: | --- | --- | --- | --- |
| `InstantActionMode` | 0/1 (FFA) | all `team=0` | existing `InstantAction.lua` attach; `ResetScore` | single `GetSpawnLocation` list | IA `SCORE_*_KEY` |
| `TeamInstantActionMode` | rules teams | lowest count, then number order | IA hooks + `SetTeam` | team filter + neutral flag | IA keys |
| `PowerStruggleMode` | 2 (`tan`,`black`) | lowest count, then tan/black | PS attach + `SetTeam` + spawn group | team spawn group, no enemy/neutral default | PS/IA inherited keys |

`PowerStruggle` is `TeamInstantAction` with `TEAM_SPAWN_LOCATIONS=true`, `USE_SPAWN_GROUPS=true`,
`NEUTRAL_SPAWN_LOCATIONS=false` (`PS_RULES.md:9-13`); it is a distinct adapter, not a string branch.

## 2. Team assignment and scaling

Owner decision: **3–4 bots per team, scalable**. Use a cvar (proposed `wb_ps_bots_per_team`, default 3, clamp 3..4) and
derive the total:

```
teamCount = mode->TeamCount();                  // PS = 2
perTeam   = mode->DesiredBotsPerTeam(cvar);     // 3 or 4
desired   = teamCount * perTeam;                // 6 or 8
limit     = min(desired, BotManager::MaxBots()); // wb_maxbots is the hard cap
```

If `MaxBots()` is below `teamCount*3`, spawn `MaxBots()` in balanced order and log `PS team short`; do not create a
fifth bot per team. `AllocateTeam` picks the team with the smallest assigned count; tie by lowest team id/round-robin.
The per-team array is manager state, not `number%2` (free slots are not contiguous after removals).

This replaces `BotMatch`'s FFA-only assumptions. In IA/TIA, the same slot logic still runs but `TeamCount` may be 1;
TIA uses the rules' real team list (`GetTeamCount`, `GameRules.h:305`).

## 3. Registration and team application

Current path: `BotInstance::Spawn` calls `BotMatch::Register` at `BotInstance.cpp:68`; `BotMatch::Register` refuses
`USE_SPAWN_GROUPS` (`BotMatch.cpp:34-35`), executes `Scripts/WarsBots/InstantAction.lua` (`:40`) and writes IA score
keys (`:79-82`). `SetTeam` is never called (`Audit §2.3`). New path:

1. `BotManager::Add` resolves the mode and allocates `{number, team}` **before** choosing a position
   (`BotManager.cpp:304-325`).
2. `BotInstance::Spawn` creates the channel-0 actor (`:39-64`), then calls `mode->RegisterParticipant(id, slot)`
   instead of `BotMatch::Register`.
3. `mode->ApplyTeam(id, team)` calls `rules->SetTeam(team, id)` (`GameRules.cpp:1721-1801`), then
   `SetPlayerSpawnGroup(id, teamGroup)` when PS/`USE_SPAWN_GROUPS` (`GameRules.h:383`) and
   `CheckSpawnGroupValidity` (`GameRules.h:388`).
4. `mode->ChooseSpawn`/`PlaceAtSpawn` run **after** `SetTeam`; `GetSpawnLocation(id, ignoreTeam=false,
   includeNeutral=false, group, ...)` filters team (`GameRules.cpp:2473-2481`). For IA, the existing single-list helper
   remains (`BotManager.cpp:246-270`), but it is an implementation detail of `InstantActionMode`.
5. `EquipLikePlayer`/PS loadout runs after the team and spawn, so PS can charge/commit the spawn loadout.

`SetTeam` caveat for channel-0: `GetChannelId(id)` returns the actor channel (`GameRules.cpp:500-507`), which is 0 for
every bot. `SetTeam` writes `m_channelteams[0]`, sends `ClResetObjectives` to channel 0 and adds the bot to a voice
group (`GameRules.cpp:1750-1778,1904-1915,4452-4483`). Per-actor team (`m_entityteams`, `GetTeam`) is correct, but the
shared channel-0 map is not. T-0 must either add a bot-specific `SetTeam` path that skips channel/voice/objective RMI,
or forbid any gameplay decision from `GetChannelTeam(0)`. Add a unit assertion that two bots on different teams both
return their own `GetTeam(id)` while `GetChannelTeam(0)` is never consulted by bot code.

PS spawn groups: `BotMatch::Register` currently refuses `USE_SPAWN_GROUPS`. Replace the refusal with
`SetPlayerSpawnGroup` + `GetTeamDefaultSpawnGroup` (`GameRules.h:383,387`) and an explicit fallback to the team's first
location (`GetFirstSpawnLocation(team,0)`, `GameRules.h:368`). Do not fabricate a channel for the group callback; the
server-side spawn-group path is sufficient.

### Score keys

Keep the keys in `IBotGameMode::ScoreKeys` and use them in `BotMatch::Unregister` (currently hard-coded `:79-82`) and
in the score log (`:94-100`). IA/TIA use `SCORE_KILLS_KEY/DEATHS_KEY/HEADSHOTS_KEY/PING_KEY`; PS inherits these but
the adapter is the only place that knows. `ResetScore`/`SetSynchedEntityValue` are server calls and work for a
channel-0 actor; do not add a fake channel.

## 4. Replace every IA string branch

| Site | Current | T-0 change |
| --- | --- | --- |
| `BotMatch.cpp:14` | `strcmp(class,"InstantAction")` | `mode->IsParticipant(rules,id)` |
| `BotMatch.cpp:23` | same in `Collect` | `mode->CollectBots` |
| `BotMatch.cpp:27-56` | IA-only `Register` | `mode->RegisterParticipant` |
| `BotMatch.cpp:34-35` | refuses `USE_SPAWN_GROUPS` | PS spawn-group path |
| `BotMatch.cpp:40` | fixed `InstantAction.lua` | `mode->AttachScript()` |
| `BotMatch.cpp:76-87` | IA-only `Unregister`/score keys | `mode->Unregister`, `mode->ScoreKeys` |
| `BotManager.cpp:230-233` | `CanRunLocalIA` gate | `CanRunLocalBots(mode)` (PS allowed locally, still `sv_bind=127.0.0.1`, `sv_lanonly=1`) |
| `BotManager.cpp:246-270` | single spawn list | `mode->ChooseSpawn` / team-filtered `GetSpawnLocation` |
| `BotManager.cpp:304-325` | no team slot | allocate slot/team before spawn |
| `BotManager.cpp:510-516` | `WatchCommand` IA only | `mode->CanInspect` / `BotSpectator` |
| `BotMoveTrace.cpp:17` | IA string | `mode->IsParticipant` |
| `CryMPEngineAdapter.cpp:19` | IA gate | allow PS for bots, keep export gate separate |
| `BotInstance.cpp:68` | `BotMatch::Register` | `mode->RegisterParticipant` |
| `BotInstance.cpp:143-145` | `CanRunLocalIA` | `CanRunLocalBots(mode)` |
| `BotInstance.cpp:288,371` | channel-0 checks remain | keep as bot identity, not mode identity |
| `BotManager.cpp:196,753,759` | human vs bot channel logic | keep; mode only decides participation |
| `BotManager.cpp:780` | revive uses `GetTeam(id)` | use `mode->TeamOf(id)` or `GetTeam` after T-0 |
| `BotDemoRecorder.cpp:81` | bot-or-channel filter | keep; no mode branch |

`CryMPEngineAdapter::LocalIAGate` (`CryMPEngineAdapter.h:9-13`) currently defaults `allowPowerStruggle=false`;
`CanRunLocalExport` already passes `true` (`BotManager.cpp:236-238`). T-0 adds `CanRunLocalBots` with the same local
bind/lan/opt-in checks, but not the export-only restriction. It must still refuse a remote/dedicated public server.

## 5. Spawn ordering and the actor-before-team problem

`GetSpawnLocation` needs the actor id to read `GetTeam(id)` (`GameRules.cpp:2473`), but `BotManager::AddWith` chooses
the position before creating the actor (`:331-339`). T-0 must reorder:

```
slot = mode->AllocateSlot(number)                 // team known here
provisional = mode->FirstLocation(slot.team)       // GetFirstSpawnLocation(team,0)
actor = CreateServerActor(provisional)             // channel 0, IsPlayer=false
mode->RegisterParticipant(actor, slot)
mode->ApplyTeam(actor, slot.team)                  // SetTeam + SetPlayerSpawnGroup
spawn = mode->ChooseSpawn(actor, team, avoid)      // team-filtered now
mode->PlaceAtSpawn(actor, spawn)                   // Revive/teleport on server
mode->Equip(actor, slot)                          // IA EquipPlayer / PS buy/loadout
```

`PlaceAtSpawn` should use the native revive path (`GameRules::RevivePlayer`, `GameRules.cpp:1003-1053`) or
`CActor::NetReviveAt`/`RevivePlayerInVehicle` if the actor already has a position; do not write `SetWorldPos` directly
and do not create a second actor. For PS, a vehicle spawn uses `RevivePlayerInVehicle` (`GameRules.cpp:1055-1106`).

## 6. Tests (engine-free, plus one server probe)

`Tests/BotGameModeTests.cpp`:

1. IA 6 bots: all team 0, no `SetTeam`, single spawn list, no vehicle support.
2. PS 6/8 bots: `TeamCount=2`, allocation 3/3 or 4/4, never 5/team; free slot after removal rebalances; no enemy
   spawn returned when `includeNeutral=false`.
3. PS `USE_SPAWN_GROUPS`: `SetPlayerSpawnGroup` called with the team default, invalid/missing group falls back to
   `GetFirstSpawnLocation(team,0)`, never a fabricated channel.
4. Score keys: IA/PS register and unregister only their own keys; two bots on different teams both read their own
   `GetTeam`; `GetChannelTeam(0)` is never used by bot logic.
5. Ordering: `SetTeam` precedes `ChooseSpawn`; `PlaceAtSpawn` runs before AI start; no two bots get the same spawn when
   safe alternatives exist.

Server probes (after T-0, not in the units):

- `ps_spawn_team`: 3 bots tan + 3 black; each `GetTeam` matches, no bot within 5 m of the enemy HQ; log spawn ids.
- `ps_revive_team`: kill/revive one bot; team and spawn group survive; no channel/crash.
- `ps_register`: score keys visible in the scoreboard/`GetSynchedEntityValue`, no `IA registration refused`.
- `ia_regression`: same 6-bot IA run as before; kills/stalls unchanged within noise.

## 7. Out of scope for T-0

Buying, capture utility, vehicle driving and PS commander orders are T-1/T-9/T-10 follow-ups. T-0 only makes the bot
a legal team participant with a team-safe spawn; `PS_MIN.md` defines the first 5 minutes, `VEHICLES.md` the vehicle
entry/exit contract.
