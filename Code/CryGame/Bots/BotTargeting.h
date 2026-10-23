// WarsBots: single source of truth for "who is this actor to me".
// Independent of network channels: a channel-0 bot from our registry is a full participant.
#pragma once
#include "CryCommon/CryEntitySystem/IEntity.h"

struct IActor;

namespace WarsBots
{
enum class ParticipantKind { None, Human, Bot };
enum class Relation { Invalid, Self, NotParticipant, Spectator, Dead, Ally, Enemy };

struct BotTargeting
{
	// Human: a Player with a live server net channel. Bot: an entity owned by our registry.
	static ParticipantKind Kind(IActor* actor);
	static Relation Classify(const IActor& self, IActor* other);
	static bool IsEnemy(const IActor& self, IActor* other) { return Classify(self, other) == Relation::Enemy; }
	static const char* Name(Relation relation);
	static const char* Name(ParticipantKind kind);
};
}
