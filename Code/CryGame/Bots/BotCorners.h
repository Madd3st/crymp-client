// Path-corner following for navmesh paths (wb_bot_corners v2, default OFF). Engine-free; Tests/BotCornersTests.cpp.
//
// Why: the navmesh is eroded by 0.3 m (the bake radius) while the player capsule is 0.4 m, so a Detour corner sits
// 0.3 m from the obstacle the path turns around and a bot steering through it rubs or wedges (Steelmill stair mouth
// 1306,1353: c068/c071; spawn 176 box corner 1205.1,1363.7: c071). Rebaking at 0.4 m drops the Gauss/DSG1 platforms.
// v1 (6be78151) pushed corners away from the nearest mesh edge and gated early passes by a mesh raycast: it failed
// (c071 stalls 37 -> 135) because the edge direction at a corner is ill-defined and "reached within 0.3 m" still let
// the bot cut the turn. v2 uses the turn itself:
//  - the obstacle is on the inner side of the turn, so both legs shift 0.15 m outward and the bot aims at their
//    intersection (the corner moves along the outer bisector by 0.15 / cos(turn/2), capped);
//  - a corner counts as passed when the bot is within 0.35 m of the moved point, or has crossed the line through the
//    corner perpendicular to the next leg (the caller also requires a clear mesh ray to the next corner);
//  - sharp turns (> 50 deg) within 2.5 m ask the caller to drop sprint/Speed so the bot does not overshoot.
#pragma once
#include <cmath>

namespace WarsBots {

struct CornerPlan { float x = 0, y = 0; float turnDeg = 0; bool moved = false; };

inline CornerPlan OffsetCorner(const float* prev, const float* corner, const float* next, float dist = 0.15f, float minTurnDeg = 10.0f)
{
	CornerPlan out; out.x = corner[0]; out.y = corner[1];
	float ux = corner[0] - prev[0], uy = corner[1] - prev[1], vx = next[0] - corner[0], vy = next[1] - corner[1];
	const float lu = std::sqrt(ux * ux + uy * uy), lv = std::sqrt(vx * vx + vy * vy);
	if (lu < 1e-3f || lv < 1e-3f) return out;
	ux /= lu; uy /= lu; vx /= lv; vy /= lv;
	const float c = ux * vx + uy * vy;
	out.turnDeg = std::acos(c < -1 ? -1.0f : (c > 1 ? 1.0f : c)) * 57.29578f;
	if (out.turnDeg < minTurnDeg) return out;
	// Shift both legs `dist` to the outer side of the turn and aim at their intersection: the obstacle the path bends
	// around is on the inner side, so both the corner and the next leg gain `dist` clearance.
	const float cross = ux * vy - uy * vx;
	const float s = cross >= 0 ? 1.0f : -1.0f;            // left turn: outer side is the right
	const float nux = s * uy, nuy = -s * ux, nvx = s * vy, nvy = -s * vx;
	float mx = nux + nvx, my = nuy + nvy; const float lm = std::sqrt(mx * mx + my * my);
	if (lm < 1e-4f) { mx = ux; my = uy; } else { mx /= lm; my /= lm; } // hairpin: aim beyond the tip
	const float k = mx * nux + my * nuy, len = dist / (k > 0.35f ? k : 0.35f);
	out.x += mx * len; out.y += my * len; out.moved = true;
	return out;
}

// True when the corner is done: within `reach` of the (moved) aim point, or past the line through the raw corner
// perpendicular to the next leg by more than `beyond` metres.
inline bool CornerReached(const float* feet, const float* corner, const float* next, const CornerPlan& plan, float reach = 0.35f, float beyond = 0.05f, bool* byPlane = nullptr)
{
	if (byPlane) *byPlane = false;
	const float dx = feet[0] - plan.x, dy = feet[1] - plan.y;
	if (dx * dx + dy * dy < reach * reach) return true;
	float vx = next[0] - corner[0], vy = next[1] - corner[1]; const float lv = std::sqrt(vx * vx + vy * vy);
	if (lv < 1e-3f) return false;
	vx /= lv; vy /= lv;
	if ((feet[0] - corner[0]) * vx + (feet[1] - corner[1]) * vy > beyond) { if (byPlane) *byPlane = true; return true; }
	return false;
}

inline bool CornerSlow(const CornerPlan& plan, float distToCorner, float turnDeg = 50.0f, float within = 2.5f)
{
	return plan.turnDeg > turnDeg && distToCorner < within;
}

}
