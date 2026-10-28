# WarsBots: current project status — 2026-10-29

C153 ACTIVE: owner starts goals/weapons batch with one shared acceptance after gates. DS097 integrated as intermediate c00cae46 (CI40785778523 GREEN), but DS0995817a303 BLOCK: candidate cache can freeze; harness historic EntityId count fails after native game reset. **No runtime on c00.** Corrected source LOCAL41a7208755017d425a465b42eebd308105d18947, unpublished pending DS100 pre-CI review; six units/actual Brain syntax/actual-method caller and stale-inventory red-green checks PASS. Harness57a41e6a uses current8 roster; archive control PASS. No new game/build locally. DS0982d8fb24a offline lower-spawn report independently reproduced: lower12 =11off-mesh +1false projection12.34m. Owner Shore launch correctly refused lower spawn; packet is unreliable for arbitrary spawn, not accepted playable. Details in latest Codex outbox/handoff. Earlier accepted source remains b8 for its limited C149 route.

This is the current technical entry point. Earlier notes are preserved in [the historical snapshot](STATUS_HISTORY_2026-10-28.md); they are not the live task queue. [Documentation index](../README.md) · [Wiki](https://github.com/Madd3st/crymp-client/wiki) · [Operational handoff](Coordination/MAILBOX/CLAUDE_STATE.md).

## Source of truth

| Area | Branch / reference |
| --- | --- |
| Game code | `warsbots/feature/claude-sprint`; accepted scenario source `b8b444d6c858f27a1fdde0d93b25a318cafc6937` |
| Build | Green CI `40709459511`; code review DS093 `b3182ff9` |
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
