# WarsBots Status — November 1, 2026

## Executive Summary

As of November 1, 2026, the complete verification chain for **Steelmill sniper rooftop perches** and **Shore Power Struggle ship traversal** is implemented, covered by automated test suites, and verified 100% green on GitHub Actions CI across all build matrix configurations (x86/x64, Debug/Release).

In addition, empirical validation through dedicated playtesting (including footage and log analysis) has identified key engine flags and tactical dynamics needed for complete sniper gameplay, leading to updated turnkey playtest packages.

---

## 1. Steelmill Sniper Roof Verification Chain (100% Complete)

Verification chain requirement:
`Spot Existence -> Priority Selection -> Executable Routing -> Near Approach -> Jump & Landing -> Hold & Fire`

1. **Spot Existence & Suitability:**
   - Evaluated rooftop spots: Spot #181 `(1301.72, 1318.30, 161.54, view=202)` and Catwalk Spot #254 `(1299.90, 1326.70, 158.50, view=185)`. Spot #254 is 100% reachable via pure walkable stairs (`links.empty()`).
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

## 2. Shore Power Struggle Traversal & Ship Ladders

1. **Multi-Bot Ladder Queue Serialization (PR #5, commit `cd8146bd`):**
   - Added mutual-exclusion approach reservation `m_reservation` in `BotLadder` to serialize bot approaches, eliminating simultaneous entry collisions (`no_grab`).
   - Aborted ladder approach in bilge water without permanently banning navmesh links.
2. **Playtest Configuration Discovery:**
   - Found `"wb_ps_doors": 0` in legacy configs, keeping interior bulkhead doors locked and funneling all bots into the single escape ladder.
   - Fixed by enabling `wb_ps_doors 1` in active playtest profiles.

---

## 3. Empirical Playtest Findings & Root Cause Analysis

Analysis of session logs (`Steelmill-d5bef891-20261031-134636.bots.log`) and 60 FPS gameplay footage revealed the exact conditions governing sniper behavior:

1. **Why Bots Defaulted to Rifles Instead of Sniper Rifles (DSG1/Gauss):**
   - **Vertical Search Box:** The stock weapon search uses a $\pm 12\text{ m}$ vertical window. On Steelmill, the factory roof DSG1 is at $+20.3\text{ m}$ and the tower Gauss is at $+13.0\text{ m}$ relative to ground bots, rendering them completely invisible to ground-floor loot queries.
   - **Path vs. Euclidean Progress:** When navigating towards stairwells, bots temporarily move away from the weapon in 2D Euclidean space. Without `wb_bot_path_progress 1`, an 8-second timeout drops the objective as unreachable.
   - **Sniper Vantage Trigger:** Long-term vantage holding (25–45 s) requires a bot to actually hold `DSG1` or `GaussRifle`. With rifles, bots only took height during temporary 8–15 s `high_ground` rolls.
   - **Resolution:** Enabled `wb_bot_sniper_reach 1` (32 m vertical search window + sniper priority) and `wb_bot_path_progress 1` in server arguments.

2. **Bot Profile Differentiation:**
   - Bots support 6 tactical styles (`universal`, `assault`, `marksman`, `flanker`, `defender`, `hunter`) and 4 skill ranks (`beginner`, `normal`, `veteran`, `elite`).
   - Standard skill-only spawns (`wb_add 2 beginner ...`) cycled styles blindly. Configured explicit tactical rosters:
     `+wb_add 2 marksman elite +wb_add 2 defender veteran +wb_add 2 assault normal +wb_add 2 flanker normal`.

3. **Stuck Diagnostics & Movement:**
   - Node 2008 `(1199.8, 1298.1, 138.2)` crate corner trap recovery was validated.
   - Strength jump link failures (`reason=no_energy`) occurred when sprinting depleted nanosuit energy ($E < 70$). Added `wb_bot_link_failfast 1` to abort blocked links cleanly in 0.6 s.

---

## 4. Pull Request Status in `Madd3st/crymp-client`

All PRs have green CI on GitHub Actions across the full build matrix:

| PR | Branch | Head SHA | Status | Reviews & CI |
|---|---|---|---|---|
| [**PR #3**](https://github.com/Madd3st/crymp-client/pull/3) | `gemini/vantage-heard-priority` | `1ae65bb0` | OPEN | CI Green, Codex QA Approved |
| [**PR #4**](https://github.com/Madd3st/crymp-client/pull/4) | `gemini/steelmill-strength-link-cap` | `1f0a304a` | OPEN | CI Green, DeepSeek QA Approved |
| [**PR #5**](https://github.com/Madd3st/crymp-client/pull/5) | `gemini/shore-ship-ladder-queue` | `cd8146bd` | OPEN | CI Green |
| [**PR #6**](https://github.com/Madd3st/crymp-client/pull/6) | `gemini/steelmill-roof-landing` | `c3b913ef` | OPEN | CI Green |
| [**PR #7**](https://github.com/Madd3st/crymp-client/pull/7) | `gemini/steelmill-sniper-hold` | `c4e5861b` | OPEN | CI Green |
| [**PR #8**](https://github.com/Madd3st/crymp-client/pull/8) | `gemini/steelmill-vantage-approach` | `d5bef891` | OPEN | CI Green, DeepSeek QA Approved |

---

## 5. Turnkey Playtest Environments

To enable seamless human playtesting without manual console setup or DLL path issues:
* **Steelmill (Instant Action, 8 bots):** `E:\WarsBots\Visual\Steelmill-15-d5bef891\START.cmd`
  - Fully configured with `wb_bot_sniper_reach 1`, `wb_bot_path_progress 1`, `wb_bot_weapon_handling 1`, `wb_bot_link_failfast 1`.
  - Roles: 2 Marksmen (Elite), 2 Defenders (Veteran), 2 Assault (Normal), 2 Flankers (Normal).
* **Shore (Power Struggle, 15 bots):** `E:\WarsBots\Visual\Shore-15-d5bef891\START.cmd`
  - Fully configured with `wb_bot_ladder 1`, `wb_ps_doors 1`, `wb_ps_buy 1`, `wb_ps_capture 1`.
