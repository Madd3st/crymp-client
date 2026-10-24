# MOVE TRACE and local recording data, 2026-10-24

Game source8b9236f4 (featuredae506db, fixed-phase timer8b9236f4).
MSVC x64 Release experimental client/server PASS; matching EXEs/hashes:
`build/play-8b9236f4/claude-build.json`. Installed game EXEs untouched.
Runtime `wb_move_trace` defaults0. No AI/physics/damage/player/net-aspect writes.
No graph edits; B/C remain default OFF. Default-OFF build not rerun.

## Capture

Explicit owner launch (not executed automatically):
```powershell
./Tools/Play-WarsBotsRecording.ps1
```
Steelmill IA,6 Normal, clean DX9,r_displayInfo1, B/C/full-hotspot0,
trap recovery1, physics diagnostics1, MOVE TRACE1 on server AND client.
Existing sessions are retained/refused; no auto-stop or +quit. Requires frozen local-v1
MOVE TRACE manifest. Rejects local Bin64/D3D9.dll wrapper. Live client/human path and
actual DX9 module load of this NEW build still need owner verification (previousb9
live session used systemd3d9 successfully). No GUI was started for these QA tests.

Client and server console separately: `wb_move_trace 1` starts, `wb_move_trace 0` stops.
Records are file-only by default (wb_log1), never a HUD/network channel. Runtime cvar
can be enabled for a client with no locally owned bots. Server samples owned bots and
real Player actors; client samples only its own non-spectator human. Do not double-count
server/client observations of the same human. Native input/sprint flags on server may
differ in timing; use local=1 side=client for the owner's exact recording.

**New paths (C-008):**
- `C:\Users\Magic8\AppData\Local\WarsBots\Logs`: engine logs (explicit absolute
  -logfile in launchers) and matching .bots.log.
- `C:\Users\Magic8\AppData\Local\WarsBots\Memory`: map.mem and default.nav.json.
- Root migration-20261024.json records173 moved files and verifiedSHA256.
Ordinary Crysis profile/settings unchanged. Pre-fb578d52 frozen binaries still use the
old profile; current QA scripts reject those manifests rather than mix data locations.
QA port46040 avoids MSI's occupied64140; recording46050. No MSI process was stopped.

## Record contract for Claude

`MOVE TRACE actor=... kind=human|bot side=client|server local=0|1 t=... sampleDt=...`
then `pos=(x,y,z) vel=(vx,vy,vz) speed=... vz=... mode=... energy=... sprint=...`
`grounded=... flying=... jump=... land=... takeoffs=... jumpEdges=... landEdges=...`
`jumpTime=... landTime=... peakZ=... health=...`.

World position is entity root (metres), not eye/camera. speed is horizontalm/s; vz and
vel are native physics velocity (actor stats fallback). Nano modes0Speed/1Strength/
2Cloak/3Armor, energy native0..200; -1 if no suit. flying=-1 means no living status.
grounded uses native onGround/inAir; raw jump/land are SPlayerStats flags. Event counts
latch frame-by-frame between log samples; jumpEdges is a rising native jumped flag,
takeoffs/landEdges are observed ground transitions. They do not invent jump actions
or guarantee every wall-jump input is a distinct native flag edge.

Records follow a fixed250ms phase grid (4Hz average, frame scheduling jitter allowed).
No catch-up bursts after stalls. peakZ is max entity-rootZ observed frame-by-frame since
previous record, so a short hop need not have its apex on a250ms sample. jumpTime and
landTime are observed game-frame times (-1 before first event). Dead/spectator/removed
actors are reset/pruned; initial airborne spawn can later report a genuine landing.
No scripted movement/teleport is added. Filter existing explicit DEBUG TELEPORT/REVIVE
sessions from natural-jump data. Sampling resolution is game-frame dependent; physics
sub-frame peaks are not measured. Log flush bounded to~1s,64 tracked actors and200000
records per enabled capture/level. Toggle0/1 resets cap. Neither flag promoted by default.

## Verification

Pure BotMoveTraceTests PASS:250ms scheduling, fixed phase under30Hz frame quantization,
no catch-up, rollback/toggle reset, short jump+land entirely inside one interval,
peak latch, death/respawn reset. First link failed because new.cpp was missing from
explicit CMake source list; fixed. Both matched EXEs subsequently linked successfully.

Default-OFF60s six-bot run dae506db:0 records. ON60s:1290 records,12 actor IDs across
native match reset,6 jump-flag edges/11 takeoffs/23 landings, max329bytes/line. Initial
median0.266s revealed drift; timer fixed in8b9236f4. Corrected ON-only timing check
saved in build/move-trace-phase-8b9236f4/ (see JSON). OFF logic unchanged, not rerun.
These are pipeline/lifecycle checks, not10-minute behavioral/performance promotion
or human/client validation. Summaries generated; QA memory restored after each run.
[Metrics](move-trace-2026-10-24.json).

## C-002 / C-006 context

C-002 native selector+Spawned+UpdateAreas does NOT fix spawn195:10 valid body+5 samples
(9+1 supplement) remain pinnedz136.512 with invalid groundHeight. First native cohort
fixture failed one128-try selector refusal; do not call that entire cohort PASS. Nine
complete~12s freeze windows across runs frozen9/9. Diagnostic only, no production revive
change. [Native evidence](c002-native-hooks-2026-10-24.json).

C-00660s fixture PASS:40pairs/80directions, all first blocked byclearance h0.5;
surfaces140:68,198:4,143:2,193:6. All foreign EntityId0 (static/terrain, no IEntity names).
These first hits do not prove all are false cuts or later rays/ground tests would pass.
[Raw and counts](C006-2026-10-24/report.json), [raw.log](C006-2026-10-24/raw.log).
Native graph arrays equal d4b0854c, wbnav/1 unchanged. B parked perC-009; no refactorC-010.
Next: owner trace/video, native body/contact diagnosis for195, then reviewed graph-repair
proposal. No rail ignoring/drop/jump links implemented in this step.

Corrected ON60s run PASS: 1373 rows, effective 3.9987Hz, median interval0.254s, no duplicate actor/time records. Human client still untested; no client launched.
