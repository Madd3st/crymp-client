# Steelmill displacement recovery and graph diagnostics, 2026-10-24

Research source: `origin/warsbots/research/steelmill-triage@442296db`, FINDINGS and
NAV_EXPORT_CONTRACT, read without merging. [Evidence](steelmill-recovery-2026-10-24.json).

## Completed continuation: final source d4b0854c

15-bot continuation `build/trap-nav-soak-d4b0854c-r3/`: **PASS**,660s requested,
659.96s observed, recovery1/full-hotspot1, two consistent exports. No engine error/crash
markers. Median summed bot Update0.243ms/frame, worst single bot Update10.7ms;
217 native deaths include2 recovery suicides,214 revives before the run ended.
Recovery stages1/2/3/4:9/4/4/2. F1 at spawn195 explicitly reached nativeSelfKill at
45.02s with blocked0/grounded0. Offline Triage: pinned2.0%, traps>=60s0,117 stalls,
1970 fallbacks. This is one completed long run, not proof that navigation is fixed.
Memory restored to01F4C7F4B894AE2AC492C381487FAC5B539824F83EC76C6F902F428A165668AE.
Source/hash frozen; no successful checks rerun without new input.

Owner live match ended at883.55s. Server147120 stopped with explicit permission;
client was not forcibly closed. Final logs and recovery-events.json archived in its
`final/` folder. Three native recovery suicides, each followed by same-actor respawn
~5s later. F1 and F2 both observed. Owner impressions pending; no visual recovery PASS.

The earlier interrupted runs below remain failed/incomplete historical evidence.
Both runtime feature defaults remain0. No installed EXE or ordinary-player rules changed.
Next: bounded native physics diagnostic of F1; human spawn comparison still pending.
Native speed lead confirmed read-only: Player.lua STANCE_STAND normal1.75/max4.5,
GetStanceMaxSpeed applies speedMultiplier/zoom, CalculatePseudoSpeed returns1/2/3.
Do not treat4.5 as a universal stance/zoom-independent speed or sparse LOS samples as
precise player telemetry. No speed tuning in this continuation.

Changes, separate commits:
- b3ce761f: opt-in `wb_trap_recovery 1`, default0. Reuses BotProgress with0.5m radius;
  independent of blocked and FSM transitions. Continuous move request without displacement
  escalates at8s (repath),16s (side step),24s (reverse leg/normal jump, gated by native
  JumpSpace),45s (native IA KillPlayer -> ProcessDeath/OnPlayerKilled/GameRules).
  No direct HP/position edit. Same-actor5s respawn, native suicide scoring/broadcasts.
  Intentional stops, death, disabled movement and actual displacement reset the watchdog.
  Does not diagnose collision root cause or detect every larger movement loop.
- b53b3f87: actor IDs in suit/jump/action/refusal logs; NAV fallback reason/from/to/expanded;
  stuck-memory report with actor/node/position. Route policy and safety checks unchanged.
- e4eb64ee: `wb_nav_export [path.json]`, local IA server/Ready only, wbnav/1 contract.
  No physics rays or memory mutation. UTF-8 JSON, temporary file + atomic rename.
  `wb_nav_export after 15` schedules a gated request via native Script timer.
- d4b0854c: full weighted hotspot scan, `wb_hotspot_all_nodes 1`, default0. Legacy
  stride2 retained for comparisons. Shared pure selector test proves odd-only hotspot
  becomes selectable. O2 microbenchmark7633 nodes/5000 queries:1.970 ->3.669us/query;
  synthetic memory, not evidence of total match CPU or stronger AI.
- QA harness records flags/exports, supports660s and rejects long runs with<600s
  observed game time; validates engine error markers. Exports requested at15/25s.

BUILD PASS: MSVC x64 Release experimental client+server, final source d4b0854c.
BotTrapWatchdogTests PASS (thresholds, once-only terminal action, pauses, displacement,
vertical movement, clock rollback); BotHotspotTests PASS. No default-OFF rebuild.
Frozen matched EXEs: `build/play-b3ce761f/`, `build/play-d4b0854c/` (manifests/hashes).

## QA actually completed

6-bot b3ce761f soak:660s requested,659.24s observed, PASS. No engine crash/script errors.
Stages1/2/3/4 counts6/2/2/1. Terminal trap at(1305.98,1353.65,144.38), blocked1:
45.11s -> nativeSelfKill1 health0 at t192.15; same actor131012 revived at t197.15.
39 native death events include1 suicide (do not rank these as39 enemy kills).
Median summed bot Update0.0905ms/frame, worst single Update9.55ms; not whole-server CPU.
Claude's unmodified offline triage: pinned0%, traps>=60s0, stalls108, fallbacks1001.
Different population/duration from old owner logs: no causal percentage-improvement claim.
No spawn195 revival in this run, so physical F1 reproduction remains open.

15-bot d4b0854c pilot: stopped after expected QA fixture failure (export requested at3s
while graph still building; native refusal correct). Archived as failed, not PASS.
Corrected15-bot run: interrupted at owner request (~254s) to permit a live session.
It is NOT a completed10-minute soak. Two exports passed Claude's graph validator with
matching memory fingerprint:7633 nodes/41618 edges/36 spawns,684128 bytes,9.26/9.06ms.
Repeated nodes/edges identical; destination replacement tested. Outputs and validator
console under `build/trap-nav-soak-d4b0854c-r2/`. Validator copied read-only into ignored
`build/steelmill-triage-validator/`; no branch merge. Actual native fallback diagnostics
contain reason=no_route/from/to/expanded. Longer performance/regression remains pending.

Executed (existing bundled Python and MSVC environment):
```powershell
./Tools/Build-WarsBots.ps1 -Experimental -BuildDirectory build/multibot-on -Jobs 6
./Tools/Test-WarsBotsTactics.ps1 -GameDirectory 'E:\Steam\steamapps\common\Crysis' -BaselineDirectory build/play-a6ee9e9e -CandidateDirectory build/play-b3ce761f -ExpectedBaseline a6ee9e9e -ExpectedCandidate b3ce761f -CandidateOnly -CandidateTrapRecovery 1 -Seconds 660 -Repeats 1 -Counts 6 -OutputDirectory build/trap-soak-b3ce761f
./Tools/Test-WarsBotsTactics.ps1 -GameDirectory 'E:\Steam\steamapps\common\Crysis' -BaselineDirectory build/play-b3ce761f -CandidateDirectory build/play-d4b0854c -ExpectedBaseline b3ce761f -ExpectedCandidate d4b0854c -CandidateOnly -CandidateTrapRecovery 1 -CandidateHotspotAllNodes 1 -NavExports -Seconds 660 -Repeats 1 -Counts 15 -OutputDirectory build/trap-nav-soak-d4b0854c-r2
python -B build/steelmill-triage-validator/steelmill_triage.py graph build/trap-nav-soak-d4b0854c-r2/candidate-r1-n15-nav-1.json --mem build/trap-nav-soak-d4b0854c-r2/initial-memory.mem
```
Last long command was interrupted for requested live access; do not present it as PASS.
Use a NEW output folder to resume after owner permits stopping the live server.

## Historical live session / previous next work

Owner requested access now: d4b0854c,6 Normal, port64150, recovery1 (prior11min soak),
full-hotspot0 (no completed10min soak yet). Metadata:
`build/owner-playtests/recovery6-20261024-045803/session.json`.
Do not interrupt owner play. Use native spectator1, wb_watch1, F1.

Spawn195 geometry vs bot-only physics still needs a controlled human/native spawn
comparison and bounded actor/physics state tracing. No spawn blacklist was introduced.
Random travel Strength jumps remain; recovery mitigates traps, not their root cause.
Ladder refusal was an owner suspicion, not a verified obstacle-type reproduction.
Claude's later speed findings (fixed2.5m/s and pseudoSpeed0.5, sparse human LOS samples)
are a new investigation lead. No speed/sprint cap or human movement changes made here.
Next: finish final15-bot>=10min soak, isolate F1, then compare native player movement
parameters before adjusting bot gait/sprint. Preserve working spectator/FP.
