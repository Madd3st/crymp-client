# DS-085 / C146: lower-exit diagnosis and the minimum coherent batch

Inputs: Codex `Docs/Coordination/C146_VIDEO_ANALYSIS.md|json` (X235, weapon-goals `31dbe5b7`), the archive
`C146-native-Shore-b06681ea-20261028-151609`, and source `b06681ea` (reviewed `dd68f2ce`). Offline only: no game,
build, install or triage write. My trace numbers below are recomputed from the archived client/server `.bots.log`
(999 client / 1003 server `MOVE TRACE` rows), independent of the video.

## 1. Two corrections/additions to the pushed report

**(a) The pool rim does not read 99.5.** At the entry the trace reports a **local plane `waterZ = 89.37`** for three
samples while the player is still `swim=0`: t=35.40 `(2123.19,1884.02,89.58)` `rel=1.50`; t=35.90 `(2121.27,1884.27,88.77)`
`rel=0.61`. Only after `swim=1` (t=36.15, `z=87.81`) does `waterZ` switch to the ocean `99.50`, `relativeWater` become
the `-5.00` sentinel and the underwater timer restart (`-4.39 -> 0.09`). So: **dry deck 90.125 (sentinel `-1e6`) ->
local pool surface 89.37 -> ocean plane 99.5 + sentinel when submerged**. A single plane (either one) misrepresents
the room; that strengthens the report's conclusion but changes the numbers a bake would need.

**(b) The same native session contains a human upper Strength jump - outside the 73 s clip.** The pushed report stops
at t=148.9 (ladder exit). The client trace continues to t=300.1 and contains:

| client t | measured |
| --- | --- |
| 148.64-154.6 | deck `99.54-99.75`, walk |
| 154.90-155.40 | walkable ramp `99.75 -> 103.34` at `x 2117.7 -> 2114.8`, `grounded=1`, `vz +4.66/+4.48` |
| 155.6-156.6 | walk `103.13` west along `y~1883.3` to `(2107.373,1883.286)` |
| **156.64** | take-off `(2107.373,1883.286,103.125)`, `mode=1` **Strength**, energy `172.5 -> 105.1`, `sprint=0` |
| 157.39 | apex `z=106.682` = **+3.557 m** |
| 158.15 | land `(2102.234,1882.349,106.017)` = **rise 2.892**, horizontal 5.22, flight 1.50, `vh` 3.75 first step / 3.48 average |
| 158.4-159.4 | walks the **ring 106.0-106.1** |
| 159.65-161.40 | second (normal) jump, falls to the sea `(2120.06,1871.78,100.36)`, **HP 100->80** fall damage |
| 163-236 | boat west/north, then shore |

Consequences: the human **did** leave by an upper Strength jump - the west counterpart of the E223 link
(`2110.56,1885.58,103.18 -> 2113.98,1885.51,106.07`, rise 2.890) - with **apex 3.557, not 4.7**, and take-off
**3.5-3.75 m/s, not sprint 6.75**. So "upper Strength jump" is no longer an unobserved hypothesis for the *human*
route; what remains unobserved is the **bot's** execution. Full numbers: `C146_HUMAN_ROUTE_RESULT.md`.

## 2. Code inspection (b06681ea): what a bot can and cannot do in this water

* **Swim admission is already gated, not blocked**: `BotBlackboard.cpp:22-27` sets `waterAllowed = BotWaterAllowedFor(actor)`,
  `waterMovement = waterAllowed && swimming`, and `canMove` ends with `(waterAllowed || !swimming)`. So the old
  "ShouldSwim -> canMove false" blocker is gone; `BotMovement.cpp:75` returns `true` from `SafeStep` in water and
  `BotLocomotion.cpp:131` forces `STANCE_SWIM`.
* **Vertical swim intent does not exist for bots**. The move request itself is horizontal (`BotMovement.cpp:88`
  zeroes `direction.z`; arrival is 2-D at `:92`). Native up/down is `ACTION_JUMP`/`ACTION_CROUCH`
  (`PlayerMovement.cpp:796-802`), the automatic float-up is commented out (`:776-780`), and bots refuse jump while
  swimming (`BotLocomotion.cpp:111`). The only remaining vertical component is view-space forward
  (`PlayerMovement.cpp:834-839`), i.e. aim pitch - and the path look policy clamps pitch to **+-20 deg**
  (`BotLookPolicy.h:119`). A bot therefore cannot deliberately descend under the hull along a pool-floor path; it
  would swim at the surface (or drift down only as far as -20 deg allows) and the native surface-snap would fight it.
* **Oxygen is not the blocker**: `UpdateUWBreathing` already runs for the gated owned bot (`Player.cpp:3059-3068`), and
  the drowning delay is **60 s** (`:3076`), while the human's dive was 8.8 s and took no damage.
* **Vehicle vs infantry is separable from telemetry**: at t=47-75 and 218-236 `speed=0` while world position moves
  9-19 m/s with `grounded=0`, `fly=0/1`; on foot the same session moves at 6.7-9.9 m/s with `grounded=1` and a real
  `speed`. So a future classifier should use `speed` vs position delta, not `ShouldSwim`.

## 3. Geometry/topology facts

* Baked and usable: deck `103.1`, ring `106.03-106.05` (`DS084`), sea water area at `99.5` (cost 4).
* Not baked: the dry lower deck `90.125`, the pool surface `89.37`, the pool floor `83.66`, and the under-hull
  opening at `(2092.6,1871.1,100.1)` - all below the mesh's `zmin 99.5`, which is why the 12 lower US spawns are
  excluded. The `relativeWater=-5` sentinel inside means the engine itself reports no usable plane for that volume.
* The shaft ladder is real and native (`x=2116.125,y=1880.625`, `z 90.31->99.54`, `1.00 m/s`, 9.25 s) - telemetry,
  not video, and I agree with "no blind timer detach".

## 4. Feasibility verdict (needs the owner's scope decision, not a patch)

* **Upper Strength route: feasible now.** Geometry baked, one reviewed link authorises a 2.890 m rise
  (`BotJumpLink.h:58,94` allows up to 3.72), the human performed the same move at the same height, and the ring ->
  sea part exists in the candidate graph (`upperUS -> sea 11 corners / 1 Strength`). The only open variable is the
  executor's forced Sprint in `Act` (`BotBrain.cpp:1465`) vs the human's 3.5-3.75 m/s.
* **Lower pool route: not feasible without a scope decision.** It needs (a) a *local* sub-99.5 bake of the hull
  interior that must not expose the open seabed, (b) a dive-intent policy (pitch/stance) with its own oracle, and
  (c) acceptance that a bot may surface instead of diving. That is new data + new movement scope; nothing in the
  recording proves a bot can follow it. Do not approve it on the strength of the human clip.

## 5. Minimum coherent batch (existing features only) + one shared acceptance

1. **Data, no code**: install the reviewed candidate (`E2235993` navmesh + one-row `Shore.links.txt`, fnv64
   `c86c5b0a05b1fd89` / `68848bc62ed83f83`, no manifest or a matching one - `DS084_INSTALL_PRECONDITION.md`),
   with hash/restore of the navmesh dir.
2. **Code only if step 3 fails**: `3ec91e44` (cap band 2.5-4.0). With the measured apex the east link needs
   2.81 m/s; at 6.75 the arc lands ~4.8 m past `b` (> `landRadius 3.0`). Do not apply pre-emptively, and drop the
   4.7 term from the acceptance argument.
3. **One shared scenario** (one <=120 s native US2, `wb_bot_navmesh 1`, `wb_bot_jump_links 1`, `wb_ps_capture 1`,
   doors/vehicles off): expect `NAVMESH link kinds 1/1` + `links ON`, `LINK start kind=strength` at the 103.1 deck,
   then either `LINK done` + ring walk (z 106) + water entry (`swim=1`, `waterZ 99.5`) + swim toward a shore capture
   target, or a named failure (`missed_landing`/`no_takeoff`/`no_energy`/`too_high`). Same run also grades the
   post-ring swim, so no separate water-only test is needed.
4. **Offline oracles**: existing `BotJumpLinkTests` (rise/link-speed cases), `BotWaterPolicyTests`, plus a link-live
   check on the installed files (the `NAVMESH link kinds` line is the runtime oracle; a wrong row is red *before*
   any take-off). A dive/vertical-intent oracle is needed only if the lower route is ever approved.
5. **Exact missing observation**: the bot's own `LINK start -> done|failed` on the new data, with its landing
   position. Everything else (human jump, ladder, pool, boat, water plane) is measured.

No teleport/forced underwater link, no seabed exposure, no boat AI implied. B1 `0b39bdb6` and the candidate stay
staged; `3ec91e44` stays held until step 3 actually fails.
