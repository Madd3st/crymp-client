#!/bin/bash
# D-015 step 1: build and run every engine-free unit test (Tests/Bot*Tests.cpp) with g++ on Linux.
# BotNavMeshCoreTests also gets the Steelmill meshes. Usage: run_units.sh [repo]
REPO=${1:-.}; cd "$REPO" || exit 2
OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT
ENGINE_TESTS="BotAimControlTests BotProfileTests"; R=ThirdParty/recastnavigation; FAIL=0; M=Tools/RecastBake/meshes
for t in Tests/Bot*Tests.cpp; do
  n=$(basename $t .cpp); extra=""; args=""
  grep -q "BotNavMeshCore.h" $t && extra="Code/CryGame/Bots/BotNavMeshCore.cpp $R/Detour/Source/*.cpp -I$R/Detour/Include" && args="$M/Steelmill.navmesh $M/Steelmill.navmesh $M/Steelmill.links.txt"
  if ! g++ -std=c++17 -O1 -I. -ICode "$t" $extra -o "$OUT/$n" 2>"$OUT/$n.err"; then
    case " $ENGINE_TESTS " in *" $n "*) echo "SKIP $n (needs engine headers; Codex runs it under MSVC)";;
      *) echo "FAIL $n (does not compile)"; head -5 "$OUT/$n.err"; FAIL=1;; esac
    continue
  fi
  if $OUT/$n $args >$OUT/$n.log 2>&1; then echo "PASS $n"; else echo "FAIL $n"; tail -3 $OUT/$n.log; FAIL=1; fi
done
exit $FAIL
