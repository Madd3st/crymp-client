1. DS-044 flags/logic match: `BotBrain.cpp:386-394` firstPrimary, `:826-861` withdrawCover, `BotCombat.cpp:241-254` finishLock, `BotWeapons.cpp:111-115` pathCost; defaults 0 (`BotConfig.h:48-51`).
2. `BotAuditFixes.h` + `BotAuditFixesTests.cpp` are unchanged; CI has `BotAuditFixes` (`.github/workflows/warsbots-ci.yml:42`).
3. `BotProbeRunner.h`/`BotProbeRunnerTests.cpp` are content-identical to DS-045 (only BOM stripped); CI has `BotProbeRunner` (`:42`).
4. Lua helper is in place (`InstantAction.lua:23`); no `GetChannel()==0` guard remains in PS/Alert/InstantAction.
5. `PowerStruggleBuying.lua` guards only `:296/1159/1171/1211/1219`; `PowerStruggleRank.lua` only `:189/192`; rest matches ZPatch1/GameData line-for-line.
6. `ds044_ps_host_rmi` is superseded, not duplicated; `WarsBotsIsBot` is the single owner check.
7. `PowerStruggle.lua` guards `:345/523/534/875/925/974/975`; `PowerStruggleAlert.lua` `:656/666/751/774/776/805/826/835`; `TeamInstantAction.lua:525`; all by id.
8. DS-043 P1 stays behind `stateCommit=2` (`BotBrain.cpp:322` BeginTick, `:52-95` Change); no accidental `=1` change.
9. `BotJumpLink total=12`, PS revive delegation/30 s fallback and the 5 s log remain intact.
10. Only residual: full Buying/Rank overrides freeze the current ZPatch1/GameData stock; re-diff them when stock scripts change (no other drop/merge issue).
