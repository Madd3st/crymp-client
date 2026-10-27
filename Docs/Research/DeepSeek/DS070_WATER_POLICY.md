# DS-070: one gated PS water policy (native oxygen included)

Base `2e0a6641`, compatible with `ds067d`; patch `patches/ds070_water_policy.patch`. No build, no game. The earlier
`ds067e` is superseded: it enabled water for every WarsBot and removed both `Player.cpp` hunks, which left the bot
with no native oxygen at all.

## The gate (single source of truth)

`BotWaterPolicy.h` holds a pure facts struct and one predicate; `BotWaterPolicy.cpp` fills it once from the engine:

| Fact | Source | Why |
| --- | --- | --- |
| `experimentalBuild` | `WARSBOTS_EXPERIMENTAL_SPAWNER` | normal DLL is untouched |
| `multiplayer` | `gEnv->bMultiplayer` | server-side movement only |
| `powerStruggle` | `BotMatch::IsPowerStruggle(*rules)` | IA must keep the old behaviour |
| `owned` | `BotManager::Owns(entityId)` | client replicas and foreign actors never qualify |
| `botActor` | `CPlayer::IsWarsBotActor()` | channelId-0 actor only, never a human/stock AI |
| `captureEnabled` | `BotConfig::psCapture == 1` | the existing PS switch, no new `wb_*` |
| `navMeshEnabled` | `BotConfig::botNavmesh == 1` | water movement exists only with the Shore mesh |
| `hasNanoSuit` | `GetNanoSuit() != nullptr` | the native oxygen path dereferences the suit |

`BotWaterAllowed` requires **all** of them; one false field disables `canMove`, the ground-ray skip, the trap change
and the oxygen path together. OFF/IA/human/unowned therefore behave exactly as before.

## Patch shape

* `BotBlackboard.cpp:19-27`: `waterAllowed=BotWaterAllowedFor(actor)`, `swimming=ShouldSwim()` (recorded regardless),
  `waterMovement=waterAllowed&&swimming`; `canMove` drops only `!ShouldSwim`, replaced by `(waterAllowed || !swimming)`.
* `BotMovement.cpp:55-79`: the `waterMovement` parameter skips **only** the downward ground ray after the nine
  body/cliff rays; a wall, quay or held item still refuses, and a dry cliff takes the unchanged branch. `Move` forwards
  the same flag to the main probe and the fan.
* `BotLocomotion.cpp:107-137`: jump refused with `jump_swimming`, sprint already requires `grounded`, and
  `strafe=false` while in water (ProcessSwimming applies the request in view space, `PlayerMovement.cpp:835-836`, and
  the view follows the aim when strafing, `Player.cpp:5085-5090`).
* `BotBrain.cpp:1235`: the trap watchdog receives `canMove && !waterMovement`, so a swimmer is never a land trap and
  never reaches the level-4 `KillPlayer`.
* `Player.cpp:3060`: `UpdateUWBreathing` runs for the gated bot. `IsPlayer()==false` is unchanged for stock AI, IA and
  unowned actors; the singleplayer 30 HP/s branch at `:2961` (`!gEnv->bMultiplayer`) is **not edited**. The bot now
  uses the real timer: 60 s head-under -> 30 s suit-energy drain -> 10 s health drain (`:3067-3112`), same damage path
  as a human, no HP grant and no invulnerability.
* `CMakeLists.txt` adds the new `.cpp`; `Tests/BotWaterPolicyTests.cpp` + `warsbots-ci.yml` register the truth table
  (all-true positive, each of the eight facts flipped off, default struct).

## Deck -> water drop links (inspected, not guessed)

`BotJumpLink` completes only on `grounded` within `landRadius/landDz` (`BotJumpLink.h:158-163`), so a drop that ends
in water cannot finish and times out at 9 s, banning the link. Accepting `swimming` near `b` alone is unsafe: it would
also complete a *land* endpoint from the water 3 m away. The correct check needs the baked target poly's area
(`kWaterArea`), which the runtime has no accessor for. Exact next step, not shipped:
`NavMeshCore::WaterAt(const float* p)` (project, `getTileAndPolyByRef`, `poly->getArea()==4`) and then
`Done` only when `endpointWater && swimming && !grounded && db<=landRadius && |dz|<landDz`; otherwise the timeout stays.
No falling state may ever complete a link.

## Caveats

* Entry flips at `relativeBottomDepth >= 1.3` (`Player.cpp:2832`) with `inWaterTimer` hysteresis (`:2835-2845`); at the
  band 1.3-3.6 m the ground ray is skipped while the seabed is still within reach, so the bot swims instead of wading.
* Exit needs `ShouldSwim()` false on the bank; a quay/step higher than `climb` stays a cliff (body rays unchanged).
* While `waterMovement` the bot walks on the baked water surface at `waterZ`, not on the seabed; that is the same
  surface the navmesh exposes, and physical swimming is native.
* `UpdateUWBreathing` plays the native drowning sound for the gated bot; the suit is guaranteed by the gate.

## Oracles

1. Engine-free (CI): `BotWaterPolicyTests` — positive plus eight single-fact negatives; default struct false.
2. Engine-free movement: dry cliff with `waterMovement=false` must still refuse; `waterMovement=true` + wall ray must
   refuse; `waterMovement=true` + no wall + deep water must accept.
3. Runtime (Codex, after CI + review): PS, one owned bot, `psCapture=1`, `navMesh=1`, a 30 m water route across the
   Shore water: displacement > 0.5 m/s, HP unchanged, no `KillPlayer`, exit within `climb`; then the same run with
   `navMesh=0` -> old freeze; then IA -> old behaviour; then a submerged 60 s case -> native energy/health drain.
4. Deck drop link: after `WaterAt`, a drop ending in water completes at `b`; a land `b` near water still fails.

Apply checks (no build): patch applies alone on `2e0a6641` and after `ds067d`.
