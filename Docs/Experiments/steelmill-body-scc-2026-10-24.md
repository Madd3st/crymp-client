# Steelmill native body / SCC experiments, 2026-10-24

Source **ca198a42**, branch `warsbots/feature/weapon-goals`. Research21c4e075
read without merging. MSVC x64 Release experimental client/server build PASS;
matched immutable artifacts `build/play-ca198a42/claude-build.json`.
Installed game EXEs untouched; no graphical client launched.
[Machine-readable metrics/hashes](steelmill-body-scc-2026-10-24.json).

## Changes (separate commits)

- A `12eaec0f`: read-only native body samples on create/revive, immediately,+1s,+5s,
  cancelled on death/removal/disable. Logs actor,life epoch,spawn,entity/physics pose,
  physics type/profile,awake,active,bFlying,bStuck,groundHeight,vel/velRequested.
  Physics enable getter is unavailable in supplied interface: `enabled=unknown_api`.
- B `2b4f297e`: iterative SCC/condensation reachability after NAV ready, bounded16MiB
  bit cache; fail open on unknown projection/cache failure. Weapon/hotspot goals
  proven disconnected are skipped with `GOAL SKIP unreachable`. No A* blanket ban.
- C `46ddec07`: closed-SCC trap starts existing physical side escape at8s instead
  of repath; native jump/step safety and45s native self-kill remain.
- D `0287fa63`, predicate-preservation `08181e5e`: `wb_nav_explain from to` reports
  both native Walkable directions, failed criterion and hit metadata. No graph edits.
- QA `ca198a42`: explicit requested flags, delayed edge probe and source/hash evidence.

All new runtime flags default0: `wb_physics_diagnostics`, `wb_reachable_goals`,
`wb_sink_escape`; sink escape also needs `wb_trap_recovery 1`. Local IA only.
No fake channels, teleports, damage/speed/player/PS changes; spectator retained.

## Native F1 evidence (A)

15 bots,660.52s, no engine error markers/crash. At spawn195, all four complete
revive+5 samples remain pinned at(1247.500,1319.750,136.512), zero actual/requested
velocity, groundHeight=-1e10. Three report bStuck1; one bStuck0 is STILL PINNED.
Normal spawns:209 revive+5 samples bStuck0. Entity/physics delta0 and active1 in
both groups. Native initial creation at195 succeeds in two create+1 samples.
Awake0 occurs on normal moving actors too; bFlying0 alone does not prove grounding.
Thus revive-specific native body/contact defect remains; no physics fix inferred.
Controlled human/native spawn195 comparison is still needed.

## Completed QA

Pure C++ BotPhysicsProbe, BotReachability (including long chain/memory cap), and
BotTrapWatchdog tests PASS. Final build `build/scc-final-build.log` PASS.
Default-OFF build not rerun. Two15-bot runs use identical saved map memory and
mixed existing skill/style roster; one run/source, RNG not fixed. Descriptive only:

| Metric | A baseline12eaec0f | B/C/D ca198a42 |
|---|---:|---:|
| Observed seconds |660.52|660.01|
| Native kill callbacks (includes recovery) |228|216|
| Weapon pickups |98|42|
| Weapon timeouts |296|276|
| Route fallbacks |1904|1510|
| Blocked samples % |1.76|2.22|
| Pinned live time % |1.3|2.3|
| Offline stalls |111|142|
| Offline traps >=60s |0|0|
| Recovery stages1/2/3/4 |12/5/2/2|14/10/5/5|
| Median reported bot Update sum ms/frame |0.237|0.231|
| Worst single bot Update ms |20.76|3.04|

**Behavioral promotion FAIL / inconclusive:** fewer fallbacks, but pickups fall and
stall/recovery incidence rises. B/C remain default OFF. Do not claim smarter bots or
CPU superiority from one unpaired random match. Next isolate weapon filtering from
hotspot filtering and C with repeated identical rosters/memory before promoting.
No new duplicate-shot or client replication claim: fire diagnostics were OFF.

SCC506, largest1714/1536, native construction2.530ms.850 aggregated skip queries.
15-bot NAV arrays identical across both exports AND d4b0854c:7633nodes/41618edges;
wbnav/1 unchanged. Early closed-component level2 at8.01s observed.

D probe2733->2873 rejects rise2.174m; reverse accepted. This is consistent with
current rise1.9/drop3.2 limits, not grounds for blindly symmetrizing the graph.
Separate6-bot60s smoke PASS:2871<->2874 both reject clearance; real ray points
(1245.897,1318.500,136.609)/(1248.132,1318.500,136.576), surface140,part0,
entity0 static/terrain. Native foreign-entity naming branch not exercised here.

## Executed commands

```powershell
./Tools/Build-WarsBots.ps1 -Experimental -BuildDirectory build/multibot-on -Jobs 6
./Tools/Test-WarsBotsTactics.ps1 -GameDirectory 'E:\Steam\steamapps\common\Crysis' -BaselineDirectory build/play-d4b0854c -CandidateDirectory build/play-12eaec0f -ExpectedBaseline d4b0854c -ExpectedCandidate 12eaec0f -CandidateOnly -CandidateTrapRecovery 1 -CandidateHotspotAllNodes 1 -CandidatePhysicsDiagnostics 1 -Seconds 660 -Repeats 1 -Counts 15 -OutputDirectory build/body-soak-12eaec0f
./Tools/Test-WarsBotsTactics.ps1 -GameDirectory 'E:\Steam\steamapps\common\Crysis' -BaselineDirectory build/play-12eaec0f -CandidateDirectory build/play-ca198a42 -ExpectedBaseline 12eaec0f -ExpectedCandidate ca198a42 -CandidateOnly -CandidateTrapRecovery 1 -CandidateHotspotAllNodes 1 -CandidatePhysicsDiagnostics 1 -CandidateReachableGoals 1 -CandidateSinkEscape 1 -NavExports -ExplainNodes 2733,2873 -Seconds 660 -Repeats 1 -Counts 15 -OutputDirectory build/scc-soak-ca198a42
# Same candidate flags, Counts6 Seconds60, ExplainNodes2871,2874:
# OutputDirectory build/explain-ray-ca198a42
python -B Tools/wb_tactics_report.py build/scc-soak-ca198a42/runs.json --output build/scc-soak-ca198a42/report.json --episodes build/scc-soak-ca198a42/episodes.jsonl
python -B build/steelmill-triage-validator/steelmill_triage.py analyze build/scc-soak-ca198a42/WarsBots-Tactics-scc-soak-ca198a42-candidate-r1-n15.bots.log --mem build/scc-soak-ca198a42/initial-memory.mem --graph build/scc-soak-ca198a42/candidate-r1-n15-nav-1.json --out build/scc-soak-ca198a42/triage
```

Logs/reports/episodes preserved in each ignored build folder, compact results committed.
No live CryMP processes at final check; profile memory restored to
01F4C7F4B894AE2AC492C381487FAC5B539824F83EC76C6F902F428A165668AE.

## Manual follow-up

Use matched client/server from build/play-ca198a42 on loopback LAN InstantAction.
Explicitly set wb_probe_local_test1; optional diagnostics1 BEFORE creating bots.
Use existing wb_add/wb_watch; F1 native first person. Enable B/C only for an explicit
comparison experiment, not normal play. Commands entered separately:
`wb_physics_diagnostics 1`, `wb_nav_explain 2871 2874`.
Check revive195 body/contact and physical recovery in FP/TP; compare a real human
on same spawn. Owner feedback from recovery6 still pending. No GUI/network visual
validation was performed by this headless run. Speed and graph fixes remain deferred.
