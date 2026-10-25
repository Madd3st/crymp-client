# Review: `claude-sprint` `d0bb72ac..6f941cda` (OC-026)

Author: OpenCode (read-only). Reviewed: `warsbots/feature/claude-sprint` head `6f941cda`, diff
`d0bb72ac..head` under `Code/` (2198 lines, 26 files) plus Codex `52df15c1` (C-044 capped view).
Cross-checked against: `PlayerMovementController.cpp` (`RequestMovement` merge), `Player.cpp`
stance/eye code, Detour **v1.6.0** headers, the on-disk `Steelmill.navmesh`, and the baker
`Tools/RecastBake/recast_bake.cpp` (`steelmill-triage`).
Scope: logic bugs, Windows macro clashes, thread/network risk, breakage with gates OFF.

Verdict: the split (pure-logic headers + thin engine glue) is sound, the gates are honest, and the
default build is untouched. But **five features are silently ineffective or wrong the moment their
gate is switched on** (P1), three more are wrong in a narrower case (P2), and three are nits. None of
these is a blocker for the branch as a *candidate*; all five P1s should be fixed before any of them
is promoted past `wb_* = 0`.

| # | Pri | Gate | Symptom |
| --- | --- | --- | --- |
| 1 | P1 | `wb_bot_navmesh` | "components" are a directed reachability forest -> valid routes rejected, `Reachable()` order-dependent |
| 2 | P1 | `wb_bot_navmesh` | `SafeStep` drops every ray below 1.0 m on mesh paths -> 0.4-1.0 m props invisible |
| 3 | P1 | `wb_bot_perception` | `sinceShot`/`sprintSpeedMode`/`silenced` are never fed -> cloak reveal, gunshot hearing, sprint hearing dead |
| 4 | P1 | `wb_bot_turn_cap` | the 15 deg fire gate cancels shots during a flick while `ShotBlocked()` reports "clear" |
| 5 | P1 | `wb_bot_weapon_rank` | any weapon class outside the 8-entry table is never picked up |
| 6 | P2 | `wb_bot_turn_cap` | eye height hardcoded 1.6 m in view/cap, while the aim model uses `movement.eyePosition` |
| 7 | P2 | `wb_bot_turn_cap` | `SetWarsBotAim` dirties the aspect every frame; the replicated `warsBotViewLimited` is dead on clients |
| 8 | P3 | `wb_bot_navmesh` | a partial path is returned as `"ok"` and cached for 5 s |
| 9 | P3 | `wb_bot_navmesh` | 1.98 MB synchronous load inside the first decision tick; one failure latches for the level |
| 10 | nit | - | `DT_POLYREF64` is not pinned on either side of the container |

---

## 1. P1 - `BuildComponents` is a directed forest, not components

`BotNavMeshCore.cpp:87-103` labels polys by DFS over **outgoing** links only:

```cpp
for (dtPolyRef seed : refs)                       // :87  tile order, poly order (:79-84)
{ if (m_comp[seed] >= 0) continue;                // :89
  const int id = m_components++; ...              // :90-91
  for (unsigned int l = poly->firstLink; ...)     // :97  outgoing links only
    if (it != m_comp.end() && it->second < 0) ... // :100 already-claimed nodes are not revisited
```

Off-mesh connections are stored **one-way** unless the baker sets `DT_OFFMESH_CON_BIDIR`
(`recast_bake.cpp:168` -> `L.bidir ? DT_OFFMESH_CON_BIDIR : 0`), so a drop-down jump `A -> B`
produces exactly one link. If tile order happens to visit `B` first, `B` becomes component 0 and `A`
component 1, and then:

* `FindPath(A -> B)` returns `"other_component"` at `:144` **although `dtNavMeshQuery::findPath`
  would have found `A -> B`** through the off-mesh link;
* `Reachable(A, B)` (`:124-130`) returns false, and `Reachable(B, A)` true - the answer depends on
  the seed order, not on the graph.

Practical hit: `BotWeapons::FindNearby` uses `Reachable` as a hard filter
(`BotWeapons.cpp:119`, `++meshRejected; continue;`), so a weapon lying on the lower floor of a
two-level area can be dropped from consideration because of tile order alone.

Fix: label **weakly connected** components. Build a symmetric adjacency (`u -> v` also adds
`v -> u`, or a union-find over all links), then BFS. Detour's `findPath` still does the real
one-way reachability check, and `Reachable` becomes a conservative cheap prefilter, which is what
the call sites need. `Components()` is also logged as `"parts"` (`BotNavMesh.cpp:47`); after the fix
that number becomes meaningful.

## 2. P1 - `SafeStep` turns off the low rays for the whole mesh path

`BotMovement.cpp:67-71`:

```cpp
for (float side : {-0.3f,0.0f,0.3f})
for (float height : {0.4f,1.1f,1.6f})
    if (!(meshGuided && height < 1.0f) && RayWorldIntersection(...))
        return false;
```

The intent (comment `:65-66`) is to stop a stair riser 1-1.5 m ahead from reading as a wall. The
effect is that **both** the 0.4 m and the 1.1 m ray are skipped for *every* segment of *every* path
that came from the mesh, not just near stairs. The bake contains static geometry only
(`ENGINE_GEOMETRY_EXPORT.md`), so dynamic props - crates, sandbags, low walls, dropped weapons,
vehicles - are not in the navmesh and are now invisible to the bot until the 1.6 m ray hits them.
`meshGuided` also stays on for the final approach to a pickup or a patrol point, which is exactly
where a crate in the way matters.

Fix: keep all three rays and ignore a hit **by geometry, not by mode**: skip a low-ray hit when
`hit.pt.z - feet.z <= walkableClimb (0.5-0.75)` and the surface normal is walkable (riser/step), or
only relax the rays for the segment whose next corner is above the feet. Both keep stairs working
and keep a 0.8 m crate blocking.

## 3. P1 - perception inputs are never fed

`BotCombat.cpp:188` sets the cue only on the exact tick the target fires:

```cpp
if (theirMode && theirMode->IsFiring()) cue.sinceShot=0.f;   // else the default 1e9 (BotPerception.h:43)
```

Consumers need a *duration*, not a flag:

* `BotPerception.h:83` `revealed = sinceShot <= revealAfterShot (1.5 s)` - true for one tick only;
* `:94` muzzle-flash awareness boost needs `sinceShot <= 0.3` - same;
* `:103` `heard = sinceShot <= 0.2 ...` - same. A single-shot rifle will almost never be sampled on
  its firing tick at 10 Hz.

`cue.sprintSpeedMode` and `cue.silenced` are **never assigned anywhere** (`git grep` finds only the
declaration and the readers, `BotPerception.h:45,83,103`), so `sprintHear` is dead, cloak is never
pierced by a sprinting target, and `silencedHear` (12 m) is dead - silenced shots are heard at the
full `gunshotHear` 60 m.

Also `:191-192`: `tracked = candidate==old && m_visible` is OR-ed with `seen.visible`, so a target
that lost LOS this tick is still counted in `m_visibleEnemies` and can be re-selected. The comment
says "kept by LOS", but the code keeps it *despite* LOS; the perception state itself still receives
`losClear=false`, so the two disagree for one tick.

Fix: carry the shot/sprint facts in `TargetCue` as timestamps from the combat side
(`m_lastShotTime[id]`, `mode==NANOMODE_SPEED && sprinting`, `mode->GetType()==...silenced`) and let
`BotPerception` subtract `now`; make `tracked` require `los`.

## 4. P1 - the turn cap and the fire gate disagree

With `wb_bot_turn_cap=1`, `BotMovement::Look` sets the movement controller's fire target to the
**capped** point (`BotMovement.cpp:50`, called from `BotBrain::UpdateTurnCap` -> `m_locomotion.Look`,
`BotBrain.cpp:874`), while `BotCombat::Attack` validates the muzzle ray against the **true** aim
point `m_lastAim` (`BotCombat.cpp:236`, set at `:123`). The shot itself is then gated on

```cpp
if (direction.Dot(state.fireDirection) < 0.96f) { StopFire(); return false; }   // :251  (~15 deg)
```

so during any flick larger than 15 deg the bot silently does not fire, and `m_shotBlocked` is left
`false` (`:244` computed from the true aim). The brain's only recovery for a non-firing bot is
`if (m_combat.ShotBlocked())` -> strafe after 0.4 s (`BotBrain.cpp:602-611`), so it never reacts.
At 900 deg/s (`BotBrain::TurnRate`) a 90 deg target switch costs ~0.1 s of held fire - acceptable -
but the diagnostic and the recovery path are wrong, and `Attack` returns false for a reason nobody
records.

Fix: report the reason. Either make `ShotBlocked()` a small enum (`Cover`/`Turning`/`NoAmmo`) or add
`TurnLimited()` and let the brain wait instead of strafing; keep the 0.96 gate, it is what prevents
shooting the wall the cap is pointing at.

## 5. P1 - `wb_bot_weapon_rank=1` refuses every unknown weapon class

`BotWeaponRank.h:24-33` has eight entries; `FindWeapon` returns `nullptr` for anything else
(`:38-43`), `FightValue` returns `0` (`:48-49`), `PickupValue` returns `0` (`:79-80`), and
`BotWeapons.cpp:131` then does `if (value <= 0.01f) continue;`. So with the gate on, LAW/FGL40 (and
any mod/custom class) is never picked up - including the case where the bot has no primary at all
and the unknown weapon is the only upgrade available.

The inventory side is safe: `BestToHold` returns `-1` when every value is 0 and `SelectForRange`
then does nothing (`BotWeapons.cpp:227-228`), so a bot holding only an unknown weapon keeps it
rather than dropping to nothing.

Fix: unknown class -> neutral fallback value (e.g. `0.35f * base` or "same as current weapon"), or a
`primary` heuristic (`Suitable() && !IsPistol`) so the rank table stays a *ranking* and not an
allow-list.

## 6. P2 - two different eye origins

`BotBrain::AimModelPoint` computes the desired angle from `movement.eyePosition`
(`BotBrain.cpp:892-896`), but `UpdateTurnCap` and `CappedLook` rebuild the look point from
`GetEntity()->GetWorldPos()+Vec3(0,0,1.6f)` (`:858,880,883`), and `CPlayer::ApplyWarsBotView` does
the same (`Player.cpp:4998`). The engine's real eye is stance-dependent and interpolated
(`GetStanceViewOffset`, `Player.cpp:4189,4218`; `:3184-3186`), and the numbers come from the actor
script: stand **1.625 m**, crouch **1.0 m**, prone **0.5 m**, swim 1.5 m, zero-g 0.35 m
(`Pak/CryMP/Scripts/Entities/Actor/Player.lua:86,103,116,130,144`). The hardcoded 1.6 m is fine
standing (2.5 cm) and wrong otherwise.

Because the *view quat* is set from the 1.6 m origin, a crouched bot's shot direction is pitched
down by `(1.6-1.0)/distance`: 0.6 m low at 10 m, 1.1 m low at 10 m prone. The aim model's own
correction cannot compensate, because its output is a world point that is then re-projected from the
wrong origin.

Fix: pass `movement.eyePosition` (already fetched in `AimModelPoint`) into the cap and into
`ApplyWarsBotView`, or store the eye on the player once per frame.

## 7. P2 - per-frame aspect dirty + a replicated field no client reads

`CPlayer::SetWarsBotAim` calls `ChangedNetworkState(eEA_GameServerDynamic)` unconditionally
(`Player.cpp:4991`). With the cap on it runs every frame (`BotBrain.cpp:245`), so the whole 10-value
`WarsBotMotion` group (`Player.cpp:5234-5247`) is resent at frame rate instead of the change-only
10 Hz design from O-025 - ~6x the traffic per bot for an aim point that is already changing.

The new `warsBotViewLimited` member (`:5240`, mid-group) is **dead on the receiving side**:
`ApplyWarsBotView` returns early unless `IsWarsBotActor()` (`:4995`), and `m_warsBotActor` is only
ever set `if (gEnv->bServer)` (`:248`), so the client call from `PlayerRotation.cpp:134` is always a
no-op. The client does use `m_warsBotAim` (`Player.cpp:5042-5044`), but never this flag. So the byte
buys nothing and changes the group layout for no gain.

Fix: drop the field (the view override is server-only by construction) and gate the dirty flag on an
actual aim delta, e.g. `if (!m_warsBotAim.IsEquivalent(aim, 0.01f))` or a 10 Hz accumulator.

## 8. P3 - partial paths are reported as `"ok"`

`BotNavMeshCore.cpp:146-156` asks for 256 polys but 64 corners. Two truncation paths:

* `findPath` exhausts 256 polys -> `:149` sees `polys[npolys-1] != b`, silently retargets the end to
  the last poly's closest point and continues;
* `findStraightPath` exhausts 64 corners -> `DT_BUFFER_TOO_SMALL` is a **detail** bit
  (`DT_STATUS_DETAIL_MASK`), so `dtStatusFailed` is false and `ncorners` is simply capped at 64.

Both end with `reason = "ok"` (`:164`), and `BotBrain::Steer` then caches the path for 5 s
(`BotBrain.cpp:129`) with `m_meshPathActive = true` (`:121`), which also switches off the knee rays
via `m_intent.meshPath` (`:139`). On a long Steelmill route the bot walks to the truncated end and
only re-plans after 5 s.

Fix: treat `DT_BUFFER_TOO_SMALL` and `polys[npolys-1] != b` as `"partial"` and shorten the cache to
the 2 s retry, or size the corner buffer from `npolys` (`ncorners` is bounded by `npolys+1`).

## 9. P3 - synchronous load, permanent latch

`CurrentNavMesh` reads the whole file and builds the Detour mesh inside the first decision tick that
asks for it (`BotNavMesh.cpp:28-48`; Steelmill is 250 tiles / 1.98 MB) - a visible hitch on the first
AI tick. `s.tried = true` is set **before** the load (`:30`), and only a level-name change clears it
(`:27`), so one transient failure (file still being copied, a locked file) disables the navmesh for
the rest of the level with a single `WB_LOG(0, ...)` at `:43-45`.

Fix: latch only on success, or add a retry deadline; load on a worker or during level start.

## 10. Nits

* **`DT_POLYREF64` is not pinned.** The container stores `dtTileRef` at 4 bytes
  (`recast_bake.cpp:591`) and the loader matches (`BotNavMeshCore.cpp:18`, `sizeof(TileHeader)` = 8
  because `dtTileRef` is `unsigned int` without the macro, `DetourNavMesh.h:42-57`). The on-disk
  `Steelmill.navmesh` parses exactly to file size (250 tiles, all `DT_NAVMESH_MAGIC`), so today it is
  consistent - but nothing stops one side from being rebuilt with `DT_POLYREF64`. Add a
  `static_assert(sizeof(dtTileRef)==4)` (or a format version bump) to both.
* **`m_components` is logged as `"parts"`** (`BotNavMesh.cpp:47`) - after §1 it will be true
  components; before, the number is inflated by the directed forest.
* **`CappedLook` allocation-free but duplicated.** `:877-885` repeats the `m_lookValid` branch of
  `UpdateTurnCap`; both build the same `eye + dir * max(2, dist)` point. One helper would keep the
  eye-origin fix (§6) in one place.

---

## Verified OK

* **`BotMovement::Look` merge semantics.** `CPlayerMovementController::RequestMovement` starts from
  `m_state` and only overwrites the fields the request carries
  (`PlayerMovementController.cpp:120,127-136,170-199`), so a look/aim/fire-only request keeps the
  move target, desired speed and pseudo speed set by the previous `Move`. The per-frame `Look` does
  not cancel locomotion.
* **Windows macros.** No new header or `.cpp` declares `near`, `far`, `min`, `max`, `small` or `IN`/
  `OUT` as identifiers; `BotPerception.h` only mentions "near" in comments. `40d175e7` is clean.
* **Gates OFF.** Every new behaviour is behind a `BotConfig` int defaulting to 0
  (`BotConfig.cpp:24-30`); `CurrentNavMesh()` returns `nullptr` at `botNavmesh != 1`
  (`BotNavMesh.cpp:22`); `m_intent.meshPath` is only set when a mesh path was found
  (`BotBrain.cpp:139`); `SafeStep`'s `meshGuided` defaults to false (`BotMovement.h:19`); the new
  headers are pure logic with no engine types. The default build behaves exactly as before.
* **Ownership guards.** `SetWarsBotMotion`, `SetWarsBotAim` and `ApplyWarsBotView` all use
  `g_pGame->GetBotManager().Owns(...)` (`Player.cpp:4975,4987,4996`) - one guard, not two.
* **Container layout.** Verified against the on-disk file, not just the reader (see §10).
* **`BotSuitPolicy`, `BotGoalArbiter`, `BotLookPolicy`, `BotAimModel`.** Pure-logic, no engine
  types, no statics holding level state; the only shared state is `BotBrain`'s per-instance members,
  so there is no thread or level-change hazard beyond §9.
