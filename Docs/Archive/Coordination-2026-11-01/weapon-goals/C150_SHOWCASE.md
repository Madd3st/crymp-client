# C150: local Steelmill and Shore showcase, 2026-10-28

Prepared for the owner and friend watching on the owner's PC. Both packages use exact game source `b8b444d6c858f27a1fdde0d93b25a318cafc6937`, green CI `40709459511`. No local game build or new gameplay run was performed during preparation.

Packages: `C:/Users/Magic8/WarsBotsBuild/WarsBots-Showcase-b8b444d6`. Independent `Steelmill/` and `Shore/` folders and matching ZIP archives. Desktop shortcuts point to each folder's `START.cmd`.

| Profile | What to demonstrate |
| --- | --- |
| Steelmill | Instant Action, eight Normal bots, canonical C119 navigation. Spectator F6/F7 selects bots; F1 first-person view. |
| Shore | Power Struggle, one retained US bot (#2), E223 upper-exit/shore route data from accepted C149. PS bot-watch is unavailable. This demonstrates the route, not a validated full PS match. |

Double-click the corresponding desktop shortcut. Keep its launcher window open. Exit Crysis normally and wait for `Archived and restored` before starting the other map. `CHECK.cmd` validates preparation without starting a game. If the launcher was closed early, close Crysis, then use `RESTORE.cmd`. See each package's `README-RU.txt` for details. Installed Crysis directory is recorded in `game-dir.txt`.

Launchers check pinned payload hashes, local gates, actorless readiness, map and roster before opening the client. Only their own server is stopped. Current Memory/Navmesh files are snapshot, temporarily replaced, archived and restored, including absence and exact filename/hash checks. No game time limit or client termination is used.

Tooling commit `b6930859`; independent DeepSeek DS095 review **OK** at `46f44f26e2e6e8b531857ecd1627da19f946f5cc`. Frozen launcher SHA256 `d2a7cb3065e393a6fa73c9135c868aee809a62cc782993663ed8f7ad38c9097e`. Both PrepareOnly checks, command length checks, filesystem recovery controls (existing/absent data and active-client deferral), package payload hashes, ZIP CRC/payload hashes and shortcut targets passed. [Machine receipt](C150_SHOWCASE_READY.json) records archive hashes and paths.

Steelmill's new eight-bot preset was not freshly played on b8 during packaging. Shore's underlying C149 route passed on b8 and was independently audited by DS094; full-map navigation, lower spawns and a complete Power Struggle match remain outside acceptance. Existing unrelated QA tool edits were preserved.
