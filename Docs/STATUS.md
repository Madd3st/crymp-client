# WarsBots for CryMP: status, 2026-10-23

Repository: https://github.com/Madd3st/crymp-client.
Integration `warsbots/main`; implementation `warsbots/feature/combat-port`.
Local checkout: `C:\Users\Magic8\OneDrive\Документы\ChatGPT\Crysis wars bot\local\crymp-client`.
See [port provenance](WARSBOTS_PORT.md), [tests](AI_TESTS.md), [handoff](AGENT_HANDOFF.md).

## Implemented

Latest Wars abilities/Combat MVP migrated into CryGame/Bots, with scoped CryMP
Player/suit/network/GameRules/validator integration and a local engine adapter.
Default OFF; explicit loopback IA opt-in; one owned actor, no fabricated channel.
Native CryMP code is preserved outside bot scopes. No new binary patches/CryAI controller.
Strafe/combat-movement improvements and improved Speed chase remain pending.

## Actually checked on CryMP

- Clean upstream MSVC 19.44 x64 Release client/server BUILD PASS; Resource Compiler
  requires `/c65001` for this Cyrillic checkout. Default-OFF port also BUILD PASS
  and loaded IA Steelmill, listening on 127.0.0.1:64110, without bot commands.
- Experimental client/server BUILD PASS. Initial map crash was an integration bug:
  the define only reached CryGame while Base allocated a smaller CGame. Fixed by
  publishing the define from Base to all consumers. No native DLL patch was needed.
- Fixed build loaded Steelmill and explicitly created Player `WarsBot_01`, server
  EntityId=65501, BindToNetwork=true (required by successful CActor::Init).
- Physical movement 2.50 m/s and native test SCAR fallback observed. This run did
  not prove map pickup; logs distinguish fallback from real pickup.
- One real client joined late; motion/suit/jump replica logs received. User confirmed
  visible bot, movement and shooting. Server recorded actual bot shots damaging
  and killing player via GameRules::KillPlayer.
- Player Nomad killed the bot; death and native Revive result sameEntity=1,
  alive=1, health=100 recorded, followed by another SCAR fallback.
- Strength takeoff vz=9.60, energy about130.3, rise=3.62m and landing recorded;
  client received strength jump sequence. Repeatability fraction is NOT established.
- User observed a one-headshot death. Log: incoming damage26, health100 -> -1067
  after Lua. Damage multipliers/health were not changed; IA parity needs investigation.

Experimental artifacts in `build/baseline/` and installed in original Crysis root:
`E:\Steam\steamapps\common\Crysis` (CrySystem 6156). The build directory name is
historical: CMake cache there is currently experimental ON, not the original baseline.
SHA256 client `B29621EF5565480630D0BE66F371EE1EBF1AB375647EDFF0EE1BE59885D10640`;
server `2C71B62D567D045D7FD7575B2403C0EF71B8B7FEE46556F73D7B1EE4E513F6F3`.

Logs are under `C:\Users\Magic8\OneDrive\Документы\My Games\Crysis`, not game root:
`WarsBots-CryMP-Port-Fixed.log`, `WarsBots-CryMP-Port-Client.log`.
Local build logs: `build/baseline-build.log` (first RC failure),
`baseline-retry.log` (clean PASS), `default-off-build.log`, `port-abi-build.log`.
Full machine logs and game binaries are deliberately not committed.

## Still unproven / next

Manual remove/recreate/map cleanup, two simultaneous clients/reconnect, visual
respawn/jump animation and repeatable normal/Strength/Speed tests remain open.
No genuine strafe yet; current movement still follows facing. Channel0/IsPlayer=false
does not make a match participant, IA score/headshot parity is unverified, PS untouched.
No full navigation, multiple bots or server-only stock-client compatibility claim.
Do not replace these CryMP results with older Crysis Wars validation or vice versa.
