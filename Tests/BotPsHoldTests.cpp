// SPEC-005b (wb_bot_ps_hold): defender position choice, engine-free. Rays and navmesh are mocked as candidate fields.
#include "CryGame/Bots/BotPsHold.h"
#include <cmath>
#include <cstdio>
#include <string>

using namespace WarsBots;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static HoldCandidate C(float x, float y, float z, bool cover, bool sees, float path = 18.f)
{ HoldCandidate c; c.x = x; c.y = y; c.z = z; c.onMesh = true; c.path = path; c.cover = cover; c.seesCenter = sees; return c; }

int main()
{
	const HoldConfig cfg;
	const HoldApproach east = MakeApproach(0, 0, 200, 0);   // enemy to +x
	CHECK(east.valid && std::fabs(east.x - 1.f) < 1e-5f);
	CHECK(!MakeApproach(5, 5, 5.1f, 5).valid);

	// sector +-45 deg
	CHECK(InSector(0, 0, east, 17, 0, 45));
	CHECK(InSector(0, 0, east, 12, 12, 45));        // exactly 45 deg
	CHECK(!InSector(0, 0, east, 10, 14, 45));       // ~54 deg
	CHECK(!InSector(0, 0, east, -17, 0, 45));       // behind the centre
	CHECK(InSector(0, 0, HoldApproach(), -17, 0, 45)); // no approach known: any direction
	// 16 directions: 0, +-22.5, +-45 pass
	CHECK(SectorDirections(east, 45).size() == 5);
	CHECK(SectorDirections(HoldApproach(), 45).size() == 16);

	// score: cover +3, up +1/m to +5, sees centre +2
	CHECK(HoldScore(C(17, 0, 0, true, true), 0) == 5);
	CHECK(HoldScore(C(17, 0, 3.4f, false, false), 0) == 3);
	CHECK(HoldScore(C(17, 0, 9, true, true), 0) == 10);
	CHECK(HoldScore(C(17, 0, -2, false, true), 0) == 2);

	const std::vector<HoldPos> nobody;
	// best score in the sector and on the ring wins; outside ring / sector / path / mesh are never picked
	{
		std::vector<HoldCandidate> c = {
			C(17, 0, 0, false, true),          // 2
			C(16, 4, 0, true, true),           // 5  <- best
			C(-17, 0, 6, true, true),          // behind: excluded despite 10
			C(8, 0, 0, true, true),            // inside the ring (8 m)
			C(30, 0, 0, true, true),           // outside the ring
			C(17, -3, 0, true, true, 60.f),    // path 60 m > 35
		};
		HoldCandidate off = C(18, 1, 0, true, true); off.onMesh = false; c.push_back(off);
		CHECK(PickHold(c, 0, 0, 0, east, false, nobody, cfg) == 1);
		// nothing valid -> -1 (the caller falls back to standing inside the zone)
		std::vector<HoldCandidate> bad = {C(-17, 0, 0, true, true), C(5, 0, 0, true, true)};
		CHECK(PickHold(bad, 0, 0, 0, east, false, nobody, cfg) == -1);
		CHECK(PickHold({}, 0, 0, 0, east, false, nobody, cfg) == -1);
	}
	// tie: closer to the centre
	{
		std::vector<HoldCandidate> c = {C(19.5f, 0, 0, true, true), C(15.5f, 1, 0, true, true)};
		CHECK(PickHold(c, 0, 0, 0, east, false, nobody, cfg) == 1);
	}
	// p. 4 energy: a >= 2 m higher candidate wins over a better-scored low one; without energy the score wins
	{
		std::vector<HoldCandidate> c = {C(17, 0, 0, true, true) /*5*/, C(16, 3, 2.5f, false, false) /*2*/};
		CHECK(PickHold(c, 0, 0, 0, east, true, nobody, cfg) == 1);
		CHECK(PickHold(c, 0, 0, 0, east, false, nobody, cfg) == 0);
		std::vector<HoldCandidate> low = {C(17, 0, 0, true, true), C(16, 3, 1.5f, false, false)};   // 1.5 m: not "high"
		CHECK(PickHold(low, 0, 0, 0, east, true, nobody, cfg) == 0);
		// among high ones the score decides
		std::vector<HoldCandidate> hi = {C(17, 0, 2.2f, false, false), C(16, 3, 4.f, true, true)};
		CHECK(PickHold(hi, 0, 0, 0, east, true, nobody, cfg) == 1);
	}
	// p. 3 spacing: not within 6 m of another defender
	{
		std::vector<HoldCandidate> c = {C(16, 4, 0, true, true), C(17, -5, 0, false, true)};
		const std::vector<HoldPos> other = {HoldPos{15.f, 6.f, 0.f}};
		CHECK(PickHold(c, 0, 0, 0, east, false, other, cfg) == 1);
	}
	// p. 5 timing: period 20..40 s, a hit re-picks at once, no ping-pong without a hit
	{
		CHECK(std::fabs(HoldPeriod(0.f) - 20.f) < 1e-4f && std::fabs(HoldPeriod(1.f) - 40.f) < 1e-4f);
		CHECK(std::fabs(HoldPeriod(-3.f) - 20.f) < 1e-4f && std::fabs(HoldPeriod(7.f) - 40.f) < 1e-4f);
		HoldTimer t;
		CHECK(t.Due(0.f, -1e9f) == HoldReason::Start);
		t.Assign(10.f, HoldPeriod(0.5f));   // 30 s
		int changes = 0;
		for (float now = 10.f; now < 39.9f; now += 0.25f) if (t.Due(now, -1e9f) != HoldReason::None) ++changes;
		CHECK(changes == 0);
		CHECK(t.Due(40.f, -1e9f) == HoldReason::Rotate);
		t.Assign(40.f, 20.f);
		CHECK(t.Due(41.f, 41.f) == HoldReason::Hit);
		t.Assign(41.f, 20.f);
		CHECK(t.Due(42.f, 41.f) == HoldReason::None);   // the same hit does not re-pick twice
		CHECK(t.Due(45.f, 44.f) == HoldReason::Hit);
		CHECK(t.Due(45.5f, 44.f) == HoldReason::None);
		t.Reset(); CHECK(t.Due(50.f, 44.f) == HoldReason::Start);
		// property: over 600 s without hits at most one change per 20 s
		HoldTimer p; float last = -100.f; unsigned seed = 7u; int bad = 0;
		for (float now = 0.f; now < 600.f; now += 0.1f)
		{
			const HoldReason r = p.Due(now, -1e9f);
			if (r == HoldReason::None) continue;
			if (r != HoldReason::Start && now - last < 20.f - 1e-3f) ++bad;
			seed = seed * 1103515245u + 12345u; p.Assign(now, HoldPeriod(float((seed >> 16) & 0x7fff) / 32767.f)); last = now;
		}
		CHECK(bad == 0);
	}
	CHECK(std::string(HoldReasonName(HoldReason::Capture)) == "capture");
	std::printf(failures ? "BotPsHoldTests FAIL %d\n" : "BotPsHoldTests PASS\n", failures);
	return failures ? 1 : 0;
}
