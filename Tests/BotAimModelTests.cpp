#include "CryGame/Bots/BotAimModel.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace WarsBots;

// time (s) until the view is within the target size of a static target `deg` away
static float TimeToOnTarget(const AimSkill& s, float deg, uint32_t seed = 7)
{
	BotAimModel m(s, seed); m.SetView(0, 0);
	for (int i = 0; i < 200; ++i)
	{
		AimInput in; in.dt = 0.01f; in.hasTarget = true; in.targetId = 1; in.targetYaw = deg; in.targetSizeDeg = 2.f;
		AimOutput o = m.Update(in);
		if (o.onTarget && !o.inSaccade) return (i + 1) * 0.01f;
	}
	return -1.f;
}

int main()
{
	const AimSkill beg = AimSkillPreset(0), nor = AimSkillPreset(1), eli = AimSkillPreset(3);
	// 1. Flick time grows with the angle (Fitts) and elite is faster than beginner.
	float t30 = 0, t120 = 0, e90 = 0, b90 = 0;
	for (uint32_t s = 1; s <= 20; ++s) { t30 += TimeToOnTarget(nor, 30, s); t120 += TimeToOnTarget(nor, 120, s);
		e90 += TimeToOnTarget(eli, 90, s); b90 += TimeToOnTarget(beg, 90, s); }
	assert(t30 > 0 && t120 > t30 && e90 < b90);
	assert(e90 / 20 < 0.35f && b90 / 20 < 1.2f);
	// 2. Turn-rate cap: a beginner cannot turn 180° in less than 180/400 s.
	{
		BotAimModel m(beg, 3); m.SetView(0, 0); float maxRate = 0, last = 0;
		for (int i = 0; i < 100; ++i) { AimInput in; in.dt = 0.01f; in.hasTarget = true; in.targetId = 1; in.targetYaw = 179.f;
			AimOutput o = m.Update(in); float r = std::fabs(BotAimModel::Wrap(o.yaw - last)) / 0.01f; if (r > maxRate) maxRate = r; last = o.yaw; }
		assert(maxRate <= 401.f);
	}
	// 3. Tracking a strafing target (20°/s across view): mean error stays below 3° for normal, smaller for elite.
	auto track = [](const AimSkill& s) {
		BotAimModel m(s, 11); m.SetView(0, 0); float sum = 0; int n = 0;
		for (int i = 0; i < 400; ++i) { AimInput in; in.dt = 0.02f; in.hasTarget = true; in.targetId = 1;
			in.targetYaw = -20.f + 20.f * i * 0.02f; in.targetYawRate = 20.f; in.targetSizeDeg = 2.f;
			AimOutput o = m.Update(in); if (i > 50) { sum += o.errorDeg; ++n; } }
		return sum / n; };
	const float trN = track(nor), trE = track(eli);
	assert(trN < 3.f && trE < trN);
	// 4. Jitter has no fixed frequency: the strongest DFT bin of the resting yaw rate is not a sharp spike
	//    (a 2.7 Hz sine of the same power gives a ratio > 20; OU noise stays well below).
	{
		BotAimModel m(nor, 5); m.SetView(0, 0); const int N = 512; float x[N]; float last = 0;
		for (int i = 0; i < N + 100; ++i) { AimInput in; in.dt = 0.02f; in.hasTarget = true; in.targetId = 1; in.targetYaw = 0;
			AimOutput o = m.Update(in); if (i >= 100) x[i - 100] = (o.yaw - last) / 0.02f; last = o.yaw; }
		double best = 0, mean = 0;
		for (int k = 1; k < N / 2; ++k) { double re = 0, im = 0; for (int i = 0; i < N; ++i) { re += x[i] * std::cos(2 * 3.14159265358979 * k * i / N); im -= x[i] * std::sin(2 * 3.14159265358979 * k * i / N); }
			double p = re * re + im * im; mean += p; if (p > best) best = p; }
		mean /= (N / 2 - 1);
		assert(best / mean < 12.0);
	}
	// 5. No target: view stays put (no drift beyond the hand jitter).
	{
		BotAimModel m(nor, 9); m.SetView(45, 0); AimInput in; in.hasTarget = false; AimOutput o;
		for (int i = 0; i < 100; ++i) o = m.Update(in);
		assert(std::fabs(o.yaw - 45.f) < 1e-3f);
	}
	std::printf("BotAimModelTests OK  flick normal 30deg %.2fs 120deg %.2fs | 90deg elite %.2fs beginner %.2fs | track err normal %.2f elite %.2f deg\n",
		t30 / 20, t120 / 20, e90 / 20, b90 / 20, trN, trE);
	return 0;
}
