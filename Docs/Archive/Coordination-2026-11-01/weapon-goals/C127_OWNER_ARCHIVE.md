# C127 / C133 owner Shore archive

Exact claude-sprint **4f3a1d72e1269a644c3e602701ca170b5b59d044**, CI40526830694. Owner explicitly finished; client already closed. Only session server PID8396 was stopped after path/log identity verification. No new game, build, GUI or game-code edit.

Archive: `C:\Users\Magic8\WarsBotsBuild\crymp-multibot\build\owner-playtests\C127-4f3a1d72-20261027-185023`. Verified mirror: `C:/Users/Magic8/AppData/Local/WarsBots/Logs/C127-4f3a1d72-20261027-185023/`. Both include server/client engine and MOVE logs, demo, PS events/error stacks, native FRAME windows, exact BotBrain source snapshot, full recipe/effective CVars/session arguments, original and final memory, Triage summary, HTML report, offline analyzer and SHA256 manifest (32 files).

Native demo: **9820 frames / 493.691s**. Whole-server recording includes prejoin/postexit. Human FRAME-presence window 14.414..407.106s includes dead/spectator intervals and is not a claim of continuous active play. All six bots Normal, 3/team. Navmesh/jump_links explicitly0 under C133's relaxed first-look recipe.

Server evidence: native demo kills7, bot deaths/wave-seen **6/6**, stalls126, traps>=60s1; no server/client crash marker, no channel errors. **Three script type-error episodes (12 stack/error lines)**: `CheckVirtualInventoryRestrictions(inventory, itemClassName)` expects argument2 String, receives Null. Preserved line-number context in `PS-error-context.log`. Adjacent buy logs: two shotgun at PP100, one macs at PP200; all primary/ammo ok0. Three other buys skipped at PP0. Successful bot purchases0; completed captures per team and total PP spent UNKNOWN. Target-selection messages do not prove completed captures.

Smoke was error-free but live owner-session buying exercised a failure; do not reuse smoke PASS as owner-session Lua PASS. No diagnosis/fix asserted from adjacency alone. Please Claude/DeepSeek review this exact stack and item identifiers before a new named build.

49 steady native FRAME windows: mean(window average)31.882ms, mean(window p95)47.327ms, max169.26ms, mean botAI0.114ms/frame. Mean window p95 is not pooled p95. Client reload/FP/net/F8 evidence and owner-window actor K/D are in [C127_OWNER_ARCHIVE.json](C127_OWNER_ARCHIVE.json); visual quality and correction snaps remain UNKNOWN.

The pre-session manifest contained only Steelmill.mem: restored and actual SHA256 verified. All final memory is archived before restore. Three files absent from that snapshot were retained (Steelmill.nav.json, Plantation.mem, Shore.mem); no assumption that this session created all three and no deletion of learned/foreign data. Original raw LOCALAPPDATA inputs remain. Foreign dirty Recording.ps1 preserved.
