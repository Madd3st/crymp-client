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
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryCommon/CryAction/ILevelSystem.h"
#include "CryCommon/CryEntitySystem/IEntityProxy.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IXml.h"
#include <algorithm>
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
bool AreaDestination(int capturableId, int localAreaId, Vec3& out, int& areaEntityId, const char** why)
{
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
		}
	}
	if (!matched) return false;
	if (bestEdge < 0.f) { *why = "no_interior_sample"; return false; }
	if (bestEdge < 0.25f) { *why = "thin_area"; return false; }
	out = best;
	areaEntityId = bestEntity;
	return true;
}

const std::vector<PsCapturePoint>& BasePoints()
{
	static std::string levelName;
	static std::vector<PsCapturePoint> points;
	static NavMeshCore* cachedMesh=nullptr;
	if (!g_pGame || !g_pGame->GetIGameFramework()) return points;
	ILevel* level = g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel();
	const char* name = level && level->GetLevelInfo() ? level->GetLevelInfo()->GetName() : nullptr;
	const std::string current = name ? name : "";
	NavMeshCore* mesh=CurrentNavMesh();
	if (current == levelName && mesh==cachedMesh && !points.empty()) return points;
	cachedMesh=mesh;
	levelName = current;
	points.clear();
	if (!gEnv || !gEnv->pEntitySystem) return points;

	IEntityClass* energy = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AlienEnergyPoint");
	IEntityClass* factory = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Factory");
	IEntityItPtr it = gEnv->pEntitySystem->GetEntityIterator();
	while (IEntity* entity = it->Next())
	{
		IEntityClass* cls = entity->GetClass();
		const bool isEnergy = cls && cls == energy;
		const bool isFactory = cls && cls == factory;
		if (!isEnergy && !isFactory) continue;
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
		const bool validDestination=localAreaId && AreaDestination(int(entity->GetId()), localAreaId, destination, areaEntityId, &why);
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
		point.x = destination.x; point.y = destination.y; point.z = destination.z;
		if(validDestination) WB_LOG(0, "[WarsBots] PS CAPTURE dest id=%u area=%d shape=%d pos=(%.2f,%.2f,%.2f).",
			unsigned(entity->GetId()), localAreaId, areaEntityId, destination.x, destination.y, destination.z);
		points.push_back(point);
	}
	WB_LOG(0, "[WarsBots] PS CAPTURE %d capturable points for %s.", int(points.size()), current.c_str());
	return points;
}

} // namespace

void PSCaptureSnapshot(std::vector<PsCapturePoint>& out, CGameRules& rules,const float* from)
{
	out = BasePoints();
	NavMeshCore* mesh=CurrentNavMesh();
	for(PsCapturePoint& point:out){point.ownerTeam=rules.GetTeam(EntityId(point.id));
	 const float goal[3]={point.x,point.y,point.z};
	 if(from&&(!mesh||!mesh->Reachable(from,goal)))point.hasDestination=false;
	}
}

} // namespace WarsBots
