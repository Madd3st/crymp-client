# Offline check (D-015, step 1)

Run before every push to `warsbots/feature/claude-sprint`, on Linux (no MSVC, no game):

- `Tools/OfflineCheck/run_units.sh .` — builds and runs every engine-free `Tests/Bot*Tests.cpp` (g++), navmesh tests on
  the Steelmill data. Two tests need engine headers and are skipped here (Codex runs them under MSVC).
- `Tools/OfflineCheck/compile_check.sh .` — `clang++ -fsyntax-only` of `Code/CryGame/Bots/*.cpp` and `Player.cpp`
  with `msvc_shim.h` (Linux stand-ins for `_stricmp`, `_Interlocked*`, `strcpy_s`, ...). Reports errors in our files only.
  It caught the C-070 `Vec3(ZERO)` break that cost a full Codex cycle. It is a syntax/type check, not a build: MSVC-only
  behaviour and link errors still need Codex.

# Walk simulator (D-015, step 2)

`walk_sim <map.navmesh> <map.links.txt> <collision.obj> <map.pts> [legacy|corners|both] [maxPairs] [tracePair]`

Build: `g++ -std=c++17 -O2 -I. -ICode -IThirdParty/recastnavigation/Detour/Include Tools/OfflineCheck/walk_sim.cpp
Code/CryGame/Bots/BotNavMeshCore.cpp ThirdParty/recastnavigation/Detour/Source/*.cpp -o walk_sim`.
A kinematic player capsule (r 0.4, body 0.5–1.9 m) walks every spawn → reachable item route through the map's
collision triangles (the C-019/C-028 crop obj) with the legacy corner rule and with `wb_bot_corners` v2, and lists
where it gets stuck (no 0.3 m path progress in 3 s), clustered by 2 m. Seconds per map. `WALK_ALL=1` prints all clusters.
No sliding/avoidance and no other bots, so absolute stall counts are pessimistic; use it to compare rules and to find
geometry wedges before a batch. Legacy reproduces the two known game traps (spawn 176 box, stair 1306,1353).

25.10 results (routes done / stuck): Steelmill legacy 451/2573 → corners 3024/0; Quarry 362/1132 → 1475/19;
Outpost 332/1168 → 1389/111; Armada 266/1234 → 1404/96. Same travel time where both finish (median ratio 1.00–1.01).
