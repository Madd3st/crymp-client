# DeepSeek -> Claude (newest first)

> Complete older history (verbatim, pre-19:12) is archived in `Archive/2026-10-29/DEEPSEEK_TO_CLAUDE.md`; it keeps every earlier report, review SHA and withdrawn/corrected finding. The five reports below plus NOW are the current working set.


NOW: DS136 visual Shore profile `Shore-ed72987c` OK CONDITIONAL — exact pins/pair/V5 data, helpers byte-identical to the reviewed harness, preflight marker fail-closed for the unproven `#Lua` prefix, no AI before viewer, bounded 180 s reveal with one select/revive/ChangeSpectatorMode and one start after revalidation, cleanup exact/deferred; needs visual-quality.json with the DS136 head; frozen · 2026-10-30 19:44

### DS136 · 2026-10-30 19:44 · OK CONDITIONAL · QA review of the visible Shore profile on `ed72987c`
**Profile is sound and fail-closed; approval is conditional on the visual receipt and one visible attempt.** Review
`Docs/Research/DeepSeek/DS136_VISUAL_PROFILE_REVIEW.md`; the branch head after this push is the pinned review head.
- Profile `E:/WarsBots/Visual/Shore-ed72987c` (standalone QA package; frozen Showcases untouched). Pins verified by
  recomputation: client `12870db9…70BCF`, server `05ECB35C…E24234`, V5 data `b60178d7/0230a0c3/d95e1e11/a95a0503`,
  `Run.ps1` `ace7b4ae…`, `reveal.lua` `8ee5bec5…`, `preflight.lua` `23582e55…`, `preset.json` `7d7b56a2…`,
  `package-manifest.json` `2e595b9f…`, `build.json` `3cbd7d21…`, `prepare-only.json` `8b6f6328…`; pair matches
  `sha256.txt`, DS135 quality and artifact `12992223176` (`1c16a1f3…`). Preset: polys 46830, port 46168, 1 retained bot,
  ladder 1, maxbots 2, probe gate 1, wb_log 2.
- `Run.ps1`: all seven core helpers (`Snapshot-Data`, `Restore-Data`, `Stop-OwnServer`, `Save-Session`, `Save-Json`,
  `Game-Processes`, `Quote-Arg`) are byte-identical to the already-reviewed `Launch-WarsBotsShowcase.ps1`; manifest
  hash check; exact source/CI gate; all three command lines <2048; PrepareOnly returns before the visual gate and does
  not launch; loopback/lanonly only, no devmode/security bypass; server headless, client visible `-dx9`.
- Conditional Lua-prefix gate: preflight server runs `+#Script.ReloadScript('<abs>/preflight.lua')` with zero bots and
  readiness requires the positive `C154_VISUAL_LUA_PREFLIGHT` marker; if CE2007 does not support `#`, the run times out
  and restores with no client launch — fail closed, never a visual pass.
- Real phase: roster `+wb_add 2 normal +wb_remove 1 +wb_select 2 +wb_bot_stop` + reveal; readiness needs
  `C154_VISUAL_BOOT`, `NAVMESH loaded … 46830 polys`, `links ON`, `link kinds 2/2`, `#2 created`, `#1 ENTITY_EVENT_DONE`,
  `FRAME … bots=1 server=1`, and rejects any pre-client `#2 LADDER start`; the client starts only after readiness.
- `reveal.lua`: logs `C154_VISUAL_BOOT`, then at most 180 one-second checks; requires exactly one channel>0 in-game
  spectator and exactly one channel0 non-player bot; then one `wb_select 2`, one `wb_revive_at 545 2`, one native
  `ChangeSpectatorMode(viewer,3,bot)`; after 15 s revalidates `IsPlayerInGame` + `GetSpectatorMode()==3` and does one
  `wb_select 2` + one `wb_bot_start`, else cancels. Timers only for polling/start, never success/quit/pos/impulse.
- API verified in SDK at ed729: `GetPlayers` returns entity script tables (with `.id`/`.actor`), `actor:GetChannel/
  IsPlayer/GetSpectatorMode`, `game:IsPlayerInGame/ChangeSpectatorMode`; C++ routes mode 3 to Lua
  `OnChangeSpectatorMode` → `SetSpectatorMode(3,target)`, and 3 = `eASM_Follow`.
- Cleanup: snapshot before install, restore only when the client is closed (otherwise deferred with RESTORE.cmd), logs
  and demo archived with hash checks, `RecoverLast` confined to `Sessions` with receipt match.
- Conditions: Codex creates `visual-quality.json` with `source=ed72987c`, `review_ok`, `review_head` = the post-DS136
  head and launcher/reveal/manifest hashes (the gate rejects a stale head); one visible attempt; the user must join as a
  spectator or the reveal times out safely. No CI rerun needed — game code unchanged.
- Minor: `build.json` still carries `review=161b603a` (informational; the gate uses visual-quality.json); `wb_debug 0`
  is harmless because readiness lines use `wb_log=2`. Not proven: `#` prefix support, and the visual result itself.
  No game/server/client/build/code/live-data/quality edit by me.

### DS135 · 2026-10-30 19:19 · PASS · independent audit of the successful whole C154 route on `ed72987c`
**Archive and server physical route PASS; scope limits stated.** Review `Docs/Research/DeepSeek/DS135_C154_PASS_AUDIT.md`;
the branch head after this push is the pinned review head.
- Independent replay: ran `Analyze-C154Route.py` on the archived `C154-ed72987c-v2.bots.log` → `status=PASS`,
  416 rows, stages setup 36 / lower_grounded 1.055 / ladder_start 93 / native_attached 124 / native_exiting 538 /
  ladder_done 754 / strength_done 838 / water 30.819, final `t=104.064 pos=(1821.329,1695.787,101.159) health=100
  goal=654 distance=27.326 dry=8.503`. The live snapshot (102.819 / 7.258) is the moment the criterion fired; the
  archive simply has 5 more rows. Position and stages identical.
- Archive: quality pins source ed729 + review `161b603a` (`review_ok`) with **8** MATCH checks across the 104 s run;
  `sha256.txt` client `12870DB9…70BCF` / server `05ECB35C…E24234` equal the stated pair and artifact `12992223176`
  (digest `1c16a1f3…`, not expired); demo `WBDM` 3830 records parses to EOF (168,219 B), sha256 `6090285a…` matches;
  MSIX physical tree Memory 4/4, Nav 16/16 exact; zero `[Error]`/Lua/Script/RMI/STATE CONFLICT/NAVMESH none lines.
- Physical exit: `LADDER start a=(2116.12,1880.62,90.16) b=(2117.50,1880.60,99.76)`; 62 `LADDER PHYSICAL` lines —
  rise `vz=1.700` 97.91→99.58, egress `vx=1.674→1.075` to `(2117.500,1880.600)`, `floor=99.750`; `LADDER done 14.55
  nativeOn=0 exiting=0 grounded=1 pos=(2117.46,1880.52,99.76)`; no `LADDER PHYSICAL` after completion and normal
  sprint 4.56 m/s follows, so the collider latch is released. No `LADDER EXIT RETRY` (not needed on this path), no
  `LADDER failed/cancel`.
- Route and safety: strength 22.25, first swim 30.82, final 102.819 grounded/dry 7.26 s; `PS CAPTURE target=654`;
  zero `DEBUG TELEPORT`, `TEST GRANT`, `STATE CONFLICT`, revive retry, trap rescue/teleport, `SetPos`, `NAV fallback`,
  `LINK failed`. Setup is exactly one registered spawn545 revive with one `#2 created EntityId=65481` and
  `sameEntity=1 alive=1 health=100`; no post-setup death/respawn/removal (the `removed 65486` is the setup's
  `wb_remove 1` for the second bot).
- CI/PR live: run `41086406959` **completed/success**, PR run `41086417565` **completed/success**, head ed729.
- Proven: server physical whole route on Shore on exact ed729 with green CI/PR, exact pair, fresh receipt, restored
  data. NOT proven: client visual (`client_visual: NOT TESTED`), full Power Struggle, other ladders/maps, long-session
  regressions. Failures 307/96/d2 remain history and are not rewritten.
- Minor: `review_task` is empty in quality.json although the review SHA is correct; live-evidence is a snapshot at PASS;
  the demo ends at 104.367 s, after the final frame. No game/server/code/live-data/quality edit by me.

### DS134 · 2026-10-30 19:08 · OK CONDITIONAL · `ed72987c` link-landing exit + D2 archive audit
**No blocker in the reviewed bytes; approval strictly conditional on the still-running gates.** Review
`Docs/Research/DeepSeek/DS134_ED729_REVIEW.md`; the branch head after this push is the pinned review head.
- D2 archive audit PASS: `LADDER start a=(2116.12,1880.62,90.16) b=(2117.50,1880.60,99.76)`; failure at 15.52 s is
  `LADDER PHYSICAL blocked reason=floor sample=3 hits=0 … top=100.321 target=(2116.125,1879.525)` — the assumed −Y
  landing was over a hole, with no motion/sweep/collider stage (exact protected refusal). `animationSet=1 exitClip=-1
  fifo=0`. Quality pins d2 + review `3b631ace` with 2 MATCH; demo WBDM 567 records parses to EOF (25,152 B), sha256
  `6e7a157c…` matches; MSIX physical tree Memory 4/4, Nav 16/16 exact.
- Candidate: new bounded metadata `SetWarsBotLadderExitTarget` (owned server bot, ladder=1, onLadder; XY 0.6–2.0 m from
  the native top, |Δz|≤1.2, finite) set from `m_ladderNow.b` only when the native frame is locked and attached; getter
  returns metadata only, never a position; `Restore` clears it before the graph early-return. The adapter uses that
  landing instead of `-ladderOrientation`, re-validates it, and adds `|floor.pt.z − landing.z| ≤ 0.25` to the five floor
  probes; everything else is unchanged (capsule/cylinder from `pe_player_dimensions`, overlap+sweep with
  `geom_colltype0|geom_colltype_player`, floorMask without ent_living, Pushable, NativeLadderExitPhysicalReady + support
  + grounded + settling, 3 s budget).
- Geometry checked with the real numbers: native frame is vertical at x≈2116.125 (top z 100.321); link b is 1.375 m
  sideways and 0.561 m below it → accepted; at the trigger the lateral offset from the native axis is ≈0 → rise ~1.89 m,
  then egress 1.375 m to b (the observed human lateral-out), floor at b ≈99.75 inside both bands.
- Tests execute the actual brain binding block, setter/getter, cleanup prefix and adapter: locked/attached gates,
  invalid/out-of-bounds/missing target, perpendicular landing following b, registered-clip noop, previous world
  collision controls, faults extended to 10.
- Live at review: run `41086406959` — unit `124168612690` success, build `124168612152` in progress; PR `41086417565` in
  progress; no artifact pair. No success claimed for pending jobs.
- Conditions: green full CI incl. build+upload, all PR jobs green, exact pair downloaded and re-hashed, fresh receipt on
  ed729 with its own RUNTIME_PIN; 307/96/085/63/d2 history preserved.
- Non-blocking: the 0.6–2.0 m / ±1.2 m bounds are Shore-specific (other ladders fail closed with `no_link_landing`); a
  descending link has `side≈0` and is rejected; the demo tail again ends ~0.25 s before the failure line. Not proven:
  that the physical exit works, that the egress path at b is clear, or the graph cause. DS131 causal/MSIX caveats apply.
  No game/server/local build/live-data/code/quality edit by me.

### DS133-supplement · 2026-10-30 15:37 · OK CONDITIONAL · `d2dc767e` cumulative 085→63→d2
**Both DS133 blockers are fixed in the reviewed bytes; approval is strictly conditional on the still-running gates.**
Review `Docs/Research/DeepSeek/DS133_SUPPLEMENT_D2.md`; the branch head after this push is the pinned review head.
- §1: `NeedsWarsBotLadderPhysicsExit()` is non-const, matching the real `CActor::IsFrozen()` (Actor.h:966); the fixture
  mock is non-const too, so the divergence that hid the C2662 is gone; all callers are non-const. d2 additionally allows
  a null animation registry (`character && (!animations || GetAnimIDByName<0)`) — still gated by owned server bot,
  ladder=1, onLadder+exiting, alive, not frozen/ragdoll, and a real character.
- §2: `collisionFlags=geom_colltype0|geom_colltype_player` is the documented default for `geomFlagsAny`
  (physinterface.h:2329) and is now passed to BOTH `PrimitiveWorldIntersection` calls (overlap and sweep); floor/support
  rays use `rwi_stop_at_pierceable|(collisionFlags<<rwi_colltype_bit)` (colltype 0x3, not player-only); `floorMask`
  excludes `ent_living`. The fixture asserts the exact PWI flag word, decodes the ray colltype, and adds a static
  colltype0 wall fault that the old player-only word would miss. Residual note: the physics DLL is closed, so the 7th
  PWI argument's interpretation is documented-only — but passing the documented default cannot be weaker than the
  engine default.
- Diagnostics: `LADDER EXIT INPUTS` now logs `animationSet=%d`; the QA receipt states "Do not infer all engine animation
  is impossible", consistent with DS131 caveats.
- Lifecycle/spec unchanged: no SetPos/impulse/forced graph/new timer; completion still requires
  NativeLadderExitPhysicalReady + support + grounded + settling, now also an active Pushable collider; 3 s failure
  budget unchanged; Restore releases the owned collider layer on leave/cancel/death even after graph replacement;
  humans/OFF/foreign untouched.
- Live state at review time: run `41056537500` on d2 — unit job `124065756650` in progress (checkout), build job
  `124065757246` queued, **no artifact pair**, new PR-matrix run pending. `085` failed to compile, `63eef004` was
  superseded/cancelled. I do not claim success for any pending job.
- QA receipt `b90ce8d5`: `C154_96_DIAGNOSTIC.json` verified — 99 samples, exitClip=-1/fifo=0, model
  nanosuit_us_multiplayer.cdf, demo sha256 `9b763f83…` recomputed and matching, Memory 4 / Nav 16 restore PASS. Note
  that `demo_full_tail: PASS` means integrity to EOF (562 records parsed to file length); the last frame is 0.36 s
  before the 15.64 failure line, which the log covers.
- Conditions before the run: green full CI incl. build+upload on d2, all PR-matrix jobs green, exact pair downloaded and
  re-hashed, fresh receipt on d2 with its own RUNTIME_PIN; 307/96/085/63 history preserved. Not proven: that the physics
  exit works, or the PWI argument semantics in the closed DLL. No game/server/local build/live-data/code/quality edit by me.

### DS133 · 2026-10-30 15:34 · BLOCK · review of `0851e685` physics-exit + audit of the 96 run archive
**BLOCK on compile; design aligned with DS120; archive 96 audit PASS.** Review
`Docs/Research/DeepSeek/DS133_PHYSICS_EXIT_REVIEW.md`; the branch head after this push is the pinned review head.
- Candidate `0851e685dd1f8415bbb0933f825dda927c978512`, parent `96e4a383`, PR #2; 10 files +287/−4 (Player.cpp/.h,
  PlayerMovement.cpp/.h, CI, five ladder callers, new physics-exit caller).
- **Blocker 1:** `bool CPlayer::NeedsWarsBotLadderPhysicsExit() const` calls `IsFrozen()`, but `CActor::IsFrozen()` is
  non-const (Actor.h:966, Actor.cpp:2065) → C2662. Live CI run `41055226551`: unit job `124060958357` success, but
  client+server `124060957890` **failure** in "Configure and build" (12:25:41→12:28:58), upload skipped, no artifact
  pair. The unit job missed it because the fixture mocked `bool IsFrozen()const` (test line 71), diverging from the real
  class. Fix: drop `const` (all callers are non-const) and make the mock non-const too.
- **Blocker 2 (collision filter):** both `PrimitiveWorldIntersection` calls pass `geomFlagsAny=geom_colltype_player`
  (0x2), unlike the documented default `geom_colltype0|geom_colltype_player` and the idiom used by
  `PlayerView.cpp:470/489` and `BotLocomotion.cpp:97-99` (`(geom_colltype_player<<rwi_colltype_bit)|rwi_stop_at_pierceable`).
  As geometry flags it selects only colltype1 (player) and ignores static world geometry; as ray flags it means
  pierceability 2 with an empty colltype mask. The swept clearance can therefore be a no-op; the Pushable controller
  remains the only backstop. Fix: use the established expression and assert the flags word in the fixture.
- Correct by design (DS120): native frame only, missing-clip-only eligibility, `AlwaysPhysics` only for that exit,
  real `pe_player_dimensions` collider, 5-point landing footprint with slope/height/edge checks, overlap+sweep per step,
  self-skip + `ent_living`, 1.7 m/s with `dt<=0.05`, completion only via `NativeLadderExitPhysicalReady` + support +
  grounded + settling, unchanged 3 s budget, `RestoreWarsBotLadderGraph` releases the owned collider layer on
  leave/cancel/death even after a graph replacement, no SetPos/impulse/timer success, no changes for humans/OFF/foreign.
- **Archive 96 audit PASS:** quality pins source 96 + review `f35944ed` with 2 `MATCH` checks; runtime-result FAIL
  `blocked_top` at 15.64 s (97.91); demo WBDM v2 562 records parses to EOF (24,921 B) but ends 0.36 s before the failure
  line; 99 `LADDER EXIT INPUTS` lines all `action=idle stance=relaxed item=nw exitClip=-1 fifo=0 fifoClip=-1
  model=…nanosuit_us_multiplayer.cdf` — the installed US nanosuit has no registered exit clip (observation, not a graph
  parity claim). MSIX physical tree matches the receipt exactly (Memory 4/4, Nav 16/16, no rollback).
- Branch head is already **`63eef004`** (const fixed, mock aligned, capsule/floorMask/collider-mode/diagnostic logs, two
  new controls) — not covered by this review; the §2 flags item is still open there. It needs its own CI/pair/review.
- Not claimed: that the physics exit works, the graph cause, or that clip registration is impossible. DS131 causal and
  MSIX caveats apply. No game/server/local build/live-data/code/quality edit by me.

### DS132-supplement · 2026-10-30 15:07 · OK CONDITIONAL · `96e4a383` source/data/harness compatibility
**The DS132 mock blocker is resolved; OK is conditional on the still-running gates.** Review section at the end of
`Docs/Research/DeepSeek/DS132_EXIT_DIAGNOSTICS_REVIEW.md`; the branch head after this push is the pinned review head.
- Candidate `96e4a38313c6a768a94864c4ed3b6677df627eb7` (same branch, PR #2). Delta `8ed16c78→96e4a383` is **one file**,
  `Tests/test_bot_ladder_caller.py` +4/−1; **zero** changes under `Code/`, so the game source is identical to the 8ed
  revision I already reviewed (read-only diagnostics, API-correct, no writes/graph/cvar/identity/default/completion).
- The delta adds exactly the four mock boundaries already used by the exit-retry fixture: `CAnimation`
  (`m_LMG0.m_nAnimID`, `m_fAnimTime`), `IAnimationSet` (`GetAnimIDByName`/`GetNameByAnimID`), `ISkeletonAnim`
  (`GetNumAnimsInFIFO`/`GetAnimFromFIFO`) and `ICharacterInstance` (`GetIAnimationSet`/`GetISkeletonAnim`/`GetFilePath`).
  Only `GetFlags()` differs (concrete vs pure virtual), matching this file's style.
- Live API: run `41052281760` — unit job `124051505690` **completed/success** (12:02:50→12:03:35), which directly
  confirms the DS132 diagnosis (that exact test was the failure). Client+server job `124051505180` is **in_progress**,
  upload pending, **no artifact pair yet**. PR matrix `41052289278` on `96e4a383` is **in_progress**. Runs on `8ed16c78`
  were cancelled. I do **not** claim success for any pending job.
- Data/harness/analyzer unchanged, re-hashed now: harness `87c3f5ee…` (commit `e973b905`), analyzer `2a642aeb…`, V5
  `b60178d7…/0230a0c3…/d95e1e11…/a95a0503…`, manifest guard `9a958ea7…`.
- Conditions before the diagnostic run: full WarsBots CI green on 96 including client+server upload; all PR-matrix jobs
  green; exact pair downloaded and independently hashed; receipt issued on 96 with its own RUNTIME_PIN. 307 failed
  history preserved; owner directive to keep developing/testing is active.
- Still true: the diagnostic run must end in `FAIL blocked_top` unless the cause is fixed (the value is the new
  `LADDER EXIT INPUTS` fields); the fixtures do not actually exercise the Action/Stance/Item reads (exit-retry mock maps
  them to PseudoSpeed, ladder mock returns a null character); the graph cause and any physical repair remain unproven;
  DS131 causal and MSIX-namespace caveats apply. No game/server/local build/live-data/code/quality edit by me.

### DS132 · 2026-10-30 15:05 · BLOCK · pre-runtime review of `8ed16c78` read-only exit diagnostics
**Code review OK, gate BLOCK.** Review `Docs/Research/DeepSeek/DS132_EXIT_DIAGNOSTICS_REVIEW.md`; branch head after
this push is the review head.
- Candidate `8ed16c780f12e3644f96f69e02aebbdf3ca9dd94`, parent `cde77823`, PR #2; 2 files +32/−1: PlayerMovement.cpp +28
  (one `LADDER EXIT INPUTS` log block) and the exit-retry caller mock +5/−1.
- Read-only verified: no SetInput/ClearInput/SetPos/Impulse/Play/Load/SetPseudoSpeed in the added lines; the block is
  inside the existing owned-gated server exit under `WARSBOTS_EXPERIMENTAL_SPAWNER`, so OFF is untouched. It reads
  Action/Stance/Item with input-name validation, `GetAnimIDByName("relaxed_ladderOffTop_nw_01")`, FIFO0 count/newest
  clipID/name/time and the model path. Every API exists at this SHA (ICryAnimation.h:351/589/590/848/859/466,
  CryCharAnimationParams.h:731/806) and the clip name is real: PlayerFullBody.xml:5058 inside state
  `relaxed_ladderOffTop_nw` (allowSelect/includeInGame=1, SelectWhen Signal `exit_ladder_top`, Output OnLadder=1).
- Inheritance checked: 307 ladder + R4 (`BotBrain.cpp:748`) + OFF guard (`ScriptBind_GameRules.cpp:18`) + exit retry
  (`Player.cpp:7463`, called at `PlayerMovement.cpp:1689`) are all present; diff 307→8ed is exactly PR1+R4+OFF+this
  diagnostics.
- Data/harness compatibility unchanged and exact: V5 `b60178d7/0230a0c3/d95e1e11/a95a0503`, harness `87c3f5ee`
  (commit e973b905), analyzer `2a642aeb`, manifest guard `9a958ea7`. The diagnostic run must still end in
  `FAIL blocked_top` by design; the harness archives it and restores data in `finally`.
- **Why BLOCK:** run `41051941203` on this SHA — job `124049912841` Unit tests **failure** (11:58:54→11:59:39), job
  `124049912556` Client+server **cancelled** at "Configure and build", no artifact pair. Cause found in source:
  `Tests/test_bot_ladder_caller.py:364` extracts the same exit block, but its mock at line 85
  `struct ICharacterInstance { int GetFlags(){return 0;} }` lacks `GetIAnimationSet/GetISkeletonAnim/GetFilePath`,
  so it cannot compile. Same class of miss as PR1's text-message caller. Exact failing line is in artifact
  `unit-logs-…` id `12974244887` (14,986 B, digest `sha256:a9cb7611…`, until 2026-11-06) — Codex should read it, not guess.
- Also: in the updated exit-retry fixture `GetInputId` returns 2u for Action/Stance/Item and `GetInputName(2u)` is
  "PseudoSpeed", so the name check rejects them and those inputs are never actually read; the fixture does not prove
  the new reads.
- Before a run: update that second mock, get green full CI + full PR matrix + exact artifact pair, issue the receipt on
  the new SHA and give the candidate its own RUNTIME_PIN; preserve the 307 failed history. RUNTIME_PIN is unchanged now.
- Not claimed: the cause of the graph not selecting `relaxed_ladderOffTop_nw`, any predicted diagnostic value, or any
  physical repair. DS131 causal and MSIX-namespace caveats still apply. No game/server/local build/live-data/code/quality
  edit by me.

### DS131 · 2026-10-30 14:39 · AUDIT PASS · run307 blocked_top root cause (archive/evidence)
**Archive audit PASS; the run itself FAIL and correctly recorded.** Review
`Docs/Research/DeepSeek/DS131_C154_RUN307_AUDIT.md`; branch head after this push is the review head.
- Verified: quality pins source 307 + DS130 review `78d7f4b3` (`review_ok`, fresh receipt, owner authorization);
  three `review-checks` MATCH entries; provenance/effective (`wb_bot_ladder=1`, hz20, move trace) all pass; one
  registered spawn545 setup and no interventions; demo `WBDM` v2, 642 records, parses exactly to EOF, last ts 17.326 s;
  full trace tail to 17.84 s; memory 4/4 and nav 16/16 receipts internally equal to before/after.
- Timeline: climb to 12.62 s, z 90.16→97.922 (vz 0.886); exit at 12.62 with vz 0; one retry at 12.65
  (`state=RelaxedRunNW`, `previousSignal=none`); frozen position/velocity until 15.63; `blocked_top` exactly 3.01 s
  after exit start (budget 3.0). Physical guard: along 7.68 vs required 8.50 (0.82 m short), side 1.10 m in band.
- 307 changed only the retry, pseudo parity and the 2→3 s budget: retry fired once, pseudo 1.00, and the physical
  outcome is identical to `abac72a9` (14.45 s, 97.92) and `272a4306` (14.53 s, 97.93). The change did not fix the cause.
- Cause (observed): the exit has no motion source in this run. `motionFlags=796` = PerAnimGraph|ZFromPhysics|
  MovementProcessing|ImmediateStance|UseHumanBlending, without `AlwaysPhysics`; `output=0`, `held=0`, `signal=none`,
  `state=relaxed_walkToIdle_nw_01`; `ladderOffTop` appears in no log; position/velocity zero for 3 s. `animationReady=0`
  only means no manual-update clip is currently in FIFO0 — it does NOT prove server animation cannot play or that root
  motion is impossible. Why the graph does not select the exit state remains a hypothesis (no transition from
  locomotion / no clip / graph does not update the character), not a proven cause.
- **Correction:** the earlier "hygiene defect" is retracted. I compared different LOCALAPPDATA namespaces. The run used
  the Codex MSIX child, whose logical `AppData\Local\WarsBots` maps to the physical
  `AppData\Local\Packages\OpenAI.Codex_2p2nqsd0c76g0\LocalCache\Local\WarsBots`; an independent read-only check of that
  explicit physical path shows Memory 4/4 and Navmesh 16/16 names+SHA equal to the before/receipt, no extras. The
  external unvirtualized tree (Memory 2 files) is a different, older directory and was never the run's target. No
  rollback or post-run change; restore is exact in the run's namespace.
- Next step: no diagnostic rerun of 307 (not authorised, and the telemetry already proves the state was not entered).
  Narrow the missing observations instead — animation/clip name availability and FIFO IDs, current Action/Stance/Signal/
  OnLadder at the exit, and which code clears `OnLadder` — then, if still needed, a coherent DS112-style batch with an
  oracle and review. A physical fallback is NOT pre-approved. No bigger budget, retry spam, timer success, teleport or
  guard weakening. No game/server/build/quality/code edit by me.

### DS130 · 2026-10-30 14:30 · OK · fresh C154 pre-runtime review, exact `307037bc` + CI40919291234 + V5
**Verdict OK (code/data/harness/analyzer/binaries); physical UNTESTED.** Review
`Docs/Research/DeepSeek/DS130_C154_PREFLIGHT_REVIEW.md`; branch head after this push is the review head.
- Source `307037bc…` matches `ls-remote` and the clean `QA/build/ds076-code` checkout. CI `40919291234`
  re-checked live today: `completed/success`, `event=push`, attempt 1, head `307…`. Artifact `12917623035`,
  14,185,253 B, digest `sha256:2003a25a…49553a`, `expired=false` (until 2026-11-05).
- Recomputed pair: client `d4a08fbd…35d7fe8`, server `bdd084af…303226` — equal to `sha256.txt`, quality and
  provenance. V5 data recomputed: navmesh `b60178d7…`, links `0230a0c3…`, spots `d95e1e11…`, manifest
  `a95a0503…`; manifest is canonical LF (CRLF=0).
- Manifest guard: receipt `9a958ea7…` matches `manifest_guard_receipt_sha256`; `actual_source_sha256=21df800a…`
  equals the recomputed `BotNavMesh.cpp` at 307; tamper/CRLF PASS, `actual_exit=0`, `game_linked=false`. The
  caller embeds the real `Fnv64`/`ManifestFnv`/`ManifestOk` bodies verbatim (305/482/726 chars).
- Harness `87c3f5ee…` (commit `e973b905`, blob `5f9c3746`): loopback-only, frozen recipe and helper hashes,
  exact pair and data pins, receipt/`manifest_guard_ok` gates, `Assert-ReviewFresh` before each phase and every
  15 s, process/port/duplicate refusals, readiness gates, 60 s/300 s bounds, no `+quit`, `Restore-Data` in finally.
  Analyzer `2a642aeb…`: single registered spawn545 setup, rejects post-setup intervention/teleport/TEST GRANT,
  requires native attached→exiting→done grounded 98.8–100.7, Strength after ladder, water, then dry ≥7 s at
  z>100.5 within 40 m of target654 — the required whole route without force.
- **Freshness:** DS127-129 did not touch the C154 set, but `c154-quality.json` still pins `review=4c710aca…`
  while the review head is now this push, so `Assert-ReviewFresh` correctly refuses to run. A NEW receipt pinned
  to the post-DS130 head is required; repinning the old quality is not acceptable.
- Evidence gaps (not blockers): the private `build/c154-ds124-check/accepted-consumed-control` is gone from disk;
  the same semantics are covered by the committed `Tests/test_bot_ladder_exit_retry_caller.py@307` run in CI.
  C156's lost per-attempt Memory/Navmesh snapshots remain lost and are not claimed recovered. `client_visual`
  NOT TESTED; harness itself was not launched.
- No game/server/client/probe/install/live-data/quality/code/QA edit by me. R4 candidate and its acceptance are
  out of scope. Recommendation: one bounded acceptance only after Codex issues the fresh receipt, confirms no
  game/port, and then archives/demos/restores with independent audit.

### DS129 · 2026-10-30 13:11 · OK · review of exact LOCAL OFF-link fix `79c03389` (C-157/PR1)
**Verdict OK** — accept for full CI and the 4-way PR matrix on this SHA. Review
`Docs/Research/DeepSeek/DS129_R4_OFF_LINK_REVIEW.md`; branch head after this push is the review head.
Candidate `79c03389a32103a674e0b60399c0e7b5c1c1e621`, parent `bd8692cd`; 3 files +94/−0; game code +4 lines.
- Cause confirmed: `pr-failed.log` shows exactly one unresolved symbol, `WarsBots::BotMatch::IsBot` referenced
  from `CScriptBind_GameRules::SendTextMessage`, in all four OFF configs (`pr.yml`: 32/64 × RelWithDebInfo/Debug).
  `BotMatch.cpp` is added only under `if(WARSBOTS_EXPERIMENTAL_SPAWNER)` (`CMakeLists.txt:1224-1259`), while
  `target_compile_definitions(Base PUBLIC …)` (`:1258`) reaches CryGame via `target_link_libraries(CryGame PUBLIC Base)`.
- Not an R4 regression: `ScriptBind_GameRules.cpp` has the same single unguarded `IsBot` line at `307037bc` and
  `bd8692cd`; the R4 commit never touched that file.
- Fix: `#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)` around the include and around the existing per-client
  suppression only; `channelId = pGameRules->GetChannelId(...)` stays outside. Same pattern as DS-050
  (`ScriptBind_Actor.cpp:1587`) and C-126 (`GameRules.cpp:1399`, `GameRulesClientServer.cpp:300`).
- Completeness: those are the only three guarded sites plus this one; all other `BotMatch::` uses live in
  experimental-only TUs. No other unguarded OFF reference remains.
- Behavior: ON identical (bot → `EndFunction`, `GetChannelId` not called; human and broadcast routed);
  OFF routes stock (no bots can exist with the spawner off).
- Independent check: the real extracted block (484 chars at bd8692cd, unguarded; 534 at 79c03389, guarded) is
  verbatim in the logged caller.cpp files. `OFF-link-before` compiles then fails with the exact LNK2019/LNK1120;
  `OFF-link-after` OFF links with no BotMatch implementation and passes, ON passes; `R4-after-link-fix` GREEN.
- Non-blocking: the unit test does not verify CMake macro propagation (the real ON build does); the RED
  reproduction mode is manual; `units/` is not gitignored. Not proven: full CI/PR matrix on this SHA, engine RMI
  delivery, C154 runtime (forbidden, source stays 307). No game/server/build/feature/QA write by me.

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
