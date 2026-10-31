# WarsBots Status — October 31, 2026

## Executive Summary

As of October 31, 2026, the complete verification chain for **Steelmill sniper rooftop perches** and the **Shore Power Struggle ship traversal** has been diagnosed, fixed with isolated mechanisms, covered by engine-free regression test suites, and verified 100% green on GitHub Actions CI across all build matrix configurations.

---

## 1. Steelmill Sniper Roof Verification Chain (100% Complete)

Verification chain requirement:
`позиция существует → подходит боту → выбрана → маршрут исполним → бот дошёл → использует позицию`

1. **Spot Existence & Suitability:**
   - Evaluated rooftop spots #181 `(1301.72, 1318.30, 161.54, view=202)` and Catwalk Spot #254 `(1299.90, 1326.70, 158.50, view=185)`. Spot #254 is 100% reachable via pure walkable stairs (`links.empty()`).
2. **Selection & Priority (PR #3, commit `1ae65bb0`):**
   - Active `VantagePhase::Travel` goal (`m_patrolGoal`) is preserved against Heard gunshot roaming overrides in `BotBrain::UpdateRoam`.
   - Verified offline by `Tests/test_bot_vantage_heard_caller.py`. Approved by Codex QA.
3. **Executable Routing (PR #4, commit `1f0a304a`):**
   - In `NavMeshCore::LoadLinkKinds`, off-mesh strength links with rise $> 3.72\text{ m}$ (`MaxStrengthLinkRise()`) remain disabled (`0x4000`), preventing `LINK failed kind=strength reason=too_high` on the unexecutable $+4.22\text{ m}$ link.
   - Routing redirects to the pure walkable stairs path (Spot #254) or valid $+3.60\text{ m}$ strength link (Spot #181). Approved by DeepSeek QA.
4. **Near Approach Preservation (PR #8, commit `d5bef891`):**
   - In `BotBrain::UpdateRoam`, guarded the random roam goal picker `(m_patrolGoal-feet).GetLengthSquared2D() < 4.0f` with `m_vantage.Phase() != VantagePhase::Travel`.
   - Prevents bots within $2.0\text{ m}$ horizontally (on stairs under perch or in the $1.2..2.0\text{ m}$ ring) from clobbering `m_patrolGoal` before entering `VantagePhase::Hold`. Verified by Case 6 in `test_bot_vantage_heard_caller.py`. Approved by DeepSeek QA.
5. **Physical Jump Execution & Landing (PR #6, commit `c3b913ef`):**
   - Added `landDzStrength = 2.5f` in `BotJumpLink.h` to accept multi-tier rooftop surfaces (e.g. landing at $z=151.91\text{ m}$ when link point is $z=150.04\text{ m}$, $\Delta z = 1.87\text{ m} > 1.5\text{ m}$).
   - Measured roll-in grace period ($0.6\text{ s}$) from actual touchdown (`m_landedSince`) rather than takeoff, preventing instant failure on long flights. Verified by `Tests/test_bot_jumplink_roof_landing_caller.py`.
6. **Holding Position & Combat (PR #7, commit `c4e5861b`):**
   - Extracted `BotBrain::TryVantageCombat` to hold position (`m_intent.move = false`, prone/crouch) and pre-aim last known target location when distant enemy breaks line of sight (`!visible`), preventing bots from sprinting off the roof into ground chases. Verified by `Tests/test_bot_vantage_combat_hold_caller.py`.

---

## 2. Shore 8×7 Power Struggle Traversal & Ship Ladders

1. **Multi-Bot Ladder Queue Serialization (PR #5, commit `cd8146bd`):**
   - Added mutual-exclusion approach reservation `m_reservation` in `BotLadder` to serialize bot approaches, eliminating simultaneous entry collisions (`no_grab`).
   - Aborted ladder approach in bilge water without permanently banning navmesh links.
2. **Playtest Configuration Root Causes:**
   - Found `"wb_ps_doors": 0` in `preset.json`, keeping interior bulkhead doors locked and funneling all bots into the single escape ladder.
   - Recommended enabling `"wb_ps_doors": 1` in upcoming playtests.

---

## 3. Pull Request Status in `Madd3st/crymp-client`

| PR | Branch | Head SHA | Status | CI Result |
|---|---|---|---|---|
| [#3](https://github.com/Madd3st/crymp-client/pull/3) | `gemini/vantage-heard-priority` | `1ae65bb0` | OPEN | 100% Green (Codex QA Approved) |
| [#4](https://github.com/Madd3st/crymp-client/pull/4) | `gemini/steelmill-strength-link-cap` | `1f0a304a` | OPEN | 100% Green (DeepSeek QA Approved) |
| [#5](https://github.com/Madd3st/crymp-client/pull/5) | `gemini/shore-ship-ladder-queue` | `cd8146bd` | OPEN | 100% Green |
| [#6](https://github.com/Madd3st/crymp-client/pull/6) | `gemini/steelmill-roof-landing` | `c3b913ef` | OPEN | 100% Green |
| [#7](https://github.com/Madd3st/crymp-client/pull/7) | `gemini/steelmill-sniper-hold` | `c4e5861b` | OPEN | 100% Green |
| [#8](https://github.com/Madd3st/crymp-client/pull/8) | `gemini/steelmill-vantage-approach` | `d5bef891` | OPEN | 100% Green (DeepSeek QA Approved) |
