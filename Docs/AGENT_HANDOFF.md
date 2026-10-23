# Handoff: CryMP migration

Read STATUS and WARSBOTS_PORT first. Source WarsBots snapshot `2da4bf1` is clean and
preserved in its original repository; the new primary development repo is this fork.
Master matches upstream `eb13a69`; warsbots/main starts there. Work is on
`warsbots/feature/combat-port`; only the integrator edited shared files.

Two read-only agents audited engine integration and movement/CryAI respectively;
no competing file edits or additional worktrees. Main integrator ported, compiled
and ran tests. PR360 is merged upstream: AI.RegisterWithAI is already enabled;
this does not establish navigation data or working CryAI behavior on IA maps.

Important integration lessons:

1. `Base` compiles CryMP Server/Client code that allocates CGame. Experimental
   class-layout defines MUST propagate from Base PUBLIC to CryGame and launchers.
   CryGame-only PUBLIC caused a map-load crash in CryAction offset0x2db517.
   /MAP resolved the caller to GameFramework::StartGameContext; fixed build runs.
2. Never access GameObject/GetChannelId from early SerializeSpawnInfo. Preserve
   native teamId; initialize suit after Init/Revive. Match both experimental EXEs.
3. No new address patches; native ActorSystem channel0/BindToNetwork is retained.
   CryMP channel0 IsChannelOnHold is already safe; do not reintroduce the Wars fix.
4. OFF is a real upstream protocol build, ON changes bot spawn/motion serialization.
   The `build/baseline` cache was reused and now holds ON; use the build script flags.

The user is testing the launched CryMP client and server; do not terminate their
session or replace locked EXEs without agreement. Process IDs are local build/*.txt,
but verify actual process identity and whether the player is still connected.
The first client user confirmation covered visibility/movement/shooting. Server
confirmed death/respawn of the same actor and genuine weapon damage in both directions.

Next: finish lifecycle/late-client checks, then strafe intent + independent body/look
replication and native Speed chase. Keep Strength clearance and native energy mechanics.
Investigate headshot AI-vs-player damage parity separately; no health/multiplier change
has been made. Do not claim the entire long-term request complete.
