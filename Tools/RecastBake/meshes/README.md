Detour tiles (RecastDemo MSET v1) baked by `Tools/RecastBake/recast_bake` from the C-019/C-028 collision crops:
`--cs 0.1 --tile 256 --radius 0.3` (+ `--zmax 190` Steelmill, `--zmin 108` Armada), walk-only (no off-mesh links yet: the
jump/lift executors come later). Walk-only spawn coverage: Steelmill 36/36, Quarry 30/32, Outpost 35/35, Armada 19/40.
Install with `Tools/Install-WarsBotsNavmesh.ps1`; enable with `wb_bot_navmesh 1`.
