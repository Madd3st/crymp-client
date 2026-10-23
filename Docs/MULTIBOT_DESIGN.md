# Multi-bot design

```
CGame
 └─ BotManager            listeners, cvars, commands, registry
     ├─ unordered_map<EntityId, unique_ptr<BotInstance>>
     └─ BotInstance (×N, N ≤ wb_maxbots ≤ 16)
          identity (EntityId + IEntity* guard), number, name WarsBot_NN, BotProfile
          lifecycle: Spawn / Remove / Revive / OnEntityDone, profiling
          └─ BotBrain: FSM, BotBlackboard, BotIntent, BotCombat, BotLocomotion(BotMovement), BotSuit
BotTargeting   Kind(actor) = Human | Bot | None ; Classify(self, other) = Enemy | Ally | Dead | ...
BotMatch       IA participant registration (Lua wrapper) — unchanged except Owns()
```

## Rules

- Registry key = EntityId. Display number (1..16) is separate and shown by `wb_list`.
- Nothing mutable is shared between instances. Only profiling counters in BotCombat are global.
- `Owns(id)` is the only ownership question hooks may ask (`BotManager::Owns`).
- During `BotInstance::Spawn` the instance is visible through `m_spawning` so IA registration
  and `CPlayer::Init` (`IsCreatingActor(name)`) see it before it enters the map.
- `ENTITY_EVENT_DONE` only marks the instance expired; the map is swept after the update loop,
  so removal re-entering from `RemoveEntity` never invalidates iteration.
- Level end / reset / disconnect: remove every owned entity, then clear the registry.
- Updates run in display-number order; each brain decides at 10 Hz on a staggered phase.

## Targeting

Human = Player with a live server net channel. Bot = owned by the registry (channel 0 is fine).
Then: self → Self; spectator; dead; hidden or in vehicle → Invalid; same non-zero team → Ally;
otherwise Enemy. IA uses team 0 → free-for-all. TIA/PS teams come from `CGameRules::GetTeam`.

## Profiles

`BotProfile::Make(style, skill)`. Styles cycle by number: universal, assault, marksman, flanker,
defender, hunter. Skill (beginner/normal/veteran/elite) scales reaction delay, aim error and burst
gap. Health, damage and suit energy are never touched. Universal/Normal = `caf1a815` constants.

## Scaling to 12–16

Costs grow linearly: per bot ~2 rays per candidate per 10 Hz tick (LOS eyes/chest) plus muzzle
ray when firing. Target scans are O(N²) in actors. At 16 bots with ~16 actors: ≈ 16×16×2×10 ≈
5k rays/s worst case. If measured cost matters, time-slice scans (not every tick) before threading.
