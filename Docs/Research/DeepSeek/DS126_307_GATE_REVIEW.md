# DS126 — final pre-runtime gate, exact `307037bc`, CI `40919291234`

**DS126 307037bc CI40919291234 code/data/harness: OK**

Code/metadata gate is **OK**. Physical outcome remains **UNTESTED**: no 307 game/server/client/probe/QA/install
action was taken by me, and none is authorised while the owner plays the classic package.

## 1. Exact source / CI / pair

- Game source: `warsbots/feature/claude-sprint` head `307037bc08050d39f26310dc6cc7071dddddf4a6` (remote
  `git ls-remote` matches), parent `abac72a9`; local `QA/build/ds076-code` clean at the same SHA; cumulative diff
  is the DS125-reviewed 10 files (+650/−15).
- CI run `40919291234` (`WarsBots CI`, workflow `.github/workflows/warsbots-ci.yml`):
  - `event=push`, `head_branch=warsbots/feature/claude-sprint`, `head_sha=307037bc…`, attempt 1,
    `conclusion=success`, created `17:13:45Z`, completed `17:24:02Z`.
  - Job `Unit tests (MSVC)` `123607844493` success; job `Client + server (MSVC, experimental spawner)`
    `123607844840` success. Both step lists end `success`; the new exit-retry caller runs in the unit step.
- Artifact `warsbots-307037bc…` id `12917623035`, run `40919291234`, head `307037bc…`, 14,185,253 bytes,
  GitHub digest `sha256:2003a25a57afa20a89175093daa1f62e3f5c9124ef134cfa609d15c5bb49553a`, not expired.
- Downloaded pair (`build/ci-307037bc`) recomputed by me:
  - `CryMP-Client64.exe` = `d4a08fbd3216088a21125127a4cc87d4a4ab4d0f7347c98ce259abe2a35d7fe8`
  - `CryMP-Server64.exe` = `bdd084af320a7bd5892f521a895742c1f0c2b07421ac468e1dff21d110303226`
  - `sha256.txt` lists exactly these values; `ci-provenance.json` `client_sha256`/`server_sha256` match; its
    `run` object matches the live API job ids/timestamps; `ci=40919291234`, `source=307…`, `ci_source_match=true`,
    `ci_success=true`, `artifact_id=12917623035`.

## 2. Quality draft / manifest / data / harness / analyzer

`build/ci-307037bc/c154-quality-draft.json`:

- `source=307037bc…`, `ci=40919291234`, `ci_success=true`, `ci_source_match=true`, `artifact_id=12917623035`,
  pair hashes as above, `task=C154`, `harness_scope=c154-lower-route-v2`.
- `harness=e973b90518daa58e83d4811952c0c8bd83f8217e`, `harness_sha256=87c3f5ee79c5507e1952367edbf5b53d3
  ee3c211eb8de5614b67fef2ae24fbf4` — verified against `Tools/Invoke-WarsBotsC154Route.ps1` bytes.
- `analyzer_sha256=2a642aebcfff2d10e119746b42f8e50f48a004d732f7aee106602b3e6fff88f7` — verified against
  `Tools/Diagnostics/Analyze-C154Route.py`; matches the actual file, not only the prose.
- V5 data hashes match the frozen files exactly (recomputed): links `0230a0c3…`, manifest `a95a0503…`, navmesh
  `b60178d7…`, spots `d95e1e11…`.
- `review=d19ca975c6bcb4e06276ae14afa1840bc1a49850` (my DS125 pre-CI review), `review_ok=false` — correct
  pre-approval draft; after this OK Codex pins the final review head and sets `review_ok=true`.
- `manifest_guard_ok=true`, `manifest_guard_receipt_sha256=9a958ea78289d0c7ccd38aa9e1f9ee7931eb5d25f0feae1e
  3a208041657660d8` — matches my SHA256 of `build/c154-307-manifest/v5/result.json`.
- `client_visual=NOT TESTED`, `prior_physical_attempts=4`, `prior_controlled_attempts=4`,
  `prior_owner_previews=1` — honest history.

`build/c154-307-manifest/v5/result.json`: `source=307…`, `actual_source_sha256=21df800a…` =
SHA256 of `BotNavMesh.cpp` at 307 (verified), data hashes as above, `actual_exit=0`,
`tamper_and_crlf_controls=PASS`, `game_linked=false`. The manifest reader/normalizer is the actual extracted
`BotNavMesh.cpp` block, not a copy.

Private control `build/c154-ds124-check/accepted-consumed-control`: `run.log` = `DS124 caller GREEN PASS`;
the extra scenario (control.cpp:390-397) sends one accepted retry, consumes it to default/`RelaxedIdleNW`, calls
the exit block and the retry again, and asserts `signalAttempts` stays 1 and no egress is fabricated.

## 3. Code review basis

The pushed source is the same commit reviewed in DS125 (no post-review delta): one bounded recovery signal after
`PerAnimGraph` handoff/default signal/not playing, completion checked first with actual output + physical egress,
attempt consumed before `SetInput` even if rejected, `IsDefaultInputValue`/`ClearInput` APIs, orphan latch reset
before the Restore early return, output NUL vs null logging, PseudoSpeed id/name validation, cleanup only for the
accepted recovery signal on the same owned graph. Feature-OFF calls guarded; no new cvar/default/wire/identity and
no position/velocity/collider/completion-guard change. Physical exit still unproven.

## 4. Wake configuration (read-only)

- Scheduled task `WarsBots Mailbox Wake` is `Ready`, principal `S-1-5-21-722302237-3654468903-1605998236-1006`
  = current `magic8-pc\magic8`, trigger every 10 min, action runs
  `C:\Users\Magic8\AppData\Local\WarsBots\Mailbox\bin\wake_agents.ps1 -ConfigFile …\wake-config.json`.
  `LastTaskResult=0`, last run `20:17:50`, next `20:27:49` — normal, not bypassed.
- Active config pins `DeepSeekSession=ses_e7785dde5ffe9MiW2PwJiototK`; `session_state.py --session` finds it
  (idle now because this session is active, `pending=0`). `DeepSeekRepo` is this research worktree.
- A stale backup config exists at `build/c141-wake-before/wake-config.json` with an old session
  `ses_e8cd3a331ffe4diNAe07HImXlu`; it is not referenced by the task. The only config "split" is this backup,
  not the active pin. No config/data mutation performed.

## 5. Gate statement / next

- This is a **code/data/harness metadata** OK: exact source, green full CI, exact pair receipt, provenance,
  quality draft, manifest, harness/analyzer/data hashes, and the accepted-consumed control all check out.
- **Physical outcome: UNTESTED.** No 307 game/server/client/probe/QA/install/live action; the owner is playing
  the classic Steelmill package and all games remain on hold.
- After Codex pins the final review head in `c154-quality-draft.json` and sets `review_ok=true`, the one
  authorised lower-545 → ladder → upper floor → water → dry shore run may be scheduled only when the owner and
  machine are free. No default/PS/client promotion; no claim the exit is fixed until that run.
