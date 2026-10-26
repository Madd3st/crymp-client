# Dumbness detectors (DS-032)

Date: 2026-10-26. Spec for an automatic "did the bot do something dumb" judge over `*.bots.log` and `wbdemo/2`.
Owner input: F8 marks from playtest 3 (C-108 step 1). Output: `Tools/PlaytestReport` section + `dumbness.json`.
Every detector is a rule with a **false-positive twin** that must be tested, because most "dumb" patterns are also
legitimate play (strafe, cover, reload, hold, retreat).

## 0. Data available

- `MOVE TRACE actor=… pos=(x,y,z) vel=(…) speed=… mode=… sprint=… grounded=… jump=… health=… yaw=… pitch=…`
- `MOTION INTENT: gait=… strafe=… lateral=… facingDot=… blocked=0|1`
- `#N AI state=…` (`SEEKING_WEAPON`, `MOVING_TO_WEAPON`, `SEARCHING_ENEMY`, `CHASING_ENEMY`, `ATTACKING`, `HOLD`…)
- `TARGET CHOICE actor=… old=… target=… score=… visible=… health=… ammo=…`
- `AI decision target=… visible=1 sample=… age=…`
- `LOOK TRACE actor=… reason=… state=… interest=… clear=0|1 rays=… eye=… corner=… aim=…`
- `INTENT WHY suit=… jump=… gait=… move=… state=…`
- `FIRE SHOT shooter=… target=… class=… mode=… clip=…`, `FIRE HIT phase=… target=… distance=… material=…`
- `TRAP RECOVERY level=…`, `PROGRESS ESCAPE`, `HEARD gunshot at (…)`, `GLANCE spot=… actor=…`,
  `VANTAGE actor=…`, `REAL PICKUP`, `WEAPON RANK`
- demo (`wbdemo/2`, 20 Hz): `BRAIN`/`FRAME`/`EVENT`; event 10 = `OWNER MARK` (after DS-022).

All detectors run at 10 Hz (the brain cadence) over a rolling 10 s buffer; `t` is game time.

## 1. Detector: oscillation (movement)

**Fields:** `MOVE TRACE.vel` (horizontal), `MOTION INTENT.strafe`, `#N AI state`.

**Rule:** a direction reversal is `dot(v[i], v[i+1]) < 0`, `|v| > 1.0 m/s` on both samples, `Δt ≤ 0.6 s`.
Flag if **≥ 4 reversals in 2 s** and `strafe=0` and state ∉ {`ATTACKING`, `HOLD`}.

**Why not simply "reversals":** duels strafe by design (`MOTION INTENT strafe=1`, lateral sign flips); a single
reversal is a corner or a blocked step.

**False-positive twins to test:** (a) fight strafe at 10 m — must not flag; (b) one corner turn in a corridor —
must not flag; (c) knockback/grenade push — velocity sign flips once, no flag.

**Clip:** `[t−3, t+3]`, demo offset ±60 frames.

## 2. Detector: decision flapping

**Fields:** `TARGET CHOICE.old`, `.target`, `.health`, `.visible`; `#N AI state`.

**Rule:** count target changes with `old != 0 && target != old`; flag if **≥ 3 in 2 s** and the previous target is
still alive (`health > 0`) and visible (`visible=1`), and no `KillPlayer` for it in the window.

**Why:** switching to a *dead* target, or after a `finish` kill, is correct; switching between two live visible
enemies every 0.6 s is thrash.

**False-positive twins:** (a) finish rule (`wb_bot_finish`) switching after a kill — must not flag; (b) two enemies,
one behind cover (`visible=0`) — switch is correct; (c) target becomes invalid (entity gone) — no flag.

**Clip:** `[t−2, t+5]`.

## 3. Detector: freeze

**Fields:** `MOVE TRACE.speed`, `#N AI state`, `LOOK TRACE.clear`, `AI decision.visible`.

**Rule:** `speed < 0.3 m/s` for **≥ 1.5 s** while state ∈ {`ATTACKING`, `CHASING_ENEMY`, `MOVING_TO_WEAPON`} and
`MOTION INTENT.blocked=0`, and not (`health ≤ 0`), not (`vz > 1` — jump wind-up), not a reload (`clip=0` plus a
`Stock weapon burst`/reload line), not `HOLD`.

**Why:** a bot that stops in the open with no wall and a live goal is either stuck on a mover, waiting for a link, or
the brain wrote no movement. This is the detector that catches "statue" bugs.

**False-positive twins:** (a) deliberate hold/cover — state `HOLD` or `blocked=1`; (b) reload behind cover — allow
1.5 s if `LOOK TRACE.clear=0`; (c) jump wind-up — `vz > 1`; (d) death/ragdoll — `health ≤ 0`.

**Clip:** `[t−2, t+5]`.

## 4. Detector: wall stare / run into wall

**Fields:** `MOTION INTENT.blocked`, `MOVE TRACE.speed`, `LOOK TRACE.clear`, `TRAP RECOVERY.level`.

**Rule:** `blocked=1` in **≥ 5 consecutive samples** (0.5 s) with `speed < 0.5 m/s`, or `TRAP RECOVERY level ≥ 1`
twice in 3 s. Flag the start of the run, not every sample.

**False-positive twins:** (a) intentional cover hold with `blocked=1` — only flag if the state is
`MOVING_TO_WEAPON`/`CHASING_ENEMY` and the goal is not reached; (b) waiting at a door/lift — `LOOK TRACE.reason=path`
and no `TRAP RECOVERY`; (c) pushing a physics prop — one blocked sample, no trap.

**Clip:** `[t−3, t+3]`.

## 5. Detector: shooting into cover

**Fields:** `FIRE SHOT.target`, `FIRE HIT.target`, `LOOK TRACE.clear`, `AI decision.visible`, `#N AI state`.

**Rule:** **≥ 3 `FIRE SHOT`** at the same target in 2 s while `LOOK TRACE.clear=0` for that target or
`AI decision.visible=0`, and **≤ 1 `FIRE HIT`** in the same window. Flag the burst, not each shot.

**Why:** misses in the open are normal; firing repeatedly at a target the perception says is not visible is dumb.
Do not use "no hit" alone — spread and movement produce misses.

**False-positive twins:** (a) shooting a target behind a thin wall that the ray misses but bullets penetrate — check
`FIRE HIT material`; (b) shotgun at 3 m with a wide spread — `class=Shotgun`, allow; (c) suppression of a known
corner — no such behaviour today, so no exemption.

**Clip:** `[t−2, t+3]`.

## 6. Detector: ignoring a visible enemy

**Fields:** `AI decision.target`, `.visible`, `TARGET CHOICE`, `FIRE SHOT`, `#N AI state`, `MOVE TRACE.speed`.

**Rule:** `AI decision` says `visible=1` for target X, but for **≥ 1.0 s** there is no `TARGET CHOICE` with
`target=X` and no `FIRE SHOT target=X`, while state ∉ {`RETREAT`, `HOLD`}, ammo > 0, not reloading, not in a vehicle,
and X is within the weapon's fire range.

**Why:** this catches "bot looks at an enemy and does nothing" — the exact owner complaint. The 1 s floor avoids
reaction-time (Beginner 0.9 s) false positives; for Beginner use 1.2 s.

**False-positive twins:** (a) finishing another target — old target health < 25; (b) out of range — distance >
`fireRange`; (c) no LOS — `LOOK TRACE.clear=0`; (d) reload — `clip=0`.

**Clip:** `[t−1, t+3]`.

## 7. Detector: standing in the open under fire

**Fields:** `FIRE HIT.target` (bot), `MOVE TRACE.speed`, `LOOK TRACE.clear`, `BOT SUIT.mode`.

**Rule:** **≥ 2 `FIRE HIT` on the bot in 2 s**, `speed < 0.5 m/s`, and no cover within 3 m (raycast to the nearest
`LOOK TRACE.corner` or a `blocked=1` sample), while state ∉ {`HOLD`, `ATTACKING` at `VANTAGE`}.

**Why:** standing still while hit is sometimes correct (sniper hold, armor tanking); the exemptions matter.

**False-positive twins:** (a) sniper on a roof (`VANTAGE actor=…`) — must not flag; (b) armor + low damage
(`BOT SUIT mode=Armor`, damage < 10 per hit) — allow; (c) reload behind a wall (`blocked=1` + `clear=0`) — allow.

**Clip:** `[t−2, t+3]`.

## 8. Detector: goal stall (bonus, from DS-023)

**Fields:** `#N AI state=MOVING_TO_WEAPON`, `REAL PICKUP`, `WEAPON RANK`, `TRAP RECOVERY`.

**Rule:** same goal/state for **> 60 s** with no `REAL PICKUP` and no `TRAP RECOVERY` improvement. Flag once per
goal. This is the C-098 DSG1 pattern (12 × `weapon 98 unreachable`).

**False-positive twin:** a long legitimate rotation to a distant pickup on Quarry/Armada; use 90 s there.

**Clip:** `[t−10, t]`.

## 9. Output schema (`dumbness.json`)

```json
{
  "schema": "warsbots-dumbness/1",
  "session": "C091-40bd9f60-20261026-115617",
  "source": "…-Server.bots.log",
  "config": { "oscillation": 4, "flap": 3, "freeze_s": 1.5, "wall_s": 0.5, "blind_shots": 3,
              "ignore_s": 1.0, "open_hits": 2, "stall_s": 60 },
  "events": [
    { "type": "freeze", "t": 123.4, "actor": 131013, "bot": "WarsBot_10",
      "severity": "high", "confidence": 0.8,
      "window": [121.9, 125.4],
      "evidence": [ { "t": 121.9, "line": 12345, "text": "#10 AI state=ATTACKING" },
                    { "t": 123.4, "line": 12360, "text": "MOVE TRACE … speed=0.02" } ],
      "clip": { "t0": 118.4, "t1": 128.4, "demoOffset": 2368 },
      "falsePositiveTwin": "reload",
      "markId": null }
  ],
  "marks": [
    { "t": 300.0, "text": "bot stuck on stairs", "hit": true,
      "events": ["evt_0012"], "nearest": 1.2 }
  ],
  "summary": {
    "perDetector": { "freeze": {"events": 4, "marks": 2}, "wall": {"events": 6, "marks": 1} },
    "marksTotal": 12, "marksWithHit": 7, "hitRate": 0.58,
    "eventsPer10Min": 10.2
  }
}
```

`severity`: `low` (one sample over threshold), `medium` (sustained 2× threshold), `high` (caused a death, a lost
target, or a trap). `confidence`: `high` only if one evidence line explains it; otherwise `medium/low` + alternatives.

## 10. PlaytestReport integration

Add a **Dumbness** tab above the event list:

- one row per detector with count, hit-rate vs marks, and a sparkline;
- a timeline lane per detector (click → card);
- each card: bot, state, evidence ±5 s, the false-positive twin, the demo clip button, and a "why" link to the
  MARKS evidence card (`MARKS.md` §3);
- filter by bot/skill/style and by mark; "only marked" toggle;
- export `dumbness.json` next to `report.html`.

## 11. Validation against F8 marks (C-108)

For each owner mark at `t`:

- **hit** if any detector event starts within `[t−5, t+5]` for a candidate bot from `MARKS.md` §2;
- **miss** otherwise; record the nearest event and its type;
- **false alarm** = an event with no mark within ±10 s; report `eventsPer10Min`, not a precision (marks are sparse).

Acceptance for v1: `hitRate ≥ 0.5` on ≥ 10 marks, and the three most frequent detectors have ≤ 1 obvious
false-positive twin failing in a scripted probe each. Tune thresholds only from marks + twins, never from a single
session.

## 12. False-positive twin probes

| Detector | Probe | Expected |
| --- | --- | --- |
| oscillation | 1v1 duel at 10 m, `fightStrafe=1` | 0 events |
| oscillation | corridor corner at 1306,1353 | ≤ 1 event |
| flapping | two enemies, one behind cover | 0 events |
| flapping | finish a 4 HP target, then switch | 0 events |
| freeze | reload behind cover, 2 s | 0 events |
| freeze | jump wind-up at a link | 0 events |
| wall | hold an angle in cover, 5 s | 0 events |
| wall | stair mouth at 1306,1353 | 1 event |
| shoot-cover | shotgun at 3 m | 0 events |
| shoot-cover | rifle at a wall for 2 s | 1 event |
| ignore-enemy | Beginner reaction 0.9 s | 0 events |
| ignore-enemy | visible enemy ignored 3 s | 1 event |
| open-under-fire | sniper hold on DSG1 roof | 0 events |
| open-under-fire | standing in the open, 3 hits | 1 event |
| stall | DSG1 unreachable 90 s | 1 event |
