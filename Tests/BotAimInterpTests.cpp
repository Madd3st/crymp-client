// Oracle for owner playtest 3 ("FP spectator super jerky"): a bot turning steadily, aim packets every 100 ms, camera
// at 60 fps. The old exponential filter (BotReplicaSmooth, tau 50 ms) moves in bursts; interpolation must not.
#include "CryGame/Bots/BotAimInterp.h"
#include "CryGame/Bots/BotReplicaSmooth.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace WarsBots;

// coefficient of variation of the per-frame yaw change over the steady part
static float Cv(const std::vector<float>& d)
{
	double m = 0, v = 0; for (float x : d) m += x; m /= d.size();
	for (float x : d) v += (x - m) * (x - m); v /= d.size();
	return float(std::sqrt(v) / (m > 1e-9 ? m : 1e-9));
}

int main()
{
	const float rate = 1.2f;                 // rad/s steady turn (~70 deg/s)
	const float frame = 1.f / 60.f, packet = 0.1f;
	BotAimInterp interp; BotReplicaSmooth exp;
	std::vector<float> dInterp, dExp;
	float lastI = 0, lastE = 0, nextPacket = 0, sentYaw = 0;
	const RVec eye{0, 0, 0};
	for (int f = 0; f < 600; ++f)
	{
		const float t = f * frame;
		if (t >= nextPacket) { sentYaw = BotAimInterp::Wrap(rate * t); interp.Push(t, sentYaw, 0.f); nextPacket += packet; }
		float yi, pi; interp.At(t, yi, pi);
		const RVec aim{std::sin(sentYaw) * 20.f, std::cos(sentYaw) * 20.f, 0.f};
		exp.Update(frame, RVec{0, 1, 0}, aim, eye);
		const float ye = exp.AimYawDeg() / 57.2957795f;
		if (f > 120) { dInterp.push_back(std::fabs(BotAimInterp::Wrap(yi - lastI))); dExp.push_back(std::fabs(BotAimInterp::Wrap(ye - lastE))); }
		lastI = yi; lastE = ye;
	}
	const float cvI = Cv(dInterp), cvE = Cv(dExp);
	std::printf("per-frame yaw change CV: interp %.3f, old exponential %.3f\n", cvI, cvE);
	assert(cvE > 0.4f);   // red on the old filter: bursts then stops
	// while firing the old path reset the filter every frame = raw 10 Hz steps: one frame in six moves
	{
		std::vector<float> raw; for (int f = 0; f < 480; ++f) raw.push_back(f % 6 == 0 ? rate * packet : 0.f);
		assert(Cv(raw) > 2.0f);
	}
	assert(cvI < 0.15f);  // steady
	// latency about one packet interval
	{
		BotAimInterp a; for (int i = 0; i < 10; ++i) a.Push(i * 0.1f, 0.1f * i, 0.f);
		assert(std::fabs(a.Delay() - 0.11f) < 0.01f);
		float y, p; a.At(0.9f, y, p); assert(std::fabs(y - (0.9f - 0.11f)) < 0.02f);
	}
	// wrap across +-pi without spinning the long way
	{
		BotAimInterp a; a.Push(0.f, 3.10f, 0.f); a.Push(0.1f, -3.10f, 0.f); a.Push(0.2f, -3.00f, 0.f);
		float y, p; a.At(0.16f, y, p); assert(std::fabs(BotAimInterp::Wrap(y - 3.14159f)) < 0.1f);
	}
	// starved (no new packet): holds the newest, never extrapolates; long gap restarts
	{
		BotAimInterp a; a.Push(0.f, 0.f, 0.f); a.Push(0.1f, 0.5f, 0.f);
		float y, p; a.At(5.f, y, p); assert(std::fabs(y - 0.5f) < 1e-4f);
		a.Push(6.f, 1.f, 0.2f); a.At(6.f, y, p); assert(std::fabs(y - 1.f) < 1e-4f && std::fabs(p - 0.2f) < 1e-4f);
	}
	std::puts("BotAimInterpTests OK");
	return 0;
}
