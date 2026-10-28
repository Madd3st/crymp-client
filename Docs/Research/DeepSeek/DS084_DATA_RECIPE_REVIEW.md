# DS084 data recipe review - b06681ea / E2235993

Verdict: **`DS084 data recipe b06681ea/E2235993: OK`** with the mandatory install/telemetry conditions below.
No game/build/triage was run for this review; the checks are offline on the supplied QA data.

## Checks performed

* **Exact code**: `b06681eaa4365afe5bf7309994012dce13a7ac83`; CI **40649771503 success** (Unit tests MSVC +
  Client/server MSVC experimental spawner). DS081 review OK `dd68f2ce`.
* **Candidate hash**: `build/coordinator-c145/Shore.US-strength-candidate.navmesh`, 9 762 176 bytes,
  sha256 **e2235993ce72eb4549788ad7bd1a171ab415c8e8b4c6da69fdb49ed941ef30de** - exact match to the receipt.
* **One link**: `2110.56 1885.58 103.18 -> 2113.98 1885.51 106.07`, kind `strength`, one-way (reverse of the
  legitimate drop). Rise 2.890, horizontal 3.421, both endpoints on the mesh (cockpit floor `fromUS=1`; east ring
  106.038643 `fromUS=0`). The other ring points are baked too (west 106.049561, east2 106.040489).
* **Full graph**: actualCore US -> sea 11 corners / 1 Strength; capture 64/316/579/654 + NK reachable; 344 chosen
  point `other_component`; 316 rolling 256-poly prefix 31.02 m short. No additional raw autolinks.
* **New audit (apex4.7)**: `neededVH=2.156`, vh 2.5/2.156/3/4 clear, **vh 6.75 not collision-blocked** but the
  constant-arc endpoint is 7.3 m past the ring (overshoot). So the native speed defect is a **hypothesis**, not
  observed; the run must decide it from the actual take-off/landing/link outcome.

## Mandatory conditions (the recipe is OK only with these)

1. **Install the link kind file with the navmesh**: the candidate `Shore.navmesh` (E2235993) **plus** a
   `Shore.links.txt` containing the one `strength` row (and the updated `Shore.manifest.json` + spots) in the
   temporary candidate dir. The mesh has `offMeshConCount=1`, but `BotNavMesh.cpp:101-114` sets `kinds=0` and
   `SetLinkMask(0)` when the links text is empty, so without the row the link is never used. Hash/restore all four
   candidate files (and the original/absent navmesh dir).
2. **Initial US2 upper/on-mesh check before the 120 s**: verify the first US2 MOVE TRACE position is the upper
   cockpit (z ~103.2, on the mesh/upper component); abort otherwise. No forced spawn/teleport; the one
   add2/remove1 pending-aware settled roster stays.
3. **Goal/link acceptance**: with `wb_ps_capture 1`, log the selected PS capture target and require a
   `LINK start kind=strength` (or record "no target/link within 120 s" as **inconclusive**, not a speed failure).
   The one-link route is new; do not count an unchanged patrol as evidence.
4. **Keep the command <2048**: the v4 command is 2035 bytes with `+wb_bot_jump_links 0`; the 0->1 flip is
   length-neutral, so 2035 holds. Re-measure `full-command-bytes` before launch (guard from DS078).
5. **Telemetry in the archive**: MOVE TRACE `vel=(vx,vy,vz)`, `vz`, `peakZ`, `jumpTime/landTime`, `grounded/flying`,
   `swim/waterZ/relativeWater/underwater`; LINK start/done/failed (level 2); JUMP TAKEOFF/LANDING (actual strength
   vz/rise/displacementZ); PS ARBITER/capture target. `.bots.log` keeps all levels; demo 20 Hz archived.
6. **Safety/restore**: local PS only, `wb_debug 1`, `wb_log 2`, `wb_move_trace 1`, `wb_ps_capture 1`,
   `wb_bot_navmesh 1`, `wb_ps_doors 1`, `wb_bot_jump_links 1`; stop on client/error; full nav + memory + absent
   restore with hashes; demo archive; <=120 s; one owned US2.
7. **No code in this run**: no speed patch, no new CI, no defaults; the prospective `ds084_strength_speed.patch`
   stays on the own branch until the native route actually fails.

If the candidate links row is not installed or the initial-upper/goal checks are skipped, the run is **BLOCK**
(would repeat the disconnected-patrol ambiguity). With the conditions above the recipe answers the open question:
actual take-off vz/vx, apex, landing, link Done/Fail, water/exit, and PS goal on the newly connected route.
