# DS-102: review of the C153 live-log reader fix (harness 5e3bf6db) after the invalid v1 attempt

Scope: game `5ef9ee22` unchanged, full CI 40790759287 green; QA harness `5e3bf6db7368b6c9398d6eafb8f016a02ebcd7f5`
(blob `70903cf62f70167bb6f5862ba60c378d7137121f`, `weapon-goals`). Read-only: archive, QA check script/result, harness
text. No game, launcher, restore or source edit.

**Verdict: corrected harness 5e3bf6db = OK for ONE v2 acceptance on the same 5ef / CI40790759287.** The v1 attempt
stays INVALID_TOOLING_FAILURE (not a code failure, not a PASS).

## 1. The v1 archive is what it claims to be

`QA/build/c153-batch-5ef9ee22`: `session.json` phase `archived_restored`, `client_pid null`, server PID 11252, source
5ef; `runtime-result.json` `quality_pass=false`, `runtime_criteria_pass=false`, failure
`Exception calling "Substring" … startIndex cannot be larger than length of string`, evidence at 24.4 s: 7 current
primary pickups of 8, 19 ATTACKING, 3 death→revive cycles, 0 grants. `memory-restored.json` / `nav-restored.json` report
exact names+hashes restored; the partial demo (295 463 B) and `review-checks.jsonl` (3× MATCH `207bb951`) are archived.
The three review checks show the pin was fresh; no native crash, no client.

Cause (harness `a21`): line 100 `$side=Get-Content $trace -Raw` can yield multiple output fragments while the log grows;
the parser block then computes `$lastCreate` on the coerced string and line 109 `$side.Substring($lastCreate)` runs on the
array (member enumeration) → `startIndex cannot be larger than length of string`, exactly the archived message. Actual
input type was not captured; the team labels this a replay-supported hypothesis, which is honest — the fix below is
type-robust either way.

## 2. Delta in 5e3bf6db (only the harness, 14 lines)

* `Read-LiveLog(Path)`: `@(Get-Content -LiteralPath $Path -Raw -Encoding UTF8)` → `[string]::Concat([object[]]$pieces)`,
  so one immutable scalar is parsed; a multi-chunk read is recorded to `<out>/live-reader-audit.jsonl` (utc, file,
  chunks, characters). Used at both phases with explicit `[string]` casts (`:81`, `:109`); the parser block is unchanged.
* `-v2` scope: `$out`/`$label` gain `-v2`, so the invalid v1 archive and logs are preserved and duplicate-v2 is refused
  independently; `$quality` now requires `harness_scope -eq 'c153-live-reader-v2'`, so the old receipt cannot launch v2.
* Criteria, thresholds, roster/reset logic, preset, guards, snapshot/restore/owned-PID stop, review pin and
  `PrepareOnly` are untouched.

## 3. Independent reproduction (no game, no writes outside %TEMP%)

I replayed the same construction as `QA/build/check-c153-live-reader.ps1` against the archived bots log and the 5e3
harness text (extracted `Read-LiveLog` via AST, extracted the actual parser block, split the log into 2 fragments):

```
RED  (fragments, old parse) : Exception "Substring" … startIndex cannot be larger than length of string
GREEN (Read-LiveLog + parse): NEW_IS_SCALAR=True EXACT=True NEW_IDS=8 PICKUPS=7
controls                    : EMPTY_LEN=0 SINGLE_EXACT=True
```

The GREEN values match the archived partial evidence (8 current roster ids, 7 pickups), and the check script's own
`result.json` (`chunks=2, old=REST, new=GREEN, empty/single=PASS, current_roster=8, archived_pickup_actors=7,
game_started=false`) is consistent. The QA repo worktree is at 5e3 with the harness blob equal to the reviewed commit.

Residual notes: the audit only records that fragments occurred (not an equality re-read), and an overlapping-fragment
reader would duplicate lines rather than fail — with an append-only engine log this is acceptable, and the acceptance
criteria are presence/roster based. `Read-LiveLog` writes to `$out`, which exists before every call in the real harness
(the preflight and run phases); my replay needed `$out` set for the same reason.

## 4. Gate

OK for the single corrected v2 run on `5ef9ee22` / CI40790759287, with a new quality receipt carrying
`harness_scope: 'c153-live-reader-v2'` pinned to this review head. The final report must disclose the physical attempt
count (v1 tooling abort + v2 acceptance) and keep v1 marked invalid, not as a code FAIL. No further tooling change
requested; idle after this push.
