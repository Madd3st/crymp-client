// DS-072: capture destination geometry (engine-free). The capturable entity pivot can sit outside its linked
// captureAreaId shape (Shore 579 is 13.3 m away), so the planner must aim at an interior point and hold on native
// membership. The adapter supplies a world-XY polygon plus the world Z range; the test uses the real Shore 569 shape.
#pragma once
#include <algorithm>
#include <cmath>

namespace WarsBots
{
struct CaptureAreaPoly
{
	const float* xy = nullptr; // x0,y0,x1,y1,... in world XY
	int count = 0;
	float bottom = 0.f, top = 0.f;
};

inline bool CaptureAreaInside(const CaptureAreaPoly& a, float x, float y, float z)
{
	if (!a.xy || a.count < 3 || z < a.bottom - 0.01f || z > a.top + 0.01f) return false;
	bool inside = false;
	for (int i = 0, j = a.count - 1; i < a.count; j = i++)
	{
		const float xi = a.xy[i * 2], yi = a.xy[i * 2 + 1];
		const float xj = a.xy[j * 2], yj = a.xy[j * 2 + 1];
		if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) inside = !inside;
	}
	return inside;
}

inline float CaptureAreaEdgeDistance(const CaptureAreaPoly& a, float x, float y)
{
	if (!a.xy || a.count < 3) return 0.f;
	float best = 1e30f;
	for (int i = 0, j = a.count - 1; i < a.count; j = i++)
	{
		const float ax = a.xy[j * 2], ay = a.xy[j * 2 + 1];
		const float bx = a.xy[i * 2], by = a.xy[i * 2 + 1];
		const float dx = bx - ax, dy = by - ay;
		const float len2 = dx * dx + dy * dy;
		float t = 0.f;
		if (len2 > 1e-12f) t = std::max(0.f, std::min(1.f, ((x - ax) * dx + (y - ay) * dy) / len2));
		const float px = ax + dx * t - x, py = ay + dy * t - y;
		best = std::min(best, std::sqrt(px * px + py * py));
	}
	return best;
}

// Runtime acceptance: inside the native shape and with a small margin, never the entity pivot.
inline bool CaptureDestinationValid(const CaptureAreaPoly& a, float x, float y, float z, float minEdge)
{
	return CaptureAreaInside(a, x, y, z) && CaptureAreaEdgeDistance(a, x, y) >= minEdge;
}

// DS-072 binding rule: captureAreaId is a LOCAL area number (1), so a shape qualifies only when both the local
// number matches and the level XML binds this capturable entity to it. Several shapes may bind one factory.
struct CaptureShapeBinding
{
	int entityId = 0;      // verified AreaShape map EntityId
	int localId = 0;       // <Area Id>
	const int* targets = nullptr; // <Entities><Entity Id=.../></Entities>
	int targetCount = 0;
};

inline bool CaptureShapeBinds(const CaptureShapeBinding& s, int capturableId, int localAreaId)
{
	if (!s.entityId || s.localId != localAreaId) return false;
	for (int i = 0; i < s.targetCount; ++i)
		if (s.targets[i] == capturableId) return true;
	return false;
}
}
