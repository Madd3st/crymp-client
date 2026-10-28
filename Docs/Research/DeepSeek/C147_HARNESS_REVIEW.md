# DS-086: C147 upper->shore harness review (`build/c147-upper-to-shore.ps1`, sha256 `36afcecc...b271`)

Verdict: **ACK/OK to launch** (with the four notes below; none is a BLOCK). Reviewed against source `b06681ea`
and the previous native run `build/c145-US-b06681ea-v4`. No game/build by me.

## Recipe soundness (checked mechanically)

* **Identity**: line 5 hard-pins `b06681eaa4365afe5bf7309994012dce13a7ac83`; line 6-7 requires the local
  `native-route-quality.json` to carry that source, `ci_success` and review `dd68f2ce`; line 11 verifies the CI
  artifact hashes; line 13 pins `E2235993`. Good.
* **Data**: candidate + the one-row `US-only-strength.links.txt` (57 B, LF) + a blank spots file are staged, and a
  manifest is generated from the actual bytes (`fnv64` over the raw file). That matches the runtime contract
  (`BotNavMesh.cpp:44-56`: text hashed with `\r` stripped; our files are LF, so raw == stripped) and the install
  tool re-verifies bytes+sha256. Expected `NAVMESH link kinds 1/1` and `NAVMESH links ON: 1` - both are checked
  before any bot action (line 78), so a wrong row fails closed.
* **Flags**: base recipe `wb_probe_local_test 1`, `wb_ps_capture 1`, `wb_move_trace 1`, `wb_demo_record 1`,
  `wb_demo_hz 20`; overrides add `wb_bot_navmesh 1`, `wb_bot_jump_links 1`, `wb_maxbots 2`, `wb_log 2`,
  `wb_ps_events 1`, and force `wb_ps_doors 0`, `wb_ps_vehicles 0`. No code path, no B1, no speed patch.
* **Roster**: `+wb_add 2 normal` then `+wb_remove 1`, pending-aware; readiness requires `#1 ENTITY_EVENT_DONE`,
  `FRAME .*bots=1 server=1` and rejects `#1 removed externally` / a second `#1 created`. Matches the DS-080/081
  lifecycle contract.
* **Initial state**: aborts if `#2 created ... position=... z < 99.5`, keeps the native spawn (no resample/teleport),
  and requires the real `46614 polys` mesh, so a stale Shore navmesh cannot pass.
* **Fail-closed names**: stops on `#2 LINK failed kind=strength` (line 89, trajectory preserved), >120 s without
  completion (91), link done but no water entry by 180 s (103), any `[Error]|Lua Error|Script Error|RMI.*not a
  client` (72) and on an owner client appearing (66). One run only: duplicate dir/log/port collisions refused.
* **Archive/restore**: demos have path-containment + hash checks; both **current** memory and navmesh dirs are
  snapshotted, re-snapshotted after the run, then restored with per-file hash verification and deletion of files
  that were absent before. This restores the *present* state, not the stale C146 memory - as required.
* **Goal chain is real** (the thing that would have made the run useless): `PSCaptureSnapshot` drops every point
  whose goal is not `Reachable` (`BotPSCapture.cpp:216-219`), and the planner then needs `hasDestination`
  (`BotPSCapture.h:50`). In v4 (`jump_links 0`) that filtered all 5 points -> 0 `PS CAPTURE target=` in 180 s and
  only local deck roam. With the link live, 4 of 5 become reachable (DS-084 full-graph), so the planner will select
  a shore target and its path must use the link (the upper US component's only sea connection).

## Budget: 300 s is justified

Load ~30-40 s (C146 client reached its first trace at t=31.7), readiness gate <=50 s, link (target select + path +
Strength prepare <=6 s + run-up + 1.5 s air) well inside the 120 s gate, ring walk/drop ~10-20 s, then a
100-250 m swim at native 2-5 m/s = 30-80 s. Typical total ~120-180 s, so 300 s leaves real margin; the 180 s water
gate still leaves 120 s for the swim leg. One run, no fragments.

## Non-blocking notes (worth fixing before launch if cheap)

1. **4 bytes of command budget**: replicating lines 37-43 on this machine gives **2044/2048** bytes for the real
   command (the guard uses the same `>=2048` rule, so it fails closed). Any extra token or a longer log path kills
   the launch - freeze the recipe. The **preflight** command (`-preflight.log`, +10 chars, minus the 4 add/remove
   tokens) is not length-checked; it measures 2015 bytes, so it is fine but unguarded.
2. **Timing clocks start at process launch** (line 63), not at readiness: ~30-40 s of map load is charged to the
   120 s link gate (and to the 180 s water gate). Expected link time is ~15-25 s after spawn, so it fits, but if the
   load is slow the run aborts on the wrong clock. Cheap fix: start the link/water clocks at `$ready`.
3. **Links file is not hash-pinned** (only the navmesh is, line 13): a wrong/changed row is caught by the runtime
   `kinds 1/1` check, so it fails closed, but it would waste the only run. Pinning
   `68848bc62ed83f83` / the 57-byte file is one line.
4. **Verdict input**: add `PS CAPTURE target=` presence to `run-complete.json`, so "no goal beyond the link" is
   distinguishable from "no LINK". The `dry_land_candidate` heuristic stays a candidate only; final PASS remains the
   offline trace/nav-shore + target-progress review, as stated in the scenario.

No game/build/install by me; candidate, B1 `0b39bdb6` and the held `3ec91e44` unchanged.
