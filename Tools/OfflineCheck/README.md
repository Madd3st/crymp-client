# Offline check (D-015, step 1)

Run before every push to `warsbots/feature/claude-sprint`, on Linux (no MSVC, no game):

- `Tools/OfflineCheck/run_units.sh .` — builds and runs every engine-free `Tests/Bot*Tests.cpp` (g++), navmesh tests on
  the Steelmill data. Two tests need engine headers and are skipped here (Codex runs them under MSVC).
- `Tools/OfflineCheck/compile_check.sh .` — `clang++ -fsyntax-only` of `Code/CryGame/Bots/*.cpp` and `Player.cpp`
  with `msvc_shim.h` (Linux stand-ins for `_stricmp`, `_Interlocked*`, `strcpy_s`, ...). Reports errors in our files only.
  It caught the C-070 `Vec3(ZERO)` break that cost a full Codex cycle. It is a syntax/type check, not a build: MSVC-only
  behaviour and link errors still need Codex.
