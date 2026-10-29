# DS-103: independent audit of the completed C153 v2 archive

Archive: `C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build/c153-batch-5ef9ee22-v2` (read-only). Source
`5ef9ee22dd2a84896f8ed33cc6c32e6e1261ccb9`, CI 40790759287, pre-run review `38c6ad66`, harness `5e3bf6db`, analyzer
`91db7d8f`. No game, launcher, restore or feature/QA edit.

**Verdict: PASS, with the limits below.** Two physical attempts total: v1 = tooling abort (kept INVALID), v2 = this
acceptance. No archive inconsistency found.

## 1. Runtime archive (recomputed from the raw trace, not from summary.json)

| Check | Recomputed | Evidence |
| --- | --- | --- |
| Run duration / observation | first `0.00` s, last `92.17` s; pass recorded at 90.81 s | `C153-5ef9ee22-v2.bots.log`, `runtime-result.json` |
| Roster after reset | 16 `#N created` lines → 8 slots, 8 current ids (`131002/07/12/18/41`, `196559/65/70`) | log |
| Current real primary pickups | **8/8**; 15 historic actors with pickups, 28 real primary pickups total | log, `summary.json` |
| Combat | 84 ATTACKING at pass (86 by log end), attacks only after the last creation | log |
| Death→revive | **8** same-slot `AI state=DEAD` → `revive result … sameEntity=1 alive=1` cycles | log |
| Grants / errors / conflicts | 0 `TEST GRANT SCAR`, 0 `[Error]/Lua Error/Script Error/RMI…`, 0 `STATE CONFLICT` | log |
| Goal lifecycle | 197 WEAPON GOAL events, **0 mid-pursuit replacements**, **0 repeated pursue anchors**; 8 timeouts, 3 unavailable cancels | log, `weapon-goals.json` |
| Client | `client_pid: null`, no client log, `client_visual=NOT TESTED` | `session.json`, `runtime-result.json` |
| Hash pins | trace sha256 `10d37295…` = summary; demo sha256 `966a69f2…` = archive record | recomputed |
| Quality/fresh review | `quality_pass=true`, 8× `MATCH` at `38c6ad66` (pre-stage, post-preflight, every 15 s, post-run) | `review-checks.jsonl` |
| Receipt | `c153-quality.json`: source/ci_success/ci_source_match/review_ok, `harness_scope=c153-live-reader-v2`, harness sha256 `821f8ff1…` (= reviewed 5e3 file) | QA `build/ci-5ef9ee22` |
| Reader fix exercised | 6 multi-chunk reads recorded (2–3 chunks, 450 KB–1.8 MB) | `live-reader-audit.jsonl` |

## 2. Demo (parsed independently, binary layout from `demo.py`)

`WBDM` v2, header build `5ef9ee22…`, map `Multiplayer/IA/Steelmill`, hz 20, server `C153-5ef9ee22-v2`, 1821 frames,
record stream ends without truncation; 16 bot actors (two generations), 13 kill events between bots, 12 revives, 28
pickups, 448 shots, 166 hits; **all 8 current bots appear as actors and have an armed primary in the inventory records**
(SCAR/FY71/SMG/Shotgun/Hurricane with clip+reserve > 0). This independently satisfies the analyzer's demo checks.

## 3. Analyzer correction `91db7d8f` (reviewed)

One-line regex change: `since=([\d.]+)` → `since=(\d+(?:\.\d+)?)`, excluding the log sentence terminator (`since=0.188.`)
that raised `ValueError` on the real archive. No assertions relaxed. I re-ran the same logic independently: 66 pursue
lines would have been misparsed by the old class, and the corrected parse yields 197 events, 0 replacements, 0 repeated
anchors — identical to the published summary.

## 4. Restore and the live-current limitation (honest scope)

The archive's own restore receipts (`memory-restored.json`, `nav-restored.json`) list exact names+hashes and the
analyzer reported `current_names_and_hashes=PASS` for 4 Memory + 16 Navmesh files. A Codex-produced current-live receipt
exists (`build/c153-current-live-codex.json`, UTC 00:40:49Z, producer explicitly “Codex exec current live host; not
DeepSeek independent live read”); its 4+16 hashes match the v2 `*-before` manifests exactly, and my own read of the
packaged profile (`…\Packages\OpenAI.Codex_…\LocalCache\Local\WarsBots`) also matched 4/16 at audit time.

**Limitation:** my plain `%LOCALAPPDATA%\WarsBots` view during this audit shows an older/isolated set (2 Memory files,
no C153 logs), and per the mailbox note my external shell may see an isolated AppData view. I therefore do **not** claim
my own live-process verification; live-current is supported by the Codex receipt + archive receipts, not by my view.
This is not a game/restore FAIL — no files were touched, and nothing was restored or deleted by me.

## 5. Limits of the acceptance

Headless server-only 90.8 s Steelmill IA run; no client, visual, full-Shore, PS-capture, or A/B claim. Timeout and
stolen/unavailable paths did occur (8 / 3) and the goal lifecycle was observed; urgent-fight/damage and other candidate
branches remain covered by the offline red-green tests, not by this runtime. v1 remains invalid tooling abort
(`build/c153-batch-5ef9ee22`, committed `Docs/Coordination/C153_ATTEMPT_V1_INVALID.json`); physical attempt count = 2.
