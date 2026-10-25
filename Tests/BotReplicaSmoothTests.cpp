#include "CryGame/Bots/BotReplicaSmooth.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace WarsBots;

static bool Near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main()
{
	const RVec eye{0, 0, 1.6f};
	// 1. First update snaps to the replicated pose.
	{
		BotReplicaSmooth s; s.Update(0.016f, RVec{0, 1, 0}, RVec{0, 10, 1.6f}, eye);
		assert(s.Initialized() && Near(s.DirYawDeg(), 0.f, 1e-3f) && Near(s.Aim(eye).y, 10.f, 1e-3f));
	}
	// 2. A 90° step in direction becomes a curve: after one frame well under 90°, after 0.5 s ≈ done.
	{
		BotReplicaSmooth s; s.Update(0.016f, RVec{0, 1, 0}, RVec{0, 10, 1.6f}, eye);
		s.Update(0.016f, RVec{1, 0, 0}, RVec{0, 10, 1.6f}, eye);
		assert(s.DirYawDeg() > 5.f && s.DirYawDeg() < 30.f);
		for (int i = 0; i < 30; ++i) s.Update(0.016f, RVec{1, 0, 0}, RVec{0, 10, 1.6f}, eye);
		assert(Near(s.DirYawDeg(), 90.f, 1.f));
	}
	// 3. Frame-rate independence: 10 x 10 ms == 1 x 100 ms.
	{
		BotReplicaSmooth a, b;
		a.Update(0.01f, RVec{0, 1, 0}, RVec{0, 10, 1.6f}, eye); b.Update(0.01f, RVec{0, 1, 0}, RVec{0, 10, 1.6f}, eye);
		for (int i = 0; i < 10; ++i) a.Update(0.01f, RVec{1, 0, 0}, RVec{10, 0, 1.6f}, eye);
		b.Update(0.1f, RVec{1, 0, 0}, RVec{10, 0, 1.6f}, eye);
		assert(Near(a.DirYawDeg(), b.DirYawDeg(), 0.01f) && Near(a.AimYawDeg(), b.AimYawDeg(), 0.01f));
	}
	// 4. Wrap-around: 170° -> -170° turns 20° through 180°, not 340° the long way.
	{
		BotReplicaSmooth s; const float r = 170.f / 57.2957795f, l = -170.f / 57.2957795f;
		s.Update(0.016f, RVec{std::sin(r), std::cos(r), 0}, RVec{0, 10, 1.6f}, eye);
		s.Update(0.05f, RVec{std::sin(l), std::cos(l), 0}, RVec{0, 10, 1.6f}, eye);
		const float y = s.DirYawDeg();
		assert(y > 170.f || y < -170.f);
	}
	// 5. Big jumps (revive / about-face > 120°) snap immediately.
	{
		BotReplicaSmooth s; s.Update(0.016f, RVec{0, 1, 0}, RVec{0, 10, 1.6f}, eye);
		s.Update(0.016f, RVec{0, -1, 0}, RVec{0, -10, 1.6f}, eye);
		assert(Near(std::fabs(s.DirYawDeg()), 180.f, 1e-2f) && Near(std::fabs(s.AimYawDeg()), 180.f, 1e-2f));
	}
	// 6. Aim distance: a new far target re-bases the distance at once (no point sliding through the air).
	{
		BotReplicaSmooth s; s.Update(0.016f, RVec{0, 1, 0}, RVec{0, 5, 1.6f}, eye);
		s.Update(0.016f, RVec{0, 1, 0}, RVec{0, 40, 1.6f}, eye);
		assert(Near(s.Aim(eye).y, 40.f, 0.1f));
	}
	std::puts("BotReplicaSmoothTests OK");
	return 0;
}
