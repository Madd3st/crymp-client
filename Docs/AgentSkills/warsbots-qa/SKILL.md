---
name: warsbots-qa
description: Independently review WarsBots patches, engine-free caller tests and GitHub CI against exact source revisions. Use for QA acceptance or verification of another agent's claims, not ordinary gameplay implementation.
---

# WarsBots QA

## Locate the actual candidate

Read the current task and applicable AGENTS.md, then current status. Check branch, HEAD and dirty files before acting. Resolve the review base and head to full SHAs; review the actual diff, not an agent's summary.

The current project is Crysis 2007 through CryMP, repository `Madd3st/crymp-client`; `Madd3st/WarsBots` is the historical Crysis Wars stage. Useful entry points:
- `https://github.com/Madd3st/crymp-client/blob/warsbots/feature/weapon-goals/START_HERE.md`
- `https://github.com/Madd3st/crymp-client/blob/warsbots/research/steelmill-triage/Docs/Research/STATUS.md`

Local starting points, to verify rather than assume: QA checkout `C:/Users/Magic8/WarsBotsBuild/crymp-multibot`; candidate worktrees under `E:/WarsBots/Development`. QA Code/ may be stale. Use START_HERE and `git worktree list` to find the right source. Folder names need not match branches. Do not switch or edit another participant's working tree merely to inspect a revision; use `git show`/`git diff`.

## Check claims and tests

- Trace the changed caller and its relevant state/cleanup paths. Separate a code defect from a demonstrated runtime consequence. Verify quoted methods, symbols and constants actually exist at the claimed SHA.
- For extracted caller tests, inspect which real function bodies and headers are compiled, and which boundaries or internal decisions are mocked. A copied implementation is not independent proof of the caller.
- Reproduce semantic failure on the baseline and success on the patch where appropriate. A compiler error is not semantic RED. Some `--expect old` modes exit successfully after reproducing a defect; report this accurately. Also use the new expectation on old source when supported.
- Cover the relevant release/cancel/timeout path, not just the initial happy path. Test success does not establish physical movement or visuals.
- Use targeted existing tests. Keep generated objects/executables/logs outside source trees, normally `E:/WarsBots/QA/<task>-<sha>/`. Respect a user request for no file writes; in that case inspect only, or use existing results and disclose the limitation.
- Engine-free MSVC tests are distinct from game builds. Verify the installed compiler using vswhere with `-products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64`, then enter the x64 developer shell. Discover the available Python runtime; do not install one merely for convenience.

## Verify CI, not just the badge

Prefer connected GitHub tools; `gh` is available as a fallback. With gh specify `-R Madd3st/crymp-client` so fork discovery cannot select upstream. Check the PR's base, full head SHA, draft state and all checks. Distinguish source head from a synthetic PR merge revision.

WarsBots CI and the ordinary PR matrix are separate: experimental client/server and unit tests do not establish x86/x64 Debug/other configured builds. Inspect the current workflows for required coverage. Confirm the new test is executed, its exit code fails the job, and its output is retained. Say “wired into CI” separately from “executed successfully in CI”. Do not rerun already green checks without new changes or a specific unresolved concern.

Do not infer runtime authorization from a green build or this skill. Follow the user's current scope. Review-only tasks do not authorize game/server launch, merge, cleanup or external posting. Runtime evidence must match the exact compatible client/server artifact, source, data and scenario.

## Deliver a bounded verdict

Lead with actionable defects, with verified file/line references; if none, say so. Report separate verdicts for code, test adequacy, CI, document accuracy and runtime/visual acceptance. Give exact SHA, commands/results, remaining unknowns and the smallest next step. Do not turn every wording correction into a new gameplay patch.

Unless requested, report in chat rather than adding another status document. If a handoff update is part of the task, update its canonical source once; preserve original reports as historical evidence and identify superseded conclusions.
