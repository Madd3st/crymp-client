# DS-090: independent review of `def8a65d` (water start + B1 roam guard)

Exact `def8a65d99e4640c9c765375f5739d010835e67e`, parent `b06681ea`, CI 40703800474 (pending). Offline review
only; no build/game. 11 files, +246/-29, reviewed in full.

## Verdict: BLOCK - one concrete defect (B1 water-roam goal box), rest is OK

### Blocking: B1's local roam leg cannot be evaluated in water (`BotBrain.cpp:959-970`)

The guard samples the local 6-10 m leg at 25/50/75/100 % and calls

```cpp
sampled[i] = reachable(feet + (local - feet) * fraction);     // :965-966
reachable  = [&](const Vec3& p){ return mesh->Reachable(from, q, 2.6f, startBounds); };   // :936
```

`startBounds` lifts only the **start**. The **goal** `q` keeps the swimmer's feet z (the local leg is horizontal, so
`q.z == feet.z`), and `Reachable` projects the goal with the dry goal box
`[q.z - 2.6, q.z + 1.0]` (`BotNavMeshCore.cpp:264`). C147 measured feet `97.2..98.6` against the baked water poly
`99.511`, so the goal box top is `98.2..99.6`: only floats at `z >= 98.511` reach the water polygon. In practice
**all four samples return false**, `RoamFallbackFraction` returns 0, and `m_patrolGoal = feet` (`:970`) - the bot
treads water for 4 s and repeats. That converts the old "steer 6-8 m" water roam into a hold, which is exactly the
recovery path a bot needs when the capture goal is dropped in one of the 41 water holes.

Minimal fix (keeps the guard's intent, 1 line at `:965`): probe the water plane when the water start is active, so the
goal projects onto the water polygon and the component test still rejects a hull-blocked leg:

```cpp
Vec3 p = feet + (local - feet) * (1.0f - 0.25f * float(i));
if (waterStart) p.z = m_blackboard.waterZ;      // DS-090: the local leg is a water leg
sampled[i] = reachable(p);
```

(`waterStart` is already in scope at `:929`.) The same dry-goal-box limitation applies to the heard-gunshot goal
(`:899-907`) when the sound comes from a swimmer; not blocking (rare), but worth the same treatment or a comment.
`Tests/BotRoamFallbackTests.cpp` covers only the pure fraction function, so it cannot catch this; after the fix, the
water case is covered by the C148 run's roam samples, or by a small engine-side check in the oracle.

### Verified OK

* **Fixture (my earlier RED was wrong, Codex is right).** `verts` are *quantized* (`dtNavMeshBuilder.cpp:490-497`:
  `v = bmin + iv*cs`), so `x*100` with `cs 0.1` is the same physical 40x30 m tile and `y=100` with `ch 0.1`,
  `bminY=90` is the plane `100`. The old degenerate `bminY==bmaxY=100` gave the tile a zero-height bbox, so the
  outside-range query was admitted; 90..110 removes it. All six controls (raw-dry RED, water GREEN, deep float,
  land/wrong area, hole, ray) are retained, none weakened; the measured `planeZ` print is kept.
* **Core**: one `StartRef`, water-only acceptance (`IsWaterPoly`, area 4), no dry/seabed/stacked fallback, dry
  callers byte-identical (`1.5/1.0/3.0`, `RayClear` `0.5/1.0/2.0`), `Project` 6th arg defaulted, `Reachable`/
  `FindPathLinks`/`PSCaptureSnapshot` defaults keep every other caller compiling.
* **Plane sizing**: `WaterQueryBounds(lift) = clamp(lift+0.30, 1.2, 2.8)`, used from
  `waterMovement && waterValid && lift > 0.05`; deep-float ten addressed without a global widen.
* **Corner advancement**: `SteerCorners` passes the same bounds to `RayClear`; the 12/1095 blocked rays are
  OBSERVED and keep the old conservative "don't skip the corner" path - no claim that all water rays are clear.
* **B1 dry behaviour**: `RoamFallbackFraction` ordering/legacy-no-mesh cases are right, `mesh` null keeps 1.0, and
  the dry goal box is unchanged - the defect above is specific to the water goal projection.
* **Scope**: no speed patch, no flag/default, no ladder/boat/teleport, B1 preserved as accepted.

## C148 gate

One shared upper->sea->shore run is fine **after** this one line is fixed and CI is green on the new SHA; otherwise the
run can end in a water hold at a hole and be misread as a shore failure. No game/build/install by me.
