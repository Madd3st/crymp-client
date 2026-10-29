# DS-108: independent review of local `0b02dc27` + real ladder caller controls

Reviewed exact local source `0b02dc27bf90a4ed554450b16a5b9c053d1cf55d` in
`C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build/ds076-code` against DS107r2 baseline
`01e15a93b35e8ef88e34c5d06e4dab4ac65442f5`, read via `git -C <repo> show`, not remote `5ef` and not my stale
branch. No game, build/install, launcher, restore, feature or QA write.

**Verdict:** no blocker found in the reviewed caller scope. The new revision fixes the r2 caller defects and the
real-caller regression is RED on `01e15a93` / GREEN on `0b02dc27`. Residual limits are listed at the end; the
dedicated-server native graph/exit is still not runtime-proven.

## 1. Code review of `0b02dc27`

| Control | New revision | Old `01e15a93` |
| --- | --- | --- |
| Grab probe/lock | Probes `IsLadderUsable` only in `Grab`; `SetNative` validates the native UP frame; `m_ladderNativeLocked` is set only after native `isOnLadder`; no ray mutation while climbing. | Probes in `Approach` and locks before Grab, so the later `Grab` sees no fresh grab; never grabs. |
| Cancel/reset | `CancelLadder` clears `m_ladderCancelPending` before the early return; one-shot cleanup. Fail bans once; stop/death/OFF pass `ban=false` and do not globally ban a valid link. | Early return leaves pending true; repeat cleanup/path clear; stop passes `ban=true`. |
| Native exit | `PlayerMovement` rejects null, empty and non-`0`/`1` `OnLadder` output for the owned bot; valid `0` still requires `NativeLadderExitPhysicalReady` (long shaft + lateral egress); only then `ReachedEnd`. Human/client path unchanged. | Null-only guard; valid/empty `0` can leave before physical egress. |
| Climb movement | `ExecuteLadder` requires `nativeAttached && isOnLadder && !isExitingLadder`; uses `GetStanceMaxSpeed(STANCE_STAND)` full forward. Approach/settle use the ordinary safe executor. | Accepts attached flag without checking native state and uses 1.0 m/s (normalizes to a too-slow climb). |
| Settle/Done | Done requires `!onLadder && !exiting`, grounded, `fabs(dz)<=1.0`, and `Dist2D<=0.8`; reattach/exiting in Settle is `Lost`, not Done. | Accepts 2.5 m and one-sided z; can finish before the real walk-off. |
| Generic links | `ApplyLinkExecutor` accepts only Drop/Jump/Strength; Ladder/Lift are not interpreted as Drop. | Ladder/Lift fall through to Drop. |
| Stop order | `Stop` sets `m_running=false` before `CancelLadder`; cleanup cannot re-enter AI. | Cancels while `m_running=true`. |
| Net-zero | `wb_bot_ladder` default 0 added; diagnostic `wb_nav_knee_fix` binding/field removed; `wb_nav_audit [0|1|2|stop]` default 0 with no-actor guard. Registry count 56 -> 56. | Old `wb_nav_knee_fix` path; only default/stop variants. |

Scoped ownership/off-human checks are correct in the reviewed code: `WarsBotLadderServer` requires
`gEnv->bServer && IsWarsBotActor && BotConfig::ladder==1 && BotManager::Owns`; human/client, flag-OFF and non-owned
actors keep the old behavior. No bot-side `SetPos`/`SetWorldTM`; only the stock native `ProcessMovementOnLadder`
calls `eLAT_ExitTop`.

## 2. Real-caller regression deliverable

`ds108_ladder_caller.py` extracts actual source, does not copy decision logic, and compiles it against a mock world
(no SDK link). It accepts `--repo`, `--old`, `--new`, `--out`; output is private/temp only.

Extracted/executed:
- actual `BotBrain::CancelLadder` body (renamed only to a mock method);
- actual `BotBrain::UpdateLadder` grab-probe block and `if (o.grab)` block;
- actual `BotLocomotion::ExecuteLadder` body;
- actual `PlayerMovement` `WarsBotLadderServer` helper and `if (m_stats.isExitingLadder)` block;
- actual `BotBrain::ApplyLinkExecutor` start prefix (kind filter + `m_link.Start`);
- actual `BotBrain::Stop` prefix through `m_combat.Reset()`;
- actual `BotManager::NavAuditCommand` body;
- the real `BotLadder.h` policy and `NativeLadderExitPhysicalReady` for the new revision.

Controls (9): grab-lock/once/no mid-climb ray mutation; pending cleanup; fail bans vs stop does not; native exit
null/empty/0-before-egress/physical/human; ExecuteLadder attach/speed; settle 1.5 m vs 0.8 m and reattach; generic
link kinds; Stop order; nav audit default/0/1/2/stop/invalid/no-actor.

Measured:
- `01e15a93`: 11 expected failures (RED).
- `0b02dc27`: `DS108 real caller controls PASS` (exit 0).

Run under a VS shell (needs `cl`) with:
`python ds108_ladder_caller.py --repo <QA/build/ds076-code> --old 01e15a93... --new 0b02dc27... --out <temp>`.

## 3. Data V4 review

`QA/build/c154-route-data-v4` / `Docs/Coordination/C154_SHORE_DATA_CANDIDATE.json`:

- navmesh `Shore.navmesh` sha256 `b60178d7090fd8403b4ba0f4407d6a722c0057afd559a0024148ef0645930609`;
- links `e7204de31a25482fd0d6d75feb55cb7e85255dc1e72f8a9bbf4f38f3191b7c2b`;
- spots `d95e1e11fa06194e44e1b1d3887fb00334d7f0ccc9528c9a1b439d7ba78b548b`;
- manifest `c7e96b3d0458e93cab370989ca476126fe191ba54d5edff38f14db5fffd44747`.
- 124/124 spawns project; mask 14 lower->dry shore 0/12, mask 46 12/12; lower545 40 corners/2 links; reverse UP
  no path; upper->dry shore PASS. Metadata correctly says `wb_ps_capture` (no `wb_ps_swim`).
- Candidate is **not installed**, no game run, no full-Shore/client/physical acceptance.

## 4. Limits / blockers

- **Player movement-mode blocker (new, current revision):** `Player.cpp:1575-1580` in `PrePhysicsUpdate` does, for
  every `m_warsBotPose`, `params.flags &= ~eACF_PerAnimGraph; params.flags |= eACF_AlwaysPhysics;` inside the
  `ShouldUsePhysicsMovement()` multiplayer/third-person branch. The native ladder exit depends on the animation
  graph/`OnLadder` completion and root motion (`ProcessMovementOnLadder`); forcing `AlwaysPhysics` may suppress that
  root motion, so the physical exit can remain `isExitingLadder`/never reach the observed lateral egress. This is a
  real **blocker/unknown** for the native ladder route; it must be resolved by Codex in source integration (e.g. a
  scoped, gated movement-mode exception for the owned ladder actor) and proven in a real adapter, not by our mock.
  `AutoDisablePhysics` `eADPM_Never` at `Player.cpp:838` is a different setting and is not the cause; do not confuse
  the two.
- Engine-free mock only; the real engine adapter is not linked here. Codex's eight units and ON/OFF syntax are the
  compile gate; the dedicated-server native animation/`OnLadder` completion remains the real unknown.
- The fixture allows the old `isRagdoll` spelling only so the old semantic RED can execute; the real SDK typo is
  separately covered by Codex `/Zs` on `01e15a93`.
- `UpdateLadder` is exercised as its real probe/grab blocks, not as one whole method; the full adapter still needs
  Codex integration/CI. `ApplyLadderExecutor` start/ownership is reviewed but not separately extracted.
- No runtime pin; no game/install/launch/restore. Codex must wire `Tests/test_bot_ladder_caller.py` with a safe main
  guard and the CI unit list after review.
