# DS114 — C154 attempt 3 on `272a4306`: archive audit and native exit blocker

Status: offline diagnosis only. No engine/QA/game/data/install/launcher/restore writes by DeepSeek.
Archive: `C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build/c154-route-272a4306-v2` (`archived_restored`).

## 1. Archive audit (independent, read-only)

- `session.json`: source `272a4306…`, `phase=archived_restored`, `client_log/client_pid=null`, `bots=1`,
  server = `build/ci-272a4306/CryMP-Server64.exe`, map Shore, started 13:46:43Z.
- This is physical attempt **3** (archive 1 of this code + V1 invalid packaging + V2 zero-climb); no retry after
  the run, no second server start. `physical_gameplay_attempts=1` in `summary.json` is per-archive, `prior=2`.
- `quality` in `summary.json` = my DS113 receipt: source `272a4306`, CI `40882465370`, pair
  `25c78bf3…/780c67be…`, harness `e973b905`/`87c3f5ee…`, analyzer `2a642aeb…`, V5 4 hashes,
  `manifest_guard_ok=true`, receipt `43da44b7…`, `review=1fd76799…`, `review_ok=true`.
- Restore, checked by hashing every file **in the MSIX namespace the game used**
  (`%LOCALAPPDATA%` → `C:/Users/Magic8/AppData/Local/Packages/OpenAI.Codex_2p2nqsd0c76g0/LocalCache/Local/WarsBots`):
  Memory 4 files and Navmesh 16 files are byte-identical to `memory-before`/`nav-before` (no extra/missing file).
  The plain `C:/Users/Magic8/AppData/Local/WarsBots` tree is a **different** (owner) copy — the run never wrote
  there: its logs/demo exist only inside `LocalCache`. Audit must always use that namespace.
- Demo `Multiplayer_PS_Shore-20261029-164645-1.wbdemo`: 23 369 B, sha256 `4dab8cf6…`, 281 frames,
  `tail_complete=true`, header build `272a4306`. A 0-byte `…-164642-1.wbdemo` also exists: it belongs to the
  actorless preflight server (`C154-272a4306-v2-pre.log`), not to gameplay.
- Cvars pass (`effective.json`), incl. `wb_bot_ladder=1`, doors/vehicles 0, demo 20 Hz, `wb_log=2`.
  Nav load at 0.06: 46 830 polys, 3 857 parts, **2 links** (V5 data, ManifestOk).

## 2. Climb fix is confirmed in the real runtime

- `2.02` LADDER start `a=(2116.12,1880.62,90.16)` `b=(2117.50,1880.60,99.76)`.
- `4.00–12.41` 271 `LADDER NATIVE` rows: `input=(0,1,0)`, stock `velocity=(0,0,0.900)`,
  `animationReady=0`; phase 3, `vz=0.89`, z rises `90.33 → 97.84`.
- So DS112 was right: the animation FIFO was the climb blocker, and the owned-bot bypass lifts it without
  touching axis/speed/collision.

## 3. Exit blocker (the new, exact failure)

- `12.47` phase 4, `exiting=1`, pos `97.93` (1.83 m below the ladder top). Trigger: stock top ray
  (`PlayerMovement.cpp:1650-1672`) found the forward exit clear and called `RequestLeaveLadder(eLAT_ExitTop)`.
- `12.47–14.53` 21 `LADDER EXIT` rows: `graph=1 output=0 motionFlags=796 pos=(2116.125,1880.625,97.927)`
  — position frozen, `vz=0`, `MOVE TRACE … flying=1 mode=3` (Fly), `grounded=1`, `peakZ=97.927`.
- `14.53` policy `LADDER failed reason=blocked_top` + `cancel`; same actor 65481, HP 100.
- Chain, actual source:
  - `Player.cpp:7326-7336` `LeaveLadder(eLAT_ExitTop)` only sets `isExitingLadder=true` and starts the **exit
    animation**. `RequestLeaveLadder(eLAT_ReachedEnd)` is the only thing that actually detaches, and it may only
    be issued from `PlayerMovement.cpp:1616-1618` when `QueryOutput("OnLadder")[0]=='0'`.
  - `Player.cpp:7444-7484` `UpdateLadderAnimation` returns false unless layer-0 FIFO has an animation with
    `CA_MANUAL_UPDATE`; a headless server bot never queues one (`animationReady=0` for 271 samples).
  - `motionFlags=796` (`0x31C`) does **not** contain `CA_MANUAL_UPDATE` (`0x0001`,
    `CryCharAnimationParams.h:53`), i.e. the animated character exists (`graph=1`) but is not in manual-update
    mode, so the graph never leaves the ladder state and `OnLadder` is an empty string (`output=0` = NUL, not
    `'0'`=48). `PlayerMovement.cpp:1604/1608` therefore return every frame — fail-closed, as designed by DS-107.
  - Consequence: the bot is left hanging mid-shaft by an **animation-scripted** exit; no timer or fake
    `ReachedEnd` fired, and the policy correctly aborted.
- Command bytes unchanged 1460/1506; data/harness/source identical to the DS113-approved packet.

## 4. Options for DS114 (need a new candidate + FULL CI + fresh review; no game run before that)

A. **Root/asset path** — make the bot queue a real ladder animation with `CA_MANUAL_UPDATE` so the graph emits
   `OnLadder`. Requires skeleton/anim support on a dedicated-server bot and passes through the
   `assert(pSkeletonAnim)` path; heavy, asset-dependent, and it also re-couples climbing to animation.
B. **Bounded physical exit for owned bots** (recommended by the evidence): keep the stock top ray, but when the
   graph completion signal is structurally unavailable (`pAC` present, output empty/unrecognised) do not sit
   frozen in `isExitingLadder`: continue the stock native climb/exit physics with the same axis/speed and finish
   `ReachedEnd` **only** on physical evidence (feet at/above `ladderTop.z` within an epsilon, forward clearance
   ray clear, grounded on the upper floor, still inside the same ladder volume/actor) — no `SetPos`, no impulse,
   no timer, no manifest bypass; policy `blocked_top` stays as the fail-closed backstop.
   This changes the DS-107 graph requirement for **owned bots only**; humans/OFF/non-owner/client keep the graph
   gate, so it needs an explicit red/green oracle: human+graph '1' stalls, human+graph '0' completes,
   owned+empty+dry-top completes, owned+empty+no-top does **not** complete, owned+not-owned/OFF/dead/frozen/
   ragdoll unchanged.
C. **Probe first**: log `OnLadder` over the whole climb (never non-empty here) plus the top-ray result and
   `isOnLadder` to prove the graph can never signal. Cheap, but does not fix; the current `LADDER EXIT` line
   already shows `graph=1 output=empty` at 21 consecutive frames, so the added value is small.

### 4b. Refinement after reading the exiting branch end-to-end (prefer this over B)

The exiting branch (`PlayerMovement.cpp:1587-1620`) never sets `m_request`: it checks the graph, optionally calls
`ReachedEnd`, then `return`s. So once `RequestLeaveLadder(eLAT_ExitTop)` has run, an owned bot has **no motion
source at all** — the observed exact freeze. Implementing B by "continuing physics inside the exiting state"
would therefore mean re-adding motion to a state that stock code deliberately leaves to the animation.

D. **Do not enter the animation exit for owned bots at all** (cleanest, same physical guarantee):
   - in the top trigger (`:1650`) keep the stock forward-clearance ray, but for `WarsBotLadderServer` require the
     feet to actually reach the top (`topDist` ≈ 0 / `z >= ladderTop.z - eps`) instead of the 2.41 m radius;
   - when that holds, call `RequestLeaveLadder(eLAT_ReachedEnd)` directly (no `isExitingLadder`, no exit anim),
     which is the stock "detach" path (`Player.cpp:7337-7344` clears `isOnLadder`, restores the collider);
   - keep the normal native climb tail (`eCMT_Fly` + stock `move`) until then, so the last 1.8 m are real physics;
   - humans/OFF/non-owner/client keep the exact old graph/animation exit.
   Oracle: owned bot must reach `z >= top-eps` with a clear forward ray before detaching; if the last metres are
   blocked it must fail closed (`blocked_top`), never detach mid-shaft; human controls unchanged.
   This keeps the stock axis/speed/collision and needs no `SetPos`/impulse/timer, and it removes the deadlock
   instead of keeping the bot in a state stock code cannot advance.

## 5. What is still unproven / risks

- Whether any server-bot animated character can ever produce `OnLadder` (option A) — unproven; current evidence
  (271 frames `animationReady=0`, 21 frames empty output) points to "structurally no".
- The exit is 1.83 m below the top when triggered; option B must therefore include real upward completion inside
  the shaft, or the bot would detach in mid-air (which is why a bare `ReachedEnd` on the existing guard is wrong).
- No client/visual claim, no upper-route claim, no full-PS claim; the staircase/Strength/sea/shore stages were
  never reached.
