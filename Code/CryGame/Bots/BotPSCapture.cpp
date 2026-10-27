// DS-048 #3: PS-1 capture goal data. Caches the capturable entities once per level (class, index, capture time,
// position) and refreshes only their owner team, which changes during the match.
#include "PortIncludes.h"
#include "CryGame/Bots/BotPSCaptureRuntime.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryCommon/CryAction/ILevelSystem.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include <cstring>
#include <string>
#include <vector>

namespace WarsBots {
namespace {

const std::vector<PsCapturePoint>& BasePoints()
{
	static std::string levelName;
	static std::vector<PsCapturePoint> points;
	if (!g_pGame || !g_pGame->GetIGameFramework()) return points;
	ILevel* level = g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel();
	const char* name = level && level->GetLevelInfo() ? level->GetLevelInfo()->GetName() : nullptr;
	const std::string current = name ? name : "";
	if (current == levelName && !points.empty()) return points;
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
		const Vec3 pos = entity->GetWorldPos();
		PsCapturePoint point;
		point.id = int(entity->GetId());
		point.value = index > 0 ? index : 1;
		point.captureSeconds = int(captureTime);
		point.power = isEnergy;
		point.x = pos.x; point.y = pos.y; point.z = pos.z;
		points.push_back(point);
	}
	WB_LOG(0, "[WarsBots] PS CAPTURE %d capturable points for %s.", int(points.size()), current.c_str());
	return points;
}

} // namespace

void PSCaptureSnapshot(std::vector<PsCapturePoint>& out, CGameRules& rules)
{
	out = BasePoints();
	for (PsCapturePoint& point : out) point.ownerTeam = rules.GetTeam(EntityId(point.id));
}

} // namespace WarsBots
