#!/bin/bash
# WarsBots offline syntax/type check (D-015 step 1): clang -fsyntax-only on Linux with an MSVC shim.
# Usage: check.sh <repo> [files...]  (default: Code/CryGame/Bots/*.cpp + Player.cpp). Prints errors in our files only.
REPO=${1:-.}; shift; cd "$REPO" || exit 2
FILES=${@:-$(ls Code/CryGame/Bots/*.cpp) Code/CryGame/Actors/Player/Player.cpp}
R=ThirdParty/recastnavigation; SHIM=$(dirname $(readlink -f $0))/msvc_shim.h; FAIL=0
for f in $FILES; do
  out=$(timeout 300 clang++ -fsyntax-only -std=c++20 -fms-extensions -ferror-limit=0 -DWARSBOTS_EXPERIMENTAL_SPAWNER \
    -include $SHIM -I. -ICode -ICode/CryCommon -ICode/CryGame -I$R/Detour/Include -I$R/Recast/Include -Wno-everything "$f" 2>&1 \
    | grep "error:" | grep -E "^Code/CryGame/(Bots/|Actors/Player/Player)")
  if [ -n "$out" ]; then echo "== $f"; echo "$out" | head -15; FAIL=1; fi
done
[ $FAIL = 0 ] && echo "CHECK OK"
exit $FAIL
