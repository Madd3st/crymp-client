#include "PortIncludes.h"
#include "CryGame/Bots/BotWaterPolicy.h"
#include "CryGame/Bots/BotConfig.h"
#include "CryGame/Bots/BotManager.h"
#include "CryGame/Bots/BotMatch.h"
#include "CryGame/Bots/BotNavMesh.h"
#include "CryGame/Bots/BotNavMeshCore.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"

namespace WarsBots
{
BotWaterFacts BotWaterFactsFor(const CPlayer& actor)
{
	BotWaterFacts f;
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
	f.experimentalBuild = true;
#endif
	const CGameRules* rules = g_pGame ? g_pGame->GetGameRules() : nullptr;
	f.multiplayer = gEnv && gEnv->bMultiplayer;
	f.powerStruggle = rules && BotMatch::IsPowerStruggle(*rules);
	f.owned = gEnv && gEnv->bServer && g_pGame && g_pGame->GetBotManager().Owns(actor.GetEntityId());
	f.botActor = actor.IsWarsBotActor();
	f.captureEnabled = BotConfig::Get().psCapture == 1;
	f.navMeshEnabled = BotConfig::Get().botNavmesh == 1;
	f.hasNanoSuit = actor.GetNanoSuit() != nullptr;
	// Avoid lazy loading for ordinary actors and disabled policies. Missing data stays fail-closed.
	const NavMeshCore* mesh = f.owned && f.botActor && f.powerStruggle && f.captureEnabled && f.navMeshEnabled ? CurrentNavMesh() : nullptr;
	f.navMeshReady = mesh && mesh->Ready();
	return f;
}

bool BotWaterAllowedFor(const CPlayer& actor) { return BotWaterAllowed(BotWaterFactsFor(actor)); }
}
