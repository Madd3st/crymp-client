# AI experiments and training

## Current stage

Rule-based FSM + native execution + persistent per-node map experience. This is not a
trained combat policy. No new ML dependency or training work runs in the game thread.
`BotNav` currently records stuck/kills/deaths and adjusts route costs / roaming goals.
Memory validation, decay, topology fingerprint and an explicit reset command remain work.
Do not treat existing counters as contextual-bandit training data.

## Native 1v1 data, 2026-10-24

[Measured duel/aim results](Experiments/duel-aim-2026-10-24.md) distinguish actual
remote=0 shot cadence, first observed targeted fire vs first impact, initial Armor
energy, legitimate between-impact HP changes and revive/replica context. Six fixed
SOCOM runs and a mixed regression are evaluation evidence, not exploratory bandit
data. No action propensities or trained model. Do not combine later random respawns
with first-duel paired comparisons or leak authoritative outcome fields into features.

## P0 diagnostic extension, 2026-10-24

[P0 fire-control evidence](Experiments/fire-control-2026-10-24.md): existing exporter
now has additive bounded TTK diagnostics with actual shots, effective HP, separate
Armor energy and contributor accounting. These authoritative outcome records are not
model features; hidden state/after-impact knowledge must not leak into future actions.
Old logs remain unavailable for full TTK. Action propensity, dataset split and asynchronous
logging still need work before training; no bandit model implemented.

## Reproducible record

Use the SHA-guarded `Tools/Test-WarsBotsTactics.ps1` only with isolated resources.
Do not use the old multibot build runner for weapon-goals. Keep source SHA, server/client
SHA256 from `build/multibot-on/claude-build.json`, durations, bot counts/profiles, log hashes
and initial map memory snapshot. Never overwrite a user's running EXE.

`python Tools/wb_ai_metrics.py <run.bots.log> [...] --output results.json` exports state
sample shares, blocked movement, native pickups, goal cancellation, jumps, combat/lifecycle
and performance counters. State sample share is not exact wall-clock time; jump takeoff
and landing totals are not a per-bot success rate. Truncated logs can have unmatched events.
The exporter counts crash markers but cannot establish absence of every error or leak.

First baseline: existing j13 at `df3ceb8e`, 6/240 s and 15/300 s. Compare the weapon-goal
experiment with the same counts/durations. Historical runs share evolving map memory and
nondeterministic engine physics: descriptive comparison only, not an isolated causal claim.
No existing successful baseline is repeated merely to make a nicer report.

## Current bounded dataset and evaluation (2026-10-23)

Implemented schema-2 bounded episode export and repeated fixed-memory native QA:
[tactical experiment](Experiments/tactics-2026-10-23.md). Exporter
`Tools/wb_tactics_report.py` defaults to 10,000 rows / 8 MiB, masks missing baseline
context and visible-vs-last-seen distance, and records actual HP loss without overkill.
Armor energy damage is not included. No online policy adaptation or training yet.
26 completed runs do not establish tier ranking; rejected broad navigation recovery
is OFF in the final build. Four balanced slot rotations are the next comparison.

## Next training increment (remaining)


Before selecting a bandit algorithm, extend the existing bounded per-bot combat episodes with
life/episode ID, only perceived target state, distance band, weapon, action/profile,
suit mode and energy, native damage outcomes, disengage reason and stuck duration.
Current global jump lines lack actor IDs; do not invent per-bot correlations from them.
Keep an immutable baseline policy and separate training/evaluation memory. Run multiple
alternating baseline/adaptive matches with fixed recorded profiles and memory snapshots.
Measure pickup completion, route completion, damage ratio, deaths, stuck duration and
frame cost as well as kills. Reject an adaptation that only increases kills by exploiting
stationary opponents or causes navigation/performance regressions.

## Weapon-goal experiment, 2026-10-23

All new executables: MSVC x64 Release ON, BUILD PASS. Existing j13 baseline was reused.
[Machine-readable evidence](Experiments/weapon-goals-2026-10-23.json) includes source SHA,
client/server hashes and log hashes. Planned durations: 6 bots/240 s, 15 bots/300 s.

| Variant | Bots | Weapon state samples | Blocked movement | Pickups | Kills | Strength takeoff/landing events |
| --- | --- | --- | --- | --- | --- | --- |
| j13 `df3ceb8e` | 6 | 40.34% | 2.72% | 17 | 16 | 15 / 15 |
| strict `20bf32bf` | 6 | 17.51% | 11.55% | 7 | 11 | 18 / 17 |
| revised `eea973f0` | 6 | 13.73% | 1.64% | 17 | 12 | 11 / 11 |
| j13 `df3ceb8e` | 15 | 30.87% | 2.80% | 38 | 87 | 22 / 22 |
| strict `20bf32bf` | 15 | 15.79% | 1.01% | 23 | 121 | 37 / 37 |
| revised `eea973f0` | 15 | 28.05% | 0.89% | 52 | 121 | 38 / 35 |

Strict graph-route rejection was removed: six-bot blocked share rose and pickup coverage
fell in both counts. Revised code retains native item-lifetime cancellation and fixes
failed A* retry caching while keeping local steering fallback. Revised 15-bot run actually
cancelled 13 unavailable item goals. No crash markers; all QA processes exited by their
own harness after the configured no-client run. This is not leak or visual verification.

Worst reported AI frame: baseline 6/15 = 0.88/50.62 ms, strict = 3.07/2.36 ms,
revised = 2.79/1.32 ms. Max reported window-average including startup, revised 6/15 =
0.39/1.14 ms; these are NOT whole-run averages. Six-bot kill count fell, so no general
combat-quality improvement is claimed. Memory evolution and nondeterministic simulation
confound causal attribution; repeat controlled batches before tuning tactical policies.

Commands actually used: `Tools/Build-WarsBots.ps1 -Experimental -BuildDirectory
build/multibot-on -Jobs 6`, then installed `ClaudeQA/Claude-QA-Bots.ps1` with
`-Bots 6 -Seconds 240` / `-Bots 15 -Seconds 300`, `-Expect` matching each source SHA.
A Ninja dry run was captured separately; it stopped at glob/CMake regeneration and does
not establish per-object dependency reasons. OFF was not rebuilt for this bot-only delta;
the option still defaults OFF and these sources remain inside its CMake conditional.
