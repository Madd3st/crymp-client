Tactical spots v1 (CS-style hiding/sniper spots, Killzone-style position data), from `tactical_spots.cpp`:
`tactical_spots meshes_links/<map>.navmesh <crop.obj> <map>.pts spots/<map>.spots.txt` (C019 collision crops, spawn
`.pts`; only polygons reachable from spawns). Flags: 1 cover (1..11 of 16 sides protected at chest height within 1.5 m,
≥ 3 open), 2 low cover (shoot over), 4 sniper (view of random targets 15–150 m in the top 5 % and ≥ 3 m above the median
floor), 8 exposed. Steelmill render: grey = floor (lighter = higher), green = cover, red rings = sniper, blue = spawns.
Runtime use (next): Marksman/Defender with DSG1/Gauss go to sniper spots, angle holding from cover spots.
