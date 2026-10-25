# Target choice and long range (DS-015)

Build `5b90bfb6`. All line numbers are from `warsbots/feature/claude-sprint`.

## 1. Exact target-switch rule

`BotCombat::FindEnemy` (`BotCombat.cpp:135-233`) runs on the brain tick (10 Hz), but the actor scan is cadenced:

1. The current target is observed every call. `Observe` sets `m_visible=false` when the eye ray to chest/head fails
   (`BotCombat.cpp:89-133`, especially `:95-97`).
2. A target is `remembered` if it is still valid, was seen at most 3.0 s ago and its last position is within 80 m
   (`BotCombat.cpp:144`). While remembered, a scan is skipped until `m_nextScan`; otherwise the scan cadence is 0.6 s
   (`BotCombat.cpp:146-147`).
3. A candidate is skipped if it is not an enemy, is the `m_avoid` target inside its 8 s avoid window, or is farther
   than 80 m (`BotCombat.cpp:171-174`). LOS is chest ray or head ray (`BotCombat.cpp:72-77,177`). With
   `wb_bot_perception=1`, the candidate must also pass the 110-degree cone / awareness test, except the old target is
   kept when it is still tracked by LOS (`BotCombat.cpp:178-201`).
4. Every visible candidate gets `TargetUtility` (`BotCombat.cpp:210-211`, formula at `BotProfile.h:72-81`):

       score = 1 - distance/80
             + targetJudgement * (0.7*rangeFit + 0.35*clearMuzzleAndAmmo
                                  + threat + 0.8*retaliator)

   `rangeFit = 1 - |distance-desiredRange| / max(desiredRange,8)`; `desiredRange` comes from
   `BotProfile::WeaponRange` (`BotProfile.h:47-64`). `threat` is `+0.25` when the candidate
   faces the bot, or `-0.35` if that candidate faces the bot while the bot's own HP is below 40; `retaliator` means
   "this candidate damaged me in the last 3 s" (`BotCombat.h:16,26-29`). The highest score wins; ties go to the lower
   EntityId (`BotCombat.cpp:213-214`).
5. Hysteresis: if the old target is remembered and still visible, a different best is accepted only when
   `heldSeconds >= targetHold` and `newScore >= oldScore + targetSwitchMargin`
   (`BotCombat.cpp:216-217`, `BotProfile.h:69-70`). Values: hold 3.0/2.0/1.6/1.3 s and margin 0.45/0.35/0.28/0.25
   for Beginner/Normal/Veteran/Elite (`BotProfile.h:128-129,133`).
6. If no candidate survives, a remembered old target is kept (`BotCombat.cpp:218`); otherwise the target is cleared
   (`:219`). A real change resets `m_selectedAt`, stops fire, rerolls the head/body choice and restarts the reaction
   delay (`BotCombat.cpp:220-229`; reacquire path at `:100-107`).

**Why it drops a 4-HP target.** The bot has no target-HP input at all: `TargetUtility` receives only distance,
desired range, muzzle clearance, facing, retaliator, **own** health and ammo (`BotProfile.h:72-81`). The only health in
`FindEnemy` is `actor.GetHealth()` (`BotCombat.cpp:211`). Therefore low target HP is worth exactly zero in the score.
If a full-HP enemy is closer to the bot's desired range, has a clear muzzle, is facing the bot, or just damaged the bot
(retaliator +0.8), it can win: after the hold time while the low-HP target stays visible, or immediately once the
low-HP target is hidden (next paragraph). There is also no "finish him" lock: after `targetHold` the switch is allowed
for a visible target.

**The one-scan LOS/cone loss is the main drop path.** The hysteresis block is entered only when
`remembered && m_visible` (`BotCombat.cpp:216`), where `m_visible` is the **old target's** observation result from
`:142`. When the old target is hidden for one scan or fails the perception cone, `m_visible=false`, so the guard is
skipped entirely: a different visible best is accepted immediately, without `targetHold` and without the score
margin. If no other target is visible, `remembered` keeps the old target (`:218`). `oldScore` is assigned only if
`candidate==old` in the scan (`:212`), so on that hidden scan it is `-1e9`; it is not the gate here (the gate is
already off), but it confirms there is no fair comparison while the old target is unseen. With a 0.6 s scan cadence
(`:147`), a 4-HP target that steps behind a corner while a second enemy is visible is lost within 0.6 s.

**Stuck chase discards the target too.** `UpdateCombatPost` calls `AvoidCurrent(now,8.0f)` when the bot makes no
progress toward its combat goal (`BotBrain.cpp:889-893`), which clears `m_target` and makes the candidate skipped
for 8 s (`BotCombat.cpp:171`, `BotCombat.h:17-18`). This is gated by `wb_progress_experiment=1`
(`BotBrain.cpp:900-905`, `BotConfig.cpp:21`). A low-HP target that runs behind cover and is chased into geometry is
dropped by this path, not by an HP decision.

So the observed 24 "switch away from low HP" events are consistent with: the target breaking LOS/cone for one 0.6 s
scan while any other enemy is visible (immediate switch), a higher-utility healthy enemy after the 1.3-3.0 s hold while
the low-HP target stays visible, or the 8 s avoid on a stuck chase. The logs' "dropped at 4 HP" is not a decision
input.

## 2. Proposed scoring fix

Do not read enemy HP through walls. Keep an honest hit/damage memory fed by the bot's own confirmed hits (or by a
damage event where the game already reports it), and use it only while the target is or was recently visible.

```cpp
struct TargetMemory {
    EntityId id = 0;
    float lastSeen = -1e9f;
    float lastHit = -1e9f;       // own confirmed hit on this target
    float damage = 0.f;          // accumulated, capped
    float lastBase = 0.f;        // base utility excluding finish, for a fair switch comparison
    float finishUntil = -1e9f;
};

float FinishBonus(const TargetMemory& m, float now, bool visible) {
    if (m.id == 0 || now - m.lastHit > 5.0f) return 0.f;
    const float frac = clamp(m.damage / 100.0f, 0.f, 1.f);
    const float seen = visible ? 1.0f : 0.35f; // memory, not wallhack
    return 0.55f * frac * seen;                 // up to +0.55
}

bool CanSwitchTarget(float held, float oldTotal, float newTotal,
                     float oldFinish, float newFinish,
                     float targetHold, float margin) {
    const bool finishing = oldFinish > 0.25f;
    const float hold = targetHold + (finishing ? 0.8f : 0.f);
    const float reserve = finishing ? 0.45f : 0.f; // the old target gets this much extra
    return held >= hold && newTotal + newFinish >= oldTotal + oldFinish + margin + reserve;
}
```

Integration in `FindEnemy`:

```cpp
// per candidate
float base = m_profile.TargetUtility(...);
float total = base + FinishBonus(mem[candidate], now, visible);
if (candidate == old) { oldBase = base; mem[old].lastBase = base; } // every scan
if (total > bestScore) { best = candidate; bestScore = total; bestBase = base; }
// ...
if (remembered && best && best != old) {         // no m_visible guard
    const bool seen = m_visible;                 // old target's Observe result
    const bool finishing = mem[old].finishUntil > now;
    const float grace = finishing ? 2.5f : 1.2f;  // short cover is not a lose
    const float lost = now - mem[old].lastSeen;
    const bool maySwitch = seen ? (now - m_selectedAt) >= m_profile.targetHold
                                : (lost >= grace);
    const float oldBaseScore = seen ? oldBase
                         : mem[old].lastBase - 0.20f * lost;
    if (!maySwitch || !CanSwitchTarget(now - m_selectedAt, oldBaseScore, bestBase,
                         FinishBonus(mem[old],now,seen), FinishBonus(mem[best],now,true),
                         m_profile.targetHold, m_profile.targetSwitchMargin))
        best = old;
}
```

- Store `lastBase`, `lastSeen` and `finishUntil` on the selected target; when it is hidden, compare against a
  decayed remembered base (0.20/s) instead of `-1e9`, and keep it through 1.2 s of cover (2.5 s while finishing).
  This alone removes the one-scan drop.
- `FinishBonus` is capped at +0.55 so a damaged target does not beat a real threat forever; `targetJudgement` and
  retaliator still matter.
- `finishUntil = now+2.5f` when estimated HP <= 25 and the target was hit within 3 s. While it is set, only a
  retaliator that is currently shooting and has a clear muzzle may break the lock, and only after 1.0 s of lost LOS.
- Add a `damageDealt` accumulator in the existing hit path; if no confirmed-hit event exists, expose one from
  `GameRules`/the weapon hit callback. Do not add `candidate->GetHealth()` to `TargetUtility`: that would be an
  information advantage and would also make the bot shoot through smoke/cover.

### Tests

- `BotCombatTests`: old target visible at 4 HP, new full-HP target +0.3 utility: no switch before `targetHold`, no
  switch at all while `FinishBonus >= 0.25`; after 6 s without hits the new target may win.
- `BotCombatTests`: old target hidden for exactly one scan (`m_visible=false`, `remembered=true`) and a new target
  visible: no immediate switch; the old target is kept through the 1.2 s cover grace, and the decayed remembered
  score is used instead of `-1e9`.
- `BotCombatTests`: same, with `finishUntil` active: old target is kept through 2.5 s of cover even if the new target
  has a higher base score; a firing retaliator may break it only after 1.0 s.
- `BotCombatTests`: `AvoidCurrent` + 8 s: target is skipped, then can be reacquired; no crash, no stale score.
- `TargetMemoryTests`: damage accumulation cap, 5 s expiry, no bonus without a hit, no HP read for an unseen target.
- Probe `finish_4hp`: two bots, one at 4 HP behind a corner, one full HP in the open at 25 m; expected: the low-HP
  target is not dropped for the healthy one within 2 s of losing LOS; kill share of the 4-HP target > 0.7.
- Probe `switch_log`: count `TARGET CHOICE old!=new` where the old target had `health<=25` at the switch; target 0
  (currently 24 in the owner run), and no switch within 1 s of a hit on the old target.

## 3. Every range limit

| Limit | Value | Where | Effect |
| --- | --- | --- | --- |
| Candidate scan | `< 80 m` (80.0 excluded) | `BotCombat.cpp:174` | Hard detection radius, all skills/styles |
| Sight | 80 m | `BotPerception.h:20-21` | Same cap when perception is on |
| View cone | 110 deg full (55 deg half-angle) | `BotPerception.h:20,80-82` | Only with `wb_bot_perception=1` (default 0) |
| Cloak sight | 12 m | `BotPerception.h:22` | Cloaked target beyond 12 m invisible unless revealed |
| Cloak reveal | 1.5 s after shot / Speed sprint | `BotPerception.h:24,83-85` | Reveals a cloaked target |
| Hearing | gunshot 60 m, silenced 12 m, sprint 12 m | `BotPerception.h:25-27` | Position only, not a sighting |
| Reaction / reacquire | .75/.35/.25/.22 s | `BotProfile.h:121`, `BotCombat.cpp:100-107,835` | Fire intent and reacquire delay |
| Memory | 3.0 s, last position within 80 m | `BotCombat.cpp:144` | Keeps a hidden target, never fires through cover |
| Scan cadence | 0.6 s | `BotCombat.cpp:146-147` | Target re-selection rate |
| Eye LOS | full ray, no distance cap | `BotCombat.cpp:51-64,72-77` | `ent_all` world ray; reaches the 80 m scan cap |
| Chest/head aim | eye to 75 % body / eye | `BotCombat.cpp:79-83,95-96` | Head ray only if chest is blocked |
| Fire dot gate | 0.96 (16.3 deg) | `BotCombat.cpp:260` | Hard trigger gate vs current fire direction |
| Fire distance while chasing | Assault 32, Universal/Flanker/Hunter 40, Defender 50, Marksman 65 m | `BotProfile.h:97,103,112`, defaults `:22`; used `BotBrain.cpp:854` | Beyond it the bot closes without firing |
| Engage / hold | 18-28 / 22-34 m by style | `BotProfile.h:16-17,95-115`; `BotBrain.cpp:748` | Attack state vs chase |
| Desired weapon range | SOCOM 10, Shotgun 6, SMG 12, SCAR/FY71 18, DSG1/Gauss 35, Hurricane 20 m | `BotProfile.h:47-64` | Strafe target, **not** a hard fire cap |
| Zoom | SCAR/FY71 20 m, SMG 15, DSG1/Gauss 8 | `BotWeaponHandling.h:15-30` | Only with `wb_bot_weapon_handling=1` (default 0) |
| Weapon table | SCAR 90, FY71 80, Hurricane 45, SMG 40, Shotgun 18, SOCOM 30, DSG1/Gauss 300 m | `BotWeaponRank.h:25-32` | Ranking falloff, not a fire gate |
| Aim error at 50 m | floor .10/.07/.05/.035 m, max .25*2/1/.7/.45 | `BotProfile.h:30-33,137-142` | 0.25 m at Normal = 0.29 deg; not the range cap |
| Turn / view | skill cap 400-900 deg/s (only `wb_bot_turn_cap=1`) | `BotBrain.cpp:1154-1161` | Tracking speed, not distance |

### Which limit bites first at 50 m / 80 m

- **At 50 m:** the scan (80 m), sight (80 m) and LOS are not the cap. The first hard gate is **`fireRange`**:
  Universal/Flanker/Hunter 40 m and Assault 32 m do not fire while chasing; Defender 50 m and Marksman 65 m do.
  If already in ATTACKING within `holdRange` (22-34 m), the bot is closer than 50 m, so this is a chase-range case.
  For a Marksman at 50-65 m, the next limits are reaction (0.22-0.25 s), fire-dot 0.96, and the weapon's
  `desiredRange` (18 m SCAR / 35 m DSG1) which makes it walk closer rather than stop firing. The 16.3-degree dot
  gate is not tight at 50 m; aim error is sub-degree.
- **At 80 m:** the **80 m candidate scan** and `BotPerception::maxSight` are the hard wall (80.0 is rejected because
  the test is `distance < 80.0f`). Even if a target were detected, no style can fire there: maximum `fireRange` is
  65 m (Marksman). So 80 m is detection-only in theory and no engagement in practice. The owner's "no bot hit beyond
  40 m" is explained directly by the default 40 m `fireRange`, not by the 80 m scan.
- **Roof targets:** a target on a roof at 50 m is inside the scan radius and inside `maxSight`; whether it is seen
  depends on the cone (if perception is on) and LOS, not on a range limit.

## 4. Do bots ever look up / check roofs?

**No deliberate roof check exists.**

- The look policy is path/interest only: visible target, recent attacker, point of interest (last known position,
  heard gunshot, weapon to fetch), else the path 3 m ahead (`BotBrain.cpp:1089-1131`, rules in
  `BotLookPolicy.h:6-13,81-116`). Path corners are looked at with pitch clamped to +/-20 deg
  (`BotLookPolicy.h:113-115`); target/interest look at true pitch, but only after that point is already an interest.
  There is no "scan high points", "look at roof", pitch sweep, or sniper-spot check for a non-sniper bot.
- `UpdateVantage` only handles a Marksman/Defender holding a DSG1/Gauss with `camper >= 0.5`
  (`BotBrain.cpp:650-680`). It travels to a baked sniper spot and holds a **yaw** along the best view, near-horizontal
  (`BotBrain.cpp:674-678`). That is a sniper holding a spot, not a general roof search; `wb_bot_vantage` is default 0
  (`BotConfig.cpp:35`).
- With `wb_bot_perception=1` the 110-degree full cone is centered on the current view (`BotPerception.h:80-84`), and
  the view is normally the path/interest direction, so a roof target outside that cone is not noticed even with a
  clear ray. A roof target already inside the cone can be seen; there is no upward look to acquire one.
- With the shipped default `wb_bot_perception=0` (`BotConfig.h:30`) there is **no cone/awareness model** (the profile
  reaction delay still applies): any enemy within 80 m with a clear chest/head ray is a candidate from any direction,
  including a roof. So the reason roof snipers are "untouched" is not a roof-specific invisibility: bots do not
  deliberately acquire or engage them, the default fire range stops at 40 m, and a distant roof target is usually
  outside the path/interest view when perception is enabled. Do not claim "bots cannot see roofs"; claim "bots never
  look for them".

## Bottom line

The low-HP drops are a missing target-state problem, not an HP comparison: `TargetUtility` has no target HP or damage
term, and a hidden old target skips the hysteresis branch entirely (`BotCombat.cpp:216`) so any other visible enemy
is taken immediately. Add honest hit memory + a finish bonus + a remembered score and a cover grace for hidden
targets. The long-range wall is the 40 m default `fireRange` for Universal/Assault/Flanker/Hunter, then the 80 m
scan/sight cap at the extreme; Marksman 65 m is the only style that reaches past 40 m, and only while it actually
holds a sniper weapon and closes to its desired range.
