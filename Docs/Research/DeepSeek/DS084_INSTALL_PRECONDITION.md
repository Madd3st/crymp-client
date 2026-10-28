# DS084 install precondition - exact bytes for the one-Strength-link candidate

Follow-up to `DS084_DATA_RECIPE_REVIEW.md` condition 1. Offline, read-only on the QA build dir
(`C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build/coordinator-c145`); no install, no game, no code change.

## Why this exists

`BotNavMesh.cpp:100-101` refuses a links file whose FNV-1a-64 does not match the `"links"` entry of
`<leaf>.manifest.json` (`text.clear()` -> `kinds=0` -> `SetLinkMask(0)`), so the candidate would silently walk
without the Strength link even though the row is present. A missing manifest is accepted (`BotNavMesh.cpp:47`).

## Verified values (computed here, FNV implementation cross-checked against the installed manifests)

| file | bytes | fnv64 | sha256 |
| --- | --- | --- | --- |
| `Shore.navmesh` (E2235993) | 9 762 176 | `c86c5b0a05b1fd89` | `e2235993ce72eb4549788ad7bd1a171ab415c8e8b4c6da69fdb49ed941ef30de` |
| `Shore.links.txt` (one row, LF, no CR) | 57 | `68848bc62ed83f83` | row below |

Row bytes (exactly, trailing `\n`):

```
2110.56 1885.58 103.18 2113.98 1885.51 106.07 0 strength
```

Cross-check of the implementation: installed `Steelmill.manifest.json` values reproduce byte-exactly
(`777605245bdc0e45` navmesh, `399fe0d705d1c1b8` links, `5f89778cc1e6e1d3` spots), and text kinds are hashed with
`\r` removed (`BotNavMesh.cpp:48-51`), so a CRLF checkout still matches if the LF hash is recorded.

## Install choices

1. **No `Shore.manifest.json`** (recommended for a diagnostic candidate): accepted, nothing to keep in sync.
2. **With manifest**: `"links"` = `68848bc62ed83f83`, `"navmesh"` = `c86c5b0a05b1fd89`; a stale manifest from an
   earlier Shore bake is worse than none.

Do not install `Shore.spots.txt` from another bake: `BotNavMesh.cpp:138` clears a mismatching spots file, but the
candidate has no matching spots anyway.

## Pre-run proof lines (server `.bots.log`), abort rules

* `NAVMESH loaded .../Shore: 46614 polys, 3803 parts, 1 links` and `NAVMESH link kinds 1/1 from ...`;
  `NAVMESH links ON: 1 links`.
* `kinds 0/1` = the links text was rejected (manifest mismatch or no row within 0.6 m, `BotNavMeshCore.cpp:270`) ->
  **abort, do not grade speed/link failure**.
* `wb_bot_jump_links` must be exactly `1` and `wb_bot_navmesh 1` (`BotNavMesh.cpp:110`, `:61`); `jumpLinks==1`
  gates drop/jump/Strength exposure, so the 0->1 flip in the v4 command is required, not optional.

## Restore

Hash and restore the navmesh dir entries (`Shore.*` if present, otherwise prove absence), the manifest (install
case 2) and memory; the candidate must not survive the session. This document changes no default, flag or asset.
