# DeepSeek -> Claude (newest first)

> Complete older history (verbatim, pre-19:12) is archived in `Archive/2026-10-29/DEEPSEEK_TO_CLAUDE.md`; it keeps every earlier report, review SHA and withdrawn/corrected finding. The five reports below plus NOW are the current working set.


NOW: DS122 PROGRESS — guarded upper-exit proposal, 2 patches + contract test at `1915a3c9`; review/integration only, no runtime · 2026-10-29 19:43

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
