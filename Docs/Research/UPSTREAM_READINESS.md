# Upstream readiness — WarsBots into crymp-net/crymp-client (D-033, owner 27.10 17:52)

Goal: a demo good enough for the CryMP maintainers, then a small series of PRs they can accept. Our repo is a fork of
`crymp-net/crymp-client` (default `master`). Measured on claude-sprint eabb3824, 27.10:

| Area | Now | Upstream target |
| --- | --- | --- |
| Bot module `Code/CryGame/Bots/` | 89 files, ~11.2k lines | ~6–7k core; tools out; ~1k deleted |
| Game code outside Bots | 26 files touched (Player, weapons, scopes, GameRules, HUD, ShotValidator, Client) | each hook justified, minimal, zero effect with bots off |
| Lua | 8 files +1.8k, full copies of stock PowerStruggleBuying/Rank | small hooks into stock scripts, no copies |
| `wb_*` flags | 54 (all referenced) | 4–6 user settings: count, skill, mode, enable; rest promoted or deleted |
| Console commands | ~50 `wb_*` | 5–6 for server admins; the rest dev-only |
| `BotBrain.cpp` / `BotManager.cpp` | 1698 / 1305 lines, functions up to 133 lines | per-state files, functions < 60 lines |
| Mode handling | 9 `IsPowerStruggle`/"PowerStruggle" checks | one `IBotGameMode` (IA, PS) |
| Legacy nav | `BotNav` graph (~900 lines) still a fallback | deleted, navmesh only |
| Research tools in the game DLL | geometry export, demo recorder, move trace, probe runner, physics probe (~1.5–2k) | behind a dev build option, off in release |
| Dead code | `BotBoatDriver.h` unused | wired (PS vehicles) or removed |
| Performance | bot AI 0.54 ms/frame, 15 bots (< 2 % of a 31 ms frame) | measure on a weak server; budget 1 ms/frame for 16 bots |
| Stability | 11-min runs, one map | **4 h soak: map rotation, joins/leaves, bot add/kick, memory per hour flat** |
| Tests / CI | 44 unit test files, CI every push; Lua and ScriptBind_* not checked offline | Lua syntax + all touched game files compiled in CI |
| Branches | code in `claude-sprint`, stale code in `weapon-goals` (misled DS-S7) | one integration branch; report branches without game code |
| Data | navmesh/links/spots per map from our Python Recast tools | reproducible bake, documented, shipped with the server pak |

## Fundament wave (order, each behaviour-identical, gated by a paired run)

1. Flags cleanup + net-zero rule (DS-050 #1) — promote what has evidence, delete the rest.
2. `IBotGameMode` (DS-050 #2) — before any more PS features.
3. PS measurement (DS-050 #3) — MOVE TRACE/summary for PS.
4. Delete `BotNav` fallback — after a 4-map navmesh-only run shows no extra stalls.
5. Dev tools behind `WARSBOTS_DEVTOOLS` build option (release build without them compiles and runs).
6. Split `BotBrain` (per state) and `BotManager` (console, spawn, match).
7. Lua: replace stock copies with hooks.
8. Soak test nightly (C-129).

## Progress measure (DS-S7)

Bible rows (DS-042): 2 PASS / 18 PARTIAL / 15 FAIL on 26.10 → 4 / 16 / 15 on 27.10 (first primary, finish). Moved little;
grade every playtest and soak on the Bible, PS rows included once PS is measured. Promotions need a paired run on the
same build (C-121 was single).

## Questions the maintainers will ask (answer before the demo)

- Bots off: is the game byte-for-byte unchanged? (test: bots-off build vs upstream, same demo)
- Do bots inflate the server list player count? (show/hide policy)
- ShotValidator / anti-cheat changes: why, and are they inert for humans?
- Do clients need an update? (target: no — server only; client smoothing optional)
- Server cost, network traffic per bot.
- Who maintains it; how are map data produced for new maps.

## Before contacting them

Demo = owner playtest video IA + PS, soak report, this table mostly green. First contact is a short proposal/issue
with the demo, not a big PR. Their answer decides: in-tree module vs optional server plugin.
