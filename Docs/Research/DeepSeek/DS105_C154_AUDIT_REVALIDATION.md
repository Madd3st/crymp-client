# DS-105: revalidation of the owner's historical 96a41b30 audit against exact 5ef9ee22

Historical audit: `Docs/Research/Audits/OWNER_AUDIT_96a41b30_2026-10-29.md` (checkpoint
`96a41b30da44844e4ddba775099b8477587442bb`, single-bot `warsbots/feature/ia-participants`). Current source:
`warsbots/feature/claude-sprint` `5ef9ee22dd2a84896f8ed33cc6c32e6e1261ccb9` (accepted C153 IA head). Every citation
below is from `git show 5ef9ee22:<path>`; I did not read the stale `Code/` on my own branch, did not build, and did not
run the game.

Classification: **fixed in source** = the old defect is gone by inspection; **confirmed remaining** = the defect is
still present; **risk** = contract gap remains, game impact unproven; **tool superseded** = old QA/CI weakness no longer
gates the current acceptance, even if the old script still exists.

| # | Old severity | Current status | Current evidence |
| --- | --- | --- | --- |
| R1 | P1 Lua/C++ bridge | **Fixed in source** | `BotMatch.cpp:62,69,81,89,101` use `Script::CallMethod(table,"Method",...)`; no `Script::Call` table+name call remains in the bot bridge. |
| R2 | P1 damage parity | **Fixed in source; runtime parity unproven** | `InstantAction.lua:20-36` wraps `GetDamageTable`: bot involved + both participants -> `DamagePlayerToPlayer`, human->bot -> `DamagePlayerToAI`, bot->human -> `DamageAIToPlayer`; `BotMatch.cpp:90` sets `warsBotParticipant` before play. No human-vs-bot damage run yet. |
| R3 | P2 round lifecycle | **Confirmed remaining for the Lua round transition** | `InstantAction.lua:1634-1638` (`InGame:OnBeginState`) -> `ResetPlayers(false)` -> `ReviveAllPlayers` (`1486-1494`), but the bot wrapper (`InstantAction.lua:49-57`) deliberately sets `warsBotsConnectionOnly`, and `GetPlayers()` is connection-based, so the bot is neither revived nor score-reset. `BotManager.cpp:537-556` reacts only to native `eAE_resetBegin`; that is level/engine reset, not the IA PreGame->InGame Lua transition. C153's "16 created -> 8 current" is roster recreation on that native event and does **not** prove round-only reset. |
| R4 | P2 ammo search | **Confirmed remaining** | `BotBrain.cpp:337` sets `m_weaponSearchStarted` only in `ResetLife()`. `UpdateWeaponSeek` uses the original life stamp at `744` and `755`: after `m_now - m_weaponSearchStarted >= 6 s`, a newly found `m_weaponTarget` is ignored and the one-per-life fallback path is taken. The C153 upgrade-goal lifecycle (`SetUpgradeTarget`/`UpdateUpgrade`) is a different subsystem and does not touch this clock. |
| R5 | P2 respawn protection | **Fixed in source; runtime unproven** | `BotInstance.cpp:365` calls native `RevivePlayer`, then `376` calls `BotMatch::EquipLikePlayer`; `BotMatch.cpp:93-101` reads `INVULNERABILITY_TIME` and applies `SetInvulnerability(true)` + timeout on the bot's `CNanoSuit`. No post-respawn hit test yet. |
| R6 | P2 scoped diagnostics | **Fixed in source** | `BotManager::TraceHit` (`BotManager.cpp:227-233`) returns early when neither target nor shooter is owned; `LogScores` (`BotMatch.cpp:126-130`) also requires a bot side. Human-vs-human hits no longer emit WarsBots hit lines. |
| R7 | P2 acceptance weakness | **Old script still weak; current acceptance tool superseded it** | `Tools/Test-WarsBotsServer.ps1:37` accepts any TAKEOFF+LANDING pair, `:42-47` any one strafe line, `:52-53` one run/sprint frame, `:69` reports `PhysicalPass` without an attempt id/window. The accepted C153 run used the pinned QA harness on `weapon-goals` (`5e3bf6db`, analyzer `91db7d8f`) with exact source/CI/review/hash receipts, not this script. The old tool must not be quoted as current acceptance evidence. |
| R8 | P2 build provenance | **Risk reduced, not eliminated** | `Tools/Build-WarsBots.ps1:2` still defaults to one `build/warsbots` directory for ON/OFF and writes no manifest. Current acceptance instead pins the exact SHA, full-green CI run, artifact hashes and `c153-quality.json`; CI uploads `unit-logs-<sha>`. Local manual builds remain ambiguous if someone reuses the directory. |
| R9 | P2 jump contract | **Risk confirmed in source** | `BotLocomotion.cpp:103-120` validates clearance/landing for the direction of this frame, then `:139` passes `jumpTraverse=true` while `m_pending||m_airborne`. `BotMovement.cpp:122` skips `SafeStep` in that mode, and `Execute()` recomputes `direction` from the new `intent.destination` every frame. No approved jump direction/target is stored until landing. |

## What C153 did and did not cover

C153 (`5ef9ee22`, CI 40790759287, DS103 PASS) exercised the weapon-goal/upgrade lifecycle, real pickups, combat and
same-slot death->revive in a 90.8 s headless Steelmill run. It did **not** exercise: human->bot damage tables (R2),
the IA round-only reset (R3), ammo exhaustion after a long life (R4), post-respawn invulnerability (R5), or a moving
jump whose destination changes in flight (R9). No statement in the C153 report should be read as covering them.

## Small red/green oracles worth adding (no rewrite)

- **R4 (highest value, cheapest):** extract the `UpdateWeaponSeek` cancellation/search predicate into an engine-free
  helper and drive it with a fake clock. RED on 5ef: at `t=10 s`, a found item yields no move; GREEN after the search
  episode re-stamps `m_weaponSearchStarted` (keep `m_fallbackGiven` one-per-life). Also assert the existing C153
  upgrade cancellation is untouched.
- **R3:** an engine-free state-machine test is weak; the honest oracle is a scenario contract: PreGame (bot damaged or
  dead, >0 ammo spent) -> InGame without level reload -> bot must be revived/reset by an owned hook. Do not use
  `eAE_resetBegin` as a substitute. Implementation should be a scoped C++ hook called from the IA wrapper/round start,
  not membership in `ReviveAllPlayers`.
- **R9:** store `m_jumpDir`/`m_jumpTarget` at acceptance and either re-use it while pending/airborne or re-run
  `JumpSpace` for any changed direction; unit-test the helper with a destination change during `m_pending`.
- **R5:** log/assert invulnerability at revive (`suit->IsInvulnerable()` already appears in the FIRE HIT trace), then
  one controlled post-respawn hit test.
- **R2:** the current wrapper is structurally right; only a paired human/bot damage measurement can close it.
- **R7/R8:** keep the pinned C153 harness as acceptance; add a manifest/SHA line to the old physical smoke tool or
  retire it. Do not reintroduce it as proof.

## Limits

Source revalidation only. I did not compile, build, install, launch a server/client, edit feature/QA files, or read the
owner's live AppData. R2/R5 are source-level fixes whose runtime effect is untested; R3/R4/R9 are open code defects with
concrete file:line evidence above.
