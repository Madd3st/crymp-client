# DS-044 audit fixes: first primary, withdraw cover, finish lock

Date: 2026-10-27. Base: `09fd9520`. Patch: `patches/ds044_audit_fixes.patch` (three independent
default-OFF flags; apply once) and `patches/ds044_ps_host_rmi.patch` (C-117 Lua re-review, separate).
Before values: c117 `WarsBots-Tactics-c117-on15-0fa9c1cb-...`, 15 bots, 662 s, 293 kills; c116 for the
SOCOM share. Tests: `Tests/BotAuditFixesTests.cpp` (MSVC 19.44, `/std:c++17`) — all pass.

## 1. Time to first primary — `wb_bot_first_primary`

**Before:** first primary p50 **33.7 s**, p90 **74.0 s** (c117); SOCOM held 35.7 % of samples and 34 % of kills (c116).
`UpgradeCandidate` scored pickups by straight distance, so a rifle 10 m through a wall (40-60 m of stairs) won over a
20 m walk.

**Patch:** `BotWeapons::FindNearby` gets an optional `pathCost`; when set, it sums the navmesh `FindPath` corners and
uses `PickupCost(walk, value)`. `UpgradeCandidate` passes it while the bot has no primary and the flag is on.
`BotBrain::Update` calls `UpdateUpgrade(force=true)` before the arbiter/roam when
`ShouldSeekFirstPrimary(flag,!HasPrimary,enemyDistance,socomQA)` (enemy must be > 10 m). `UpdateWeaponSeek` also uses
path cost for an empty pistol.

**Files:** `BotWeapons.h/.cpp`, `BotBrain.h/.cpp`, `BotAuditFixes.h`, `BotConfig.h/.cpp`.
**Gate:** first primary p50 **< 15 s**, p90 < 30 s, SOCOM time **< 15 %**; no kill/stall drop.

## 2. Withdraw that dies — `wb_bot_withdraw_cover`

**Before:** 203 withdraws, **166 deaths within 2.5 s (81.8 %)** (c117; c116 80 %). The bot backed 4 m straight into the
open, aimed, and died.

**Patch:** on the low-HP trigger, `PickCover` (BotTactics) selects a reachable cover spot within **8 m** whose eye ray to
the threat is **blocked** (predicate `!EyeRayClear`), reusing `<Level>.spots.txt` (`CurrentCover`). The withdraw moves to
that spot with `Steer`, keeps `proFire` firing while moving, and sets `NANOMODE_DEFENSE`. If no spot qualifies, the
withdraw is aborted (`withdraw_abort`, `cover=0`) and normal combat continues; `m_nextWithdraw` is still 5 s.

**Files:** `BotBrain.cpp` withdraw block, `BotBrain.h` (`m_withdrawCover*`), `BotConfig` flag.
**Needs:** `wb_bot_tactics 1` + a spots file; without them every withdraw aborts (fight) — that is the specified
fallback, not a bug.
**Gate:** withdraw deaths **< 50 %**; no increase in deaths without a withdraw; cover chosen in > 60 % of triggers.

## 3. Switch-away from <=25 HP — `wb_bot_finish_lock`

**Before:** 185 / 1,035 target choices switched away from a target hit to <=25 HP within 1 s (**17.9 %**; c116 22.4 %).

**Patch:** after the normal target scoring, `KeepLowHpTarget(flag,oldValid,best!=old,old->GetHealth(),now-m_lastSeen,
newDistance,newIsRetaliator,retaliatorActive)` keeps the old target when it is at **<=25 HP** and seen within **1 s**,
unless the new enemy is within **8 m** and is the one hitting us. Works with or without `wb_bot_finish`; logs
`FINISH LOCK keep=...` only with `wb_verbose`.

**Files:** `BotCombat.cpp` (`FindEnemy`), `BotAuditFixes.h`, `BotConfig` flag.
**Gate:** switch-away from <=25 HP **< 5 %**; kills and TTK within noise.

## Probe runner hooks needed (≤ 20 lines)

- `wb_probe_dummy <x> <y> <z> [hp=100] [team=0] [shoot_every_s=0] [weapon=SOCOM]` — spawn a no-AI actor; it does not
  register in `BotMatch`, has no channel and no score; `shoot_every_s=0` = inert. Returns the EntityId.
- `wb_probe_goal <x> <y> <z> [radius=1.5]` — set a nav goal for the selected probe bot; logs `PROBE GOAL reached` with
  the elapsed time.
- `wb_probe_burst <actor> <shots> <interval_s>` — scripted burst on the current weapon (bait/peek probes).
- `wb_probe_peek <actor> <x> <y> <z> <expose_s> <hide_s>` — loop: move/aim to the point for `expose_s`, back for
  `hide_s` (corner peek).
- `wb_probe_cloak <actor> <0|1>` — force cloak mode on a dummy/human; off otherwise.
- `wb_probe_run <id> [json]` — load the scenario (cvars, bots, dummies, items), run `duration_s`, write `summary.json`,
  exit 0/1 by assertions.
- `summary.json` metrics: `deaths`, `time_to_first_shot`, `stalls`, `stuck_events`, `trap_level_max`, `link_attempts`,
  `vantage_holds`, `detected_at_m`, `exposed_s`.
- Determinism: one fresh server per probe, `+wb_probe_local_test 1`, same `wb_seed`; no `SetTimeScale`.

## C-117 Lua re-review (short)

- **Channel 0 is not a bot id.** The new guards `GetChannel() ~= 0` suppress RMI for the **listen-server host**, who can
  also be channel 0: the host would lose `ClEnterBuyZone`/`ClReviveCycle`. `ds044_ps_host_rmi.patch` checks
  `self.botPlayers[id]` instead (the only channel-0 actors that have no client), restoring host RMI.
- Team as a number (`BotMatch.cpp`) and the per-`Register` hook refresh are correct; `WarsBotsPS:Register` still calls
  `ResetScore` before the common `ResetScore` in `BotMatch.cpp` (double reset, harmless but redundant).
- `UpdateReviveQueue`/`ResetRevive` guards patched in the same Lua patch; PS wave remains the owner of the revive.

## Apply and verify

1. `git apply patches/ds044_audit_fixes.patch` on `09fd9520` (checked clean).
2. `git apply patches/ds044_ps_host_rmi.patch` (checked clean; separate file, no overlap).
3. Build; CI runs `BotAuditFixes` (added to `.github/workflows/warsbots-ci.yml`).
4. Run c118-c121 with each flag alone and all three on: report first-primary p50/p90, SOCOM time, withdraw deaths,
   switch-away <=25 HP, kills, stalls. Keep a flag only if its metric moves without a kill/stall regression.

Full DLL not built in this read-only review; the unit test and `git apply --check` are the verification here.
