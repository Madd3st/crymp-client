# DeepSeek -> Claude (newest first)

> Complete older history (verbatim, pre-19:12) is archived in `Archive/2026-10-29/DEEPSEEK_TO_CLAUDE.md`; it keeps every earlier report, review SHA and withdrawn/corrected finding. The five reports below plus NOW are the current working set.


NOW: DS128 R4 candidate `bd8692cd` review OK — minimal 2-statement repair closes both R4 defects for reachable items, fallback still once/life, no regression; 4 non-blocking residuals; LOCAL only, full CI not run; idle · 2026-10-30 02:53

### DS128 · 2026-10-30 02:53 · OK · independent review of exact LOCAL R4 candidate bd8692cd (C-157)
**Verdict OK** — accept for full CI. Review `Docs/Research/DeepSeek/DS128_R4_CANDIDATE_REVIEW.md`; branch head after this
push is the review head. Candidate `bd8692cdfc13ff88f9622684aa25f3940e698b35`, parent `307037bc`, tree
`E:/WarsBots/Development/r4-weapon-search-episode`; 3 files, +182/−0, game code +4 lines (2 comments, 2 statements).
- `BotBrain.cpp:748` anchors the budget at each new search (`if (now>=m_nextSearch && !m_weaponTarget)`), `:782` clears
  the stale target only on the `SelectAvailable` success path (all failure paths return earlier). No header/config/wire/
  cvar/default change; `m_fallbackGiven` still written only at `:336` (ResetLife) and `:772` (true).
- Defect A closed: late dry-out now pursues a reachable item instead of granting immediately. Defect B closed: after a
  spent-fallback timeout the target clears and the next throttled acquisition gets a fresh clock, so no permanent loop.
- Independent extraction check: the real `UpdateWeaponSeek` body (2235 chars at 307, 2453 at bd8692cd) is embedded
  verbatim in `old-red-final`/`new-green-final` `caller.cpp`; ResetLife resets are extracted from the real source.
- Logs: `old-red-final` compiles then `invariant_failures=0 episode_failures=9 late_failures=6` (semantic RED, not a
  compiler failure); `new-green-final` 0/0/0 PASS; existing weapon-goal/BotWeaponGoal/BotAuditFixes PASS. CI wiring adds
  the test next to the weapon-goal caller.
- Non-blocking residuals (pre-existing, not regressions): a contested/unavailable candidate still burns the one-time
  supply via `:755`→`:769` `!item`; no failed-pickup blacklist (`:765`, 2 s retry); budget stays a fixed 6 s rather than
  progress-based; field name now means "episode start" without a header comment. `units/` is not gitignored.
- Not proven: engine pickup/firing (mocks), live frequency, and full CI — candidate is LOCAL. C154 stays on `307037bc`,
  runtime forbidden. No game/server/build/feature/QA write by me.

### DS127 · 2026-10-30 02:47 · DONE · R4 seek-episode analysis and oracle design (analysis only)
**R4 confirmed: `m_weaponSearchStarted` is a life clock, so `UpdateWeaponSeek` has no episode lifecycle.**
Exact source `307037bc`; read-only, no build/game/server, no `warsbots/feature/*` write. Full spec and controls in
`Docs/Research/DeepSeek/DS127_R4_SEEK_EPISODE.md`.
- A: `BotBrain.cpp:339` sets the clock in `ResetLife`; `:756` pursues only while `now-start<6`; `:767` therefore grants
  `GiveTestSCAR` on the first dry-out after 6 s even when `FindNearby` (`:747`) just returned a reachable item.
- B: once `m_fallbackGiven` is true, `:767` is false and `:773` clears `m_weaponTarget` every tick with
  `WBI(move,false)` — the bot finds an item, discards it in the same tick, never moves and never picks up until
  death/restart. `ResetLife` is not called on ammo exhaustion (`:314`, `:418`).
- Budget vs radius: 30 m search (`:747`) needs ~12 s at run 2.50 m/s but the window is 6 s, so "deadline expired" does
  not mean "unreachable". `PickUp` failure (`:763`) leaves no blacklist; `m_badItems` TTL is 90 s (`:150`).
- Proposed minimal lifecycle: episode clock plus a separate fallback decision (6 s), progress/total pursuit budget via
  the existing `WeaponGoalExpired` (8/60 s, `BotWeaponGoal.h:24`), 10 s cooldown, no-candidate closes immediately,
  `MarkBadItem(id,5)` on pickup failure, fallback still exactly once per life, `SelectAvailable` keeps priority.
- Oracle: `Tests/test_bot_weapon_seek_caller.py` extracting the real method body (pattern of
  `Tests/test_bot_weapon_goal_caller.py`); 5 RED on 307 (controls 1,3,4,5,7), 4 invariants GREEN (2,6,8,9);
  old-compiler-failure never counts as RED.
- Not proven: live frequency of B and the real `GiveTestSCAR` failure rate; 10 s/8 s/60 s are proposals, not
  measurements. C154 runtime pin and DS126 review commit `4c710aca` are untouched; ready to review Codex's exact R4
  candidate on request.

### DS126 · 2026-10-29 20:33 · OK · exact307 CI40919291234 code/data/harness gate (physical UNTESTED)
**DS126 307037bc CI40919291234 code/data/harness: OK**
Review `Docs/Research/DeepSeek/DS126_307_GATE_REVIEW.md` (commit `4e27ac48`); final review head is the branch
head after this outbox push. No game/server/client/probe/install/QA/feature write by me; owner may be playing.
- CI `40919291234`: `success`, exact head `307037bc`, push to `claude-sprint`, attempt 1; unit job
  `123607844493` success (new exit-retry caller ran), client/server job `123607844840` success (17:13:45Z→17:24:02Z).
  Artifact `12917623035`, digest `sha256:2003a25a57afa20a89175093daa1f62e3f5c9124ef134cfa609d15c5bb49553a`.
- Downloaded pair recomputed: client `d4a08fbd3216088a21125127a4cc87d4a4ab4d0f7347c98ce259abe2a35d7fe8`,
  server `bdd084af320a7bd5892f521a895742c1f0c2b07421ac468e1dff21d110303226`; `sha256.txt` and
  `ci-provenance.json` match, provenance run jobs/ids/timestamps match the live API, `ci_source_match=true`.
- `c154-quality-draft.json`: source/ci/pair/artifact/task/scope correct; harness `e973b905…` and
  `harness_sha256=87c3f5ee…` verified on `Tools/Invoke-WarsBotsC154Route.ps1`; analyzer
  `2a642aebcfff2d10e119746b42f8e50f48a004d732f7aee106602b3e6fff88f7` verified on the actual file; V5 data
  `0230a0c3/b60178d7/d95e1e11/a95a0503` recomputed; `manifest_guard_receipt_sha256=9a958ea7…` matches the new
  `build/c154-307-manifest/v5/result.json` (source hash `21df800a…` = `BotNavMesh.cpp` at 307, tamper/CRLF PASS,
  game_linked=false). `review_ok=false` is the correct pre-approval state.
- `accepted-consumed-control` GREEN: accepted retry consumed to default/Idle is never resent, no fabricated
  egress. Code is the DS125-reviewed 307 with no post-review delta; feature-OFF guarded, no new cvar/default/
  wire/identity or physical-guard change. Physical root cause/outcome remains UNTESTED.
- Wake read-only: `WarsBots Mailbox Wake` Ready, same user SID `…-1006`, LastTaskResult 0, active config pins
  `ses_e7785dde5ffe9MiW2PwJiototK` (exists, pending 0); stale backup config only in
  `build/c141-wake-before` (old session, unreferenced). No mutation.
- Next: Codex pins this final review head in `c154-quality-draft.json` and sets `review_ok=true`; then the ONE
  lower-545 -> upper floor -> water -> dry shore run only when owner/game free. No default/PS/client promotion;
  no claim the exit is fixed before that run. IDLE.

### DS125 · 2026-10-29 20:11 · PRE-CI OK · exact `307037bc08050d39f26310dc6cc7071dddddf4a6`
Report `Docs/Research/DeepSeek/DS125_PRECHECK_307037BC.md` on `warsbots/research/deepseek`. Tree clean, parent
`abac72a9`, cumulative diff exactly 10 files, +650/−15. No game/QA/feature write; this is review-only.
- Scope verified: no new cvar/default/wire/identity, no position/velocity/collider/physics-guard change; only
  `BotLadder` policy budget and the ladder executor pseudo argument. `exitWait=2.0` top wait unchanged;
  `exitBudget=3.0` used only by WaitExit (`BotLadder.h:58-61,167-170`).
- Hardening verified: OFF calls guarded (`Player.cpp:7335-7345`); real SDK `GetFlags` non-const
  (`ICryAnimation.h:430`); attempt consumed before `SetInput` even if rejected (`Player.cpp:7487-7489`);
  `IsDefaultInputValue` + `ClearInput` used (`7481,7519`); orphan latches cleared before Restore early return
  (`7557`); output NUL vs null `-1` preserved (`PlayerMovement.cpp:1598-1600`); PseudoSpeed id/name validated
  before read; completion output+PhysicalReady first and immediate return (`1645-1658`); retry only after
  PerAnimGraph + default signal + `ladderOffTop` not current + same owned graph/valid id (`7463-7492`).
- Lifecycle: only `m_warsBotLadderGraphOverride` writers are `7543 true` / `7559 false`; Clear tracks only the
  accepted recovery signal, rechecks same graph/ownership/current value; original stock pending is not claimed.
  Cancel/Done/OFF/death/Revive/graph replacement funnel through Restore; controls cover all.
- Oracle: new `Tests/test_bot_ladder_exit_retry_caller.py` extracts the real Retry/Reset/Clear/Restore/ExitTop/
  exit block/ExecuteLadder/CalculatePseudo and the actual `BotMovement::Move` `SetPseudoSpeed` statement (no copied
  ternary). Old-abac RED = 11 failed controls; current GREEN (`build/c154-ds124-check/old-red-final`,
  `new-green-final`). Rejected attempt/no-repeat/non-default API/orphan controls included. Existing ladder/
  graph callers use explicitly isolated stubs; real lifecycle has the dedicated caller. CI step added in the same
  job with failure propagation; compiler failure raises, never RED.
- Read-only QA evidence: BotLadder/BotConfig units + climb/graph/ladder/mode callers PASS; Player ON/OFF and
  Brain/Loco/NavMesh/Manager/Movement ON + Movement OFF syntax logs empty. No local game link/build claimed.
- Non-blocking notes for the run: retry is gated on output != `'1'`, so a stuck climb output `1` waits out the
  budget (fail-closed); `IsDefaultInputValue` is AND-all-layers (conservative); wrapper `GetInput` is log/cleanup
  only (debug assert note); QA config/adapter logs are exit-code based.
- Next: Codex may push `307037bc` and run the new exact FULL CI pair + separate fresh all gate; then **one** whole
  lower-545 -> upper floor -> water -> shore when owner free. No claim the physical exit is fixed.

### DS124 · 2026-10-29 19:52 · PROGRESS · bounded native exit retry after DS122 block
Report/patches `4068d8998f1fd78f346fc4f2becd277cc2625bc0` on `warsbots/research/deepseek`:
`DS124_EXIT_RETRY_REVISION.md`, `ds124_exit_retry.patch`, `ds124_ladder_pseudo_parity.patch`,
`ds124_exit_retry_caller_test.py`. `ds122_exit_signal_hold.patch` is BLOCKED/superseded — do not integrate it.
No game/server/QA/install writes; game checkout remains clean `abac72a9`.
- Completion first: copy output byte/state text BEFORE any graph write; output+`NativeLadderExitPhysicalReady` ->
  stock `ReachedEnd` + immediate return, no mutation. At most ONE recovery signal per exit
  (`TryWarsBotLadderExitRetry`), only after stock Signal consumed/default (`empty|none`), no `ladderOffTop`
  current, same graph/owned/server/flag1/alive/notfrozen/notragdoll/on+exiting/valid Signal id+name/PerAnimGraph.
  No movement/SetPos/collider/identity/graph Update. `IsSignalledInput` no longer used.
- Lifecycle: latch reset on `ExitTop`; cleanup in `LeaveLadder` other reasons + `RestoreWarsBotLadderGraph`
  (Done/cancel/OFF/Revive/graph replacement, `Player.cpp:3923`, `BotBrain.cpp:1557,1618`), clearing ONLY our
  still-current `exit_ladder_top` on the same owned graph; unrelated inputs/replaced graph untouched.
- Actual ordering documented: `PrePhysicsUpdate` computes mode with `exiting=false` on the trigger frame
  (`Player.cpp:1578-1585`), stock signal later in movement (`PlayerMovement.cpp:1675`, `Player.cpp:7333-7337`);
  next frame PerAnimGraph. Handoff risk, not proven cause -> one bounded retry, not per-frame spam.
- Separate fail budget `exitBudget=3.0` for `WaitExit` only (`BotLadder.h:55-60,160-167`); human curve ~2.0 s has
  no headroom in old 2.0 s. Failure only; physical/grounded Done unchanged; no timer/height success.
- Pseudo parity separate patch (probe7/pseudo10 verified; native 1.0): valid consistency gap, rootcause unknown.
- Actual-caller oracle (real extracted blocks, not substring): abac `--expect old` -> `DS124 caller RED PASS`;
  patched `--expect new` -> `DS124 caller GREEN PASS`; cross-runs fail exactly 7/6 properties (not self-fulfilling);
  compiler failure raises, never RED; `IsSignalledInput` counter 0. Bundled Python + VS2022 vcvars used; logs only
  in scratch. `git apply --check` PASS for both patches on exact abac blobs.
- Next: Codex reviews P1/P2 separately (NOT DS122), new exact FULL CI pair, fresh all-review, then ONE whole
  lower-545 -> upper floor -> water -> shore when owner free. Retry sent but `held=0` -> selection-rule issue, not a
  resend; `upd=0` -> character update lifecycle. No runtime claim.

### DS122 · 2026-10-29 19:43 · PROGRESS · guarded upper-exit repair proposal (offline; no game/build/QA)
Report/patches `1915a3c9bdd7689212e3851849549a78d096387c` on `warsbots/research/deepseek`:
`Docs/Research/DeepSeek/DS122_UPPER_EXIT_REPAIR.md`, `ds122_exit_signal_hold.patch`,
`ds122_ladder_pseudo_parity.patch`, `ds122_exit_signal_contract_test.py`. No game/server/QA/install action.
- Narrowing: attempt-4 `12.40 LADDER EXIT output=1 state=RelaxedIdleNW motionFlags=796 pos 97.920`, `12.43 output=0
  RelaxedRunNW`, frozen to `14.45 blocked_top`. `796` = `eACF_PerAnimGraph` (`Player.cpp:1578-1585`), so the AC exit
  mode is already graph-driven; the live gap is graph state selection/hold.
- Graph contract: `relaxed_ladderOffTop_nw` selected only by `Signal=exit_ladder_top` (`PlayerFullBody.xml:5054-5060`);
  from the climb state `RelaxedIdleNW` link precedes the off-top link (`:49535` before `:49536`) and always matches
  (`:1119-1127`). Signal is set once (`Player.cpp:7333-7337,7498-7519`); bot `WaitExit` sends nothing
  (`BotBrain.cpp:1639-1642`). Human success is client-side root motion (DS119), not proof for the server graph.
- PRIMARY patch: re-issue the same stock signal in the stock exit branch only while the off-top state is not current
  (`!exitHeld`, pinned state name); no position/velocity/collider/completion write; stock `OnLadder` +
  `NativeLadderExitPhysicalReady` + exactly one `ReachedEnd` stay (`PlayerMovement.cpp:1587-1624`). Folded `WB_LOG(2)`
  adds `held/sig/pseudo/upd` to the existing line and one `LADDER EXIT SIGNAL` line in `LeaveLadder`.
- PARITY patch (separate, not sold as rootcause): `ExecuteLadder` pseudo `-1.f`->`CalculatePseudoSpeed(false)`=1.0;
  verified arg positions probe=7/pseudo=10 (`BotMovement.h:14`), native `Player.cpp:2503-2515`; both 0.5/1.0 match the
  locomotion SelectWhen ranges, so it does not explain the lost exit.
- Checks: `git apply --check` PASS for both patches on exact `abac72a9` blobs; contract test RED on abac / GREEN on
  patch (not executed here, no Python; assertions hand-checked on both blobs). No runtime, no default promotion.
- Decision table for ONE later run: held+egress -> native repaired; signal reissued but held=0 -> selection-rule loss
  (reviewed forced native state or swept-capsule fallback, never point rays/`PushesPlayersOnly` alone); `upd=0` ->
  character update lifecycle gap. Next: Codex review/integration only.

### DS121 · 2026-10-29 20:41 · C155 CLASSIC PACKAGE OK · Steelmill IA 8 bots, abac pair, C119 data
C155 CLASSIC PACKAGE OK
- Package `E:\WarsBots\Showcases\Steelmill-Classic-abac72a9` (new versioned package on E, C: untouched by it).
  `Run.ps1` sha256 `b4a863629dcfb677a1698434463d7f438309190bf7b122a2170885cea7ef8420` = QA
  `Tools/Launch-WarsBotsSteelmillClassic.ps1` (identical); `package-manifest.json` sha256
  `64c703df5a7d37d36eb47c8dedf648c82d608430d4ff3d617cdbe4c0794418e6`; all 16 manifest entries re-hashed by me
  (PACKAGE_MANIFEST_OK).
- Inputs pinned: source `abac72a9a1eb02793a2f4ab2b1a72f7c243dd72e`, CI `40890940918`, pair
  `0f8f250f…dccc2b1` / `b7c18962…ddf1706` (manifest + build.json), data = canonical C119 Steelmill, every file
  byte-identical to the frozen C150 Steelmill data (`nav cfc1181…`, `links 7c384d1…`, `spots 25c3eec…`,
  `manifest 546abe5…`).
- Preset: Steelmill / InstantAction, port 46166, `retained_bots 8`, `polys 9333`, `wb_maxbots 8`,
  `wb_bot_ladder 0` in both flags and critical, PS capture/buy/doors/vehicles 0, demo 20 Hz, move trace, log 2.
- Run.ps1 diff vs the C150 helper is exactly the reviewed set: header, source/CI pins, map restricted to
  Steelmill, roster `+wb_add 2 beginner +wb_add 2 normal +wb_add 2 veteran +wb_add 2 elite`, and readiness now
  requires all eight actual `#N created … profile=<style>/<skill>` lines in order (1-2 beginner, 3-4 normal,
  5-6 veteran, 7-8 elite). Snapshot/restore of Memory+Navmesh, PID+start-time ownership, game-process guard,
  port-free guard, log/demo archive and `archived_restored` are the unchanged helpers.
- Command size re-computed independently from the preset with the launcher's own construction: preflight 1471,
  server 1537, client 427 bytes, all < 2048 (Codex's PrepareOnly PASS matches).
- Delivery (owner runs when ready):
  `E:\WarsBots\Showcases\Steelmill-Classic-abac72a9\START.cmd` (or
  `powershell -NoProfile -ExecutionPolicy Bypass -File "E:\WarsBots\Showcases\Steelmill-Classic-abac72a9\Run.ps1"`),
  `CHECK.cmd` for the dry run, `RESTORE.cmd` if recovery is ever needed. F6/F7 spectator previous/next, F1 view.
- Game-free checked now (no CryMP/Crysis processes); owner must re-check before starting, and again before any
  later runtime. This is a classic Steelmill showcase only — **no Shore/upper-exit acceptance claim**, ladder is
  off by design; frozen C150/b8 and the Shore preview package remain untouched.
- Cleanup/wiki: not executed (no deletions by me). Proposed safe plan: inventory first, remove only obsolete
  build/review/cache trees and stale mailbox state, keep current source, current evidence, uncommitted foreign
  edits, licences and frozen owner packages; the successor can execute it with the owner after the showcase.
- Next: return to the DS120 offline upper-exit design (already pushed `3f57c40a`); no runtime until a coherent
  fix + new exact full green CI + fresh review + explicit owner finished.

### DS120 · 2026-10-29 20:22 · OFFLINE ONLY · reviewable exit repair design, no runtime, frozen package untouched
- Update-lifecycle audit (code contract, not runtime): `CS_FLAG_UPDATE_ALWAYS` is set only inside
  `!IsClient()` **and** a non-null render proxy (`Player.cpp:4449-4469`); `SetForceSkeletonUpdate(4)` under
  `m_warsBotPose` (`:1718-1723`) is skeleton-pose only and does **not** prove that flag. Whether a server bot owns
  a render proxy stays a runtime unknown.
- The bot's AG does tick: the owner log shows real state changes `IdleNW -> WalkNW -> walkToIdle`, so "graph never
  updates" is disproven; what is unproven is whether an entered ladder exit state would drive **root motion** on
  the dedicated server. First-person contract only skips `skipFirstPerson` states — no proof the template is
  player-only. No `IsPlayer`/channel flips, no physical RELAXED, no double graph Update.
- Verdict: the native graph is not disproven as the exit owner; the observed failure is state selection. Since a
  standalone instrumentation run is not authorised, the fallback is specified as the "if needed" branch.
- Collision-safe fallback design (owned/live/on-ladder/exiting/flag1 only, humans/OFF/non-owned untouched):
  native link frame only (`m_stats.ladderTop/Bottom/orientation/upDir`), stock `eCMT_Fly` climb to the native top,
  then native lateral movement along `ladderOrientation` with **explicit swept clearance** (feet/mid/head rays,
  same world mask as the stock top ray `PlayerMovement.cpp:1657`) and a **ground probe** before committing.
  `GrabOnLadder` sets `eColliderMode_PushesPlayersOnly` (`Player.cpp:7255-7256`), so "physical movement" alone is
  not collision — the design requires a normal collider mode for the owned bot during phase 2 and the stock
  restore (`:7343-7344`) on real leave. Completion only via stock `ReachedEnd` when
  `NativeLadderExitPhysicalReady` (near top + lateral out of shaft) holds and the policy sees the upper floor
  grounded; blocked ray/no floor/bounds → existing `blocked_top`, no pass-through, no timer/SetPos/impulse.
- Batch item 2 (preview): the immediate first attempt fails and bans the link before the 60 s revive. Design:
  keep the ladder closed until the reveal by generalising the existing delayed-command pattern
  (`wb_revive_at after`, `BotManager.cpp:951-959`) into a local-IA-only `wb_after <1..300> <command…>`, so a
  future versioned package can start with `wb_bot_ladder 0`, let the client join, then enable + revive at 545.
  Frozen current package untouched.
- Controls proposed: RED on `abac72a9` (no motion/no state hold), fallback GREEN with blocked hatch/wall and
  missing-floor counterexamples, collider-mode restoration on leave/cancel/death, human/client/OFF zero-call
  controls, no SetPos/impulse/timer in the extracted block. Code contract vs runtime unknown kept explicit.
- Doc: `Docs/Research/DeepSeek/DS120_EXIT_REPAIR_DESIGN.md`. No game/QA/install/live writes; own branch only.

### DS119 · 2026-10-29 20:05 · OFFLINE ONLY · same-shaft human trace vs bot: caller difference + corrected hypotheses
- Owner hold respected (no game/server/client/probe/live data/install/recovery). Vision used once
  (`frame-070.jpg`, upper corridor/deck spectator view, no shaft in that frame); no frame sync, audio not used.
- Human actor 65479 at the same shaft: climb `90.454 -> 97.839` at `vz=1.077` (bot 0.900); exit is a two-phase
  root-motion curve — trigger `97.934 vz 0.449`, `~0.73 m/s` up to `98.85`, then diagonal to `99.35 y=1879.792`,
  `grounded=1` at `99.392`, settles `99.75` at `(2116.2, 1879.7)`. About 1.9 m up + 0.85 m lateral in ~2 s.
- Owner-session bot (same pair abac/V5): start 1.05, prepare 3.74 `activeBefore=1 stanceBefore=combat`, exit 12.30
  `output=49 IdleNW`, 12.33 `output=0 WalkNW`, 12.36-14.42 `relaxed_walkToIdle_nw_01`, feet frozen `97.932`,
  fail 14.42. So the movement request **decays** (Idle -> Walk -> walkToIdle) instead of being stale — DS118 v1
  wording corrected. Headless attempt4 showed RunNW; the reason for the difference is unknown.
- Caller difference (source): a client calls `RequestLeaveLadder` -> `SvRequestLeaveLadder` RMI
  (`Player.cpp:7310-7324`) and its own animation/root motion moves the client actor (client-authoritative MP);
  the owned server bot calls `LeaveLadder` locally, the exiting branch sets no `m_request`
  (`PlayerMovement.cpp:1587-1620`), so only a server-side animation could move it — and the server graph does not
  enter the exit state. That is the concrete correlate; C-2 (first-person/closed template) stays a hypothesis.
  Clock correction accepted: ~3 graph frames per 0.1 s at ~30 Hz, not 6-20.
- Preview vs defect: the link ban is **failure-derived** — the first immediate attempt fails at the top and bans
  the link, so the 60.07 revive finds nothing to climb (log: revive ok, no second `LADDER start`). With a working
  exit the first attempt would not ban and the same schedule would show the reveal. Do not clear bans blindly or
  edit the frozen package.
- Narrow candidate (needs explicit owner decision): either (a) prove/hold the server exit state (unproven; needs
  the AG input trace the owner ruled out as a standalone probe), or (b) a physically validated server exit
  executor, using the human trace as the reference: stock `eCMT_Fly` climb to the shaft top, then real
  forward/lateral movement onto the deck at `y≈1879.7`, completion only with `NativeLadderExitPhysicalReady`
  (near top + lateral out of shaft) and the upper floor grounded (`z≈99.39..99.75`). No SetPos/impulse/timer/
  manual ReachedEnd/new cvar; humans/OFF/non-owned unchanged. (b) is an invariant change, then new source + full
  green CI + fresh review + owner finished before one route.
- Doc: `Docs/Research/DeepSeek/DS119_HUMAN_VS_BOT_EXIT.md`. No runtime; no package/source change by DeepSeek.

### DS118 · 2026-10-29 19:47 · OFFLINE ONLY · corrected diagnosis; no behaviour patch is justified yet
- v1 corrections accepted and verified: `MOTION INTENT requestedBase` is telemetry from `m_intent`
  (`BotBrain.cpp:492-494`), not a request event; `ExecuteLadder` uses stance max speed + **constant pseudo 1.5**
  and returns when exiting (`BotLocomotion.cpp:151-164`); `BotMovement::Stop` first calls `SetWarsBotMotion` and
  **overwrites the aim** (`BotMovement.cpp:14-15`); the exit signal is emitted inside the movement update
  (`PlayerMovement.cpp:1650-1672` -> `Player.cpp:7332-7336/7449`), i.e. 6-20 graph updates before the next ~10 Hz
  brain tick, so a `Stop` at WaitExit is late by construction; the native path is not "always zero"
  (`CNetPlayerInput::PreUpdate`, `NetPlayerInput.cpp:15-68`, refreshes delta movement and
  `CalculatePseudoSpeed` every frame; a human holding forward also has pseudo > 0).
- Withdrawn: the v1 "Stop in WaitExit" patch, its "aim retained" oracle point, and any claim that the stale
  request is the proven physical cause. Still confirmed from the log: prepare worked, climb unchanged, the graph
  sits in `RelaxedIdleNW`/`RelaxedRunNW` at `97.920` and the guard refuses egress.
- Candidate causes kept separate, none proven: C-1 request shape (bot move-target + pseudo 1.5 vs human
  delta-movement + `CalculatePseudoSpeed`); C-2 graph environment (native `IsPlayer`/first-person vs owned bot,
  closed `IdleSignalPlayerAnimation` template with `animationControlledView=1`); C-3 one-shot Signal consumed in
  the same graph update before the ladder exit state can hold (ordering fact, cause depends on C-1/C-2).
  Not assumed: `forceGuard=0` as a hard filter, graph flags = animation `CA_*` flags, physical `STANCE_RELAXED`.
- No blind skeleton/collider enabling: bots already get `SetForceSkeletonUpdate(4)` under `m_warsBotPose`
  (`Player.cpp:1718-1723`) and `CS_FLAG_UPDATE_ALWAYS` in `PostPhysicalize` (`:4463-4467`); installed graph hash
  `80232bb2…` equals the extracted asset, so no override mismatch.
- Coherent next step (offline/authorised run only): instrumentation in the existing `WB_LOG(2)` exit line —
  `PseudoSpeed`, `desiredVelocity`, `Action` string, `FirstPersonMode`, `Item`/`Stance` inputs (state name already
  logged). No new flag/cvar, no forced `Update()`, no behaviour change. That run separates C-1 from C-2 before any
  patch is integrated.
- Meaningful actual-caller checks proposed instead of self-fulfilling ones: (1) ordering refutation — real
  top-ray/`RequestLeaveLadder` block vs real `BotBrain` WaitExit branch with a 10 Hz brain clock, asserting the
  signal always precedes the first WaitExit tick (proves the WaitExit-Stop class cannot act in time); (2) input
  shape comparison of the real `CNetPlayerInput::PreUpdate` vs `BotMovement::Move` request construction.
  A fix, if any, only after a discriminating run, validated against the observed transition (exit state must hold
  and feet must leave the shaft), not against the patch's own intent.
- Owner hold respected: no game/server/probe/install/data action. Doc v2:
  `Docs/Research/DeepSeek/DS118_UPPER_EXIT_DIAGNOSIS.md`. No runtime requested.

### DS118 · 2026-10-29 19:28 · OFFLINE ONLY · upper exit: stale locomotion request, not a missing animation
- Owner hold respected: no game/server/probe/client, no live data, no install/recovery; only source/archive reads
  and own-branch docs. Reused the DS117 audit (`ccbbe1cf`) and attempt-4 archive; source `abac72a9` clean.
- Confirmed from the log: prepare worked (`activeBefore=1, stanceBefore=combat`), climb unchanged, and at the top
  `output='1' state=RelaxedIdleNW` -> `output=default state=RelaxedRunNW`, feet pinned at `97.920` until
  `blocked_top 14.45`; exactly one movement line in the whole exit window
  (`12.43 MOTION INTENT gait=0 requestedBase=1.30 blocked=1`), no stop/zero request.
- Asymmetry found in source: a human's movement request is refreshed every frame, but an owned server bot has no
  per-frame writer — `CPlayer::UpdateWarsBotMotion` (`Player.cpp:5082-5142`) only re-issues `RequestMovement` in
  its client-replica branch; on the server it just turns the body. So when the ladder WaitExit branch
  (`BotBrain.cpp:1639-1642`, "Do not send Stop") sends nothing, the controller keeps the last walk request and
  `PseudoSpeed > 0` keeps `RelaxedIdleNW`/`RelaxedRunNW` (`PlayerFullBody.xml:1119-1127/1140-1145`) eligible, while
  the one-shot `Signal(exit_ladder_top)` state (`:5054-5060`, set by `Player.cpp:7335/7449`) cannot hold. The
  exiting branch then builds no `m_request` (`PlayerMovement.cpp:1587-1620`) and the capsule cannot move.
- Hypotheses kept separate: H-A stale input (primary), H-B signal consumed before the state can hold (covered by
  H-A), H-C PerAnimGraph/root-motion/collider policy (unproven; the exit state never ran, so nothing supports it).
  Not assumed: Stance as a hard filter (`forceGuard=0`), graph flags = animation `CA_*` flags, physical
  `STANCE_RELAXED` (player.lua -2).
- Minimal candidate proposal (own branch only, Codex integrates): in the owned ladder WaitExit branch call the
  existing `BotMovement::Stop(actor)` (`BotMovement.cpp:12-23`: clear target, desired 0, pseudo 0, real request)
  once on entering WaitExit, scoped to owned/alive/not-frozen/not-ragdoll/on-ladder/exiting/flag1. Everything else
  stays: stock top ray + exit signal, `NativeLadderExitPhysicalReady`, empty-default rule, prepare/restore,
  physical stance/capsule/speed; no SetPos/impulse/timer, no manual `ReachedEnd`, no new cvar.
- Oracle (engine-free): RED on `abac72a9` because WaitExit issues no request (stale 1.30/0.29 expected); GREEN with
  one zero request (cleared target, desired 0, pseudo 0, aim kept); negative controls for human/OFF/non-owner/
  not-on-ladder/dead/frozen/ragdoll, latch-once, and no leave/position/timer calls from Stop.
- Doc: `Docs/Research/DeepSeek/DS118_UPPER_EXIT_DIAGNOSIS.md`. No runtime requested; the eventual ONE route needs
  a coherent source patch + new full green CI + fresh review AND the owner explicitly finished.
