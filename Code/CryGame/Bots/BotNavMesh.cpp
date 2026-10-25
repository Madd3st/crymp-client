#include "PortIncludes.h"
#include "CryGame/Bots/BotNavMesh.h"
#include "CryGame/Bots/BotNavMeshCore.h"
#include "CryGame/Bots/BotVantage.h"
#include "CryGame/Bots/BotTactics.h"
#include "CryGame/Bots/BotConfig.h"
#include "CryGame/Bots/BotDataPath.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Game.h"
#include "CryCommon/CryAction/IGameFramework.h"
#include "CryCommon/CryAction/ILevelSystem.h"
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

namespace WarsBots {
namespace {
struct State { NavMeshCore mesh; std::string level; bool tried = false; int kinds = 0; };
State& S() { static State s; return s; }

// T-6: <leaf>.manifest.json (Tools/RecastBake/make_manifest.py) records FNV-1a 64 of each data file; a file that does
// not match its manifest belongs to another bake and is refused (navmesh: warning only). No manifest = old install, accepted.
std::string Fnv64(const void* data, size_t n)
{
	unsigned long long h = 0xcbf29ce484222325ull; const unsigned char* p = static_cast<const unsigned char*>(data);
	for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ull; }
	char buf[17]; std::snprintf(buf, sizeof buf, "%016llx", h); return buf;
}
std::string ManifestFnv(const std::string& leaf, const char* kind)
{
	const std::filesystem::path path = DataDirectory(L"Navmesh") / std::filesystem::u8path(leaf + ".manifest.json");
	std::string text;
	if (FILE* f = _wfopen(path.c_str(), L"rb")) { char c[4096]; size_t n; while ((n = fread(c, 1, sizeof c, f)) > 0) text.append(c, n); fclose(f); }
	const size_t k = text.find(std::string("\"") + kind + "\"");
	if (k == std::string::npos) return "";
	const size_t end = text.find('}', k);                    // DS-005: stay inside this file's object
	const size_t f = text.find("\"fnv64\"", k);
	if (f == std::string::npos || (end != std::string::npos && f > end)) return "";
	const size_t q = text.find('"', text.find(':', f) + 1);
	return q == std::string::npos ? "" : text.substr(q + 1, 16);
}
// true when the data may be used; logs a mismatch
bool ManifestOk(const std::string& leaf, const char* kind, const std::string& data)
{
	const std::string want = ManifestFnv(leaf, kind);
	if (want.empty()) return true;
	// DS-005: a Windows checkout may turn LF into CRLF in the text files; the manifest hashes LF -> hash without '\r'
	std::string lf; lf.reserve(data.size());
	if (std::strcmp(kind, "navmesh")) { for (char c : data) if (c != '\r') lf.push_back(c); }
	const std::string& h = std::strcmp(kind, "navmesh") ? lf : data;
	const std::string got = Fnv64(h.data(), h.size());
	if (got == want) return true;
	WB_LOG(0, "[WarsBots] NAVMESH data %s for %s does not match its manifest (%s, manifest %s).", kind, leaf.c_str(), got.c_str(), want.c_str());
	return false;
}
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
		s.tried = true; s.kinds = 0;
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
		if (!data.empty()) ManifestOk(leaf, "navmesh", std::string(data.begin(), data.end()));   // warning only
		if (data.empty()) WB_LOG(0, "[WarsBots] NAVMESH none for %s (expected %ls); BotNav graph stays in use.", name.c_str(), path.c_str());
		else if (!s.mesh.LoadMemory(data.data(), data.size(), &error))
			WB_LOG(0, "[WarsBots] NAVMESH load failed for %s: %s; BotNav graph stays in use.", name.c_str(), error.c_str());
		else
		{
			WB_LOG(0, "[WarsBots] NAVMESH loaded %s: %d polys, %d parts, %d links, %u bytes.", name.c_str(), s.mesh.Polys(), s.mesh.Components(), s.mesh.Links(), unsigned(data.size()));
			s.kinds = 0;
			if (s.mesh.Links() > 0)
			{
				const std::filesystem::path links = DataDirectory(L"Navmesh") / std::filesystem::u8path(leaf + ".links.txt");
				std::string text;
				if (FILE* f = _wfopen(links.c_str(), L"rb"))
				{
					char chunk[1 << 16]; size_t n;
					while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) text.append(chunk, n);
					fclose(f);
				}
				if (!text.empty() && !ManifestOk(leaf, "links", text)) text.clear();   // links of another bake: walk only
				s.kinds = text.empty() ? 0 : s.mesh.LoadLinkKinds(text.c_str());
				if (s.mesh.LinkMismatches()) WB_LOG(0, "[WarsBots] NAVMESH %d links matched by end points (links.txt order differs from the bake).", s.mesh.LinkMismatches());
				WB_LOG(0, "[WarsBots] NAVMESH link kinds %d/%d from %ls.", s.kinds, s.mesh.Links(), links.c_str());
			}
		}
	}
	if (s.mesh.Ready())
	{
		// C-036: links only with wb_bot_jump_links=1 (and only kinds with an executor: drop, jump, Strength).
		const unsigned short want = BotConfig::Get().jumpLinks == 1 && s.kinds > 0
			? (unsigned short)(NavLinkFlag(NavLinkDrop) | NavLinkFlag(NavLinkJump) | NavLinkFlag(NavLinkStrength)) : 0;
		if (want != s.mesh.LinkMask())
		{
			s.mesh.SetLinkMask(want);
			WB_LOG(0, "[WarsBots] NAVMESH links %s: %d links, %d parts.", want ? "ON" : "OFF", s.mesh.Links(), s.mesh.Components());
		}
	}
	return s.mesh.Ready() ? &s.mesh : nullptr;
}

const CoverCandidate* CurrentCover(int& count)
{
	count = 0;
	if (BotConfig::Get().tactics != 1 || !g_pGame) return nullptr;
	ILevel* level = g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel();
	if (!level || !level->GetLevelInfo()) return nullptr;
	static std::vector<CoverCandidate> spots; static std::string loaded; static bool tried = false;
	const std::string name = level->GetLevelInfo()->GetName();
	if (name != loaded) { loaded = name; tried = false; spots.clear(); }
	if (!tried)
	{
		tried = true;
		const size_t slash = name.find_last_of("/\\");
		const std::string leaf = slash == std::string::npos ? name : name.substr(slash + 1);
		const std::filesystem::path path = DataDirectory(L"Navmesh") / std::filesystem::u8path(leaf + ".spots.txt");
		std::string text;
		if (FILE* f = _wfopen(path.c_str(), L"rb")) { char c[1 << 16]; size_t n; while ((n = fread(c, 1, sizeof c, f)) > 0) text.append(c, n); fclose(f); }
		if (!text.empty() && !ManifestOk(leaf, "spots", text)) text.clear();
		for (size_t p0 = 0; p0 < text.size();)
		{
			size_t p1 = text.find('\n', p0); if (p1 == std::string::npos) p1 = text.size();
			const std::string line = text.substr(p0, p1 - p0); p0 = p1 + 1;
			if (line.empty() || line[0] == '#') continue;
			CoverCandidate c; unsigned flags = 0, mask = 0;
			if (sscanf(line.c_str(), "%f %f %f %u %u", &c.p.x, &c.p.y, &c.p.z, &flags, &mask) == 5 && (flags & 3)) { c.coverMask = mask; spots.push_back(c); }
		}
		WB_LOG(0, "[WarsBots] COVER %d spots for %s.", int(spots.size()), name.c_str());
	}
	count = int(spots.size());
	return spots.empty() ? nullptr : spots.data();
}

VantageMap* CurrentVantage()
{
	if (BotConfig::Get().vantage != 1 || !g_pGame) return nullptr;
	ILevel* level = g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel();
	if (!level || !level->GetLevelInfo()) return nullptr;
	static VantageMap map; static std::string loaded; static bool tried = false;
	const std::string name = level->GetLevelInfo()->GetName();
	if (name != loaded) { loaded = name; tried = false; map = VantageMap(); }
	if (!tried)
	{
		tried = true;
		const size_t slash = name.find_last_of("/\\");
		const std::string leaf = slash == std::string::npos ? name : name.substr(slash + 1);
		const std::filesystem::path path = DataDirectory(L"Navmesh") / std::filesystem::u8path(leaf + ".spots.txt");
		std::string text;
		if (FILE* f = _wfopen(path.c_str(), L"rb"))
		{
			char chunk[1 << 16]; size_t n;
			while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) text.append(chunk, n);
			fclose(f);
		}
		if (!text.empty() && !ManifestOk(leaf, "spots", text)) text.clear();
		const int n = text.empty() ? 0 : map.Load(text.c_str());
		WB_LOG(0, "[WarsBots] VANTAGE %d sniper spots for %s from %ls.", n, name.c_str(), path.c_str());
	}
	return map.Count() > 0 ? &map : nullptr;
}
}
