#include "CryGame/Bots/BotMoverLink.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace WarsBots;

// Tiny world: a lift between z=110 and z=121 that moves at 1 m/s toward a called floor; the bot walks at 4 m/s.
struct World {
	MV3 bot{0, 0, 110}; float platZ = 121.f, platV = 0.f, target = 121.f; MV3 centre{10, 0, 0};
	bool liftDead = false;
	void Use(float botZ) { if (!liftDead) target = (std::fabs(platZ - target) < 0.01f) ? (std::fabs(botZ - 110.f) < 1.f && platZ > 115.f ? 110.f : (platZ < 115.f ? 121.f : 110.f)) : target; }
	void Step(const MoverOutput& o, float dt)
	{
		if (o.use) Use(bot.z);
		const float d = target - platZ; platV = std::fabs(d) > 0.01f ? 1.f : 0.f;
		const float on = std::sqrt((bot.x - centre.x) * (bot.x - centre.x) + (bot.y - centre.y) * (bot.y - centre.y)) <= 1.0f && std::fabs(bot.z - platZ) < 0.7f;
		const float dz = std::fabs(d) > 0.01f ? (d > 0 ? 1.f : -1.f) * std::fmin(std::fabs(d), dt) : 0.f;
		platZ += dz; if (on) bot.z += dz;
		if (o.move) { const float dx = o.moveTo.x - bot.x, dy = o.moveTo.y - bot.y, L = std::sqrt(dx * dx + dy * dy);
			if (L > 1e-3f) { const float s = std::fmin(L, 4.f * dt); bot.x += dx / L * s; bot.y += dy / L * s; } }
	}
};

static MoverState Run(World& w, MoverFail* fail = nullptr, float* took = nullptr)
{
	BotMoverLink m; MoverLink l; l.switchPos = {8, 1.5f, 110}; l.platformCentre = {10, 0, 110}; l.fromZ = 110; l.toZ = 121; l.exitPos = {13, 0, 121};
	m.Start(l, 0);
	float t = 0; MoverOutput o;
	for (int i = 0; i < 2000 && m.Active(); ++i)
	{
		MoverInput in; in.now = t; in.pos = w.bot; in.platform = {w.centre.x, w.centre.y, w.platZ}; in.platformSpeed = w.platV;
		o = m.Update(in); w.Step(o, 0.05f); t += 0.05f;
	}
	if (fail) *fail = o.fail;
	if (took) *took = t;
	return m.State();
}

int main()
{
	// 1. Lift waiting at the top: walk to the switch, call it, wait ~11 s, board, ride up, exit.
	{ World w; float took = 0; assert(Run(w, nullptr, &took) == MoverState::Done); assert(w.bot.z > 120.5f && took > 20.f && took < 40.f); }
	// 2. Lift already at our floor: board directly (no switch), faster.
	{ World w; w.platZ = 110.f; w.target = 110.f; float took = 0; assert(Run(w, nullptr, &took) == MoverState::Done); assert(took < 20.f); }
	// 3. Dead lift: fails with a platform timeout instead of standing forever.
	{ World w; w.liftDead = true; MoverFail f; assert(Run(w, &f) == MoverState::Failed && f == MoverFail::PlatformTimeout); }
	std::puts("BotMoverLinkTests OK");
	return 0;
}
