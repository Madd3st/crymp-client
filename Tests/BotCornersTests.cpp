#include "CryGame/Bots/BotCorners.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace WarsBots;
static float D(float ax, float ay, float bx, float by) { return std::hypot(ax - bx, ay - by); }
int main()
{
	// Steelmill stair mouth (c068/c071): down the stair (south), turn east along the walkway; railing end post at
	// (1306.57, 1353.54) on the inner side. Raw corner is 0.35 m from the post, the moved one must clear 0.4 m.
	{
		const float a[2] = {1306.2f, 1356.4f}, c[2] = {1306.44f, 1353.20f}, n[2] = {1312.0f, 1353.0f};
		const CornerPlan p = OffsetCorner(a, c, n);
		assert(p.moved && p.turnDeg > 80 && p.turnDeg < 100);
		assert(D(c[0], c[1], 1306.57f, 1353.54f) < 0.4f);
		assert(D(p.x, p.y, 1306.57f, 1353.54f) > 0.45f);
		// the c071 wedge pose (1306.165, 1353.67) is neither near the moved point nor past the corner line
		const float wedge[2] = {1306.165f, 1353.67f};
		assert(!CornerReached(wedge, c, n, p));
		// standing at the moved point passes it; so does being 0.3 m along the walkway past the corner
		const float at[2] = {p.x + 0.1f, p.y}; assert(CornerReached(at, c, n, p));
		const float past[2] = {c[0] + 0.3f, c[1] - 0.5f}; bool plane = false; assert(CornerReached(past, c, n, p, 0.35f, 0.05f, &plane) && plane);
		assert(CornerSlow(p, 2.0f) && !CornerSlow(p, 3.0f));
	}
	// Spawn 176 (c071): spawn (1204.25,1365.0) -> corner (1204.94,1363.30) -> (1207.42,1363.17) around a box whose
	// corner is (1205.12, 1363.69). The next leg from the moved corner must pass the box face y=1363.69 by > 0.4 m.
	{
		const float a[2] = {1204.25f, 1365.0f}, c[2] = {1204.94f, 1363.30f}, n[2] = {1207.42f, 1363.17f};
		const CornerPlan p = OffsetCorner(a, c, n);
		assert(p.moved);
		assert(D(p.x, p.y, 1205.12f, 1363.69f) > 0.5f);
		assert(1363.69f - p.y > 0.45f);
		const float stuck[2] = {1204.59f, 1363.68f}; assert(!CornerReached(stuck, c, n, p)); // c071 stop pose: keep the corner
	}
	// Straight-ish paths are untouched; degenerate input is safe.
	{
		const float a[2] = {0, 0}, c[2] = {10, 0}, n[2] = {20, 0.5f};
		const CornerPlan p = OffsetCorner(a, c, n); assert(!p.moved && p.x == 10 && p.y == 0);
		const float z[2] = {10, 0}; const CornerPlan q = OffsetCorner(z, c, n); assert(!q.moved);
		const float back[2] = {0, 0}; const CornerPlan u = OffsetCorner(a, c, back); assert(u.moved && u.x > 10.1f && std::fabs(u.y) < 1e-4f); // hairpin around a wall end: aim beyond the tip
	}
	std::puts("BotCornersTests OK");
	return 0;
}
