# Withdraw deaths on C-121 (DS-049 #2)

Raw: `build/c121-on15-d6a9a521/candidate-r1-n15.wbdemo` (20 Hz frames+brain+kills), the matching `.bots.log`,
`Steelmill.spots.txt`. 104 actual withdraws; **72 die within 2.5 s**. For each death: the brain row at the withdraw
start gives `dest` = the chosen cover and `target` = the threat we hid from; the kill event gives the shooter; the
frames give both positions.

## Per-death result

| Measure | Count / value |
| --- | ---: |
| matched deaths | 72 |
| shooter == the threat we hid from | **44 (61 %)** |
| shooter = another enemy (third party) | **28 (39 %)** |
| died within 1.5 m of the cover | 25 (35 %) |
| died en route to cover | 47 (65 %) |
| median time to cover | **1.39 s** |
| median cover -> shooter at death | 12.3 m |
| median victim -> shooter at death | 10.9 m |
| shooter inside the cover's protected sector | 20 |
| shooter outside the protected sector | 52 |
| cover spot not resolvable (dest 0 / no spot) | 15 |

## What it means

1. **The static cover is wrong by the time the bot arrives.** 52/72 shooters are outside the mask sector the spot
   was chosen for. 44 of those are the *same* threat: it moved during the 1.4 s approach, and the code never
   re-evaluates the spot (`BotBrain.cpp:837-848` picks once, then only moves for 2.5 s).
2. **A third of the deaths are crossfire.** 28/72 were shot by an enemy the bot was not hiding from; a closer-threat
   spot bake cannot fix those.
3. **Low cover is a trap.** ~10 of the 25 at-cover deaths are at spots with flag 2 only: the mask says body-covered,
   the standing eye ray is clear. The bot has no crouch, so those spots do not hide it.
4. **65 % die before reaching cover**, median 1.39 s: speed is not the main lever; direction/choice is.

## Patch `ds049.patch`: `wb_bot_withdraw_full` (default OFF)

- `CoverCandidate` now carries the spot `flags` (`BotNavMesh.cpp:145`), and `PickCover` takes `minFlags`
  (`BotTactics.h:23,28`).
- With `wb_bot_withdraw_full=1` the first pick uses **full cover only** (flag 1). During the 2.5 s withdraw it
  **re-picks every 0.5 s against the live threat** (`BotBrain.cpp:926-927` start pick with `minFlags`, `:938-955`
  re-pick, spot changes only if > 1.5 m to avoid oscillation); if no full cover exists it aborts and fights instead
  of running to low cover.
- Test `BotTacticsTests.cpp`: full-cover filter selects the flag-1 spot, low-only is rejected. MSVC pass.
- Expected: the low-cover at-cover deaths become aborts (not counted as withdraw deaths), and the moved-threat cases
  can switch to a spot that still protects the new angle. The 28 third-party deaths remain; target < 50 % needs a run.
- No armor item exists in the PS buyList; the free Armor suit mode is already set by the withdraw branch.
