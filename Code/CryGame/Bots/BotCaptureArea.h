// DS-072: capture destination geometry (engine-free). The capturable entity pivot can sit outside its linked
// captureAreaId shape (Shore 579 is 13.3 m away), so the planner must aim at an interior point and hold on native
// membership. The adapter supplies a world-XY polygon plus the world Z range; the test uses the real Shore 569 shape.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

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

// Playtest 09.11 (Shore prototype 344): the most interior navmesh sample of a zone can sit on a navmesh piece that is
// cut off from both bases while another sample of the same zone 1.4 m away is on the main piece. Candidates are tried
// from the most interior one; a candidate within 'skip' metres of one that already failed is not tried again (samples
// of one cell snap to the same polygon). Returns the first index the reach test accepts, or -1 (no test passed or
// maxTests used up): the caller then keeps its old most-interior choice. Engine-free; the adapter supplies the test.
template<class Reach>
inline int PickReachableCandidate(const float* xyz, const float* edge, int count, float minEdge, int maxTests, float skip, Reach reach)
{
	if (!xyz || !edge || count <= 0 || maxTests <= 0) return -1;
	std::vector<int> order;
	for (int i = 0; i < count; ++i) if (edge[i] >= minEdge) order.push_back(i);
	std::stable_sort(order.begin(), order.end(), [edge](int a, int b) { return edge[a] > edge[b]; });
	std::vector<int> failed;
	int tests = 0;
	for (int i : order)
	{
		bool near = false;
		for (int f : failed)
		{
			const float dx = xyz[i * 3] - xyz[f * 3], dy = xyz[i * 3 + 1] - xyz[f * 3 + 1], dz = xyz[i * 3 + 2] - xyz[f * 3 + 2];
			if (dx * dx + dy * dy + dz * dz < skip * skip) { near = true; break; }
		}
		if (near) continue;
		if (tests++ >= maxTests) return -1;
		if (reach(xyz[i * 3], xyz[i * 3 + 1], xyz[i * 3 + 2])) return i;
		failed.push_back(i);
	}
	return -1;
}
}
