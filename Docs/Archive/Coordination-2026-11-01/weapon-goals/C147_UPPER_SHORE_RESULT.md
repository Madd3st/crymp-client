# C147 upper exit and shore: one shared native run

Exact claude-sprint **b06681eaa4365afe5bf7309994012dce13a7ac83**, CI40649771503GREEN, independent code reviewDS081dd68f2ce, harness reviewDS08645d78996. No code patch applied: B1staged, speculative Strength speed patch held. Temporary map E2235993 plus matching oneStrengthlink/manifest; nav1/jump_links1/ps_capture1, doors0/vehicles0. Actorless effective-recipe preflight passed; ONE actual native US#2 observation (startup add2/remNK1 settled toone), 301.68wallseconds. No force spawn/teleport/client game/local game build.

**Partial result: upper exit PASS; full route to dry shore FAIL.**

- 0.28: native PS capture target654 selected, destination1794.5,1690.6,101.5.
- 0.93: Strength link starts; 4.07 native Strength jump; **5.55 LINK done** at2114.53,1885.63,105.92. Existing executor succeeds; do not apply speculative speed patch.
- **14.027 sea entry**, ShouldSwim1/waterZ99.45 at2054.337,1880.289,97.566. Continues swimming, health100 throughout. Native body feet typicallyz98.0 while waterplane~99.5.
- Navigation then falls back142times:127start_unprojected,14no_route,1other. Later goals are fallback roam/NKspawn rather than consistent capture654 navigation. By~130s reaches x~2120,y~1620, then loops near x2115..2134/y1595..1620. Lastsample299.791 at2115.825,1615.310,98.101, stillswim1. No dry land candidate; cannot claim shore/capture/Playable.
- Suspected common cause to investigate, not yet a proven fix: NavMeshCore Nearest permits only1m above actorfeet, whereas baked water surface is~1.5–1.8m above native swimmerfeet. This can fail to project or select a different submerged component; inspect capture Reachable and path start together. Do not globally widen queries across stacked submarine decks or change actorworldposition.

No Lua/script/RMI/errors observed.1199native traces,1136swim samples. WBDM2 demo verified5984frames, SHA2564dbfcb6f9e787955609bbfe92ddfb1b586e79f28bd4204094a5b117434c74203. Local archive `C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build/c147-upper-shore-b06681ea-v1`: logs, demo, args, effective recipe, summary.json, trace.json, memory/nav before-after/restored receipts. **Independent current-memory/nav exact filenames AND hashes match fresh pre-run snapshots**, including removing temporary Shore data. No active game retained. C146oldermemorydrift untouched; current snapshot preserved.

[Machine-readable result](C147_UPPER_SHORE_RESULT.json). Next DS087 joint offline water projection/goal/shore diagnosis and minimal coherent repair batch with red/green real-core oracles before new CI/review/run. No unchanged repeat or separate water-only game.
