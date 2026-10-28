# DS-095: C150 frozen showcase payload review (no game)

Frozen root `C:/Users/Magic8/WarsBotsBuild/WarsBots-Showcase-b8b444d6/{Steelmill,Shore}`, tooling
`weapon-goals b6930859` (`Tools/Launch-WarsBotsShowcase.ps1`, `Prepare-WarsBotsShowcase.py`,
`Test-WarsBotsShowcase.ps1`). Read-only: no START/RESTORE, no server/client, no install, no build.

## Verdict: **`DS095 C150: OK`** - no blocking defect; four non-blocking notes below

### Integrity (all recomputed locally)

* `Run.ps1` both profiles **d2a7cb30...9097e** = the mailbox pin; it is byte-identical to
  `Tools/Launch-WarsBotsShowcase.ps1` (normalized), so the frozen launcher is the reviewed tooling.
* Exes `AE9EE3FA...889E` client / `8FD34B53...4D70` server = CI 40709459511 artifacts; `ci-sha256.txt` agrees;
  `build.json` pins source `b8b444d6`, CI 40709459511, `ci_success`, both artifact hashes, and Run.ps1 verifies
  `source`/`ci`/`ci_success` before anything else.
* Shore data = `E2235993...30de` (46614 polys) + the one-row `Shore.links.txt` + blank spots; Steelmill = canonical
  C119 (`cfc11810` nav, `7c384d12` links, `25c3eecf` spots). I recomputed every `<map>.manifest.json` FNV-1a-64
  (text hashed without `\r`): **navmesh/links/spots all match** for both maps, so the runtime will not refuse them.
* `package-manifest.json` pins build.json, both exes, all `Data/*`, preset, Run/START/RESTORE/CHECK, ci-sha256 and
  both licenses; paths are relative and `..`/absolute entries are rejected. `game-dir.txt` and the README are
  intentionally unpinned; no engine DLL/resource is shipped or overwritten.
* Command sizes recomputed from the presets: Steelmill server **1454** / preflight 1441 / client 429; Shore **1459** /
  1433 / 375 - all far below the 2048 native limit.

### Runtime contract

* `Run.ps1` refuses while any `CryMP-*64`/`Crysis*` runs, requires the profile port to be free, snapshots the **current**
  memory+nav dirs with hashes/absence, then runs a hidden **actorless preflight** (gate/level/critical CVars, `bots=0/N`)
  and only then the real roster. Shore readiness requires the 46614-poly mesh, `NAVMESH links ON`, `link kinds 1/1`,
  `#2 PS initial spawn team=2 group=537`, `#1 ENTITY_EVENT_DONE`, `FRAME bots=1`, and the initial US2 z >= 99.5
  (aborts otherwise, no resampling); Steelmill requires `#8 created`. Only after that is the visible client started
  (`-dx9`, `r_displayInfo 0`, F8 mark, F6/F7 = `wb_watch` only on IA - it is a console command, not a CVar, and the
  README says PS has no watch).
* **Stop scope**: only the owned server, and only if PID **and** exe path **and** UTC start time still match; the
  client is never killed; any other game defers cleanup (`restore_deferred_*`) and `RESTORE.cmd` (`-RecoverLast`)
  only accepts a receipt inside this profile's `Sessions/` with matching source/directory. Restore writes back exact
  names/hashes and removes the temporary Shore files (verified by the existing filesystem test that runs the real
  functions: existing bytes, absent dirs, active-client deferral without a process stop).
* No `+quit`/timer: the script waits for the owner to close the client, then archives logs/demo (path-contained,
  hash-checked) and restores.

### Notes (non-blocking)

1. `Restore-Data` archives logs/demos **before** the memory/nav restore; a demo-path/hash guard error would abort
   before the data restore (temp Shore files stay until `RESTORE.cmd` is run again). Wrapping the archive step in its
   own try/catch, or restoring data first, would harden this.
2. The client is launched without checking `$server.HasExited` a moment earlier; a server that dies right after
   readiness gives a connect failure instead of a clean message (the wait loop only warns).
3. The launcher uses `wb_debug 0` while parsing the critical CVar listing from the log; the verified C147/C149 runs
   used `wb_debug 1`. If a future build suppressed the listing at 0, the launcher would abort fail-closed
   ("Effective preset mismatch") - acceptable, but worth remembering.
4. Restore deletes files that were not in the snapshot (accepted C147/C149 policy); a concurrent non-game data change
   during a session would be removed.

Scope remains a **static local showcase**: Steelmill 8-bot IA is a prepared preset, not a proven full game; Shore is
the C149-proven upper route only (no lower deck, no 41 holes, no capture completion); a human player can change the
bot's choices. No game, build, install or source change by me.
