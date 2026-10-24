# Local IA reproduction commands

Manual only; requires experimental build and existing loopback LAN IA gate:
sv_bind127.0.0.1, sv_lanonly1, wb_probe_local_test1. No automatic relocation/revive.

- `wb_revive_at 195 1`: native kill (if alive) then native revive of owned bot#1
  at registered spawn EntityId195, using the same +0.2m offset as automatic revive.
- `wb_revive_at 195`: same, for selected bot (`wb_select 1`).
- `wb_revive_at 195 me`: listen-server local player; on a dedicated server only
  the sole real player (refuses none/ambiguous). Refuses spectators/vehicles/frozen actors.
- `wb_teleport 1 1247.5 1319.75 136.51`: relocate owned live bot#1, preserve AI on/off.
  Native ClMoveTo broadcasts rather than addressing nonexistent channel0.
- `wb_physics_diagnostics 1` before revive enables existing now/+1/+5 bot samples.

Bot numbers come from wb_list, spawnId is an EntityId registered with current GameRules,
not an ordinal. Invalid IDs/coordinates/closed gate refuse before mutation. The commands
are diagnostic: native forced death affects match statistics, so exclude these sessions
from performance/combat comparisons. Teleport is not an automatic recovery fix.
Human native revive is explicit test-only; ordinary player lifecycle unchanged.

Build: MSVC x64 Release experimental client/server PASS (initial ZERO constructor compile
error corrected; final log build/debug-commands-build-r2.log). No graphical test yet.
