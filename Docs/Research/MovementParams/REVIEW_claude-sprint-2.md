# Review 2: `claude-sprint` `6f941cda..717853ff` (OC-027)

Author: OpenCode (read-only). Reviewed: `warsbots/feature/claude-sprint` head `717853ff`, commits `1e343d81`,
`979e50a8`, `a9bc600e`, `0470fe7e`, `8214f16a`, `717853ff` (30 files). Cross-checked against the vendored
**Detour v1.6.0** sources (`dtNavMesh::baseOffMeshLinks` / `connectExtOffMeshLinks`), the baker
`Tools/RecastBake/recast_bake.cpp` (`steelmill-triage`), `BotLocomotion.cpp`, and the earlier OC-026 review.
Scope: the jump-link executor, the navmesh connectivity rewrite, client smoothing. Read only.

Verdict: the connectivity rewrite is **correct** - I traced Detour's off-mesh link layout and the Tarjan +
directed-reachability design matches it, including the `no_directed_route` hole from O-026 §8. The executor is
structurally sound but its failure policy is too harsh: three of the four ways it can fail are *timing* artefacts,
and every one of them bans a physically valid link **for the whole level**. Details below; §1.1-§1.3 are the ones
worth fixing before `wb_bot_jump_links` leaves 0.

---

## 1. Jump-link executor (`BotJumpLink.h`, `BotBrain::ApplyLinkExecutor`)

### 1.1 P1 - a good link is banned for the level by a timing artefact

`BanLink` is permanent for the level (`BotNavMeshCore.h:61-62`, implementation `BotNavMeshCore.cpp:244-248`: zeroes
the off-mesh poly's flags, rebuilds the parts, never un-bans). The caller bans on every failure except `NoEnergy`
(`BotBrain.cpp:845`), but two of those failures are reachable without the link being bad:

* **`NoTakeoff` vs the jump cooldown.** `BotLocomotion.cpp:118` sets `state.nextJump = now + (pace ? 0.5f : 3.0f)`
  and `:111` refuses the jump while `now < nextJump`. The Act phase allows only `noTakeoff = 2.0 s`
  (`BotJumpLink.h:53`). With `wb_bot_pace 0` (default is 1, `BotConfig.cpp:14`) a bot that jumped less than 3 s
  before the take-off - obstacle jump, travel jump, a previous link - runs past `a` without jumping and fails with
  `NoTakeoff` after 2 s, which bans the link. The Act phase never looks at `nextJump`.
* **`MissedLanding` vs the land radius.** `landRadius = 3.0 m` / `landDz = 1.5` (`BotJumpLink.h:49-50`) judged
  0.6 s after touchdown (`:150`). A grenade knock, a slide on ice, or landing 3.1 m from `b` (still on the right
  floor) fails the link and bans it.

Both are one-shot events that permanently remove a route from every bot on the level. Fix: count failures per link
ref and ban on the second/third, or ban only `NoTakeoff` when the bot was actually standing still at `a`; and make
`MissedLanding` a soft failure (re-path, no ban) when the landing point is on the same part as `b`.

### 1.2 P1 - the run-up is not verified

`RunUp` (`BotJumpLink.h:115-123`) walks 2.5 m back along `b -> a` and leaves for `Act` when it is within 0.7 m of
the run-up point **or after 1.2 s, whichever comes first** (`:121`). Nothing checks that the run-up actually
happened. If the space behind `a` is a wall, a railing or another drop, `SafeStep`/the cliff ray refuse the
movement and the bot enters Act from wherever it got to - i.e. a short or standing jump, exactly the c062
undershoot (`:7,54`) the run-up was added for. The result is `MissedLanding` -> ban (§1.1).

Fix: distinguish "reached the run-up point" from "gave up"; on give-up either retry the approach or fail with a new
`NoRunUp` reason that is **not** banned.

### 1.3 P1 - the suit overrides the executor after it runs

`ApplyLinkExecutor` is called at `BotBrain.cpp:767`, *before* the legacy suit blocks at `:783-786` and before the
suit policy at `:787`. Only the policy is guarded (`&& !m_link.Active()`), so:

* pace on (default): `:786` `if (jump != Strength && (damaged || combat == Fire)) suitMode = DEFENSE`;
* pace off: `:783-784` the same for `damagedUntil || combat != None || energy < 45`.

During **Prepare** the executor deliberately requests no jump (`BotJumpLink.h:109-113` sets only `wantStrength`),
so `m_intent.jump == None` and the condition `jump != Strength` is true. A Strength link whose bot is shot at, or
sees an enemy, is forced back to DEFENSE every tick; `Ready()` (`BotJumpLink.h:166`) needs
`strengthActive && energy >= 70`, so Prepare can never finish, times out after `prepareMax = 4 s` (`:51,113`) and
fails with `NoEnergy` - which is *not* banned (`BotBrain.cpp:845`) but also not backed off: `:846` clears the path
and sets `m_nextRepath = m_minRepath = 0`, so the next tick re-plans into the same link. Under sustained fire that
is a livelock: re-path -> Prepare -> DEFENSE -> 4 s -> re-path.

Fix: add the same `!m_link.Active()` guard to `:783-786` (the executor already owns `suitMode` there), and add a
per-link cooldown after `NoEnergy`.

Related, same block: `:780` forces Armor while falling (`velocity.z < -3`), which is what a Strength landing wants;
`m_link.Reset()` on death/revive (`BotBrain.cpp:176`) and on trap level 1 (`:726`) are correct, and `Steer`'s
early return (`:110`) means no re-path can pull the path out from under a running link.

### 1.4 P3 - `m_wasAir` is sticky

`Step` sets `m_wasAir = true` on any airborne tick (`BotJumpLink.h:92`) and never clears it. For a Drop link,
`(m_wasAir && m_move == Drop)` (`:101`) jumps straight to Act even 1.9 m from `a` (the start gate is 2 m,
`BotBrain.cpp:817`), and `Act` then sees `!grounded`/a lower z and enters Air (`:131`) - landing far from `b` ->
`MissedLanding` -> ban. Clear it when `grounded` is true again, or require `da < 1.5` for that shortcut.

### 1.5 P3 - the start gate has no fallback

The executor starts only within 2 m / 2 m in z of `a` (`BotBrain.cpp:817`). `Steer` refuses to advance past a link
corner (`:147`), so if the bot reaches the corner but cannot satisfy the gate (e.g. a different floor, a 2.1 m
drop), it keeps steering to `a`, arrives, and stands there - the path never advances until the trap watchdog clears
it (`:726`). A time-limited "cannot start this link" that bans/advances would be safer than an unbounded wait.

---

## 2. Navmesh connectivity (`BotNavMeshCore.cpp`)

### 2.1 Verified correct

* **Detour's off-mesh link layout matches the SCC model.** `baseOffMeshLinks` adds off-mesh -> start and
  start -> off-mesh (`DetourNavMesh.cpp:588-599,602-615`); `connectExtOffMeshLinks` adds off-mesh -> landing
  (`:489-500`) and, only for `DT_OFFMESH_CON_BIDIR`, landing -> off-mesh (`:503-518`). So a one-way drop is exactly
  one directed edge `start -> off-mesh -> landing`, which is what the Tarjan pass (`:112-138`) and the reachability
  bitsets (`:140-152`) assume. O-026 §1 is fixed, and the weak-part optimism is gone.
* **Corner mapping.** `L.corner = i - 1` (`:284`) where `i` is the raw `findStraightPath` corner index and `xyz`
  starts at corner 1 - so `corner` is the index into `m_path`, which is what `LinkAtCorner` (`BotBrain.cpp:147,816`)
  and the Done handler (`:836`) use. `a` = `corners[i]` (off-mesh start), `b` = `corners[i+1]` (landing) matches
  Detour's `DT_STRAIGHTPATH_OFFMESH_CONNECTION` semantics.
* **`no_directed_route`** (`:268`) closes the O-026 §8 hole: a partial path is now a failure, not a silent truncated
  path, and it only fires when the buffer was *not* exhausted (`npolys < 256`).
* **Filter/mask plumbing.** Walkable polys carry flag 1 (`recast_bake.cpp:156`), off-mesh polys start at 2
  (`:169`) and `LoadLinkKinds` rewrites them per kind (`:228`), unknown/lift/ladder get `0x4000` which no mask
  includes; `SetLinkMask` sets `include = 1 | mask` (`:236-241`) so the components are built on exactly the graph
  the query filter uses. `user id = i + 1` (`recast_bake.cpp:169`) matches the `links.txt` line order (`:469`) and
  `kinds[id-1]` (`:226`). Loading order (load -> `SetLinkMask(0)` -> kinds -> `SetLinkMask(want)`,
  `BotNavMesh.cpp:60-72`) leaves the graph walk-only when the gate is off, as documented.

### 2.2 P2 - rebuild cost lands in the decision tick

`BuildComponents` is Tarjan plus, per part, an OR of every successor's `words`-wide bitset (`:146-152`) - roughly
`O(parts * edges * parts/64)`. It runs on every `SetLinkMask` (`:240`) and **on every `BanLink`** (`:246`). At load
that is up to three full builds (`LoadMemory`, `LoadLinkKinds` -> `SetLinkMask`, `CurrentNavMesh` -> `SetLinkMask`)
inside the first decision tick (`BotNavMesh.cpp:30-72`), on top of the synchronous file read from O-026 §9. A
mid-game ban (a failed link, §1.1) then pays the same cost again during play.

Fix: build once per mask change (skip the rebuild in `LoadLinkKinds` when the mask did not change), and rebuild
lazily after a ban (dirty flag + rebuild on the next `FindPath`) rather than inside `BanLink`.

### 2.3 P2 - the links file is not validated against the mesh

`LoadLinkKinds` maps `con.userId` to `kinds[id-1]` and silently treats anything out of range as `-1` (`:226`). A
`links.txt` from a different bake therefore does not fail: every line shifts by one and a *drop* can be labelled
`jump` (or become `0x4000` and be skipped). The log prints `link kinds N/M` (`BotNavMesh.cpp:61`) but nothing
rejects `N != M`. Since a mislabelled kind sends the wrong executor arc into the level and then bans the link, add
`if (int(kinds.size()) != Links()) return 0;` and log it as an error.

### 2.4 Nit - `m_reach` is quadratic

`m_reach.assign(parts * words, 0)` (`:143`) is `parts^2 / 8` bytes: negligible for a few hundred parts, ~50 MB if
a mesh degenerates to ~20k isolated polys. Fine in practice; worth a cap or a note.

---

## 3. Client smoothing (`BotReplicaSmooth.h`, `Player.cpp`)

### 3.1 P2 - it is a lag filter, not interpolation

`Update` approaches the replicated yaw/pitch with a first-order filter (`BotReplicaSmooth.h:40-44`, `Alpha` at
`:61`), i.e. the rendered value is *always* behind the replicated value, by roughly `tau` once the input steps.
With `aimTau = 0.05 s` (`:16`) and a 10 Hz step that is up to 90 deg at the 900 deg/s turn cap, the replica's
shown view - and the fire target the replica feeds into its own `CMovementRequest` (`Player.cpp:5064`) - can sit a
large part of one step behind what the server was aiming at. `snapDist` (`:18,44`) re-bases only the distance, and
the 120 deg snap (`:65`) does not catch a 90 deg flick. Tracers/muzzle direction drawn from the client's weapon
direction therefore lag the real shot.

Fix: either keep a one-step buffer and render 100 ms behind (smooth *and* consistent), or snap yaw/pitch on a fire
event for that bot. A `tau` below one frame is the cheap version.

### 3.2 Verified OK

* Listen-server host: `smooth = !gEnv->bServer && clientSmooth == 1` (`Player.cpp:5015`) - the host always uses the
  raw values, so the authoritative view is never filtered.
* Reset paths: revive (`Player.cpp:4148`) and first replica activation (`:5273`); `else m_warsBotSmooth.Reset()`
  (`:5025`) keeps a stale filter from surviving a cvar toggle.
* `WarsBotViewAim()` returns the raw aim on the server (`:5003-5008`), so `ApplyWarsBotView` (`:5000`) is unchanged
  server-side; the FP spectator (`:1932`) gets the same smoothed aim the third-person view shows - consistent.
* `wb_bot_client_smooth` is registered on clients too (`BotManager::Initialize` from `Game.cpp:351`), so the cvar
  is reachable where it is read.

### 3.3 Nit - pitch has no snap

`m_aimYaw` snaps at >120 deg, `m_aimPitch` never does (`:42-43`); a target straight above/below (a 60-80 deg pitch
step) is smoothed over 50 ms. Harmless visually, but if the aim point is used for the fire target it is the same
class of lag as §3.1.

---

## 4. Other commits in the range

* **Hearing (O-026 P1 #3)**: `cue.speed` and `cue.sprintSpeedMode` are now set from `pe_status_dynamics`
  (`BotCombat.cpp:189-190`), so `sprintHear` and the Speed-through-cloak reveal work. `sinceShot` still only sets
  0 on the firing tick - with hearing now driven by `sprintSpeedMode`/`damagedMe` that is acceptable, but
  `revealAfterShot` and the 0.3 s muzzle-flash awareness remain effectively dead (O-026 §3).
* **Look policy**: `Keep` now uses the live `viewPitch` (`BotLookPolicy.h:106`) and `pathActive` replaces the speed
  test (`:85`); the `Keep` early return in `BotBrain::ApplyLookPolicy` is gone, which is consistent because Keep
  returns the current view.
* **`SafeStep`**: the O-026 §2 note is right - only the 0.4 m ray is skipped, 1.1/1.6 stay. That still hides
  0.4-1.0 m props on mesh paths (crates, sandbags), so the original finding stands in reduced form; a height/slope
  test on the hit would close it without touching stairs.
* **`BotConfig.h:33-35`**: the `botNavmesh` comment got merged into the `jumpLinks` line (`jumpLinks` now carries
  two trailing comments and `botNavmesh` none). Cosmetic, but this is the file people read to learn the gates.
