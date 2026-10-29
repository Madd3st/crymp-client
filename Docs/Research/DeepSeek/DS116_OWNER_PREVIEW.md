# DS116 — Owner live-preview delivery plan (Shore lower route, after the shared route passes)

Status: read-only proposal. No game/install/QA/code writes. Nothing here is executed by DeepSeek.
Trigger: only after the one shared `abac72a9` lower-545 route passes and its archive audit is clean.

## 1. Goal

Give the owner the shortest safe way to *watch* the C154 lower-545 route (native grab -> climb -> native
animated exit -> settle -> stairs/Strength -> water -> dry shore) on the reviewed binaries, before any wiki/docs
work. It is a preview, not an acceptance run, and it must not overwrite the frozen C150 package.

## 2. What exists (read-only inspection)

- Frozen package `C:/Users/Magic8/WarsBotsBuild/WarsBots-Showcase-b8b444d6/Shore` (source `b8b444d6`, CI
  `40709459511`, pair `ae9ee3fa…`/`8fd34b53…`), files: `Run.ps1` (copy of
  `Tools/Launch-WarsBotsShowcase.ps1`), `package-manifest.json`, `build.json`, `preset.json`, `Data/`,
  `START.cmd`/`RESTORE.cmd`/`CHECK.cmd`, `game-dir.txt`, README/SHORE-KNOWN-ISSUE.
- `Run.ps1` flow: verify package hashes -> verify `build.json` source/CI -> snapshot `%LOCALAPPDATA%/WarsBots/
  {Memory,Navmesh}` -> actorless preflight server -> stop it -> install `Data/*` into Navmesh -> server with
  `+wb_add 2 normal +wb_remove 1` -> readiness -> start client -> wait -> on exit stop own server, archive
  logs/demo, restore Memory/Navmesh exactly (`archived_restored`).
- Server console commands are startup-only; there is no RCON and none will be invented. `wb_bot_start`/`wb_add`
  are server-side. One existing command already supports a delayed diagnostic action:
  `wb_revive_at after <1..300 s> <spawn> <bot>` (`BotManager.cpp:951-959`, Lua `Script.SetTimer` ->
  `System.ExecuteCommand`), used as the disclosed one-shot native revive in C154 (`+wb_revive_at 545 2`).

## 3. C150 details that must NOT be copied blindly

- Readiness `NAVMESH link kinds 1/1` -> V5 Shore has **2/2** links; the old gate fails on correct V5 data.
- Readiness `#2 created … z >= 99.5` (upper-route spawn) -> the preview deliberately spawns the default bot and
  later revives it at 545 (z ~90); the gate must not reject that.
- `polys: 46614` -> V5 loads **46830** polys.
- `build.json` pins (`b8b444d6`/`40709459511`/old pair) and the C150 showcase flag set (`wb_ps_*`, no
  `wb_bot_ladder`) -> must be replaced by the abac source/new CI/new pair and the C154 effective ladder set.
- Labels/paths: a new package directory and labels; the C150 `Shore` folder stays untouched (no in-place edit,
  no overwrite, no shared `last-session.txt`).

## 4. Proposed package (built by Codex, reviewed before delivery)

`C:/Users/Magic8/WarsBotsBuild/WarsBots-Preview-abac72a9/` (versioned by source SHA), containing:

- `CryMP-Client64.exe`, `CryMP-Server64.exe` from the **new** green CI run for `abac72a9` (pair hashes recorded);
- `Data/` = the frozen V5 four files (nav `b601…`, links `0230…`/fnv `ece812…`, spots `d95e…`, manifest `a95a…`);
- `package-manifest.json` = sha256 of every packaged file (verify before launch, as C150 does);
- `build.json` = `{source:"abac72a9…", ci:<new green run>, ci_success:true, artifacts:{client,server}, manifest_receipt:"c154-abac-manifest/v5 result.json sha256"}`;
- `preset.json` = Shore/PS, `level:"multiplayer/ps/Shore"`, new port (e.g. 46261), `label:"P154A"`,
  `polys:46830`, `retained_bots:1`, C154 effective flags (`wb_probe_local_test 1`, `wb_bot_navmesh 1`,
  `wb_bot_jump_links 1`, **`wb_bot_ladder 1`**, corners=1, jump_sanity=1, doors/vehicles 0, demo 20 Hz,
  `wb_move_trace 1`, `wb_log 2`, …) and `critical` = the 16 values from the route's `effective.json`;
- `preview.json` = `{delay_seconds:60, spawn:545, bot:2, expected:"approach->climb->exit->settle"}` so the delay
  is data, not a code change;
- `Run.ps1` (adapted copy), `START.cmd`, `RESTORE.cmd`, `CHECK.cmd`, `README-RU-PREVIEW.txt`;
- licences kept as in C150.

## 5. Launcher adaptation (minimal, relative to the C150 `Run.ps1`)

1. Keep all package-integrity, PID-ownership (PID + start time), "existing game retained", port-free and
   snapshot/restore logic unchanged; keep the actorless preflight and exact Memory/Navmesh restore.
2. Server roster becomes `+wb_add 2 normal +wb_remove 1 +wb_revive_at after <delay> 545 2 +wb_list +wb_status`
   so the lower-545 native revive happens **after** the owner's client is in game. No RCON, no new command, no
   custom coordinates, no teleport.
3. Readiness gates: `NAVMESH loaded … Shore: 46830 polys`, `NAVMESH links ON:`, `NAVMESH link kinds 2/2`,
   `#2 created EntityId=`, `#1 ENTITY_EVENT_DONE`, `FRAME … bots=1 server=1`; **no** upper-spawn z check.
   After launch, tail the server log and print `route started (revive at 545)` when the revive lands.
4. Client starts right after readiness exactly as in C150 (`+connect 127.0.0.1:<port>`, `wb_log 2`,
   `wb_move_trace 1`); the launcher prints the wall-clock time of the scheduled revive, e.g.
   “watch the lower ladder at 19:04:12”.
5. Keep the cleanup: stop only our server (PID+start), archive logs/demo, restore Memory/Navmesh exactly,
   `archived_restored`. `RESTORE.cmd` remains the manual recovery path.
6. Namespace honesty: the launcher and the game it starts share one context, so snapshot/restore targets the
   files the game actually reads. Record `logical_localappdata` + `namespace` in the session like C154 does
   (Codex shell -> `…/Packages/OpenAI.Codex_2p2nqsd0c76g0/LocalCache/Local/WarsBots`; owner shell -> plain
   `%LOCALAPPDATA%`). Never mix: do not start the game from a different context than the launcher.

## 6. What the owner will see (timeline, provisional)

`T+0` launcher starts preflight and snapshots data; `T+~15 s` server up, bot spawns; `T+~20 s` client starts;
`T+60 s` (relative to server start) the scheduled revive places the bot at 545; approach ~2 s, climb ~9 s, native
exit + settle; then the remaining route. If the client is not in game by the revive, the owner missed the climb —
the honest remedy is “close and run again”, not a hidden retry.

## 7. Honest limits (to be repeated at delivery)

- Preview only: one route on the exact reviewed pair + V5; no full PS match, no client-code acceptance, no
  default promotion, no upper-route claim.
- The route outcome is whatever the server log shows; the launcher does not label it PASS.
- Nothing is installed into the game directory; data lives in the session snapshot and is restored exactly.
- C150 b8 package remains frozen and untouched.
- DeepSeek does not run it; owner runs it when the PC is free.

## 8. Command to deliver after the route passes (template, frozen at delivery)

```
powershell -NoProfile -ExecutionPolicy Bypass -File "C:\Users\Magic8\WarsBotsBuild\WarsBots-Preview-abac72a9\Run.ps1"
```
with `-PrepareOnly` as a reviewed dry run, `START.cmd` / `RESTORE.cmd` / `CHECK.cmd` next to it. The delivered
text will pin: package directory, source `abac72a9…`, CI run id, pair hashes, V5 data hashes, port, delay, and
the expected revive wall-clock line.
