# WarsBots architecture review (2026-10-23)

Reviewed: `warsbots/feature/ia-participants` @ `6718af16` (BotManager, BotSpawner, BotBrain,
BotMovement, BotLocomotion, BotCombat, BotSuit, BotWeapons, BotMatch, adapter, Player/GameRules/
ShotValidator/HUD hooks, `Pak/Scripts/WarsBots/InstantAction.lua`). ~1950 lines of bot code.

## Done well (keep)

- **Engine boundary.** No fake INetChannel, no global `IsPlayer()` change, native ActorSystem
  `CreateActor(0)` + native `BindToNetwork`. Hooks in Player/GameRules/ShotValidator are one-line
  `Owns(id)` guards behind `WARSBOTS_EXPERIMENTAL_SPAWNER`. Upstream merge risk is low.
- **Identity guard.** EntityId + cached `IEntity*` compared, never dereferenced. Correct.
- **Intent/executor split.** `BotIntent` is a value; BotLocomotion/BotSuit execute it through
  native movement, sprint, suit energy and jump. No teleport, no HP writes.
- **Shot validation.** Accepts only a real, non-remote server projectile of an owned weapon.
- **IA Lua wrapper.** Instance-local overrides; native human scoring runs once, bot side is added
  only if native did not change it. Logic reviewed and correct for bot-vs-human, human-vs-bot,
  bot-vs-bot and suicide.
- **Per-brain state.** BotBrain already owned blackboard, combat, locomotion, suit: a natural
  BotInstance core.

## Temporary workarounds

| Item | Note |
|---|---|
| Weapon fallback `GiveTestSCAR` | OK for testing, logged; not a match behaviour |
| Local patrol (golden-angle legs) | Not navigation; acceptable until P5 |
| `LogScores` hard-coded keys 100/101 | Match IA constants today; read from Lua if they diverge |
| Loopback-only runtime gate | Correct safety posture; must stay until network QA |

## Problems, severity, fix

| Sev | Problem | Fix / status |
|---|---|---|
| **P0 blocker** | `Script::Call(pSS, table, "Method", arg)` overload does not exist (C2660, BotMatch.cpp:41/46) | `Script::CallMethod(table, "Method", arg)` (pushes `self`). **Fixed `6a8c241c`**, not yet compiled |
| P1 | Registration set `warsBotParticipant=true` before `ResetScore`; failure left a half-registered bot | Order swapped (`6a8c241c`) |
| **P1 multi-bot** | `BotSpawner` = one `m_entityId` + one `m_brain`; hard-coded name `WarsBot_01`; ~15 call sites use `GetSpawner().Owns()` | BotManager registry + BotInstance (**`a8a56583`**) |
| **P1 bot-vs-bot** | `Enemy()` required `GetChannelId()!=0` **and** a live `INetChannel`: bots could never target bots | `BotTargeting::Classify` (human / registry bot), team-aware (**`a8a56583`**) |
| P2 | Initial spawn always `GetFirstSpawnLocation()`: N bots would stack in one point | Shared `ChooseSpawnLocation` (native proximity + body sweep) for spawn and revive |
| P2 | Identical timers (strafe period, aim noise `sin(now)`, patrol) → lockstep bots | Per-slot phase, staggered 10 Hz ticks, profile-driven timings |
| P2 | Headshot: incoming 26 → health −1067 | Not fixed. Hypothesis below; needs `SinglePlayer.lua` from game paks |
| P3 | No per-bot profiling | Update time, ray and scan counters (`wb_status`, `wb_debug 1`) |
| P3 | Combat: single 80 m nearest-visible scan, no memory beyond 3 s, no cover | P4 work; not blocking 6 bots |
| P3 | Movement: local avoidance only; enclosed spawns redirect strafes | P5 navigation |
| Risk | Both client and server must run experimental builds (bot serialization groups) | Unchanged; stock-client compatibility not claimed |

## Headshot −1067 hypothesis (unverified)

`InstantAction.DamagePlayerToPlayer.head = 2.25`. Before the IA wrapper the bot was not
`IsPlayer()`, so stock `GetDamageTable` chose the **AI→player / player→AI** table inherited from
`SinglePlayer.lua` (game pak, not in this repo). Those SP tables carry much larger head multipliers.
~1167 damage / 26 ≈ 45× → consistent with an SP head multiplier plus helmet/kevlar factors.
The wrapper makes bot↔human and bot↔bot use `DamagePlayerToPlayer`. **To confirm:** extract
`Scripts/GameRules/SinglePlayer.lua` from `Game/Scripts.pak`, read `DamageAIToPlayer` /
`DamagePlayerToAI`, then retest one headshot with the IA build. No multiplier was changed.

## What blocks what

- **Several bots:** single spawner, single name, spawn stacking → resolved in `a8a56583` (uncompiled).
- **Strong AI:** no navigation graph, no cover model, no target memory, no prediction. P4/P5.
- **Network compatibility:** experimental serialization in both EXEs; two simultaneous clients never
  tested. Bot count does not change the protocol, only replication volume (6 × Player).

## Deferred on purpose

CryAI integration (no proven nav data on IA maps), Power Struggle, ML, threading. Bot decisions
run on the main thread because entity/physics APIs used here are not documented thread-safe.
