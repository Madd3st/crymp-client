#include "PortIncludes.h"
#include "CryGame/Bots/BotNavMesh.h"
#include "CryGame/Bots/BotNavMeshCore.h"
#include "CryGame/Bots/BotConfig.h"
#include "CryGame/Bots/BotDataPath.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Game.h"
#include "CryCommon/CryAction/IGameFramework.h"
#include "CryCommon/CryAction/ILevelSystem.h"
#include <string>
#include <vector>
#include <cstdio>

namespace WarsBots {
namespace {
struct State { NavMeshCore mesh; std::string level; bool tried = false; };
State& S() { static State s; return s; }
}

NavMeshCore* CurrentNavMesh()
{
	if (BotConfig::Get().botNavmesh != 1 || !g_pGame) return nullptr;
	ILevel* level = g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel();
	if (!level || !level->GetLevelInfo()) return nullptr;
	const std::string name = level->GetLevelInfo()->GetName();
	State& s = S();
	if (name != s.level) { s.mesh.Reset(); s.level = name; s.tried = false; }
	if (!s.tried)
	{
		s.tried = true;
		// "Multiplayer/IA/Steelmill" -> Steelmill.navmesh
		const size_t slash = name.find_last_of("/\\");
		const std::string leaf = slash == std::string::npos ? name : name.substr(slash + 1);
		const std::filesystem::path path = DataDirectory(L"Navmesh") / std::filesystem::u8path(leaf + ".navmesh");
		std::vector<unsigned char> data;
		if (FILE* f = _wfopen(path.c_str(), L"rb"))
		{
			unsigned char chunk[1 << 16]; size_t n;
			while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) data.insert(data.end(), chunk, chunk + n);
			fclose(f);
		}
		std::string error;
		if (data.empty()) WB_LOG(0, "[WarsBots] NAVMESH none for %s (expected %ls); BotNav graph stays in use.", name.c_str(), path.c_str());
		else if (!s.mesh.LoadMemory(data.data(), data.size(), &error))
			WB_LOG(0, "[WarsBots] NAVMESH load failed for %s: %s; BotNav graph stays in use.", name.c_str(), error.c_str());
		else
			WB_LOG(0, "[WarsBots] NAVMESH loaded %s: %d polys, %d parts, %u bytes.", name.c_str(), s.mesh.Polys(), s.mesh.Components(), unsigned(data.size()));
	}
	return s.mesh.Ready() ? &s.mesh : nullptr;
}
}
