# Why snipers drop DSG1 after the FY71 (DS-038)

Date: 2026-10-26. Read-only at `claude-sprint` `2fee59c2`. Evidence: C-111 `sniper_reach_dsg1` / `gauss` probe logs and
`probe-result.json` (recipe), `c111-state-comparison.json`. Patch: `patches/ds038_sniper_upgrade_gate.patch`.
**Re-check 2026-10-27:** still applies cleanly to `fd12a5eb` (and together with `ds040_elite_nofire.patch`).

## Verdict

The bot does not "drop" DSG1 — it **never searches for another weapon after it has any primary**, unless the goal
arbiter is enabled. The C-111 sniper probes do **not** enable `wb_bot_goal_arbiter`, so the arbiter path never runs; the
legacy `UpdateUpgrade` gate only searches while `!HasPrimary`. After the FY71 at 1.36 s, `hasPrimary=true`, the gate
closes, and no upgrade candidate is evaluated again for the rest of the 75 s.

## The exact gate

| Site | Code | Effect |
| --- | --- | --- |
| `BotBrain.cpp:343` | `if (BotConfig::Get().goalArbiter==1 && ... UpdateArbitrated ...)` | arbiter path is skipped when the flag is 0 (default, `BotConfig.h:31`) |
| `BotBrain.cpp:502` | `force \|\| ((!enemy \|\| !m_combat.Visible()) && !BotWeapons::HasPrimary(actor))` | with a primary and no arbiter force, `UpdateUpgrade` returns before `UpgradeCandidate` |
| `BotBrain.cpp:1340` | `(!hasPrimary \|\| ranked) && ... UpgradeCandidate` | the only search that would run with a primary is inside the arbiter path |
| `BotBrain.cpp:485` | `PickupRange() = camper>=0.7 ? 60 : 20` | an Elite marksman is sniper-minded (range 60), so the choice math *would* prefer DSG1 |

Probe recipe (`probe-result.json` → `recipe`) lists `wb_bot_navmesh`, `wb_bot_jump_links`, `wb_bot_weapon_rank`,
`wb_bot_path_progress`, `wb_bot_vantage`, `wb_bot_state_commit` — **no `wb_bot_goal_arbiter`**. The DSG1 log has no
`ARBITER` line at all, and only one `REAL PICKUP`:

```
1.36 [WarsBots] REAL PICKUP actor=65501 item=109 acquired=1.
```

`item=109` is the FY71. After 1.36 s: no `ARBITER`, no `WEAPON RANK`, no `GOAL SKIP` (the only one is at 0.23 s before
the pickup), and `unreachable=0` in `probe-result.json`. That is the signature of "the search never ran", not "the
search ran and rejected DSG1".

## Why the search would choose DSG1 if it ran

At the probe spawn (1316.9, 1339.0, 138.8), held FY71, typical range 60 m:

| Item | Distance | Value over FY71 at 60 m | `PickupCost` |
| --- | ---: | ---: | ---: |
| DSG1 | 24.0 m | 0.354 | **9.85** |
| Gauss | 91.0 m | 0.454 | 72.85 |
| SCAR (nearest) | 41.7 m | 0.170 | 34.88 |

`PickupCost = distance - value*40`, so the DSG1 is the cheapest upgrade; the nearby FY71 at 7.2 m was the correct first
pickup (it beat the SOCOM by a large margin), but it must not close the search. This is reproduced by
`Tests/BotUpgradeGateTests.cpp` (MSVC: `DSG1 cost 9.85, SCAR 34.88`).

## Patch

`Docs/Research/DeepSeek/patches/ds038_sniper_upgrade_gate.patch` (applies cleanly to `2fee59c2`):

1. New engine-free `Code/CryGame/Bots/BotUpgradeGate.h`:
   ```
   ShouldSearchUpgrade(hasPrimary, ranked, sniperMind, force, enemyVisible)
     force -> true
     enemyVisible -> false
     !hasPrimary -> true
     ranked && sniperMind -> true   // a marksman with a rifle still wants the sniper rifle
   ```
2. `BotBrain.cpp:502` uses it with `sniperMind = PickupRange() > 30` (Elite marksman 60). No search while an enemy is
   visible unless the arbiter forces a weapon tick; no behavior change for non-snipers or for a bot with no primary.
3. New `Tests/BotUpgradeGateTests.cpp` covers the gate and the DSG1/SCAR/Gauss cost; built and passed.

## Which probe line proves it

`build/c111-sniper_reach_dsg1-ec46f1a3/Probe-sniper_reach_dsg1-19ae05bd-repeat-1.bots.log:36` is the FY71 pickup;
there is no `ARBITER`/`WEAPON RANK`/`GOAL SKIP` after it. With the patch, the next 3 s search (`m_nextUpgradeSearch`)
must produce `WEAPON RANK ... select=DSG1` or `DSG1 ... unreachable`; the probe then either picks it up or reports a
real unreachable event, instead of silence.

## After the patch

- Run `sniper_reach_dsg1` and `sniper_reach_gauss` with the same recipe; the assertion `pickup:DSG1 <= 45 s` /
  `pickup:GaussRifle <= 45 s` should now be reachable in principle. If it still fails, the log must show a
  `GOAL SKIP navmesh kind=weapon` or `weapon ... unreachable` for the sniper item — that is a different cause (path /
  ban), not this gate.
- Keep `wb_bot_goal_arbiter` optional: the patch makes the no-arbiter path behave like the arbiter's
  `(!hasPrimary || ranked)` rule, so both configurations are testable.
- The Gauss probe’s `link:failed=3`, `landing_z=136` is a separate link-quality issue (DS-033 / filtered links), not
  this gate.
