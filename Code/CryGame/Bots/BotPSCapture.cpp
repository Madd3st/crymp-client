// DS-048 #3: PS-1 capture goal data. Caches the capturable entities once per level (class, index, capture time,
// destination) and refreshes only their owner team, which changes during the match.
//
// DS-072: Properties.captureAreaId is a LOCAL area number (1), not an entity id. The linked shape is found from the
// level's mission_mission0.xml: an AreaShape entity whose <Area Id> equals captureAreaId AND whose <Entities> list
// contains this capturable entity id. Several shapes may bind one factory (Shore 344 -> 323+330). The destination is
// the most interior sample of the live native area that is on the navmesh; the entity pivot is never a destination.
#include "PortIncludes.h"
#include "CryGame/Bots/BotPSCaptureRuntime.h"
#include "CryGame/Bots/BotCaptureArea.h"
#include "CryGame/Bots/BotNavMesh.h"
#include "CryGame/Bots/BotNavMeshCore.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotConfig.h"
#include "CryGame/Bots/BotTeamOrders.h"
#include "CryGame/Bots/BotManager.h"
#include "CryGame/Bots/BotInstance.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryCommon/CryAction/ILevelSystem.h"
#include "CryCommon/CryEntitySystem/IEntityProxy.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IXml.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace WarsBots {
namespace {

struct AreaShapeInfo
{
 int entityId=0,localId=0;
 Vec3 pos=Vec3(0,0,0);
 std::vector<int> targets;
};

const std::vector<AreaShapeInfo>& LevelAreaShapes()
{
 static std::string cachedLevel;
 static std::vector<AreaShapeInfo> shapes;
 if(!g_pGame||!g_pGame->GetIGameFramework()||!gEnv||!gEnv->pSystem)return shapes;
 ILevel* level=g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel();
 const ILevelInfo* info=level?level->GetLevelInfo():nullptr;
 if(!info||!info->GetPath())return shapes;
 const std::string name=info->GetName()?info->GetName():"";
 if(cachedLevel==name&&!shapes.empty())return shapes;
 cachedLevel=name;shapes.clear();
 const ILevelInfo::TGameTypeInfo* mission=info->GetDefaultGameType();
 std::string file=info->GetPath();if(!file.empty()&&file.back()!='/')file+='/';
 file+=mission&&!mission->xmlFile.empty()?mission->xmlFile.c_str():"mission_mission0.xml";
 // Use the engine XML reader through mounted CryPak: no private XML grammar or absolute install path.
 XmlNodeRef root=gEnv->pSystem->LoadXmlFile(file.c_str());
 XmlNodeRef objects=root?root->findChild("Objects"):XmlNodeRef();
 if(!objects){WB_LOG(0,"[WarsBots] PS CAPTURE no mission Objects in %s.",file.c_str());return shapes;}
 for(int i=0;i<objects->getChildCount();++i){
  XmlNodeRef entity=objects->getChild(i);
  if(std::strcmp(entity->getTag(),"Entity")||std::strcmp(entity->getAttr("EntityClass"),"AreaShape"))continue;
  XmlNodeRef area=entity->findChild("Area");if(!area)continue;
  AreaShapeInfo binding;entity->getAttr("EntityId",binding.entityId);entity->getAttr("Pos",binding.pos);area->getAttr("Id",binding.localId);
  XmlNodeRef targets=area->findChild("Entities");if(!targets)continue;
  for(int j=0;j<targets->getChildCount();++j){int id=0;XmlNodeRef target=targets->getChild(j);if(!std::strcmp(target->getTag(),"Entity")&&target->getAttr("Id",id)&&id)binding.targets.push_back(id);}
  if(binding.entityId&&binding.localId&&!binding.targets.empty())shapes.push_back(std::move(binding));
 }
 WB_LOG(0,"[WarsBots] PS CAPTURE %d AreaShape bindings for %s.",int(shapes.size()),name.c_str());
 return shapes;
}

// DS-072: pick a destination inside one of the shapes bound to this capturable entity. The capturable pivot can sit
// outside the area (Shore 579 is 13.3 m away), so every sample is checked against the live native area and the world
// polygon; the most interior sample wins and a navmesh landing is preferred. No map-specific coordinates.
// wb_bot_ps_reach_dest 1 (playtest 09.11): 'anchors' are the snapped team bases; the destination is then the most
// interior sample with a path from every anchor (PickReachableCandidate), else the old most interior one. *reach says
// which: "off" (flag OFF or no anchors), "bases" or "interior_fallback".
bool AreaDestination(int capturableId, int localAreaId, Vec3& out, int& areaEntityId, const char** why,
	const std::vector<Vec3>& anchors, const char** reach, int* rank)
{
	*reach = "off"; *rank = 0;
	std::vector<float> candXyz, candEdge; std::vector<int> candEntity;   // only filled when anchors are given
	*why = "no_binding";
	const std::vector<AreaShapeInfo>& shapes = LevelAreaShapes();
	NavMeshCore* mesh = CurrentNavMesh();
	if(!mesh||!mesh->Ready()){*why="navmesh_unavailable";return false;}
	float bestEdge = -1.f;
	Vec3 best;
	int bestEntity = 0;
	bool matched = false;
	for (const AreaShapeInfo& s : shapes)
	{
		const CaptureShapeBinding binding = { s.entityId, s.localId, s.targets.data(), int(s.targets.size()) };
		if (!CaptureShapeBinds(binding, capturableId, localAreaId)) continue;
		matched = true;
		IEntity* e = gEnv->pEntitySystem->GetEntity(EntityId(s.entityId));
		if (!e) continue;
		const char* cls = e->GetClass() ? e->GetClass()->GetName() : nullptr;
		if (!cls || std::strcmp(cls, "AreaShape")) continue;             // stale XML entity id
		if ((e->GetWorldPos() - s.pos).GetLengthSquared() > 1.0f) continue; // map id resolved to a different entity
		IEntityAreaProxy* proxy = static_cast<IEntityAreaProxy*>(e->GetProxy(ENTITY_PROXY_AREA));
		if (!proxy || proxy->GetAreaType() != ENTITY_AREA_TYPE_SHAPE) continue;
		if (proxy->GetID() != localAreaId) continue; // cross-check the local number of the live shape
		const int n = proxy->GetPointsCount();
		const Vec3* pts = proxy->GetPoints();
		if (!pts || n < 3) continue;

		const Matrix34 transform=e->GetWorldTM();
		std::vector<float> poly;
		poly.reserve(size_t(n) * 2);
		float minx = 1e30f, maxx = -1e30f, miny = 1e30f, maxy = -1e30f, bottom = 1e30f, top = -1e30f;
		for (int i = 0; i < n; ++i)
		{
			const Vec3 w = transform.TransformPoint(pts[i]);
			poly.push_back(w.x); poly.push_back(w.y);
			minx = std::min(minx, w.x); maxx = std::max(maxx, w.x);
			miny = std::min(miny, w.y); maxy = std::max(maxy, w.y);
			bottom = std::min(bottom, w.z); top = std::max(top, w.z);
		}
		const float height = proxy->GetHeight();
		if (height > 0.f) top = bottom + height * transform.GetColumn2().GetLength();

		CaptureAreaPoly shape;
		shape.xy = poly.data(); shape.count = n; shape.bottom = bottom; shape.top = top;

		const float step = 1.0f;
		int samples=0;
		for (float x = minx; x <= maxx + 1e-3f && samples<250000; x += step)
		for (float y = miny; y <= maxy + 1e-3f && samples<250000; y += step)
		for (float z = bottom; z <= top + 1e-3f && samples<250000; z += step)
		{
			++samples;
			if (!CaptureAreaInside(shape, x, y, z)) continue;
			if (!proxy->IsPointWithin(Vec3(x, y, z))) continue;
			Vec3 p(x, y, z);
			if (mesh)
			{
				const float in[3] = { x, y, z }; float snap[3];
				if (!mesh->Project(in, snap)) continue; // fail closed: off-mesh interior is not a goal
				p = Vec3(snap[0], snap[1], snap[2]);
				if (!proxy->IsPointWithin(p)) continue; // the landing must still be inside the area
			}
			const float edge = CaptureAreaEdgeDistance(shape, p.x, p.y);
			if (edge > bestEdge) { bestEdge = edge; best = p; bestEntity = s.entityId; }
			if (!anchors.empty() && candEdge.size() < 20000)
			{ candXyz.push_back(p.x); candXyz.push_back(p.y); candXyz.push_back(p.z); candEdge.push_back(edge); candEntity.push_back(s.entityId); }
		}
	}
	if (!matched) return false;
	if (bestEdge < 0.f) { *why = "no_interior_sample"; return false; }
	if (bestEdge < 0.25f) { *why = "thin_area"; return false; }
	out = best;
	areaEntityId = bestEntity;
	if (!anchors.empty())
	{
		int tests = 0;
		const int pick = PickReachableCandidate(candXyz.data(), candEdge.data(), int(candEdge.size()), 0.25f, 32, 1.0f,
			[&](float x, float y, float z) {
				++tests; const float goal[3] = { x, y, z };
				for (const Vec3& a : anchors) { const float start[3] = { a.x, a.y, a.z }; if (!mesh->Reachable(start, goal)) return false; }
				return true; });
		*rank = tests;
		if (pick >= 0) { out = Vec3(candXyz[pick * 3], candXyz[pick * 3 + 1], candXyz[pick * 3 + 2]); areaEntityId = candEntity[pick]; *reach = "bases"; }
		else *reach = "interior_fallback";
	}
	return true;
}

const std::vector<PsCapturePoint>& BasePoints()
{
	static std::string levelName;
	static std::vector<PsCapturePoint> points;
	static NavMeshCore* cachedMesh=nullptr;
	static int cachedSquads=-1;   // SPEC-005: bunkers join the targets only with wb_bot_ps_squads 1
	static int cachedReach=-1;    // wb_bot_ps_reach_dest: destinations change with the flag
	if (!g_pGame || !g_pGame->GetIGameFramework()) return points;
	ILevel* level = g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel();
	const char* name = level && level->GetLevelInfo() ? level->GetLevelInfo()->GetName() : nullptr;
	const std::string current = name ? name : "";
	NavMeshCore* mesh=CurrentNavMesh();
	const int squads=BotConfig::Get().psSquads==1?1:0;
	const int reachDest=BotConfig::Get().psReachDest==1?1:0;
	if (current == levelName && mesh==cachedMesh && squads==cachedSquads && reachDest==cachedReach && !points.empty()) return points;
	cachedMesh=mesh; cachedSquads=squads; cachedReach=reachDest;
	levelName = current;
	points.clear();
	if (!gEnv || !gEnv->pEntitySystem) return points;

	IEntityClass* energy = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AlienEnergyPoint");
	IEntityClass* factory = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Factory");
	IEntityClass* spawnGroup = squads ? gEnv->pEntitySystem->GetClassRegistry()->FindClass("SpawnGroup") : nullptr;
	// wb_bot_ps_reach_dest 1: the non-capturable SpawnGroups (team bases) snapped like BuildCache does; the team id is
	// not needed here, so this works before the rules hand out the bases.
	std::vector<Vec3> anchors;
	if (reachDest && mesh && mesh->Ready())
	{
		IEntityClass* groupClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("SpawnGroup");
		IEntityItPtr bases = gEnv->pEntitySystem->GetEntityIterator();
		while (IEntity* e = bases->Next())
		{
			if (!groupClass || e->GetClass() != groupClass) continue;
			IScriptTable* table = e->GetScriptTable(); SmartScriptTable props; int capturable = 0;
			if (!table || !table->GetValue("Properties", props)) continue;
			props->GetValue("bCapturable", capturable);
			if (capturable) continue;
			const Vec3 p = e->GetWorldPos(); const float q[3] = { p.x, p.y, p.z }; float snapped[3];
			if (mesh->Project(q, snapped, 8.f, 3.f, 10.f)) anchors.push_back(Vec3(snapped[0], snapped[1], snapped[2]));
		}
		WB_LOG(0, "[WarsBots] PS CAPTURE reach_dest bases=%d.", int(anchors.size()));
	}
	IEntityItPtr it = gEnv->pEntitySystem->GetEntityIterator();
	while (IEntity* entity = it->Next())
	{
		IEntityClass* cls = entity->GetClass();
		const bool isEnergy = cls && cls == energy;
		const bool isFactory = cls && cls == factory;
		const bool isBunker = cls && spawnGroup && cls == spawnGroup;   // SpawnGroup.lua:149 MakeCapturable, bCapturable below
		if (!isEnergy && !isFactory && !isBunker) continue;
		IScriptTable* table = entity->GetScriptTable();
		if (!table) continue;
		SmartScriptTable props;
		if (!table->GetValue("Properties", props)) continue;
		int capturable = 0, index = 1;
		float captureTime = 15.f;
		props->GetValue("bCapturable", capturable);
		if (!capturable) continue;
		props->GetValue("nCaptureIndex", index);
		props->GetValue("captureTime", captureTime);

		int localAreaId = 0; // local area number from the map, never an EntityId
		props->GetValue("captureAreaId", localAreaId);
		Vec3 destination(0,0,0);
		int areaEntityId = 0;
		const char* why = "no_area_id";
		const char* reach = "off"; int reachTests = 0;
		const bool validDestination=localAreaId && AreaDestination(int(entity->GetId()), localAreaId, destination, areaEntityId, &why, anchors, &reach, &reachTests);
		if (!validDestination)
		{
			WB_LOG(0, "[WarsBots] PS CAPTURE skip id=%u area=%d reason=%s.", unsigned(entity->GetId()), localAreaId, why);
			// Keep native metadata for the summary. hasDestination=false keeps this out of the planner.
		}
		PsCapturePoint point;
		point.id = int(entity->GetId());
		point.areaId = areaEntityId;
		point.areaLocalId = localAreaId;
		point.hasDestination = validDestination;
		point.value = index > 0 ? index : 1;
		point.captureSeconds = int(captureTime);
		point.power = isEnergy;
		point.kind = isEnergy ? int(PsKind::Energy) : isBunker ? int(PsKind::Bunker) : int(PsKind::Factory);
		if (isFactory)
		{
			const char* label = nullptr;   // Shore 344 Prototype-Factory6: Properties.szName = "prototype"
			if (props->GetValue("szName", label) && label && !_stricmp(label, "prototype")) point.kind = int(PsKind::Prototype);
		}
		point.x = destination.x; point.y = destination.y; point.z = destination.z;
		if(validDestination) WB_LOG(0, "[WarsBots] PS CAPTURE dest id=%u area=%d shape=%d pos=(%.2f,%.2f,%.2f) reach=%s tests=%d.",
			unsigned(entity->GetId()), localAreaId, areaEntityId, destination.x, destination.y, destination.z, reach, reachTests);
		points.push_back(point);
	}
	WB_LOG(0, "[WarsBots] PS CAPTURE %d capturable points for %s.", int(points.size()), current.c_str());
	return points;
}

} // namespace

void PSCaptureSnapshot(std::vector<PsCapturePoint>& out, CGameRules& rules,const float* from, const NavQueryBounds* startBounds)
{
	out = BasePoints();
	NavMeshCore* mesh=CurrentNavMesh();
	for(PsCapturePoint& point:out){point.ownerTeam=rules.GetTeam(EntityId(point.id));
	 const float goal[3]={point.x,point.y,point.z};
	 if(from&&(!mesh||!mesh->Reachable(from,goal,2.6f,startBounds)))point.hasDestination=false;
	}
}


// ---------------------------------------------------------------------------------------------------------------
// SPEC-005 stage 1: team commander runtime (wb_bot_ps_squads 1). Pure planning lives in BotTeamOrders.h; here: the
// path-length cache built once per level/navmesh, ownership/energy/roster/respawn events, PS ORDER and PS OWN logs.
namespace {

float PathLength(NavMeshCore& mesh, const Vec3& a, const Vec3& b, bool& found)
{
	std::vector<float> xyz; std::vector<NavPathLink> links; const char* why = "navmesh";
	const float s[3] = {a.x, a.y, a.z}, g[3] = {b.x, b.y, b.z};
	found = mesh.FindPathLinks(s, g, xyz, links, &why, nullptr);
	if (!found) return 1e9f;
	float len = 0.f; Vec3 prev = a;
	for (size_t i = 0; i + 2 < xyz.size(); i += 3) { const Vec3 p(xyz[i], xyz[i + 1], xyz[i + 2]); len += (p - prev).GetLength(); prev = p; }
	return len;
}

struct TeamRuntime
{
	std::vector<TeamOrder> orders;
	float nextPlan = 0.f, nextCheck = 0.f;
	TeamEvent pending = TeamEvent::None;
	std::set<unsigned> roster;
	std::map<unsigned, bool> alive;
	std::map<int, bool> reach;           // last logged reachability per target (PS ORDER reason=unreachable once)
	std::map<int, float> ownedSec;       // PS OWN
	std::map<int, int> lost, recaptured;
};

struct Commander
{
	std::string level; NavMeshCore* mesh = nullptr; bool cacheBuilt = false; int cacheTries = 0; float nextCacheTry = 0.f;
	bool baseFound[3] = {false, false, false};
	std::map<int, float> fromBase[3];                  // target -> path length from the team base (absent: no path)
	std::map<std::pair<int, int>, float> between;      // (from target, to target) -> path length (absent: no path)
	std::map<int, int> owner;                          // last seen owner per target
	TeamRuntime teams[3];
	float lastTick = 0.f, nextOwnLog = 0.f;
};

Commander& Cmd() { static Commander c; return c; }

void BuildCache(Commander& c, NavMeshCore& mesh, const std::vector<PsCapturePoint>& points, CGameRules& rules)
{
	const auto start = std::chrono::steady_clock::now();
	IEntityClass* spawnGroup = gEnv->pEntitySystem->GetClassRegistry()->FindClass("SpawnGroup");
	Vec3 base[3]; int found = 0;
	IEntityItPtr it = gEnv->pEntitySystem->GetEntityIterator();
	while (IEntity* e = it->Next())
	{
		if (!spawnGroup || e->GetClass() != spawnGroup) continue;
		IScriptTable* table = e->GetScriptTable(); SmartScriptTable props; int capturable = 0;
		if (table && table->GetValue("Properties", props)) props->GetValue("bCapturable", capturable);
		const int team = rules.GetTeam(e->GetId());
		if (capturable || team < 1 || team > 2 || c.baseFound[team]) continue;   // the team's own non-capturable base
		const Vec3 p = e->GetWorldPos(); const float q[3] = {p.x, p.y, p.z}; float snapped[3];
		if (!mesh.Project(q, snapped, 8.f, 3.f, 10.f)) { WB_LOG(0, "[WarsBots] PS ORDER cache team=%d base=%u pos=(%.1f,%.1f,%.1f) off_mesh.", team, unsigned(e->GetId()), p.x, p.y, p.z); continue; }
		base[team] = Vec3(snapped[0], snapped[1], snapped[2]); c.baseFound[team] = true; ++found;
		WB_LOG(0, "[WarsBots] PS ORDER cache team=%d base=%u pos=(%.1f,%.1f,%.1f) snapped=(%.1f,%.1f,%.1f).", team, unsigned(e->GetId()), p.x, p.y, p.z, snapped[0], snapped[1], snapped[2]);
	}
	int paths = 0, ok = 0;
	for (int team = 1; team <= 2; ++team)
	{
		if (!c.baseFound[team]) continue;
		int reachable = 0;
		for (const PsCapturePoint& p : points)
		{
			if (!p.hasDestination) continue;
			bool f = false; const float len = PathLength(mesh, base[team], Vec3(p.x, p.y, p.z), f); ++paths;
			if (f) { c.fromBase[team][p.id] = len; ++reachable; ++ok; }
		}
		WB_LOG(0, "[WarsBots] PS ORDER cache team=%d reachable=%d/%d from base.", team, reachable, int(points.size()));
	}
	for (const PsCapturePoint& a : points)
		for (const PsCapturePoint& b : points)
		{
			if (a.id == b.id || !a.hasDestination || !b.hasDestination) continue;
			bool f = false; const float len = PathLength(mesh, Vec3(a.x, a.y, a.z), Vec3(b.x, b.y, b.z), f); ++paths;
			if (f) { c.between[std::make_pair(a.id, b.id)] = len; ++ok; }
		}
	const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
	WB_LOG(0, "[WarsBots] PS ORDER cache targets=%d bases=%d paths=%d found=%d ms=%.1f.", int(points.size()), found, paths, ok, ms);
	// Base teams are set by the rules at match start; retry a few times if one base was not found yet.
	c.cacheBuilt = found == 2 || ++c.cacheTries >= 6;
}

void Tick(Commander& c, CGameRules& rules, float now)
{
	ILevel* level = g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel();
	const char* name = level && level->GetLevelInfo() ? level->GetLevelInfo()->GetName() : "";
	NavMeshCore* mesh = CurrentNavMesh();
	if (c.level != (name ? name : "") || c.mesh != mesh || now < c.lastTick - 1.f)
	{
		c = Commander(); c.level = name ? name : ""; c.mesh = mesh;   // new level, navmesh or a restarted clock
	}
	if (c.lastTick > 0.f && now - c.lastTick < 0.25f) return;   // every bot calls this; work at 4 Hz
	const float dt = c.lastTick > 0.f ? std::min(now - c.lastTick, 1.f) : 0.f;
	c.lastTick = now;
	std::vector<PsCapturePoint> points;
	PSCaptureSnapshot(points, rules, nullptr);
	if (!mesh || !mesh->Ready()) return;
	if (!c.cacheBuilt && now >= c.nextCacheTry)
	{
		c.nextCacheTry = now + 10.f;
		for (int team = 0; team < 3; ++team) { c.baseFound[team] = false; c.fromBase[team].clear(); }
		c.between.clear();
		BuildCache(c, *mesh, points, rules);
	}

	// ownership events and PS OWN bookkeeping
	for (const PsCapturePoint& p : points)
	{
		auto o = c.owner.find(p.id);
		const int was = o == c.owner.end() ? -1 : o->second;
		if (was >= 0 && was != p.ownerTeam)
		{
			for (int team = 1; team <= 2; ++team)
			{
				if (p.ownerTeam == team) { c.teams[team].pending = StrongerEvent(c.teams[team].pending, TeamEvent::Captured); if (c.teams[team].lost[p.id] > 0) ++c.teams[team].recaptured[p.id]; }
				if (was == team) { c.teams[team].pending = StrongerEvent(c.teams[team].pending, TeamEvent::Lost); ++c.teams[team].lost[p.id]; }
			}
		}
		c.owner[p.id] = p.ownerTeam;
		if (p.ownerTeam >= 1 && p.ownerTeam <= 2) c.teams[p.ownerTeam].ownedSec[p.id] += dt;
	}
	if (now >= c.nextOwnLog)
	{
		if (c.nextOwnLog > 0.f)
			for (int team = 1; team <= 2; ++team)
				for (const PsCapturePoint& p : points)
					WB_LOG(1, "[WarsBots] PS OWN team=%d target=%d kind=%s owned=%.0f lost=%d recaptured=%d t=%.0f.", team, p.id, PsKindName(PsKind(p.kind)),
						c.teams[team].ownedSec[p.id], c.teams[team].lost[p.id], c.teams[team].recaptured[p.id], now);
		c.nextOwnLog = now + 60.f;
	}

	for (int team = 1; team <= 2; ++team)
	{
		TeamRuntime& tr = c.teams[team];
		if (now < tr.nextCheck && tr.pending == TeamEvent::None) continue;
		tr.nextCheck = now + 0.5f;
		// targets with cache reachability: from the base, or from one of the team's own points
		std::vector<TeamTarget> targets;
		for (const PsCapturePoint& p : points)
		{
			TeamTarget t; t.id = p.id; t.kind = PsKind(p.kind); t.owner = p.ownerTeam;
			auto b = c.fromBase[team].find(p.id);
			if (p.hasDestination && b != c.fromBase[team].end()) { t.reachable = true; t.path = b->second; }
			for (const PsCapturePoint& own : points)
			{
				if (!p.hasDestination || own.ownerTeam != team || own.id == p.id) continue;
				auto w = c.between.find(std::make_pair(own.id, p.id));
				if (w != c.between.end()) { t.reachable = true; t.path = std::min(t.path, w->second); }
			}
			if (p.ownerTeam == team && p.hasDestination) t.reachable = true;   // the team got there: it can defend it
			auto r = tr.reach.find(p.id);
			if (r == tr.reach.end() || r->second != t.reachable)
			{
				if (!t.reachable) WB_LOG(1, "[WarsBots] PS ORDER team=%d target=%d kind=%s reason=unreachable.", team, p.id, PsKindName(t.kind));
				if (r != tr.reach.end() && !t.reachable) tr.pending = StrongerEvent(tr.pending, TeamEvent::Unreachable);
				tr.reach[p.id] = t.reachable;
			}
			targets.push_back(t);
		}
		// the team's bots: alive, AI running; respawn and roster events
		std::vector<TeamBot> bots; std::set<unsigned> roster;
		g_pGame->GetBotManager().ForEachBot([&](const BotInstance& inst)
		{
			CActor* a = inst.GetActor();
			if (!a || !inst.AIEnabled() || rules.GetTeam(a->GetEntityId()) != team) return;
			const unsigned id = unsigned(a->GetEntityId());
			const bool isAlive = a->GetHealth() > 0;
			const auto was = tr.alive.find(id);
			const bool respawned = isAlive && was != tr.alive.end() && !was->second;
			tr.alive[id] = isAlive;
			if (!isAlive) return;
			roster.insert(id);
			TeamBot b; b.id = id; b.skill = int(inst.Profile().skill); b.respawned = respawned; bots.push_back(b);
		});
		bool anyRespawn = false; for (const TeamBot& b : bots) anyRespawn |= b.respawned;
		if (roster != tr.roster && !tr.roster.empty()) tr.pending = StrongerEvent(tr.pending, TeamEvent::Roster);
		tr.roster = roster;
		if (now < tr.nextPlan && tr.pending == TeamEvent::None && !anyRespawn) continue;
		tr.nextPlan = now + 5.f;
		std::vector<TeamOrder> next = PlanTeamOrders(targets, bots, team, now, tr.pending, tr.orders);
		for (const TeamOrder& o : next)
		{
			if (o.since != now) continue;   // unchanged task: no line
			WB_LOG(1, "[WarsBots] PS ORDER team=%d bot=%u target=%d kind=%s role=%s pair=%d reason=%s t=%.1f.", team, o.bot, o.target,
				PsKindName(o.kind), o.hold ? "hold" : "capture", o.pair, o.reason, now);
		}
		tr.orders.swap(next);
		tr.pending = TeamEvent::None;
	}
}

} // namespace

const TeamOrder* PSTeamOrder(unsigned actor, int team, CGameRules& rules)
{
	if (team < 1 || team > 2 || !gEnv || !gEnv->pTimer || !g_pGame) return nullptr;
	Commander& c = Cmd();
	Tick(c, rules, gEnv->pTimer->GetCurrTime());
	for (const TeamOrder& o : c.teams[team].orders) if (o.bot == actor) return &o;
	return nullptr;
}

} // namespace WarsBots
