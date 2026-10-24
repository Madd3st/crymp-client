# Steelmill AI Lab

Offline tools to study Steelmill, bot movement, map memory and ML data quality — **without
running Crysis**. Branch `warsbots/research/steelmill-lab`, based on Codex's checkpoint
`warsbots/feature/weapon-goals` @ `53023d52`. No game code, build system, Codex tool or shared
document was changed. Python 3.9+ standard library only (Pillow optional, for the minimap).

| Document | Content |
|---|---|
| [STEELMILL_DATA_SOURCES.md](STEELMILL_DATA_SOURCES.md) | What data really exists, where, and what each source can prove |
| [NAVIGATION_FINDINGS.md](NAVIGATION_FINDINGS.md) | BotNav as implemented + problems measured on real logs |
| [BOTNAV_EXPORT_CONTRACT.md](BOTNAV_EXPORT_CONTRACT.md) | Format and minimal task for Codex to export the real graph |
| [ML_DATA_READINESS.md](ML_DATA_READINESS.md) | Audit of the `wb_tactics_report.py` episodes for a contextual bandit |

## Tools (`Tools/SteelmillLab/`)

| Tool | Does |
|---|---|
| `collect.py` | copies real files (level.pak, minimap, `.mem`, logs) into the lab folder, read-only, with SHA256 |
| `lab.py` | runs everything below and builds the viewer bundle |
| `wbmem.py` | validates and summarises a `WBMEM01` map memory copy; links it to a graph export of the same build |
| `botlog.py` | parses `.bots.log`: tracks, A* failures, weapon timeouts, stuck reports, stalls, loops, traps, no-approach |
| `level_data.py` | spawns, weapons, ammo, areas, spectator points, minimap bounds, CryAI nav export sizes from level.pak |
| `navgraph.py` | validates/analyses a BotNav graph export, replays logged failed A* queries, writes the fixture graph |
| `ml_validate.py` | independent validator for exporter episodes (schema 2), with optional log cross-checks |
| `viewer/steelmill_viewer.html` | standalone viewer (canvas, no libraries) |

## Run (Windows, from the repository root of this branch)

```bat
python Tools\SteelmillLab\collect.py ^
  --level-dir "E:\Steam\steamapps\common\Crysis\Game\Levels\Multiplayer\IA\Steelmill" ^
  --mem "%USERPROFILE%\OneDrive\Документы\My Games\Crysis\WarsBots\Multiplayer_IA_Steelmill.mem" ^
  --logs "%USERPROFILE%\OneDrive\Документы\My Games\Crysis\WarsBots-Tactics-*.bots.log" ^
  --dest build\steelmill-lab\sources
python Tools\SteelmillLab\lab.py --sources build\steelmill-lab\sources --single-file
start build\steelmill-lab\out\viewer\index.html
rem or the portable one-file version: build\steelmill-lab\out\viewer\steelmill_viewer_bundle.html
```

`collect.py` only reads the originals. Everything else reads the copies and writes to
`build/steelmill-lab/` (git-ignored by the existing `/[Bb]uild*` rule). Game-derived outputs
(map JSON, minimap PNG, movement JSON) must stay there and never be committed.

Optional ML audit (uses Codex's exporter unmodified, output into the lab folder):

```bat
python Tools\wb_tactics_report.py build\steelmill-lab\ml\runs.json ^
  --output build\steelmill-lab\ml\report.json --episodes build\steelmill-lab\ml\episodes.jsonl
python Tools\SteelmillLab\ml_validate.py build\steelmill-lab\ml\episodes.jsonl ^
  --runs build\steelmill-lab\ml\runs.json --json build\steelmill-lab\ml\validation.json
```
`runs.json`: `[{"log": "../sources/<file>.bots.log", "tag": "<name>", "source_sha": "<commit>"}]`.

When Codex adds the graph export, put `<level>.graph.json` into the sources folder: `lab.py`
validates it, links the `.mem` to node positions and the viewer draws nodes, edges and memory.

## Viewer

Open `build/steelmill-lab/out/viewer/index.html` (data is embedded in `data.js`; more files can be
dropped onto the map). Drag to pan, wheel to zoom, click a marker for its record.

* Background: the level's own minimap, placed with the engine's HUD mapping
  (`CHUDRadar::GetPosOnMap`: screen right = +Y, down = +X). Bot routes follow corridors on it,
  which supports the mapping; it is not verified inside the game.
* Height filter (z) with presets; layers for spawns, weapons/ammo, routes, A* failures, weapon
  timeouts, stuck reports, stalls, traps, loops, no-approach, kills, deaths, pickups, graph, memory.
* Filled marker = event logged by the bot/engine; ring = inferred by the analyzer.
* A red banner marks any FIXTURE (synthetic) data. Map memory is drawn only when linked to a
  graph of the same build; without the graph its node positions are unknown and nothing is drawn.

## Tests

```sh
python -m unittest discover -s Tests/SteelmillLab -v
```
21 tests: parsers, bounds and corruption handling, graph validation/components/A* replay, viewer
data contracts on fixtures, ML validator rules; real-data tests run when copies exist in
`build/steelmill-lab/sources` (or `$STEELMILL_LAB_SOURCES`) and are skipped otherwise.
