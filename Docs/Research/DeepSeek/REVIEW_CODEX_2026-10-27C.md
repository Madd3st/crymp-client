# Review: Codex claude-sprint 815bcb7e, 96f7f624, ba4d5f4e (DS-055)

Prompt C / D-031: Codex coordinates, DeepSeek reviews every new claude-sprint SHA. Reviewed against
`warsbots/research/deepseek` patches and the C-137/C-138/C-139 raw reports.

## 815bcb7e (C-136)

* Initial spawn: `BotManager::AddWith` now picks `GetTeamDefaultSpawnGroup(TeamForSlot(number))` and
  `GetSpawnLocation(0, true, false, group, ...)` (`BotManager.cpp:383-401`), no IA-picker fallback in PS. This is
  the missing DS-052 #1 fix; X-186 reports 12/12 own-base spawns. **OK.**
* Buy chooser: `smg` 75 / `shotgun` 50 / `lam` 25 / `shotgunshell` 5 / `smgbullet` 5 match stock
  `PowerStruggleBuying.lua:38,39,110,111,145`. **OK.**
* **BLOCK:** `PSTryBuy` reads the Lua `WarsBotsBuy` result through `int ok=0; EndCall(ok)` (`BotBrain.cpp:527`).
  `WarsBotsBuy` returns a boolean (`PowerStruggleBuying.lua:490`, `:1033`), so `EndCall(int)` leaves 0 on success.
  X-192: purchases happened (PP 100->25/50, SMG/Shotgun on all 12 actors) while `ok=0`; therefore `primary` is
  false and the new laser branch (`:529`) never runs; C-139 laser 0/6. Fix: `bool ok=false; EndCall(ok);`.
  Patch: `patches/ds055_buy_return.patch`, applies on `ba4d5f4e`.

## 96f7f624 (DS-052 integration)

* Encoding: no BOM in the changed C++/Lua; the stock PowerStruggle files keep their single Latin-1 byte (Márcio),
  LF preserved. CI list still has `BotPSBuy`; 43 suites pass per Codex. **OK.**
* Flags: `wb_ps_doors`, `wb_ps_vehicles`, `wb_ps_events` all default 0 (`BotConfig.h:59-61`). **OK** on default
  OFF; `wb_ps_events` is a diagnostic without a named exit date (net-zero rule note, not a code blocker).
* **BLOCK:** doors call `Script::CallMethod(dt,"OnUsed",...)` (`BotBrain.cpp:585`). Stock `Door:OnUsed` is the
  client path and sends `self.server:SvRequestOpen` (`Door.lua:356`); from a bot it reaches the client RMI
  dispatcher, exactly C-138 X-190 ("RMI via client ... not a client"). DS-053's door hunk is the same bad path
  and must not be integrated. DS-054 (`6a74b8e5`, `patches/ds054_doors.patch`) calls the server `Door:Open(user,1)`
  directly, open-only + 2 s per-door cooldown; it applies clean on `ba4d5f4e`. Integrate it first.
* Vehicle path is not exercised by X-190 (0 pairs); no verdict yet. It is behind `wb_ps_vehicles 0`.

## ba4d5f4e (C-139)

* Alive buy at spawn: `UpdateRoam` tries every 2 s for 20 s and sets `m_psBought` on a primary (`BotBrain.cpp:809-812`);
  `ChoosePsBuy` no longer refuses alive (`BotPSBuy.h:39`). Logic is sound, but it inherits the bool-return BLOCK
  above, so C-139's laser gate is not proven.
* No new flags; `wb_ps_buy`/`wb_ps_events` remain 0. Encoding/CI unchanged.

## Verdict

* `review 815bcb7e: BLOCK` - `BotBrain.cpp:527` int/boolean result; patch `ds055_buy_return.patch`.
* `review 96f7f624: BLOCK` - `BotBrain.cpp:585` Door:OnUsed client RMI (C-138); integrate `ds054_doors.patch`.
* `review ba4d5f4e: BLOCK` - inherits both; no new issue.
* After both patches: C-139 laser gate and C-138 door gate can be rerun. No other lost DS-052 hunk found.
