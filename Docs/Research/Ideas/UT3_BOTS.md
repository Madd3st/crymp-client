# What to take from Unreal Tournament 3 bots (2026-10-25)

Author: Claude. Sources: UDN/UDK docs (AI Overview, Waypoints Technical Guide), Unreal Wiki (Bot Pathing, Bot Mind),
plus knowledge of the public UT2004/UT3 UnrealScript (`UTBot`, `UTSquadAI`, `UTWeapon`). **Ideas only** — Epic's
script is under the UDK/UT EULA, nothing is copied; we re-implement in our own engine-free modules.

## How a UT3 bot is built (short)

| UT3 piece | What it does | Our analogue today |
| --- | --- | --- |
| `WhatToDoNext` → `ExecuteWhatToDoNext` | one decision point; squad gives orders, bot picks fight / hunt / pickup / roam | `BotGoalArbiter` (C-032) |
| `UTSquadAI` | team objectives, who defends / attacks / escorts | — (IA is free-for-all; needed for TIA / Power Struggle) |
| `FindInventoryGoal`, `BotDesireability`, `RatePickup` | pickup value / path distance, knows respawn times of power-ups | `BotWeaponRank` + `PickupCost` (C-022), no respawn memory |
| `GetAIRating`, `SuggestAttackStyle`, `RecommendSplashDamage` | weapon value by range/target, preferred engagement distance | `BotWeaponRank` range curves |
| `PeripheralVision`, `SeePlayer`, `AcquireTime`, `HearNoise`/`MakeNoise` | view cone, time to notice, sound loudness | `BotPerception` (C-037) + hearing (979e50a8) |
| `AdjustAim`, skill 0–7, `Accuracy`, `ReactionTime`, lead for projectiles, splash at feet | human-ish error that shrinks while tracking | `BotAimModel` (C-038): flick + tracking + lead; **no splash-at-feet** |
| Personality: `Aggressiveness`, `StrafingAbility`, `CombatStyle`, `Jumpiness`, `Tactics`, `FavoriteWeapon` | continuous traits per character | discrete `BotStyle` (Marksman / Defender / ...) |
| `ReceiveWarning` / projectile warning → dodge | incoming rocket/grenade/shot → dodge by skill probability | random Strength "dodge" after being hit |
| `Hunting`, `StakeOut` states, `LastSeenPos` | chase last seen position, or hold and fire at it | search via LKP in look policy; no stake-out fire |
| ReachSpecs (walk / jump / ladder / lift) + `SpecialHandling`, `SuggestMovePreparation` on nav points | special moves are annotations on graph edges, executed by the point | Recast off-mesh links (flag 2) — executor C-036 **in progress** |
| Dynamic path costs ("killing zones", e.g. sniper-covered areas) | routes avoid where bots die | hotspots attract; nothing repels |
| `bAdjustSkill` / `AdjustSkill` (UT2004) | skill drifts with the score vs humans | fixed skill per bot |
| Voice/taunts | flavour | — |

## Worth taking (ordered by value / cost for us)

1. **Special moves as link annotations** — confirms C-036 design: each off-mesh link has a kind (drop / jump / Strength /
   lift / ladder) and its own executor (approach → prepare → act → land, timeout → ban link). Cost: in progress.
2. **Incoming-fire warning + dodge** (`ReceiveWarning`): rockets, grenades and a zoomed sniper aimed at the bot →
   sidestep / crouch / Strength jump with skill probability and reaction delay. Projectiles are entities we can see
   server-side. ~1 day, engine-free module + small hook. Big "feels alive" win.
3. **Danger map = learned path costs**: deaths (victim position, killer position) raise a per-poly cost for N minutes;
   Detour supports this via a custom `dtQueryFilter::getCost`. Bots stop running into the same sniper lane. ~1 day.
4. **Continuous personality traits** (aggression, strafing, jumpiness, tactics, favourite weapon) seeded per bot name,
   feeding existing modules (arbiter weights, aim, suit policy, weapon rank). Mostly data. ~0.5 day.
5. **Stake-out / suppress**: lost target recently, ray to last seen position clear → hold the angle and fire short bursts
   at it for 1–2 s (style/aggression dependent) before hunting. ~0.5 day.
6. **Splash aim** for rockets / grenade launcher: aim at feet / nearby surface. ~0.5 day inside `BotAimModel`.
7. **Adaptive skill** (`AdjustSkill`): small drift of reaction/accuracy toward a target human K/D (opt-in cvar). ~0.5 day.
   Owner decision: fair "smart" bots vs. rubber banding.
8. **Pickup respawn memory**: remember when a weapon was taken, return near respawn time. ~0.5 day.
9. Later (team modes): squad AI with objectives — only for TIA / Power Struggle; 1–2 weeks.
10. Fun: taunts / chat lines via server chat on kill/death. ~0.5 day.

## Not worth taking / not applicable

* Waypoint graphs (`PathNode`, `ReachSpec` build) — our baked Recast navmesh is strictly better and already done.
* Translocator, double jump, impact jump — no such movement in Crysis; Strength jump covers it (links already baked).
* UnrealScript latent states as an architecture — our 10 Hz brain + modules is the same idea without the engine.
* Any literal code — license.

## Is it realistic?

Yes for items 1–8: each is a small engine-free module plus a hook, the same pattern as the sprint (default-OFF cvar,
unit tests, batch QA by Codex). Total ≈ 5–6 days of my code + 2–3 test batches. Squad AI is a separate project for team
modes.
