# Why Elite sometimes stops shooting (DS-040)

Date: 2026-10-26. Owner playtest 4: `C112-2fee59c2-20261026-225932` (8 bots, 4 Elite, 21.3 min).
Logs: `...\Packages\OpenAI.Codex_2p2nqsd0c76g0\LocalCache\Local\WarsBots\Logs\C112-…-Server.bots.log`.
No game build. Patch: `patches/ds040_elite_nofire.patch` (applies to `2fee59c2`).

## Correction (2026-10-27, before DS-041)

The first version of this report read `BOT SUIT mode=3` as Speed. `ENanoMode` in `NanoSuit.h:37-47` is
`0=SPEED, 1=STRENGTH, 2=CLOAK, 3=DEFENSE`. **mode 3 is Armor.** 50 of the 53 gap windows were Armor and 3 were
Speed, so the suit is **not** the cause and the "191.7 s Speed" number is retracted. The cause table below is
re-derived with the bot's own `MOVE TRACE sprint=` field, which is the real blocker: native fire is disabled while
sprinting, so `weapon->CanFire()` stays false. The patch item "force Armor" was replaced by "drop sprint".

## Measurement caveat

C112 was launched **without fire diagnostics**: the server log has **0 `FIRE SHOT` and 0 `FIRE HIT`**. So "no
`FIRE SHOT`" cannot be read literally. I used the existing signals:

- `AIM ZONE actor=… target=…` at 0.5 s as "a visible target is being aimed at";
- `Stock weapon burst requested weapon=… target=…` as the actual shot/trigger request;
- `TACTIC SAMPLE` for `skill/style/weapon/suit/distance/action/visible`;
- `AI telemetry … weapon=… ammo=…` for the current gun and ammunition;
- `MOVE TRACE` (`sprint=`, `mode=`, `speed=`) for gait and suit;
- `WEAPON RANK` for switches.

A no-fire stretch = a gap ≥ 0.7 s between two `AIM ZONE` samples of the same Elite with **no** burst from that Elite,
while the last `TACTIC SAMPLE` says `visible=1` and `action` is `ATTACKING`/`CHASING_ENEMY` and distance ≤ 100 m
(Elite fire range). This is conservative; it may miss gaps where `AIM ZONE` is not logged, and it cannot see
`ShotBlocked` because that is not logged.

## Counts (all 4 Elite, 21.3 min, corrected)

**53 stretches / 2.49 per minute**, far above the proposed gate 0.3/min.

| Cause | Stretches | Seconds | Signal |
| --- | ---: | ---: | --- |
| **Sprint** (sprint=1 for ≥ 60 % of the gap) | 12 | **116.5** | `MOVE TRACE sprint=1`; native fire blocked while sprinting |
| **Aim or muzzle** (not separable without `FIRE SHOT`) | 19 | 80.3 | `NOFIRE` absent in C112; needs the patch |
| **Out of ammo** (`AI telemetry ammo=0`) | 9 | 50.1 | clip/reserve empty, bot keeps aiming |
| **Sidearm** (current weapon SOCOM) | 11 | 35.6 | pistol in an active fight |
| **Weapon switch** (`WEAPON RANK`) | 1 | 21.3 | deliberate switch mid-fight |
| **Chase far** (CHASING, >38 m) | 1 | 0.9 | closing, not yet in hold band |

Suit mode of the 53 windows: **Armor 50, Speed 3** — the opposite of the retracted claim. Bots did switch to Armor
correctly; they still did not fire, because they were sprinting in Armor (10 of 12) or Speed (2 of 12).

Typical episodes:

| t (s) | Bot | Gap | Sprint % | Weapon/ammo | Distance | Cause |
| ---: | ---: | ---: | ---: | --- | ---: | --- |
| 16.42–17.84 | 4 | 1.42 | 0 | SCAR / 27 | 14.8 | aim gate (view settling) |
| 46.92–66.52 | 4 | 19.6 | 33 | SOCOM / 8 | 11.4 | sidearm + weapon goal |
| 114.04–131.10 | 8 | 17.06 | 78 | FY71 / 7 | 29.0 | sprint |
| 318.68–340.87 | 4 | 22.19 | 20 | SCAR / 40 | 17.7 | aim/muzzle |
| 425.75–441.44 | 4 | 15.69 | 86 | SCAR / 31 | 10.1 | sprint |
| 624.92–646.25 | 4 | 21.33 | 78 | SOCOM / 5 | 9.6 | weapon switch |
| 918.26–932.59 | 4 | 14.33 | 89 | FY71 / 0 | 10.8 | sprint + empty |
| 1038.64–1094.04 | 4 | **55.4** | 62 | FY71 / 4 | 16.2 | sprint |

The 55 s and 22 s gaps are gait/aim, not burst cadence or reaction. The 19.6 s and 21.3 s gaps are weapon-goal and
switch; the 14.3 s gap is sprint into an empty clip. The remaining 80 s of "aim or muzzle" needs the new line.

## Code gates that can produce this

| Gate | File:line | Effect |
| --- | --- | --- |
| sprint / gait | `BotBrain.cpp:1017` (pace ON) | forces `NANOMODE_DEFENSE` on damage/Fire but leaves `gait=Sprint`; `weapon->CanFire()` is false while sprinting |
| sprint / gait (legacy) | `BotBrain.cpp:1014-1015` | drops Sprint, but only when `!BotPaceEnabled()` |
| muzzle ray | `BotCombat.cpp:279` | eyes see the target, barrel blocked → `StopFire`, no log today |
| ammo/reload | `BotCombat.cpp:280` | empty clip/reserve → `StopFire`, then the bot keeps aiming |
| fire tolerance | `BotCombat.cpp:286-288` | Elite 2°; while the view settles, no shot |
| Fire vs Aim | `BotBrain.cpp:754,859,883` | one reaction clock (DS-035) but still a hard `Aim` until it elapses |
| weapon goal in a fight | `BotBrain.cpp:1340-1355` | arbiter can choose Weapon with a visible enemy, causing sidearm/switch gaps |

`ShotBlocked`, zoom state, clip/reserve and `fireDot` are not in the C112 trace, so the patch adds them.

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
5. **Drop sprint inside the hold range**: when `visible && distance <= holdRange` and no jump link, force
   `gait=Run` and `m_speedTravel=false`. This is the pace-ON path that the old code missed; it addresses the
   116.5 s sprint cluster.

## Gate after the patch

- `NOFIRE` lines per Elite per minute, split by reason; target **< 0.3 visible combat no-fire/min**.
- `sprint` gaps must be ~0: count `MOVE TRACE sprint=1` samples inside visible hold-range combat.
- `sidearm`/`weapon_switch` gaps should drop to ~0; `ammo` gaps must be followed by a `WEAPON RANK`/pickup within 1 s.
- If `blocked` dominates after the patch, the next fix is `ShotBlocked` step-out timing (currently 0.4 s in
  `UpdateAttacking`), not the fire gate.

## Missing fields to add to the recorder

- `FIRE SHOT` with shooter/target/class/distance/`wb_fire_diagnostics` in every run (C112 had none).
- `NOFIRE` with `reason`, `clear`, `fireDot`, `tolDeg`, clip/reserve, `weaponClass`, `zoom`/`zooming`, `suitMode`,
  `state`, `selectedAge`.
- `ShotBlocked` per actor/target/obstruction (currently only `m_shotBlocked` internally).
- Reload start/end and weapon switch reason.
- `BOT SUIT` reason (`SuitReason`) and refused mode (`ACTION REFUSED`) on the same line as state.
