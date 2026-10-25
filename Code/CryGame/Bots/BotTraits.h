// Character traits (T-8, Quake 3 / UT3 character files): continuous numbers instead of hard style switches. Styles and
// skills are presets; each bot gets a small deterministic jitter so two "Normal Assault" bots do not play identically.
// Read by the tactics (jumper, holder), vantage (camper, prone) and later the PS roles (objective focus, teamwork).
// Pure logic; Tests/BotTraitsTests.cpp.
#pragma once

namespace WarsBots {

struct BotTraits {
	float aggression = 0.5f;   // push vs keep distance
	float jumper = 0.3f;       // hops in duels
	float holder = 0.5f;       // holds angles from cover instead of chasing
	float camper = 0.2f;       // takes and holds sniper spots
	float prone = 0.f;         // lies down on a held spot (elite marksmen only)
	float objective = 0.5f;    // PS: plays the objective vs hunts kills
	float teamwork = 0.5f;     // PS: follows squad orders
};

inline float Clamp01T(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

// skill 0 beginner .. 3 elite; style 0 universal 1 assault 2 marksman 3 flanker 4 defender 5 hunter (BotProfile order)
inline BotTraits MakeTraits(int skill, int style, unsigned seed)
{
	BotTraits t;
	static const float jumpBySkill[4] = {0.10f, 0.30f, 0.50f, 0.70f};
	const int sk = skill < 0 ? 0 : (skill > 3 ? 3 : skill);
	t.jumper = jumpBySkill[sk];
	switch (style)
	{
	case 1: t.aggression = 0.8f; t.jumper += 0.15f; t.holder = 0.2f; t.camper = 0.0f; t.objective = 0.6f; break;   // assault
	case 2: t.aggression = 0.3f; t.jumper -= 0.10f; t.holder = 0.9f; t.camper = 0.9f; t.objective = 0.3f; break;   // marksman
	case 3: t.aggression = 0.7f; t.jumper += 0.15f; t.holder = 0.2f; t.camper = 0.0f; t.objective = 0.5f; break;   // flanker
	case 4: t.aggression = 0.3f; t.jumper -= 0.10f; t.holder = 0.9f; t.camper = 0.6f; t.objective = 0.8f; break;   // defender
	case 5: t.aggression = 0.7f; t.holder = 0.5f; t.camper = 0.1f; t.objective = 0.3f; break;                       // hunter
	default: break;                                                                                                   // universal
	}
	t.prone = (sk == 3 && style == 2) ? 0.8f : 0.f;
	// deterministic jitter +-0.1 per trait from the bot seed
	unsigned h = seed * 2654435761u + 0x9E3779B9u;
	auto j = [&h]() { h ^= h << 13; h ^= h >> 17; h ^= h << 5; return (float(h % 2001) / 1000.f - 1.f) * 0.1f; };
	t.aggression = Clamp01T(t.aggression + j()); t.jumper = Clamp01T(t.jumper + j());
	t.holder = Clamp01T(t.holder + j()); t.camper = Clamp01T(t.camper + j());
	t.objective = Clamp01T(t.objective + j()); t.teamwork = Clamp01T(t.teamwork + j());
	if (t.jumper > 0.9f) t.jumper = 0.9f;
	return t;
}

} // namespace WarsBots
