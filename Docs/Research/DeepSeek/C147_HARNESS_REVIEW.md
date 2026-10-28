# C147 upper exit -> shore harness review (DS-086)

Script: `build/c147-upper-to-shore.ps1`, SHA256 `36afcecc4a8824f99b3007bb49e1411e6dc92fd5aa51877f6e79417d3372b271`
(matches the task). Verdict: **OK** with the notes below; no game/build by me.

## Gate checks (verified)

* Exact source `b06681eaa4365afe5bf7309994012dce13a7ac83`, CI 40649771503, review `dd68f2ce`; the script refuses
  any other SHA and re-hashes the CI artifacts.
* Candidate `Shore.US-strength-candidate.navmesh` sha256 **e2235993...30de**; one-row `links` FNV
  **68848bc62ed83f83**, navmesh FNV **c86c5b0a05b1fd89**, blank spots FNV 9bce2faedebbca34 -> the generated
  manifest is the known matching one; no other links.
* No code change: the b066 binary (no B1, no speed patch); only data + `jump_links 1`.

## Runtime shape (sound)

* Actorless preflight first: same effective CVar set, no bots, `RULES INFO class=PowerStruggle`,
  `gate=1 levelReady=1 bots=0/2`, no `Usage: wb_debug`, all expected effective values checked.
* One native US2: `wb_add 2` + deferred `wb_remove 1`; readiness requires `#2 PS initial spawn team=2 group=537`,
  **initial z >= 99.5** (lower/offmesh aborts, no resample/forced spawn), `NAVMESH loaded ... 46614 polys`,
  `NAVMESH link kinds 1/1`, `NAVMESH links ON: 1 links`, native water fields present, `#1 ENTITY_EVENT_DONE` +
  settled `bots=1 server=1`, no `#1 removed externally`.
* Timing: 120 s to `LINK done kind=strength`, 180 s to first `swim=1`, hard 300 s total. The link base is ~0.7 m
  from the upper spawn, so 120 s is a fail-fast cap, not a wait; the water entry follows the ring drop in seconds;
  the remaining ~280 s covers a real 100-300 m swim at 2.5-4 m/s. 300 s is justified. The capture target 64 is
  ~540 m away, so the final acceptance must be **shore + partial target progress**, not a full capture.
* Stops: first `LINK failed kind=strength`, `[Error]/Lua/RMI` error, owner client, readiness timeout; no repeat.
* Early dry-land heuristic (`swim + grounded + z > 100.5 + >100 m from the sub`) only breaks the loop; the
  `run-complete.json` marks `dry_land_candidate`, not PASS. Final PASS remains the offline raw-trajectory/nav shore
  classification + target progress.
* Archive/restore: logs/trace/demo (hash+bytes), arguments/recipe/preflight/effective, memory+nav **current**
  snapshot with per-file hashes, restore removes the staged `Shore.*` nav files and restores the current files;
  the current `Shore.mem` drift is preserved (not the stale C146 archive); command bytes <2048 guard; no boat AI.

## Notes / residual risks

1. The current script **requires the memory and nav directories to exist**; if the nav dir were absent, it throws
   before any change (safe, but the C146-style "absence" dir restore is not implemented). Current dirs exist
   (nav: Armada/Outpost/Quarry/Steelmill, no Shore; memory: Steelmill + Shore), so the staged `Shore.*` files are
   the absent ones and the restore removes them. This is acceptable for this run; a clean-machine absence case
   needs the dir-existed flag.
2. The current executor forces Sprint in `Act` (6.75 m/s), while the human upper jump measured 3.5-3.75 m/s. The
   link may therefore fail as overshoot; the script stops on the first failure and preserves the trajectory. That
   is the intended evidence, not a bug in the harness; the speed patch `3ec91e44` stays held.
3. The preflight command bytes are not separately measured (the observation command is); the preflight is shorter,
   so this is low risk.
4. The PS capture flag (`wb_ps_capture 1`) is inherited from the c127 recipe and checked; the selected capture
   target/progress is in the archived `.bots.log`, not asserted by the heuristic; the final review reads it.

OK to launch once the owner/Codex accepts these notes; the run is one shared upper exit -> water -> shore, not a
repeated 120 s fragment.
