# WarsBots: current project status — 2026-10-29

C154 source **`3cfd859676d83bdecc1b091d3b9202323b039135`** pushed after independent DS109 `e8e63bd8` OK. Native ladder executor, scoped graph-driven top exit, cleanup and caller controls are integrated; full CI **40855444355 GREEN**, exact paired artifact downloaded and hashed. No local game build. Shared lower545→ladder→stairs→Strength→water→dry-shore harness `fe42d8ef` has DS110 pre-game OK `0c4cd6c5`; fresh post-CI final approval is pending. No new game or data installation yet; physical lower-route acceptance remains open. [Source and checks](https://github.com/Madd3st/crymp-client/blob/warsbots/feature/weapon-goals/Docs/Coordination/C154_SHORE_INTEGRATION.md).

Frozen C154 DATA V4: graph12/12 lower→shore with UP-ladder, 0/12 when disabled; navmesh `b60178d7…`, manifest `c7e96b3d…`. Vertical cell0.05 fixes two observed stair gaps; bounded dry-room source triangles retained without reducing the agent capsule. All 12 lower projections are on the right floor. Native execution is not proved by graph connectivity. Gate `wb_bot_ladder` defaults0; net-zero56→56 retires diagnostic `wb_nav_knee_fix`, preserving variants as `wb_nav_audit [0|1|2|stop]`.


C154 ACTIVE: owner historical audit revalidated jointly (DS105 corrected0a9aeaf7). R4 ammo-search episode deadline confirmed remaining; R9 jump-direction contract gap confirmed, physical consequence unproven. R1/R2/R5/R6 source-fixed; R2/R5 runtime parity not tested. R3 ordinary round chain covered via framework reset; no demonstrated direct bypass, first DS105 false positive corrected. Plan updated with these distinctions. Next active work Shore lower dry-floor + real transition; offline545 reaches observed ladderbase on existing low90 mesh but not top/sea. No new runtime pin/game. Details C154_AUDIT_REVALIDATION.md.

C153 DONE: goals/weapons shared headless acceptance 5ef9ee22dd2a84896f8ed33cc6c32e6e1261ccb9 / FULLGREEN CI40790759287 / independent DS103 1e7d868299615f7c3fa6496d4ef97416eaab7970. Two physical attempts: V1 INVALID_TOOLING_FAILURE preserved and restored, V2 PASS90.81s/current8primary/84attacks/8matcheddeathrevive/0grants; demo1821frames/13kills/fulltail. Activegoalreplacements0/repeatedanchors0 in197events, current Memory4/Nav16 exactrestore. No client/AB/fullShore claim; no more game. C150 showcase unchanged b8. DS098 offline lower12=11off+1falseprojection12.34m; native545 failure explained, lower route NOT fixed, no resample/teleport/loosergate. Result QA Docs/Coordination/C153_GOAL_WEAPON_RESULT.md/json; next planned batch routes/topology, not automatic runtime.

This is the current technical entry point. Earlier notes are preserved in [the historical snapshot](STATUS_HISTORY_2026-10-28.md); they are not the live task queue. [Documentation index](../README.md) · [Wiki](https://github.com/Madd3st/crymp-client/wiki) · [Operational handoff](Coordination/MAILBOX/CLAUDE_STATE.md).

## Source of truth

| Area | Branch / reference |
| --- | --- |
| Game code | `warsbots/feature/claude-sprint`; C153 IA source `5ef9ee22dd2a84896f8ed33cc6c32e6e1261ccb9`; C149 Shore upper-route source `b8b444d6c858f27a1fdde0d93b25a318cafc6937` |
| Build | C153 green CI `40790759287`, DS101/102 code/harness review, DS103 archive audit `1e7d8682`; C149 uses CI `40709459511` |
| Coordination, research, wiki source | `warsbots/research/steelmill-triage` |
| QA tools, X reports and receipts | `warsbots/feature/weapon-goals`; its game code is stale |
| Independent review and proposed patches | `warsbots/research/deepseek` |

Codex is interim coordinator/integrator and QA under D031/D035. DeepSeek supplies analysis, patches and independent review. Claude is out of limits; resume via handoff. No game builds locally on the owner's PC; download the exact green CI pair. No gameplay while the owner plays, no purposeless isolated runs, no default promotion without owner decision.

## Latest completed work

| Task | Result and evidence | Limits |
| --- | --- | --- |
| C149 upper submarine → water → shore | PASS b8b444d6; independent DS094 audit `11425704`; QA report `83042341` | One native US bot, E223 data, 84.78 s, HP100, zero NAVfallback/errors, dry endpoint 31.235 m from target654; capture/full map not accepted |
| C150 owner-PC showcase | READY tooling `b6930859`, report `75f9a670`, independent frozen-package review DS095 `46f44f26` | Steelmill IA8normal and Shore oneUS route; PrepareOnly/recovery/hash/ZIP checks PASS, no fresh gameplay during packaging |

C150 local root: `C:/Users/Magic8/WarsBotsBuild/WarsBots-Showcase-b8b444d6`. Two folders, ZIPs and desktop shortcuts. Keep launcher open, exit Crysis normally and wait for `Archived and restored` before switching maps. [Human instructions](../Wiki/Running-and-Playing.md).

QA evidence on weapon-goals:
- [C149 route result](https://github.com/Madd3st/crymp-client/blob/warsbots/feature/weapon-goals/Docs/Coordination/C149_UPPER_SHORE_RESULT.md)
- [C150 package instructions](https://github.com/Madd3st/crymp-client/blob/warsbots/feature/weapon-goals/Docs/Coordination/C150_SHOWCASE.md)
- [C150 integrity receipt](https://github.com/Madd3st/crymp-client/blob/warsbots/feature/weapon-goals/Docs/Coordination/C150_SHOWCASE_READY.json)

## What remains unaccepted

Full Shore/Power Struggle match, lower 12 submarine spawns, historical 41 water coverage holes, isolated objectives, end-to-end capture/economy/vehicle routes and current visual client quality. A module, offline path or upper-exit PASS does not establish these outcomes. Earlier C148 physical evidence had a stale-review procedural failure; the valid accepted route is C149 on new b8 with fresh gates.

## Current documentation task

C152 planning deliverable: [CS-bot maturity plan](CS_BOT_MATURITY_PLAN.md), independently informed by DeepSeek DS096 `97cb2c78` / `e4051c5f`. Six batches with source locations, cheap red/green checks and proposed acceptance criteria; no games/builds/code changes. Integrator rejects purposeless standalone baseline, already-met 95% water threshold and premature grenade scope. First implementation candidate is weapon-goal lifecycle; Shore data audit can proceed offline in parallel. This plan does not automatically dispatch gameplay or establish parity with CS.

C151 DONE: source `cebeb1b055cf2db772791ff47c9a2f879cae695c`, published wiki `3370918d4171620481ce021c5673f02eae675bdc`, QA report `4eea479a` / X250. Steelmill launch/ZIP links delivered first. Wiki now separates newcomer onboarding, engineering navigation, launch instructions, evidence and troubleshooting. All 16 published page blobs match committed source; 73 local and 27 Git source links checked PASS. Documentation-only work, no game build or runtime. Pre-existing duplicate mailbox IDs DS050/DS052 detected and left unchanged.

For live assignments use [TO_CODEX](Coordination/MAILBOX/TO_CODEX.md), [TO_DEEPSEEK](Coordination/MAILBOX/TO_DEEPSEEK.md), [decisions](Coordination/MAILBOX/DECISIONS.md) and [protocol](Coordination/MAILBOX/PROTOCOL.md). Historical STOP items apply to their task only. Large artifacts stay outside Git, with hashes and archive paths in reports.
