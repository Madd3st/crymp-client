# Snipers still 0/9 on C-121 (DS-048 #1)

Raw: `build/c121-{roof_strength,sniper_reach_dsg1,sniper_reach_gauss}-d6a9a521/` (3 repeats each, base
`d6a9a521`), 15-bot `build/c121-on15-d6a9a521/`. `wb_bot_sniper_reach` was effective 1 in every preflight.

## The goal is chosen now; the route dies on one roof Strength link

DSG1/roof probes: `weapon 98 unreachable; skipping it` at 25.4-35.7 s (the goal was selected, then the progress
timer expired). Every attempt then uses the Strength link
`a=(1332.64,1342.76,142.64) b=(1323.30,1342.71,140.00)`:

| Trial | link outcome | last z | pickup |
| --- | --- | ---: | --- |
| dsg1-1 / roof-1 | `no_takeoff` at (1333.6,1342.4,142.56), twice | 142.6 | none |
| dsg1-2 / roof-2 | `missed_landing` at (1325.4,1342.7,142.56) | 142.6 | none |
| dsg1-3 / roof-3 | `missed_landing` at (1325.6,1342.7,142.48) | 142.5 | none |
| gauss 1-3 | `1236.33,1372.61,136.09 -> 1229.40,1372.76,139.14` then `1229.74 -> 1365.49`: `no_energy`, `no_takeoff`, `missed_landing` | 136-142 | none |

The 15-bot run has the same 0/0 and `too_high: 3`, `no_takeoff: 4`, `missed_landing: 31` globally.

## Why this link cannot work

1. **Take-off is behind a physical block.** The bot reaches `(1333.63,1342.40,142.56)` at 4.3 s and stands there
   with `blocked=1`, speed 0, `gait=run`, for 30 s. `a` is 1.0 m further west, behind the railing/step; the
   navmesh edge is walkable, the collision is not. The old executor waits in Act for `da<=0.6 || along>-0.2` and
   only then jumps, so it times out `no_takeoff` (3.5 s) and retries.
2. **The landing is on a disconnected island.** When it does jump (`missed_landing` trials) it lands at
   `(1325.44,1342.73,142.56)`, which `NavMeshCore::Project` puts on a different component from DSG1
   (`FindPath` -> `other_component`). The link's `b=(1323.30,140.00)` is 2.4 m lower, behind that high roof edge.
3. **No walk alternative is tried.** Offline, with the two roof links disabled, the path from the stuck point to
   DSG1 is a **117.0 m walk** on the DSG1 component (corners go south to `1333.94,1330.50,146.64` and around
   `1353`), and from spawn 141.6 m. The goal timer (20 s) expires at the block before the link is banned, and
   `MarkBadItem(98)` then hides DSG1 for 90 s.

## Patch `ds048.patch` (apply-clean on 27da613c)

- **Data:** `Tools/RecastBake/meshes/Steelmill.links.txt:562` and `:572` changed from `strength` to `removed`
  (unknown kind = never used by any mask, so the two unexecutable roof links are gone; the walk route is used).
  `Steelmill.manifest.json` links bytes 57087 -> 57085, fnv64 `523caf82c65722fc`, sha256 updated. Codex must
  reinstall the canonical Steelmill links (the runtime checks the manifest FNV).
- **Executor:** `BotJumpLink.h:101-103` fails fast when blocked 2 m short in Approach (0.8 s) and `:143-145` in
  Act (0.6 s) instead of waiting `noTakeoff` 3.5 s; `Enter` resets the block timer when a deliberate stop begins
  (`:173`). A bad link is banned in two attempts, not 7 s.
- **Goal timer:** `BotBrain.cpp:652` does not age `m_upgradeBestAt` while `m_link.Active()`, so a link attempt is
  not mistaken for "no progress"; after the ban the bot repaths to the walk route inside the 90 s budget.
- Test `Tests/BotJumpLinkTests.cpp` case 7: blocked 1 m short in Act -> `NoTakeoff` within 0.7 s. MSVC pass;
  `BotPSCaptureTests` pass.

Not addressed: the Gauss tower links (`no_energy` at 28-35, `no_takeoff`, `missed_landing` at 142.5) are a
separate Strength-executor issue; the DS-048 target is `sniper_reach_dsg1 >= 2/3`. Engine probe was not run here.
