// DS-070: the single policy for the owned PowerStruggle bot's native water handling.
// Every water exception (canMove, SafeStep ground ray, trap watchdog, native oxygen/drowning) must call
// BotWaterAllowed with these facts, so one false field disables all of them together and OFF/IA/human/unowned
// actors keep the old "water is impassable" behaviour. Engine-free: Tests/BotWaterPolicyTests.cpp.
#pragma once

class CPlayer;

namespace WarsBots
{
struct BotWaterFacts
{
	bool experimentalBuild = false; // built with WARSBOTS_EXPERIMENTAL_SPAWNER
	bool multiplayer = false;       // gEnv->bMultiplayer
	bool powerStruggle = false;     // BotMatch::IsPowerStruggle
	bool owned = false;             // BotManager::Owns on this server
	bool botActor = false;          // CPlayer::IsWarsBotActor (our channelId-0 actor)
	bool captureEnabled = false;    // BotConfig::psCapture == 1
	bool navMeshReady = false;      // a real mesh is loaded; flag1 alone must not enable water
	bool navMeshEnabled = false;    // BotConfig::botNavmesh == 1
	bool hasNanoSuit = false;       // the native oxygen path dereferences the suit
};

inline bool BotWaterAllowed(const BotWaterFacts& f)
{
	return f.experimentalBuild && f.multiplayer && f.powerStruggle && f.owned && f.botActor &&
		f.captureEnabled && f.navMeshEnabled && f.navMeshReady && f.hasNanoSuit;
}

// Engine-side facts for one actor. Defined in BotWaterPolicy.cpp; the pure truth table above links without it.
BotWaterFacts BotWaterFactsFor(const CPlayer& actor);
bool BotWaterAllowedFor(const CPlayer& actor);
}
