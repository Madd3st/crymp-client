# Why bots never Strength-jump onto roofs / over fences (DS-033)

Date: 2026-10-26. Read-only at `claude-sprint` `2ffc95fa`. Logs: C-091 (owner playtest 2), C-106 (owner playtest 3),
C-098 (15 bots, `path_progress=1`). Offline: `Docs/Research/DeepSeek/proto/nav_links_probe.cpp` on the real
`Steelmill.navmesh` + `Steelmill.links.txt`.

## Verdict

Four causes, in order of impact:

1. **No navmesh on the owner's playtests.** C-091 and C-106 both log `NAVMESH none` and fall back to the old
   `BotNav` graph; that graph has no links, so **0 `LINK` lines in 25/36 minutes**. Nothing about Strength links can
   work until the navmesh files are where the game expects them.
2. **A Strength link is downgraded to a Normal jump when energy < 70.** `ApplySuitPolicy` runs after the link
   executor and line 1316 turns a Strength jump into a Normal one if the policy did not choose Strength; a Normal
   jump cannot cover a +4 m link, so it falls short (`missed_landing`). 27 of 49 failures are exactly that.
3. **Suit-switch cooldown vs the link window.** `BotSuit::Apply` refuses a non-Defense switch for 2 s
   (`BotSuit.cpp:41-43`); `BotJumpLink` waits in `Act` only 2 s (`BotJumpLink.h:53`). A Strength link started just
   after a suit switch gets `no_takeoff` even with full energy (6 cases).
4. **Bad baked links.** Some `strength` links are flat/level gaps, e.g.
   `(1298.1,1343.6,142.6) → (1289.4,1343.5,142.7)`, and the bot lands at z=136.0 — it jumped off the upper floor.
   Two failures ban the link (`BanLink`), and enough bans disconnect the roof route.

`wb_bot_jump_sanity` is **not** the cause: `intent.linkJump` bypasses `JumpSpace`
(`BotLocomotion.cpp:114`), so sanity never vetoes a link jump. That is also why bad links are attempted at all.

## Evidence

### 1. Navmesh absent → zero links

| Log | Line | Meaning |
| --- | --- | --- |
| C-091 | `0.22 … NAVMESH none for Multiplayer/IA/Steelmill (expected %LOCALAPPDATA%\WarsBots\Navmesh\Steelmill.navmesh); BotNav graph stays in use.` | no mesh, no links |
| C-106 | same line at `0.22` | owner playtest 3 same state |
| C-098 | `0.24 … NAVMESH loaded … 9333 polys, 924 parts, 1133 links` / `link kinds 1133/1133` / `links ON: 1133 links, 626 parts` | links actually available |

C-091: `LINK` occurrences = **0**. C-106: **0**. C-098: 152 starts / 89 done / 49 failed / 14 unresolved. So the
owner's "never" is literally true on his machine, while the code path works in the 15-bot run.

### 2. Strength links fail most

| Kind | Starts | Failures | Failure share |
| --- | ---: | ---: | ---: |
| strength | 71 | 35 | 49 % |
| jump | 55 | 6 | 11 % |
| drop | 26 | 8 | 31 % |

Failure reasons (`C-098`): `strength/missed_landing` 27, `strength/no_takeoff` 6, `jump/missed_landing` 6,
`drop/no_takeoff` 6, `drop/missed_landing` 2, `strength/no_energy` 2.

### 3. The policy can turn a Strength link into a Normal jump

`BotJumpLink` sets `o.jump = Strength` and `o.wantStrength = true` (`BotJumpLink.h:128,140`). Then
`ApplyLinkExecutor` writes `m_intent.jump = Strength` and `suitMode = STRENGTH` (`BotBrain.cpp:1098-1100`). In the same
tick `ExecuteIntent` calls `ApplySuitPolicy` (`BotBrain.cpp:1010`), whose rule is:

```
else if (in.wantStrengthJump && in.energy >= 70 && !in.airborne) { want = Strength; }
```

If `energy < 70` (or the bot is airborne), the policy returns Armor/Speed, and line 1316:

```
if (d.mode != SuitMode::Strength && m_intent.jump == BotJump::Strength) WBI(jump, BotJump::Normal);
```

Then `BotLocomotion::Execute` sees `strength=false` and a Normal jump; for a +4 m link it lands short. C-098 examples:
`#11 LINK start strength … energy=32`, `#14 … energy=10`, `#3 … energy=20`, followed by `missed_landing` at z 136-138.
Even when energy is ≥70, `BotSuit::Apply` refuses if `state.nextSuitChange` is within 2 s (`BotSuit.cpp:41`), so the
mode stays Armor; `Execute` refuses with `jump_strength_mode_or_energy` (`BotLocomotion.cpp:112`) and the link times out
(`no_takeoff`).

### 4. Link geometry / ban cascade

- Flat `strength` link `(1298.1,1343.6,142.6) → (1289.4,1343.5,142.7)` appears in the C-098 log; the bot lands at
  z=136.00 (`LINK failed … missed_landing at=(1289.58,1349.79,136.00)`). The link is a level gap on the upper floor,
  not a jump up; the executor treats it as a running jump and runs off the edge.
- After two failures `NoteLinkFailure`/`BanLink` removes the poly (`BotNavMeshCore.cpp:304-309`); the roof component
  loses links one by one. The DSG1 route’s key link
  `(1354.96,1347.60,147.82) → (1351.94,1351.98,152.04)` is attempted six times (C-098 t=394, 466, 481, 491, 503, 598)
  with energy 200/141/57/177/45/56; the low-energy attempts are the ones most likely to be downgraded.
- The baker’s capsule check is not the native jump: a link can be “geometrically connected” and still not match the
  Strength impulse/forward speed.

### 5. Paths exist — the roof is not disconnected in the bake

`proto/nav_links_probe` on the real Steelmill data:

```
mesh polys=10466 components=626 links=1133 kinds=1133 mismatches=0 mask=0xe
project DSG1=1 (1307.89,1330.67,157.94) Gauss=1 (1225.44,1332.10,148.94)
SUMMARY DSG1 ok=34/34 withLinks=23 totalLinks=54
SUMMARY Gauss ok=34/34 withLinks=34 totalLinks=174
```

- All 34 spawn points reach DSG1 and Gauss when all link kinds are enabled.
- 23/34 DSG1 routes use at least one link (54 total); **11/34 are walk-only**, so a ground route to the roof exists
  without any Strength jump.
- DSG1 item at z=159.11 projects to z=157.94 (1.17 m above the mesh, on the roof surface), so the goal is valid.
- The DSG1 routes’ links top out at z≈152 (the `1355→1351.9,152` Strength link), then walk to 157.94. The Gauss
  routes use four Strength links (174 total), all at z≈136-149.

Conclusion: the **pathfinder and bake are not the blocker**; the executor/policy is. The only bake issue is link
quality (flat links, native-arc mismatch), not missing connectivity.

### 6. `ApplyLinkExecutor` start conditions

- Requires `m_meshPathActive`, `m_intent.move`, `m_intent.combat != Fire` (`BotBrain.cpp:1058`).
- The link take-off must be within 2 m and |dz| < 2 m (`:1062`); the path follower stops 0.8 m short of corners
  (DS-012), so the bot often starts the link from a near-stop, not a run-up.
- It only looks at `LinkAtCorner(m_pathIndex)`; if the corner index advanced past the link (because the bot reached the
  corner within `arrive`), the link is skipped and the executor never starts.
- In combat the bot fires and the executor is skipped; that is correct (no jump while firing), but it means links are
  only used out of combat.

## Fix design

### A. Make navmesh installation part of the run (P0)

- Playtest script copies `Steelmill.navmesh`, `.links.txt`, `.spots.txt` from `Tools/RecastBake/meshes` (or the build
  output) to `%LOCALAPPDATA%\WarsBots\Navmesh\` before start; verify size/hash.
- If `wb_bot_jump_links==1` and `NAVMESH none`, log at level 0: `NAVMESH MISSING: links disabled; fix the install`.
  Today it is an informational line and the run silently degrades.

### B. Never downgrade a link jump

- Add `bool linkJump` to `SuitInput`; in `ApplySuitPolicy`, if `in.linkJump && in.wantStrengthJump`, force Strength
  **before** the energy check (or skip the policy for that tick).
- Delete/guard line 1316: only downgrade a **non-link** Strength jump.
- Raise the link energy gate to 90 (matching `BotSuit::Apply` with pace off) and wait in `Prepare` until
  `mode==STRENGTH` and `now >= nextSuitChange`; do not start `Act` before that.
- Extend `BotJumpLink` `noTakeoff` 2.0 → 3.5 s and `total` 9 → 12 s (covers the 2 s suit cooldown + run-up).

### C. Start the link running, not standing

- In `ApplyLinkExecutor`, begin when the bot is within 3.5 m of `a` **and** `speed > 3 m/s`; if it is slower, use the
  existing `Prepare`/`RunUp` but place the run-up point on the path side, not behind a wall.
- Make the path follower treat a link corner as pass-through (do not `arrive` at `a`); the executor owns the last
  3.5 m.
- Check `LinkAtCorner(m_pathIndex)` and `m_pathIndex+1` (the corner may advance in the same tick).

### D. Link quality gate

- Offline: reject `strength` links with `|dz| < 0.8` and horizontal gap > 3.5 m (flat gaps are not jumps), and links
  whose native jump arc (run 6 m/s, Strength impulse) does not reach `b ± 1.5 m`. Re-bake.
- In-game: log `link id` in `LINK start/failed`; ban a Strength link after **1** `missed_landing` (today 2).
- Prefer a walk-only route for DSG1 when `FindPath` with mask 0 succeeds; do not force a link route.

### E. Fallback

- If the link path fails twice for the same goal, call `FindPath` with `mask=0` to the nearest walkable point within
  3 m of the goal; 11/34 spawns have such a route. This prevents the roof from becoming unreachable after bans.

## Probe fixtures

`Docs/Research/DeepSeek/proto/probes/roof_strength.json` and `fence_hop.json` (same schema as `Tools/Probes`):

```json
{"schema":1,"id":"roof_strength","map":"multiplayer/ia/steelmill","duration_s":75,"repeat":3,
 "why":"DS-033: C-098 never completed the (1355,1347.6,147.8)->(1351.9,1352,152) Strength link; owner playtest 3 had no navmesh",
 "cvars":{"wb_bot_navmesh":1,"wb_bot_jump_links":1,"wb_bot_suit_policy":1,"wb_bot_weapon_rank":1,"wb_bot_vantage":1},
 "setup":["wb_add 1 elite marksman","wb_teleport 1 1316.9 1339.0 138.8"],
 "assert":[{"metric":"link:strength:done","op":"ge","value":1},
           {"metric":"landing_z","op":"ge","value":157.0},
           {"metric":"pickup:DSG1","op":"le_seconds","value":60},
           {"metric":"link:failed","op":"eq","value":0}]}
```

```json
{"schema":1,"id":"fence_hop","map":"multiplayer/ia/steelmill","duration_s":30,"repeat":3,
 "why":"DS-033: obstacle/link jump over a fence, not only a roof; C-091 had no links at all",
 "cvars":{"wb_bot_navmesh":1,"wb_bot_jump_links":1,"wb_bot_jump_sanity":1},
 "setup":["wb_add 1 veteran assault","wb_teleport 1 1283.9 1329.6 136.1","wb_bot_gait run"],
 "assert":[{"metric":"link:jump:done","op":"ge","value":1},
           {"metric":"landing_z","op":"ge","value":136.9},
           {"metric":"link:failed","op":"eq","value":0}]}
```

The runner needs `link:<kind>:done` / `link:failed` counters and `landing_z` from `LINK done/failed` lines. Today it
has `LINK start/done/failed` but no per-probe counters; add them to the probe summary.

## What not to claim

- C-091/C-106 do **not** prove that link logic is broken; they prove the navmesh was not installed.
- C-098 proves links execute, but 35 % fail and Strength is the worst kind.
- The roof is reachable (34/34 paths, 11 walk-only); “unreachable DSG1” is a goal-drop/executor symptom, not a bake
  connectivity hole.
- `wb_bot_jump_sanity` is not the villain; it does not touch link jumps.
