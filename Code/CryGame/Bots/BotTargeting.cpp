#include "PortIncludes.h"
#include "CryGame/Bots/BotTargeting.h"
#include "CryGame/Bots/BotManager.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryGame/Actors/Actor.h"
#include <cstring>

namespace WarsBots
{
ParticipantKind BotTargeting::Kind(IActor* actor)
{
	if (!actor || !g_pGame || strcmp(actor->GetEntity()->GetClass()->GetName(), "Player"))
		return ParticipantKind::None;
	const int channel = actor->GetChannelId();
	if (channel && g_pGame->GetIGameFramework()->GetNetChannel(channel))
		return ParticipantKind::Human;
	// Ownership is a server fact; clients never run bot decisions.
	if (gEnv->bServer && g_pGame->GetBotManager().Owns(actor->GetEntityId()))
		return ParticipantKind::Bot;
	return ParticipantKind::None;
}

Relation BotTargeting::Classify(const IActor& self, IActor* other)
{
	if (!other)
		return Relation::Invalid;
	if (other->GetEntityId() == self.GetEntityId())
		return Relation::Self;
	if (Kind(other) == ParticipantKind::None)
		return Relation::NotParticipant;
	CActor* target = static_cast<CActor*>(other); // Kind() verified the Player class.
	if (target->GetSpectatorMode() != 0)
		return Relation::Spectator;
	if (other->GetHealth() <= 0)
		return Relation::Dead;
	// Hidden entities and vehicle passengers stay out of scope until vehicle combat exists.
	if (other->GetEntity()->IsHidden() || other->GetLinkedVehicle())
		return Relation::Invalid;
	if (CGameRules* rules = g_pGame->GetGameRules())
	{
		// Team 0 means "no team" (Instant Action free-for-all): everyone is hostile.
		const int mine = rules->GetTeam(self.GetEntityId());
		if (mine != 0 && mine == rules->GetTeam(other->GetEntityId()))
			return Relation::Ally;
	}
	return Relation::Enemy;
}

const char* BotTargeting::Name(Relation relation)
{
	switch (relation)
	{
	case Relation::Invalid: return "invalid";
	case Relation::Self: return "self";
	case Relation::NotParticipant: return "not-participant";
	case Relation::Spectator: return "spectator";
	case Relation::Dead: return "dead";
	case Relation::Ally: return "ally";
	case Relation::Enemy: return "enemy";
	}
	return "unknown";
}

const char* BotTargeting::Name(ParticipantKind kind)
{
	switch (kind)
	{
	case ParticipantKind::None: return "none";
	case ParticipantKind::Human: return "human";
	case ParticipantKind::Bot: return "bot";
	}
	return "unknown";
}
}
