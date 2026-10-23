# WarsBots port provenance and integration

Target: original Crysis 1.2.1 (6156), CryMP x64, not Crysis Wars 1.5 (6729).
Upstream base: `crymp-net/crymp-client` commit
`eb13a69deb3a5e87beedc5ca1b408b34d8583bbb` (v26).
Source: https://github.com/Madd3st/WarsBots, branch `codex/warsbots-abilities`.
Snapshot `2da4bf1d465cba64cd038fa82d29d0fdae6fc178`; ability code `9359c15`,
combat baseline `5d42802`, old live validation `bef7ad5`.
Original sources/history remain in that repository. Their Crytek SDK notices and
[license](https://github.com/Madd3st/WarsBots/blob/codex/warsbots-abilities/LICENSE.txt)
are retained; this migration does not relicense Crytek code.

## Reused and adapted

BotBrain, Combat, Weapons, Movement, Intent, Blackboard, Locomotion and Suit retain
the latest source logic: FSM, target memory/LOS, pickup, native firing, local steering,
energy policy and guarded native jump. No older public snapshot replaced local code.
BotManager/Spawner retain single-actor ownership and lifecycle, adapted to CryMP APIs.
CryMPEngineAdapter centralizes loopback IA gating and CreateActor(0) integration.
No virtual network connection or second AI implementation is introduced.

CryMP differences checked in source:

- Headers use CryCommon/CryGame paths; ZERO is replaced by explicit vectors.
  Coherent ladder/freefall stats use Value(); freefall <=0 includes the native sentinel.
- Native ActorSystem checks a missing GameServerChannel before use. Native CActor::Init
  requires successful BindToNetwork, so the adapter does not bind again.
- CryMP IsChannelOnHold(0) is safe; the old Wars workaround was deliberately omitted.
- CPlayer channel0 has no ordinary player input. Scoped Player hooks supply native
  movement/rotation, suit initialization and replicated motion through optional groups.
  Existing team spawn metadata and Serialize_CryMP/Serialize_Default are preserved.
- CryMP IWeapon does not expose IsFiring directly; use the current native IFireMode.
  Owned bot sprint/CanFire rules are scoped; normal players retain upstream behavior.
- ShotValidator accepts only the owned real, nonremote server projectile and owned
  weapon; arbitrary channel0 hits remain refused. No artificial shot queue registration.
- Wars-specific scoreboard storage keys are omitted: CryMP IA score is UNVERIFIED.
- Both experimental client and server need matching builds. Server-only compatibility
  is NOT established; OFF has no bot serialization or startup hooks.

## CryAI decision

Launcher already calls AllowMultiplayerAI/AllowMultiplayerRegisterWithAI. No extra
binary patches were added. These switches do not prove AI registration or nav data.
Keep WarsBots decisions/combat and native CryMP execution now; add CryAI as an optional
route provider only after a comparative map test. Full CryAI replacement is deferred.
No general navigation, multiple-bot registry, scoreboard or Power Struggle migration yet.

Strafing now has separate world movement, aim-facing and a replicated strafe flag;
combat alternates lateral movement with distance correction. Speed chase uses native
sprint with distance/energy hysteresis. See STATUS for physical tests and limits.
Strength remains native; current CryMP measurements are separate from older Wars tests.
