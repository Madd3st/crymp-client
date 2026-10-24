# recast_bake — offline Recast/Detour bake for WarsBots

Offline research tool (not part of the game build). Reads a cropped collision OBJ(.gz) exported by the bots
(Codex C-019/C-028, Crysis frame, Z up), bakes a tiled Detour navmesh with the player capsule, optionally generates
drop / jump / Strength links with geometry clearance, and reports which spawns and items lie on the main part.

Build (Linux or Windows/MSVC, CMake ≥ 3.16, zlib):

    cmake -S Tools/RecastBake -B build-recast -DCMAKE_BUILD_TYPE=Release   # fetches recastnavigation v1.6.0
    cmake --build build-recast --config Release

Run:

    recast_bake --obj Steelmill.crop.obj.gz --points Steelmill.pts --zmax 190 --jumps \
        --links-out Steelmill.links.txt --out-obj Steelmill.recast.obj --out-bin Steelmill.navmesh > Steelmill.json
    python3 Tools/RecastBake/render_mesh.py Steelmill.recast.obj Steelmill.json Steelmill.links.txt Steelmill.png [x0,y0,x1,y1]

`--points`: lines `spawn|item|probe id x y z`. `--links`: extra links `x0 y0 z0 x1 y1 z1 bidir kind`.
Agent defaults r 0.4, h 1.8, climb 0.5, slope 45, cell 0.2/0.1, tile 128; `--motions 1|2|3` limits generated links to
drop / +jump / +Strength; `--vh` horizontal take-off speed (4.5 run, 6.75 sprint). Level ceilings: `--zmax`
(Steelmill 190), sea bed: `--zmin` (Armada 108).

Verdict and numbers: `Docs/Research/SteelmillTriage/RECAST-BAKE-2026-10-24.md`.
