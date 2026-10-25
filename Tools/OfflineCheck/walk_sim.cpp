// Offline capsule walk simulator (D-015 step 2). No game: a kinematic player capsule (radius 0.4, body 0.5..1.9 m
// above the feet, from player.lua) follows navmesh paths through the map's collision triangles and reports where it
// gets stuck. Compares path-following rules before they reach a Codex batch:
//   legacy   : corner passed within 1.5 m (BotBrain::Steer without wb_bot_corners), sprint 6.3 m/s when > 3 m away
//   corners  : wb_bot_corners v2 (BotCorners.h): outer-shifted aim, reached/crossed, no sprint into sharp turns
// Usage: walk_sim <map.navmesh> <map.links.txt> <collision.obj> <map.pts> [mode legacy|corners|both] [maxPairs]
// Output: per mode routes, done, stuck, median time; stuck clusters (2 m) with counts; JSON-ish lines for tooling.
// Simplifications: no acceleration/turn rate, ground height from the navmesh, links are executed as instant hops
// (BotJumpLink is tested in game), no other bots. It finds geometry wedges, not combat behaviour.
#include "CryGame/Bots/BotNavMeshCore.h"
#include "CryGame/Bots/BotCorners.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <array>

using namespace WarsBots;

struct Tri { float a[3], b[3], c[3]; float lo[3], hi[3]; };
static std::vector<Tri> g_tris;
static std::map<long long, std::vector<int>> g_grid; // 2 m cells
static const float kCell = 2.0f;
static long long Key(int x, int y) { return ((long long)x << 32) ^ (unsigned)y; }

static bool LoadObj(const char* path, float xmin, float xmax, float ymin, float ymax)
{
	FILE* f = std::fopen(path, "rb"); if (!f) return false;
	std::vector<float> v; char line[512];
	while (std::fgets(line, sizeof line, f))
	{
		if (line[0] == 'v' && line[1] == ' ') { float x, y, z; std::sscanf(line + 2, "%f %f %f", &x, &y, &z); v.push_back(x); v.push_back(y); v.push_back(z); }
		else if (line[0] == 'f' && line[1] == ' ')
		{
			int i[3]; if (std::sscanf(line + 2, "%d %d %d", &i[0], &i[1], &i[2]) != 3) continue;
			Tri t; float* p[3] = {t.a, t.b, t.c};
			for (int k = 0; k < 3; ++k) for (int d = 0; d < 3; ++d) p[k][d] = v[(i[k] - 1) * 3 + d];
			for (int d = 0; d < 3; ++d) { t.lo[d] = std::min(t.a[d], std::min(t.b[d], t.c[d])); t.hi[d] = std::max(t.a[d], std::max(t.b[d], t.c[d])); }
			if (t.hi[0] < xmin || t.lo[0] > xmax || t.hi[1] < ymin || t.lo[1] > ymax) continue;
			const int id = (int)g_tris.size(); g_tris.push_back(t);
			for (int gx = (int)std::floor(t.lo[0] / kCell); gx <= (int)std::floor(t.hi[0] / kCell); ++gx)
				for (int gy = (int)std::floor(t.lo[1] / kCell); gy <= (int)std::floor(t.hi[1] / kCell); ++gy) g_grid[Key(gx, gy)].push_back(id);
		}
	}
	std::fclose(f); return true;
}

// closest point on triangle (Ericson, Real-Time Collision Detection 5.1.5)
static void Closest(const float* p, const Tri& t, float* out)
{
	auto sub = [](const float* a, const float* b, float* r) { r[0] = a[0] - b[0]; r[1] = a[1] - b[1]; r[2] = a[2] - b[2]; };
	auto dot = [](const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
	float ab[3], ac[3], ap[3]; sub(t.b, t.a, ab); sub(t.c, t.a, ac); sub(p, t.a, ap);
	float d1 = dot(ab, ap), d2 = dot(ac, ap);
	if (d1 <= 0 && d2 <= 0) { std::memcpy(out, t.a, 12); return; }
	float bp[3]; sub(p, t.b, bp); float d3 = dot(ab, bp), d4 = dot(ac, bp);
	if (d3 >= 0 && d4 <= d3) { std::memcpy(out, t.b, 12); return; }
	float vc = d1 * d4 - d3 * d2;
	if (vc <= 0 && d1 >= 0 && d3 <= 0) { float v = d1 / (d1 - d3); for (int k = 0; k < 3; ++k) out[k] = t.a[k] + v * ab[k]; return; }
	float cp[3]; sub(p, t.c, cp); float d5 = dot(ab, cp), d6 = dot(ac, cp);
	if (d6 >= 0 && d5 <= d6) { std::memcpy(out, t.c, 12); return; }
	float vb = d5 * d2 - d1 * d6;
	if (vb <= 0 && d2 >= 0 && d6 <= 0) { float w = d2 / (d2 - d6); for (int k = 0; k < 3; ++k) out[k] = t.a[k] + w * ac[k]; return; }
	float va = d3 * d6 - d5 * d4;
	if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) { float w = (d4 - d3) / ((d4 - d3) + (d5 - d6)); for (int k = 0; k < 3; ++k) out[k] = t.b[k] + w * (t.c[k] - t.b[k]); return; }
	float den = 1.0f / (va + vb + vc), v = vb * den, w = vc * den;
	for (int k = 0; k < 3; ++k) out[k] = t.a[k] + ab[k] * v + ac[k] * w;
}

// Push the capsule (spheres r at feet+0.9/1.2/1.5) out of walls horizontally. Returns total push length.
static float Resolve(float* feet, float r = 0.4f)
{
	float total = 0;
	for (int it = 0; it < 4; ++it)
	{
		float push[2] = {0, 0}; int n = 0;
		const int gx0 = (int)std::floor((feet[0] - 1) / kCell), gx1 = (int)std::floor((feet[0] + 1) / kCell);
		const int gy0 = (int)std::floor((feet[1] - 1) / kCell), gy1 = (int)std::floor((feet[1] + 1) / kCell);
		for (int gx = gx0; gx <= gx1; ++gx) for (int gy = gy0; gy <= gy1; ++gy)
		{
			auto itc = g_grid.find(Key(gx, gy)); if (itc == g_grid.end()) continue;
			for (int id : itc->second)
			{
				const Tri& t = g_tris[id];
				if (t.hi[2] < feet[2] + 0.5f || t.lo[2] > feet[2] + 1.9f) continue;
				if (t.hi[0] < feet[0] - r || t.lo[0] > feet[0] + r || t.hi[1] < feet[1] - r || t.lo[1] > feet[1] + r) continue;
				for (float h : {0.9f, 1.2f, 1.5f})
				{
					const float c[3] = {feet[0], feet[1], feet[2] + h}; float q[3]; Closest(c, t, q);
					const float dx = c[0] - q[0], dy = c[1] - q[1], dz = c[2] - q[2], d = std::sqrt(dx * dx + dy * dy + dz * dz);
					const float hl = std::sqrt(dx * dx + dy * dy);
					if (d < r && hl > 1e-3f && hl > 0.35f * d) { push[0] += dx / hl * (r - d); push[1] += dy / hl * (r - d); ++n; }
				}
			}
		}
		if (!n) break;
		const float pl = std::sqrt(push[0] * push[0] + push[1] * push[1]); if (pl < 1e-4f) break;
		const float s = std::min(pl, 0.1f) / pl; feet[0] += push[0] * s; feet[1] += push[1] * s; total += pl * s;
	}
	return total;
}

// Moller-Trumbore segment test
static bool SegTri(const float* o, const float* d, const Tri& t)
{
	float e1[3], e2[3], p[3], q[3], s[3];
	for (int k = 0; k < 3; ++k) { e1[k] = t.b[k] - t.a[k]; e2[k] = t.c[k] - t.a[k]; s[k] = o[k] - t.a[k]; }
	p[0] = d[1] * e2[2] - d[2] * e2[1]; p[1] = d[2] * e2[0] - d[0] * e2[2]; p[2] = d[0] * e2[1] - d[1] * e2[0];
	const float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2]; if (std::fabs(det) < 1e-9f) return false;
	const float inv = 1 / det, u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv; if (u < 0 || u > 1) return false;
	q[0] = s[1] * e1[2] - s[2] * e1[1]; q[1] = s[2] * e1[0] - s[0] * e1[2]; q[2] = s[0] * e1[1] - s[1] * e1[0];
	const float v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv; if (v < 0 || u + v > 1) return false;
	const float tt = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv; return tt >= 0 && tt <= 1;
}
static bool RayHit(const float* o, const float* d)
{
	const float ex[2] = {std::min(o[0], o[0] + d[0]), std::max(o[0], o[0] + d[0])}, ey[2] = {std::min(o[1], o[1] + d[1]), std::max(o[1], o[1] + d[1])};
	for (int gx = (int)std::floor(ex[0] / kCell); gx <= (int)std::floor(ex[1] / kCell); ++gx)
		for (int gy = (int)std::floor(ey[0] / kCell); gy <= (int)std::floor(ey[1] / kCell); ++gy)
		{
			auto it = g_grid.find(Key(gx, gy)); if (it == g_grid.end()) continue;
			for (int id : it->second) { const Tri& t = g_tris[id]; if (t.hi[0] < ex[0] || t.lo[0] > ex[1] || t.hi[1] < ey[0] || t.lo[1] > ey[1]) continue; if (SegTri(o, d, t)) return true; }
		}
	return false;
}
// BotMovement::SafeStep: 3 x 3 body rays `probe` m ahead (knee ray skipped on mesh paths); drop check omitted.
static bool g_slopeRays = false;
static bool SafeStep(const float* feet, float dx, float dy, float probe, float slope = 0)
{
	for (float side : {-0.3f, 0.0f, 0.3f}) for (float h : {1.1f, 1.6f})
	{
		const float o[3] = {feet[0] - dy * side, feet[1] + dx * side, feet[2] + h}, d[3] = {dx * probe, dy * probe, g_slopeRays ? slope * probe : 0};
		if (RayHit(o, d)) return false;
	}
	return true;
}
static int g_moveModel = 1; // 1: model BotMovement (arrival stop, SafeStep fan, full stop); 0: ideal follower

struct Result { bool done = false; float t = 0; float stuck[3] = {0, 0, 0}; };

static bool g_trace = false; static bool g_all = getenv("WALK_ALL") != nullptr;
static Result Walk(NavMeshCore& mesh, const float* start, const float* goal, bool corners, bool fixedArrive = true)
{
	Result res;
	std::vector<float> xyz; std::vector<NavPathLink> links; const char* why = nullptr;
	if (!mesh.FindPathLinks(start, goal, xyz, links, &why)) return res;
	std::vector<std::array<float, 3>> path; for (size_t i = 0; i + 2 < xyz.size(); i += 3) path.push_back({xyz[i], xyz[i + 1], xyz[i + 2]});
	auto linkAt = [&](size_t c) { for (auto& l : links) if ((size_t)l.corner == c) return true; return false; };
	float feet[3]; if (!mesh.Project(start, feet)) return res;
	float pstart[3] = {feet[0], feet[1], feet[2]};
	size_t idx = 0; size_t planFor = (size_t)-1; CornerPlan plan;
	const float dt = 0.05f; float progT = 0, bestD = 1e9f;
	float sampleT = 0, sx = feet[0], sy = feet[1], avoidUntil = -1, side = 1; // BotMovement m_sampleTime/m_avoidTime/m_side
	for (float t = 0; t < 120.0f; t += dt)
	{
		const float gd = std::hypot(goal[0] - feet[0], goal[1] - feet[1]);
		const bool atEnd = idx + 1 >= path.size() && std::hypot(path.back()[0] - feet[0], path.back()[1] - feet[1]) < 0.95f; // Move stops 0.8 m short
		if ((gd < 0.6f && std::fabs(goal[2] - feet[2]) < 2.5f) || atEnd) { res.done = true; res.t = t; return res; }
		// advance
		bool slow = false; float aim[2];
		if (!corners)
		{
			while (idx + 1 < path.size() && !linkAt(idx) && std::hypot(path[idx][0] - feet[0], path[idx][1] - feet[1]) < 1.5f && std::fabs(path[idx][2] - feet[2]) < 2.0f) ++idx;
			aim[0] = path[idx][0]; aim[1] = path[idx][1];
		}
		else
		{
			for (;;)
			{
				if (idx + 1 >= path.size() || linkAt(idx)) break;
				const float* c = path[idx].data(); const float* n = path[idx + 1].data();
				if (planFor != idx)
				{
					const float* a = idx ? path[idx - 1].data() : pstart;
					planFor = idx; plan = OffsetCorner(a, c, n);
					if (plan.moved) { const float q[3] = {plan.x, plan.y, c[2]}; float s[3]; if (!mesh.Project(q, s, 0.2f, 0.6f, 0.6f) || std::hypot(s[0] - q[0], s[1] - q[1]) > 0.05f) { plan.x = c[0]; plan.y = c[1]; plan.moved = false; } }
				}
				if (std::fabs(c[2] - feet[2]) >= 2.0f) break;
				bool byPlane = false;
				if (!CornerReached(feet, c, n, plan, 0.35f, 0.05f, &byPlane)) break;
				if (byPlane && !mesh.RayClear(feet, n)) break;
				++idx;
			}
			if (idx + 1 < path.size() && !linkAt(idx) && planFor == idx)
			{
				aim[0] = plan.x; aim[1] = plan.y;
				CornerPlan turn = plan;
				if (idx + 2 < path.size() && std::hypot(path[idx + 1][0] - path[idx][0], path[idx + 1][1] - path[idx][1]) < 1.5f)
				{ const float pc[2] = {path[idx][0], path[idx][1]}, pn[2] = {path[idx + 1][0], path[idx + 1][1]}, pnn[2] = {path[idx + 2][0], path[idx + 2][1]}; turn.turnDeg += OffsetCorner(pc, pn, pnn).turnDeg; }
				slow = CornerSlow(turn, std::hypot(path[idx][0] - feet[0], path[idx][1] - feet[1]));
			}
			else { aim[0] = path[idx][0]; aim[1] = path[idx][1]; }
		}
		// link take-off: hop to the landing when close
		if (linkAt(idx) && std::hypot(path[idx][0] - feet[0], path[idx][1] - feet[1]) < 0.6f && idx + 1 < path.size())
		{ ++idx; feet[0] = path[idx][0]; feet[1] = path[idx][1]; feet[2] = path[idx][2]; planFor = (size_t)-1; continue; }
		const float dx = aim[0] - feet[0], dy = aim[1] - feet[1], d = std::hypot(dx, dy);
		const float speed = (!slow && d > 3.0f) ? 6.3f : 4.5f;
		float mx = d > 1e-3f ? dx / d : 0, my = d > 1e-3f ? dy / d : 0; bool go = d > 1e-3f;
		if (g_moveModel)
		{
			// BotMovement::Move: stop within `arrive` of the destination; body-ray fan; full stop when all blocked
			const float arrive = (corners && fixedArrive) ? 0.2f : 0.8f;
			if (d < arrive) go = false;
			else
			{
				float probe = speed > 5 ? 2.7f : 1.5f;
				if (corners && fixedArrive) probe = std::min(probe, d + 0.3f);
				// slope of the leg toward the aim (mesh corners carry z): stairs rise ~1 m per 1.5 m
				float az = idx < path.size() ? path[idx][2] : feet[2];
				const float slope = std::max(-1.0f, std::min(1.0f, (az - feet[2]) / std::max(d, 0.5f)));
				if (t - sampleT >= 2.0f) { if (std::hypot(feet[0] - sx, feet[1] - sy) < 0.3f) { avoidUntil = t + 1.2f; side = -side; } sampleT = t; sx = feet[0]; sy = feet[1]; }
				if (t < avoidUntil || !SafeStep(feet, mx, my, probe, slope))
				{
					go = false;
					for (float a0 : {0.8f, -0.8f, 1.57f, -1.57f, 2.36f, -2.36f, 3.14159f})
					{ const float ang = a0 == 3.14159f ? a0 : a0 * side; const float cx = mx * std::cos(ang) - my * std::sin(ang), cy = mx * std::sin(ang) + my * std::cos(ang); if (SafeStep(feet, cx, cy, probe, 0)) { mx = cx; my = cy; go = true; break; } }
				}
			}
		}
		if (go) { const float step = std::min(std::max(d, 0.3f), speed * dt); feet[0] += mx * step; feet[1] += my * step; }
		const float pushed = Resolve(feet);
		if (g_trace && std::fmod(t, 0.5f) < dt) std::printf("    t=%.1f feet=(%.2f,%.2f,%.2f) idx=%zu/%zu aim=(%.2f,%.2f) push=%.2f\n", t, feet[0], feet[1], feet[2], idx, path.size(), aim[0], aim[1], pushed);
		float s[3]; if (mesh.Project(feet, s, 0.6f, 0.7f, 1.5f) && std::fabs(s[2] - feet[2]) < 1.2f) feet[2] = s[2];
		// progress: remaining path length must shrink by 0.3 m within 3 s
		float rem = std::hypot(path[idx][0] - feet[0], path[idx][1] - feet[1]);
		for (size_t k = idx + 1; k < path.size(); ++k) rem += std::hypot(path[k][0] - path[k - 1][0], path[k][1] - path[k - 1][1]);
		if (rem < bestD - 0.3f) { bestD = rem; progT = t; }
		else if (t - progT > 3.0f) { std::memcpy(res.stuck, feet, 12); res.t = t; return res; }
	}
	std::memcpy(res.stuck, feet, 12); res.t = 120; return res;
}

int main(int argc, char** argv)
{
	if (argc < 5) { std::fprintf(stderr, "usage: walk_sim map.navmesh map.links.txt collision.obj map.pts [legacy|corners|both] [maxPairs]\n"); return 2; }
	const std::string mode = argc > 5 ? argv[5] : "both"; const int maxPairs = argc > 6 ? std::atoi(argv[6]) : 100000;
	NavMeshCore mesh; std::string err;
	if (!mesh.LoadFile(argv[1], &err)) { std::fprintf(stderr, "navmesh: %s\n", err.c_str()); return 1; }
	{ std::string txt; if (FILE* f = std::fopen(argv[2], "rb")) { int c; while ((c = std::fgetc(f)) != EOF) txt.push_back((char)c); std::fclose(f); } mesh.LoadLinkKinds(txt.c_str()); mesh.SetLinkMask(NavLinkFlag(NavLinkDrop) | NavLinkFlag(NavLinkJump) | NavLinkFlag(NavLinkStrength)); } // as wb_bot_jump_links 1 (DeepSeek S1: drop was missing)
	std::vector<std::array<float, 3>> spawns, items;
	float xmin = 1e9, xmax = -1e9, ymin = 1e9, ymax = -1e9;
	if (FILE* f = std::fopen(argv[4], "r")) { char k[32]; int id; float x, y, z; while (std::fscanf(f, "%31s %d %f %f %f", k, &id, &x, &y, &z) == 5) { (std::strcmp(k, "spawn") ? items : spawns).push_back({x, y, z}); xmin = std::min(xmin, x); xmax = std::max(xmax, x); ymin = std::min(ymin, y); ymax = std::max(ymax, y); } std::fclose(f); }
	if (!LoadObj(argv[3], xmin - 60, xmax + 60, ymin - 60, ymax + 60)) { std::fprintf(stderr, "obj?\n"); return 1; }
	std::fprintf(stderr, "tris %zu spawns %zu items %zu\n", g_tris.size(), spawns.size(), items.size());
	std::vector<std::pair<int, int>> pairs;
	for (size_t s = 0; s < spawns.size(); ++s) for (size_t i = 0; i < items.size(); ++i) if (mesh.Reachable(spawns[s].data(), items[i].data())) pairs.push_back({(int)s, (int)i});
	if ((int)pairs.size() > maxPairs) { std::vector<std::pair<int, int>> sub; for (int k = 0; k < maxPairs; ++k) sub.push_back(pairs[(size_t)k * pairs.size() / maxPairs]); pairs.swap(sub); }
	if (argc > 7) { g_trace = true; const int k = std::atoi(argv[7]); auto& p = pairs[k]; std::printf("trace pair %d spawn (%.1f,%.1f,%.1f) item (%.1f,%.1f,%.1f)\n", k, spawns[p.first][0], spawns[p.first][1], spawns[p.first][2], items[p.second][0], items[p.second][1], items[p.second][2]); Result r = Walk(mesh, spawns[p.first].data(), items[p.second].data(), mode == "corners"); std::printf("done=%d t=%.1f\n", r.done, r.t); return 0; }
	if (mode == "both")
	{
		std::vector<float> ratio;
		for (auto& p : pairs)
		{
			const Result a = Walk(mesh, spawns[p.first].data(), items[p.second].data(), false);
			const Result b = Walk(mesh, spawns[p.first].data(), items[p.second].data(), true);
			if (a.done && b.done && a.t > 0.5f) ratio.push_back(b.t / a.t);
		}
		std::sort(ratio.begin(), ratio.end());
		if (!ratio.empty()) std::printf("TIME corners/legacy on routes both finish: n=%zu median=%.2f p90=%.2f\n", ratio.size(), ratio[ratio.size() / 2], ratio[ratio.size() * 9 / 10]);
	}
	if (getenv("WALK_IDEAL")) g_moveModel = 0;
	if (getenv("WALK_SLOPE")) g_slopeRays = true;
	for (int m = 0; m < 3; ++m)
	{
		const bool corners = m >= 1, fixedArrive = m == 1;
		const char* mname = m == 0 ? "legacy" : (m == 1 ? "corners" : "corners_716");
		if (mode != "both" && mode != "all" && mode != mname) continue;
		if (mode == "both" && m == 2) continue;
		int done = 0, stuck = 0, nopath = 0; std::vector<float> times; std::vector<std::array<float, 3>> st;
		for (auto& p : pairs)
		{
			const Result r = Walk(mesh, spawns[p.first].data(), items[p.second].data(), corners, fixedArrive);
			if (r.done) { ++done; times.push_back(r.t); }
			else if (r.t == 0) ++nopath;
			else { ++stuck; st.push_back({r.stuck[0], r.stuck[1], r.stuck[2]}); if (getenv("WALK_PAIRS")) std::printf("  PAIR %d stuck (%.1f,%.1f,%.1f)\n", (int)(&p - &pairs[0]), r.stuck[0], r.stuck[1], r.stuck[2]); }
		}
		std::sort(times.begin(), times.end());
		std::printf("MODE %s routes=%zu done=%d stuck=%d nopath=%d median_s=%.1f\n", mname, pairs.size(), done, stuck, nopath, times.empty() ? 0.f : times[times.size() / 2]);
		// clusters within 2 m
		std::vector<int> used(st.size(), 0); std::vector<std::pair<int, std::array<float, 3>>> cl;
		for (size_t i = 0; i < st.size(); ++i) if (!used[i])
		{
			int n = 0; float sx = 0, sy = 0, sz = 0;
			for (size_t j = i; j < st.size(); ++j) if (!used[j] && std::hypot(st[j][0] - st[i][0], st[j][1] - st[i][1]) < 2 && std::fabs(st[j][2] - st[i][2]) < 2) { used[j] = 1; ++n; sx += st[j][0]; sy += st[j][1]; sz += st[j][2]; }
			cl.push_back({n, {sx / n, sy / n, sz / n}});
		}
		std::sort(cl.begin(), cl.end(), [](auto& a, auto& b) { return a.first > b.first; });
		for (size_t k = 0; k < cl.size() && k < (g_all ? cl.size() : 15); ++k) std::printf("  STUCK %3d at (%.1f, %.1f, %.1f)\n", cl[k].first, cl[k].second[0], cl[k].second[1], cl[k].second[2]);
	}
	return 0;
}
