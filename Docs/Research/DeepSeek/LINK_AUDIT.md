# Link audit: bad baked transitions (DS-037 #3)

Date: 2026-10-26. Read-only. Tool: `proto/link_audit.cpp` (engine-free, MSVC). Inputs:
`Tools/RecastBake/meshes/{Steelmill,Quarry,Armada,Outpost}.links.txt`; outputs in this folder:
`bad_links.csv`, `<Map>.links.filtered.txt`, `<Map>.links.strict.txt`.

## What is flagged

| Reason | Rule | Class | Meaning |
| --- | --- | --- | --- |
| `flat_strength_long` | `kind=strength`, `|dz| < 0.8 m`, horizontal gap `> 3.5 m` | **remove** | level gap treated as a Strength climb; the bot runs off the edge (C-098 line 546: 1298.14,1343.57→1289.41,1343.52, gap 8.73, dz 0.10) |
| `flat_strength` | `kind=strength`, `|dz| < 0.8 m` | review | flat but short; usually a step, not a jump |
| `strength_not_up` | `kind=strength`, `dz < 0.3 m` | review | landing is not on a higher part; should be `jump`/`drop` |
| `rise_too_high` | `kind=strength`, `dz > 3.5 m` | review | above the safe native Strength arc; probe before removing (DSG1 key link is +4.30) |
| `long_gap` | `kind=strength` gap > 9 m, `kind=jump` gap > 3.5 m | review | outside the observed native range; C-098 completed 7-8 m Strength links, so 3.5 m is **not** a Strength limit |
| `drop_up` | `kind=drop`, `dz > 0.3 m` | **remove** | a drop must go down |
| `zero_length` | gap < 0.25 m and `|dz| < 0.25 m` | **remove** | degenerate |
| `duplicate` | same directed endpoints (0.1 m) and kind | **remove** | later line dropped |

`bad_links.csv` lists every flagged line with `gap`, `dz`, reasons, and the raw line. `filtered` removes only the
**remove** class; `strict` also removes every **review** class.

## Counts

| Map | Lines | Auto-remove | Review | Kept (filtered) | Kept (strict) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Steelmill | 1,133 | **78** | 491 | 1,055 | 642 |
| Quarry | 1,870 | **90** | 844 | 1,780 | 1,026 |
| Armada | 632 | **58** | 331 | 574 | 301 |
| Outpost | 717 | **33** | 291 | 684 | 426 |

`bad_links.csv` has 1,958 rows (one per flagged line, header excluded), not one per reason.

All auto-removals are `flat_strength_long` (78/90/58/33). No duplicates or zero-length lines were found.

## Validation on the real navmesh

`proto/nav_links_probe` with the **filtered** Steelmill links:

```
mesh polys=10388 components=648 links=1133 kinds=1055 mismatches=1123 mask=0xe
SUMMARY DSG1 ok=34/34 withLinks=23 totalLinks=54
SUMMARY Gauss ok=34/34 withLinks=34 totalLinks=174
```

- The filtered file keeps **34/34** spawn→DSG1 and **34/34** spawn→Gauss routes, with the same link counts as the
  full file. It removes only the dangerous flat-long links.
- The high `mismatches` count is expected: removing lines shifts the 1-based user id, so `LoadLinkKinds` matches by
  end points (DS-005 fallback). It still sets 1,055 kinds; the 78 removed links stay `0x4000` and are never used.

`strict` is **not** recommended as a drop-in: it leaves DSG1 with only one link route
(`ok=34/34 withLinks=1 totalLinks=2`) and removes 491 more links on Steelmill; use it as a re-bake diagnostic only.

## The DSG1 key link

`Steelmill.links.txt:654`:
`1354.96 1347.60 147.74 -> 1351.94 1351.98 152.04 strength`, gap 5.32, **dz +4.30**.

It is `rise_too_high` and is **not** removed by the filtered file. C-098 attempted it six times (t=394, 466, 481, 491,
503, 598) with energy 200/141/57/177/45/56 and never logged `LINK done`. Two options:

1. keep it and let the fixed executor (no downgrade, 90 energy, 3.5 s Act) try; a native-arc probe decides;
2. remove it in a re-bake and rely on the 11/34 walk-only DSG1 routes (filtered still 34/34).

Do not remove it blindly: it is the only *link* route to the roof for most spawns, and the walk route exists but is
longer.

## Baker rules

For `strength`:
- must climb: `dz >= 0.3`; if `dz < 0.8`, require gap `<= 3.5` (otherwise it is a level gap, not a climb);
- `dz <= 4.0` (probe anything 3.5-4.0 with the real jump);
- horizontal gap up to 9 m is allowed (C-098 completed 7-8 m with run-up); do not cap at 3.5 for Strength;
- landing `b` must be on the higher walkable part; if `dz < 0`, emit a `drop`, not `strength`.

For `jump`: gap `<= 3.5`, rise `<= 1.5`.
For `drop`: `dz < 0`; never a rise.

## How to use

- **Now:** copy `Steelmill.links.filtered.txt` to `%LOCALAPPDATA%\WarsBots\Navmesh\Steelmill.links.txt` (Codex owns
  the install step; this is the file to drop in). Same for Quarry/Armada/Outpost if those maps are played.
- **Re-bake:** feed the filtered list (or the rules above) to `Tools/RecastBake`; do not renumber the kept lines.
- **Diagnostic:** `Steelmill.links.strict.txt` only for a "what if we remove every suspicious link" run; it disconnects
  most link routes and is not a fix.

## Limits

- The audit is geometric; it does not simulate the native jump. `rise_too_high`/`long_gap` are review flags, not proof.
- The map's collision/terrain is not consulted; a flat link may be a legitimate gap over a hole, but it still should
  be `jump`/`drop`, not `strength`.
- `bad_links.csv` includes review rows (1,958 total, all flagged lines); filter by the `hard` column for the
  auto-remove set.
