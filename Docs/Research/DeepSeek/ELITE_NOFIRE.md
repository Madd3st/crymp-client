# Why Elite sometimes stops shooting (DS-040)

Date: 2026-10-26. Owner playtest 4: `C112-2fee59c2-20261026-225932` (8 bots, 4 Elite, 21.3 min).
Logs: `...\Packages\OpenAI.Codex_2p2nqsd0c76g0\LocalCache\Local\WarsBots\Logs\C112-…-Server.bots.log`.
No game build. Patch: `patches/ds040_elite_nofire.patch` (applies to `2fee59c2`).

## Measurement caveat

C112 was launched **without fire diagnostics**: the server log has **0 `FIRE SHOT` and 0 `FIRE HIT`**. So "no
`FIRE SHOT`" cannot be read literally. I used the existing signals:

- `AIM ZONE actor=… target=…` at 0.5 s as "a visible target is being aimed at";
- `Stock weapon burst requested weapon=… target=…` as the actual shot/trigger request;
- `TACTIC SAMPLE` for `skill/style/weapon/suit/distance/action/visible`;
- `AI telemetry … weapon=… ammo=…` for the current gun and ammunition;
- `BOT SUIT mode=` for the suit; `WEAPON RANK` for switches.

A no-fire stretch = a gap ≥ 0.7 s between two `AIM ZONE` samples of the same Elite with **no** burst from that Elite,
while the last `TACTIC SAMPLE` says `visible=1` and `action` is `ATTACKING`/`CHASING_ENEMY` and distance ≤ 100 m
(Elite fire range). This is conservative; it may miss gaps where `AIM ZONE` is not logged, and it cannot see
`ShotBlocked` because that is not logged.

## Counts (all 4 Elite, 21.3 min)

**53 stretches / 2.49 per minute**, far above the proposed gate 0.3/min.

| Cause (best available signal) | Stretches | Seconds | What it means |
| --- | ---: | ---: | --- |
| **Speed suit** (`BOT SUIT mode=3`) | 28 | **191.7** | visible target, attacking, but the suit stayed Speed; the policy should have chosen Armor |
| **Sidearm** (current weapon SOCOM) | 13 | 38.4 | Elite holding the pistol in an active fight instead of a primary |
| **Out of ammo** (`AI telemetry ammo=0`) | 9 | 50.1 | clip empty, no reserve; bot stands/aims instead of switching |
| **Chase far** (CHASING, >38 m) | 2 | 3.1 | closing, not yet in hold band |
| **Weapon switch** (`WEAPON RANK`) | 1 | 21.3 | deliberate switch mid-fight |

Typical episodes:

| t (s) | Bot | Gap | Weapon/ammo | Suit | Distance | Evidence |
| ---: | ---: | ---: | --- | ---: | ---: | --- |
| 16.42–17.84 | 4 | 1.42 | SCAR / 27 | Speed | 14.8 | `AIM ZONE` at 15.80/16.42/17.84, shot only at 17.97; view yaw swept -130→170° |
| 46.92–66.52 | 4 | 19.6 | SOCOM / 8 | Speed | 11.4 | `AIM ZONE` then `WEAPON RANK`/`MOVING_TO_WEAPON`; sidearm |
| 114.04–131.10 | 8 | 17.06 | FY71 / 7 | Speed | 29.0 | attacking, no burst, no switch |
| 318.68–340.87 | 4 | 22.19 | SCAR / 40 | Speed | 17.7 | attacking, no burst |
| 918.26–932.59 | 4 | 14.33 | FY71 / 0 | Speed | 10.8 | `AI telemetry ammo=0` |
| 1038.64–1094.04 | 4 | 55.4 | FY71 / 4 | Speed | 16.2 | attacking, no burst; longest gap |

The 55 s and 22 s gaps cannot be explained by burst cadence or reaction; they need the new diagnostics (below) to
separate muzzle-block from aim-gate. The 19.6 s and 21.3 s gaps are weapon-goal/sidearm; the 14.3 s gap is empty ammo.

## Code gates that can produce this

| Gate | File:line | Effect |
| --- | --- | --- |
| muzzle ray | `BotCombat.cpp:279` | eyes see the target, barrel blocked → `StopFire`, no log today |
| ammo/reload | `BotCombat.cpp:280` | empty clip/reserve → `StopFire`, then the bot keeps aiming |
| fire tolerance | `BotCombat.cpp:286-288` | Elite 2°; while the view settles, no shot |
| Fire vs Aim | `BotBrain.cpp:754,859,883` | one reaction clock (DS-035) but still a hard `Aim` until it elapses |
| weapon goal in a fight | `BotBrain.cpp:1340-1355` | arbiter can choose Weapon with a visible enemy, causing sidearm/switch gaps |
| suit policy | `BotSuitPolicy.h` + `BotBrain.cpp:1320-1330` | should be Armor when `enemyVisible`, but C112 stayed Speed; a timer/overwrite leaves Speed |

`ShotBlocked`, `zoom` state, clip/reserve and `fireDot` are not in the C112 trace, so the patch adds them.

## Patch (`ds040_elite_nofire.patch`)

1. **`NOFIRE` diagnostics** in `BotCombat::Attack` (rate-limited 0.5 s):
   `reason=blocked|ammo|aim`, actor, target, distance, clip, tolerance and dot. This makes the next run classifiable
   without guessing.
2. **Close-range body gate**: Elite keeps 2° at range, but at < 20 m a clear target gets `max(tol, 5°)`. A 2° gate at
   14 m is 0.5 m; the view settling cost 1.4 s in the 16.42 episode. This only widens the close body gate, not the
   long-range head gate.
3. **Empty weapon**: on `!HasAmmunition`, call `BotWeapons::SelectAvailable(actor)` before returning; no more
   14-55 s aiming with 0 rounds.
4. **No weapon shopping in a fight**: in `UpdateArbitrated`, when `enemyVisible && enemyDistance <= holdRange`, zero
   `weaponGoalAvailable/Value`; the arbiter must choose Fight, not Weapon. Removes the 19.6 s sidearm gap.
5. **Armor on a visible enemy**: if the suit policy chose Speed while `enemyVisible`, override to
   `NANOMODE_DEFENSE` (link Strength jumps exempt). This addresses the 191.7 s Speed cluster.

## Gate after the patch

- `NOFIRE` lines per Elite per minute, split by reason; target **< 0.3 visible combat no-fire/min**.
- `speed_suit` gaps should disappear; `sidearm`/`weapon_switch` gaps should drop to ~0; `ammo` gaps must be followed
  by a `WEAPON RANK`/pickup within 1 s.
- If `blocked` dominates after the patch, the next fix is `ShotBlocked` step-out timing (currently 0.4 s in
  `UpdateAttacking`), not the fire gate.

## Missing fields to add to the recorder

- `FIRE SHOT` with shooter/target/class/distance/`wb_fire_diagnostics` in every run (C112 had none).
- `NOFIRE` with `reason`, `clear`, `fireDot`, `tolDeg`, clip/reserve, `weaponClass`, `zoom`/`zooming`, `suitMode`,
  `state`, `selectedAge`.
- `ShotBlocked` per actor/target/obstruction (currently only `m_shotBlocked` internally).
- Reload start/end and weapon switch reason.
- `BOT SUIT` reason (`SuitReason`) and refused mode (`ACTION REFUSED`) on the same line as state.
