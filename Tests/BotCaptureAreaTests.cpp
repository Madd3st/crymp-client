// DS-072: captureAreaId is a LOCAL area number (1) shared by every capturable, so a shape qualifies only when the
// level XML also binds that capturable entity to it. Real Shore geometry: six AreaShapes, all <Area Id="1">, each
// binding one target (344 binds two). The regression fails if the local id alone is used, if a pivot is a
// destination, or if a shape is bound to the wrong target.
#include "CryGame/Bots/BotCaptureArea.h"
#include <cassert>
#include <cstdio>

using namespace WarsBots;

// World XY polygons from Shore.area-shapes.json (extracted mission_mission0.xml).
static const float kShape629[] = { 1777.875000, 1708.250000, 1774.125000, 1704.500001, 1774.124996, 1677.000003, 1777.874995, 1673.250003, 1819.124991, 1673.249998, 1822.874992, 1676.999997, 1822.874995, 1704.499994, 1819.124996, 1708.249994 };
static const float kShape569[] = { 2224.737800, 1350.584000, 2209.340199, 1335.510932, 2218.850392, 1326.010412, 2225.002976, 1332.110836, 2219.257730, 1337.856075, 2232.957916, 1351.733053, 2226.593951, 1358.097011, 2222.086148, 1353.589202, 2224.775435, 1349.883799 };
static const float kShape330[] = { 2142.321500, 1489.263500, 2142.321498, 1484.013500, 2144.946498, 1484.013500, 2144.946496, 1476.513500, 2130.071497, 1476.513505, 2125.446499, 1481.013506, 2125.321500, 1484.388506, 2130.196501, 1489.263504 };
static const float kShape39[] = { 1943.000000, 1366.250000, 1946.750000, 1362.500000, 1974.250000, 1362.499996, 1978.000000, 1366.249996, 1978.000005, 1407.499996, 1974.250005, 1411.249996, 1946.750005, 1411.250000, 1943.000005, 1407.500000 };
static const float kShape291[] = { 2080.068100, 1315.789300, 2074.816410, 1316.527377, 2052.853930, 1299.977463, 2052.115852, 1294.725773, 2076.940724, 1261.782052, 2082.192414, 1261.043975, 2104.154894, 1277.593889, 2104.892971, 1282.845580 };
static const float kShape323[] = { 2144.946500, 1483.888500, 2145.571500, 1483.888500, 2145.571500, 1485.638500, 2144.821501, 1485.638500, 2144.821504, 1497.763499, 2155.696503, 1497.763495, 2155.696497, 1476.638497, 2144.946498, 1476.513501 };

static const int kT64[] = { 64 };
static const int kT316[] = { 316 };
static const int kT654[] = { 654 };
static const int kT344[] = { 344 };
static const int kT579[] = { 579 };

struct Shape
{
	int entityId;
	const float* xy;
	int count;
	float bottom, top;
	const int* targets;
	int targetCount;
};

static const Shape kShapes[] = {
	{ 629, kShape629, 8, 92.25f, 130.25f, kT654, 1 },
	{ 569, kShape569, 9, 123.32f, 129.32f, kT579, 1 },
	{ 330, kShape330, 8, 110.75f, 114.75f, kT344, 1 },
	{ 39, kShape39, 8, 94.50f, 132.50f, kT64, 1 },
	{ 291, kShape291, 8, 115.68465f, 153.68465f, kT316, 1 },
	{ 323, kShape323, 8, 110.75f, 117.75f, kT344, 1 },
};
static const int kShapeCount = int(sizeof(kShapes) / sizeof(kShapes[0]));

static CaptureShapeBinding Binding(const Shape& s)
{
	CaptureShapeBinding b;
	b.entityId = s.entityId; b.localId = 1; b.targets = s.targets; b.targetCount = s.targetCount;
	return b;
}

static CaptureAreaPoly Poly(const Shape& s)
{
	CaptureAreaPoly a;
	a.xy = s.xy; a.count = s.count; a.bottom = s.bottom; a.top = s.top;
	return a;
}

static int MatchingShapes(int capturableId)
{
	int n = 0;
	for (int i = 0; i < kShapeCount; ++i)
		if (CaptureShapeBinds(Binding(kShapes[i]), capturableId, 1)) ++n;
	return n;
}

static int MatchEntity(int capturableId)
{
	for (int i = 0; i < kShapeCount; ++i)
		if (CaptureShapeBinds(Binding(kShapes[i]), capturableId, 1)) return kShapes[i].entityId;
	return 0;
}

int main()
{
	// All six shapes carry the same local area number: a local-id-only match would return the first shape (629) for
	// every target; only the explicit target list distinguishes them.
	for (int i = 0; i < kShapeCount; ++i) assert(Binding(kShapes[i]).localId == 1);
	assert(MatchEntity(579) == 569); // not 629, which is the first shape with localId 1

	// Explicit target binding, with 344 bound to two shapes.
	assert(MatchEntity(64) == 39 && MatchingShapes(64) == 1);
	assert(MatchEntity(316) == 291 && MatchingShapes(316) == 1);
	assert(MatchEntity(654) == 629 && MatchingShapes(654) == 1);
	assert(MatchEntity(579) == 569 && MatchingShapes(579) == 1);
	assert(MatchingShapes(344) == 2);
	{
		bool has330 = false, has323 = false;
		for (int i = 0; i < kShapeCount; ++i) if (CaptureShapeBinds(Binding(kShapes[i]), 344, 1))
		{
			has330 = has330 || kShapes[i].entityId == 330;
			has323 = has323 || kShapes[i].entityId == 323;
		}
		assert(has330 && has323);
	}

	// Wrong local number and unknown target fail closed.
	assert(!CaptureShapeBinds(Binding(kShapes[0]), 64, 2));
	assert(!CaptureShapeBinds(Binding(kShapes[0]), 579, 1));

	// 579: its pivot is 13.3 m outside shape 569 and must never be a destination; a real inside floor is valid.
	const CaptureAreaPoly s569 = Poly(kShapes[1]);
	assert(!CaptureAreaInside(s569, 2236.76f, 1325.84f, 124.32f));
	assert(!CaptureDestinationValid(s569, 2236.76f, 1325.84f, 124.32f, 0.25f));
	assert(CaptureDestinationValid(s569, 2212.5686f, 1335.2388f, 124.5113f, 0.25f));

	// Real inside-area floor representatives are inside their own bound shape (never the pivot).
	assert(CaptureAreaInside(Poly(kShapes[0]), 1774.875f, 1678.0f, 101.345299f)); // 654 -> 629
	assert(CaptureAreaInside(Poly(kShapes[2]), 2126.071533f, 1481.26355f, 111.111298f)); // 344 -> 330
	assert(CaptureAreaInside(Poly(kShapes[3]), 1943.75f, 1367.25f, 105.356186f)); // 64 -> 39
	assert(CaptureAreaInside(Poly(kShapes[4]), 2054.865967f, 1297.793945f, 125.149033f)); // 316 -> 291

	// Height is part of membership, and a degenerate polygon fails closed.
	assert(!CaptureAreaInside(s569, 2212.5686f, 1335.2388f, 130.32f));
	CaptureAreaPoly empty;
	assert(!CaptureDestinationValid(empty, 2212.5686f, 1335.2388f, 124.5113f, 0.f));

	std::puts("BotCaptureAreaTests OK");
	return 0;
}
