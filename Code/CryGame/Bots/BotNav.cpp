#include "PortIncludes.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotNav.h"
#include "CryGame/Bots/BotLog.h"
#include "CryCommon/CrySystem/ICryPak.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <cctype>
#include "version.h"
#include "CryGame/Bots/BotHotspot.h"
#include "CryGame/Bots/BotKneePolicy.h"
#include "CryCommon/Cry3DEngine/ISurfaceType.h"
#include "CryGame/Bots/BotDataPath.h"
#include "nlohmann/json.hpp"

namespace WarsBots
{
namespace
{
const int kGeom = ent_static | ent_terrain;
const unsigned kFlags = rwi_stop_at_pierceable | rwi_colltype_any;
bool RayHit(const Vec3& from, const Vec3& delta, ray_hit& hit)
{
	return gEnv->pPhysicalWorld->RayWorldIntersection(from, delta, kGeom, kFlags, &hit, 1) != 0;
}
bool Clear(const Vec3& from, const Vec3& to)
{
	ray_hit hit;
	return !RayHit(from, to - from, hit);
}
}

void BotNav::Reset()
{
	m_phase = Phase::Idle;
    m_reachability.Reset();m_componentsAttempted=false;
	if (m_dirty) Save();
	m_nodes.clear(); m_edges.clear(); m_pending.clear(); m_columns.clear(); m_memory.clear();
	m_level.clear(); m_dirty = false; m_events = 0; m_saveTimer = 0;
    m_auditOnly=false;m_kneeVariant=0;
	m_cursor = 0; m_buildMs = 0; m_queries = 0;
}

bool BotNav::StartAudit(int variant)
{
 if(variant<0 || variant>2)return false;
 Reset();m_auditOnly=true;m_kneeVariant=variant;
 WB_LOG(0,"[WarsBots] NAV AUDIT variant=%d exportOnly=1",variant);
 if(gEnv->p3DEngine && gEnv->p3DEngine->GetMaterialManager())
  for(int id:{140,198,193,143}){
   ISurfaceType* surface=gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceType(id);
   WB_LOG(0,"[WarsBots] NAV SURFACE id=%d name=%s",id,surface?surface->GetName():"unknown");
  }
 return true;
}

void BotNav::Begin()
{
	if (m_phase != Phase::Idle || !gEnv->pPhysicalWorld || !g_pGame) return;
	CGameRules* rules = g_pGame->GetGameRules();
	const int count = rules ? rules->GetSpawnLocationCount() : 0;
	if (!count) return;
	m_min = Vec3(1e9f, 1e9f, 1e9f); m_max = Vec3(-1e9f, -1e9f, -1e9f);
	for (int i = 0; i < count; ++i)
		if (IEntity* spawn = gEnv->pEntitySystem->GetEntity(rules->GetSpawnLocation(i)))
		{
			const Vec3 p = spawn->GetWorldPos();
			m_min.x = min(m_min.x, p.x); m_min.y = min(m_min.y, p.y); m_min.z = min(m_min.z, p.z);
			m_max.x = max(m_max.x, p.x); m_max.y = max(m_max.y, p.y); m_max.z = max(m_max.z, p.z);
		}
	if (m_min.x > m_max.x) return;
	// The fight happens around the spawns; a margin covers the space between and behind them.
	m_min -= Vec3(40.0f, 40.0f, 15.0f);
	m_max += Vec3(40.0f, 40.0f, 35.0f);
	m_nx = int((m_max.x - m_min.x) / m_cell) + 1;
	m_ny = int((m_max.y - m_min.y) / m_cell) + 1;
	m_nodes.reserve(size_t(m_nx) * size_t(m_ny));
	m_cursor = 0;
	m_phase = Phase::Nodes;
	WB_LOG(0, "[WarsBots] NAV build started: %dx%d columns, cell %.1f m, z %.0f..%.0f.", m_nx, m_ny, m_cell, m_min.z, m_max.z);
}

void BotNav::ScanColumn(int ix, int iy)
{
	const float x = m_min.x + (float(ix) + 0.5f) * m_cell;
	const float y = m_min.y + (float(iy) + 0.5f) * m_cell;
	float top = m_max.z;
	std::vector<int>& column = m_columns[Key(ix, iy)];
	// Several floors per column: roofs, building interiors, ground.
	for (int layer = 0; layer < 5 && top > m_min.z; ++layer)
	{
		ray_hit hit;
		if (!RayHit(Vec3(x, y, top), Vec3(0, 0, m_min.z - top), hit)) break;
		const Vec3 floor = hit.pt;
		top = floor.z - 0.6f;
		if (hit.n.z < 0.7f) continue;                                    // walls, steep slopes
		if (!Clear(floor + Vec3(0, 0, 0.15f), floor + Vec3(0, 0, 1.9f))) continue; // no headroom
		// Not standing under water: skip nodes whose feet are below the ocean level.
		if (gEnv->p3DEngine && floor.z < gEnv->p3DEngine->GetWaterLevel() - 0.3f) continue;
		Node node;
		node.pos = floor + Vec3(0, 0, 0.05f);
		node.column = int(Key(ix, iy) & 0x7fffffff);
		node.firstEdge = 0; node.edgeCount = 0;
		column.push_back(int(m_nodes.size()));
		m_nodes.push_back(node);
	}
}

struct BotNav::WalkTrace
{
    const char* test="ok";float sample=0,value=0;Vec3 point=Vec3(0,0,0);
    IPhysicalEntity* collider=nullptr;int surface=-1,part=-1;
};
bool BotNav::Walkable(const Vec3& a,const Vec3& b,WalkTrace* trace) const
{
    const auto fail=[&](const char* test,float sample,float value,const ray_hit* hit) {
        if(trace){trace->test=test;trace->sample=sample;trace->value=value;
            if(hit){trace->point=hit->pt;trace->collider=hit->pCollider;trace->surface=hit->surface_idx;trace->part=hit->partid;}}
        return false;
    };
    const float rise=b.z-a.z;
    if(rise>1.9f)return fail("rise",0,rise,nullptr);
    if(rise< -3.2f)return fail("drop",0,rise,nullptr);
    ray_hit hit;
    for(float h:{KneeRayHeight(m_kneeVariant),1.2f,1.75f})
    {
        const Vec3 start=a+Vec3(0,0,h),end=b+Vec3(0,0,h+max(0.f,rise));
        if(RayHit(start,end-start,hit)) {
            bool step=false;
            if(m_kneeVariant==2 && h==.5f){
                const Vec3 delta=b-a;const float length2=delta.GetLengthSquared2D();
                const float t=length2>0?clamp_tpl(((hit.pt.x-a.x)*delta.x+(hit.pt.y-a.y)*delta.y)/length2,0.f,1.f):0.f;
                const Vec3 base=a+delta*t;
                const Vec3 inside=hit.pt+Vec3(delta.x,delta.y,0).GetNormalizedSafe(Vec3(0,0,0))*.05f;
                ray_hit ground;
                if(RayHit(Vec3(inside.x,inside.y,max(a.z,b.z)+1.f),Vec3(0,0,-(fabs_tpl(rise)+4.5f)),ground))
                    step=ground.pCollider==hit.pCollider && KneeHitIsStep(m_kneeVariant,ground.pt.z-base.z,ground.pt.z-hit.pt.z,ground.n.z);
            }
            if(!step)return fail("clearance",h,hit.dist,&hit);
        }
    }
    float last=a.z;
    for(float t:{.25f,.5f,.75f,1.f})
    {
        const Vec3 p=a+(b-a)*t;
        if(!RayHit(Vec3(p.x,p.y,max(a.z,b.z)+1.f),Vec3(0,0,-(fabs_tpl(rise)+4.5f)),hit))return fail("ground_missing",t,0,nullptr);
        const float step=hit.pt.z-last;
        if(step>.75f)return fail("step_up",t,step,&hit);
        if(step< -3.2f)return fail("step_drop",t,step,&hit);
        last=hit.pt.z;
    }
    if(!(fabs_tpl(last-b.z)<.8f))return fail("endpoint",1,fabs_tpl(last-b.z),&hit);
    return true;
}

void BotNav::Explain(int from,int to) const
{
    if(!Ready() || from<0 || to<0 || from>=NodeCount() || to>=NodeCount())
    {WB_LOG(0,"[WarsBots] NAV explain refused: ready=%d from=%d to=%d nodes=%d",Ready()?1:0,from,to,NodeCount());return;}
    for(int direction=0;direction<2;++direction)
    {
        const int a=direction?to:from,b=direction?from:to;WalkTrace trace;
        const bool allowed=Walkable(m_nodes[a].pos,m_nodes[b].pos,&trace);bool edge=false;
        for(unsigned e=m_nodes[a].firstEdge;e<m_nodes[a].firstEdge+m_nodes[a].edgeCount;++e)edge|=m_edges[e].to==b;
        IEntity* entity=trace.collider?static_cast<IEntity*>(trace.collider->GetForeignData(PHYS_FOREIGN_ID_ENTITY)):nullptr;
        const Vec3& x=m_nodes[a].pos;const Vec3& y=m_nodes[b].pos;
        const int dx=abs(int((x.x-m_min.x)/m_cell)-int((y.x-m_min.x)/m_cell));
        const int dy=abs(int((x.y-m_min.y)/m_cell)-int((y.y-m_min.y)/m_cell));
        WB_LOG(0,"[WarsBots] NAV explain from=%d to=%d candidate=%d storedEdge=%d allowed=%d test=%s sample=%.3f value=%.3f hit=(%.3f,%.3f,%.3f) entity=%u name=%s class=%s surface=%d part=%d",
            a,b,dx<=1&&dy<=1&&dx+dy>0?1:0,edge?1:0,allowed?1:0,trace.test,trace.sample,trace.value,
            trace.point.x,trace.point.y,trace.point.z,entity?entity->GetId():0,entity?entity->GetName():"none",
            entity&&entity->GetClass()?entity->GetClass()->GetName():"static_or_terrain",trace.surface,trace.part);
    }
}

bool BotNav::Step(float budgetMs)
{
	if (m_phase != Phase::Nodes && m_phase != Phase::Edges) return false;
	const CTimeValue start = gEnv->pTimer->GetAsyncTime();
	auto elapsed = [&]() { return (gEnv->pTimer->GetAsyncTime() - start).GetMilliSeconds(); };
	if (m_phase == Phase::Nodes)
	{
		const int total = m_nx * m_ny;
		while (m_cursor < total && elapsed() < budgetMs)
		{
			ScanColumn(m_cursor % m_nx, m_cursor / m_nx);
			++m_cursor;
		}
		if (m_cursor >= total)
		{
			m_phase = Phase::Edges;
			m_cursor = 0;
			m_pending.assign(m_nodes.size(), {});
			WB_LOG(0, "[WarsBots] NAV nodes: %d.", int(m_nodes.size()));
		}
	}
	else
	{
		static const int dirs[8][2] = { {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1} };
		while (m_cursor < int(m_nodes.size()) && elapsed() < budgetMs)
		{
			const Node& node = m_nodes[m_cursor];
			const int ix = int((node.pos.x - m_min.x) / m_cell), iy = int((node.pos.y - m_min.y) / m_cell);
			for (const auto& d : dirs)
			{
				auto it = m_columns.find(Key(ix + d[0], iy + d[1]));
				if (it == m_columns.end()) continue;
				for (int other : it->second)
					if (Walkable(node.pos, m_nodes[other].pos))
						m_pending[m_cursor].push_back({ other, (m_nodes[other].pos - node.pos).GetLength() });
			}
			++m_cursor;
		}
		if (m_cursor >= int(m_nodes.size()))
		{
			for (size_t i = 0; i < m_nodes.size(); ++i)
			{
				m_nodes[i].firstEdge = uint32_t(m_edges.size());
				m_nodes[i].edgeCount = uint16_t(std::min<size_t>(m_pending[i].size(), 0xffff));
				m_edges.insert(m_edges.end(), m_pending[i].begin(), m_pending[i].begin() + m_nodes[i].edgeCount);
			}
			m_pending.clear();
			m_pending.shrink_to_fit();
			m_phase = Phase::Ready;
			m_memory.assign(m_nodes.size(), Memory());
			if (g_pGame && g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel())
				Load(g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel()->GetLevelInfo()->GetName());
			WB_LOG(0, "[WarsBots] NAV ready: %d nodes, %d edges, build %.0f ms of frame budget.",
				int(m_nodes.size()), int(m_edges.size()), m_buildMs + elapsed());
		}
	}
	m_buildMs += elapsed();
	return m_phase != Phase::Ready;
}

void BotNav::Status() const
{
	static const char* names[] = { "idle", "nodes", "edges", "ready" };
	WB_LOG(0, "[WarsBots] NAV phase=%s nodes=%d edges=%d buildMs=%.0f queries=%u.",
		names[int(m_phase)], NodeCount(), EdgeCount(), m_buildMs, m_queries);
}

int BotNav::Nearest(const Vec3& pos) const
{
	const int cx = int((pos.x - m_min.x) / m_cell), cy = int((pos.y - m_min.y) / m_cell);
	int best = -1;
	float bestScore = 1e9f;
	for (int dx = -2; dx <= 2; ++dx)
		for (int dy = -2; dy <= 2; ++dy)
		{
			auto it = m_columns.find(Key(cx + dx, cy + dy));
			if (it == m_columns.end()) continue;
			for (int id : it->second)
			{
				const Vec3& p = m_nodes[id].pos;
				const float dz = fabs_tpl(p.z - pos.z);
				if (dz > 2.5f) continue;                 // a different floor
				const float score = (p - pos).GetLengthSquared2D() + dz * dz * 4.0f;
				if (score < bestScore && Clear(pos + Vec3(0, 0, 0.9f), p + Vec3(0, 0, 0.9f)))
				{ bestScore = score; best = id; }
			}
		}
	return best;
}

bool BotNav::FindPath(const Vec3& start, const Vec3& goal, std::vector<Vec3>& path, PathTrace* trace) const
{
    PathTrace local;PathTrace& info=trace?*trace:local;info=PathTrace();
	path.clear();
    if(m_auditOnly){info.reason="audit_only";return false;}
	if (m_phase != Phase::Ready) return false;
	++m_queries;
	const int from = Nearest(start), to = Nearest(goal);
    info.from=from;info.to=to;
    if (from<0) { info.reason="start_unprojected";return false; }
    if (to<0) { info.reason="goal_unprojected";return false; }
	if (from == to) { info.reason="found";path.push_back(goal); return true; }
	// A* with a node budget; graph is small (thousands of nodes) so allocations per query are fine.
	const size_t n = m_nodes.size();
	std::vector<float> g(n, 1e30f);
	std::vector<int> parent(n, -1);
	std::vector<uint8_t> closed(n, 0);
	typedef std::pair<float, int> Item;
	std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
	g[from] = 0;
	open.push({ (m_nodes[to].pos - m_nodes[from].pos).GetLength(), from });
	int expanded = 0;
	while (!open.empty() && expanded < 6000)
	{
		const int cur = open.top().second;
		open.pop();
		if (closed[cur]) continue;
		closed[cur] = 1;
		++expanded;
		if (cur == to) break;
		const Node& node = m_nodes[cur];
		for (uint32_t e = node.firstEdge; e < node.firstEdge + node.edgeCount; ++e)
		{
			const Edge& edge = m_edges[e];
			// Learned stuck spots make a route expensive, not impossible.
			const float penalty = m_memory.empty() ? 1.0f : 1.0f + 0.6f * min(m_memory[edge.to].stuck, 6.0f);
			const float cost = g[cur] + edge.cost * penalty;
			if (cost < g[edge.to])
			{
				g[edge.to] = cost;
				parent[edge.to] = cur;
				open.push({ cost + (m_nodes[to].pos - m_nodes[edge.to].pos).GetLength(), edge.to });
			}
		}
	}
	info.expanded=expanded;
    if (parent[to] < 0) { info.reason=expanded>=6000?"budget":"no_route";return false; }
    info.reason="found";
	for (int cur = to; cur != from; cur = parent[cur]) path.push_back(m_nodes[cur].pos);
	std::reverse(path.begin(), path.end());
	path.push_back(goal);
	return true;
}

namespace
{
std::filesystem::path MemoryPath(const std::string& level)
{
	std::string safe;
	for (char c : level) safe += (isalnum((unsigned char)c) ? c : '_');
	std::filesystem::path dir = DataDirectory(L"Memory");
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	return dir / (safe + ".mem");
}
}

bool BotNav::Export(const char* requested) const
{
    if (!Ready()) { WB_LOG(0,"[WarsBots] NAV export refused: phase=%d",int(m_phase));return false; }
    const CTimeValue started=gEnv->pTimer->GetAsyncTime();
    std::filesystem::path temp;
    try
    {
        const auto memoryPath=MemoryPath(m_level);
        auto path=requested && *requested ? std::filesystem::u8path(requested) : memoryPath;
        if (!requested || !*requested) path.replace_extension(".nav.json");
        if((!requested || !*requested) && m_auditOnly)path.replace_filename(memoryPath.stem().wstring()+L".audit-"+std::to_wstring(m_kneeVariant)+L".nav.json");
        if (path.extension()!=L".json") { WB_LOG(0,"[WarsBots] NAV export refused: .json destination required");return false; }
        // Read fingerprint only. Export never saves/changes the map memory or graph.
        bool matched=false;
        if (FILE* f=_wfopen(memoryPath.c_str(),L"rb"))
        {
            char magic[8]={};int nx=0,ny=0,n=0;float cell=0,mx=0,my=0;
            matched=fread(magic,1,8,f)==8 && !memcmp(magic,"WBMEM01",7) &&
                fread(&nx,4,1,f)==1 && fread(&ny,4,1,f)==1 && fread(&cell,4,1,f)==1 &&
                fread(&mx,4,1,f)==1 && fread(&my,4,1,f)==1 && fread(&n,4,1,f)==1 &&
                nx==m_nx && ny==m_ny && cell==m_cell && mx==m_min.x && my==m_min.y && n==int(m_nodes.size());
            fclose(f);
        }
        using nlohmann::json;
        const auto rounded=[](float x,int digits) { const double scale=digits==2?100.:1000.;return std::round(double(x)*scale)/scale; };
        json doc={{"schema","wbnav/1"},{"level",m_level},
            {"build",{{"describe",CRYMP_VERSION_STRING},{"exported_at_game_seconds",gEnv->pTimer->GetFrameStartTime().GetSeconds()}}},
            {"params",{{"layers",5},{"headroom",1.9},{"max_rise",1.9},{"max_drop",3.2},{"max_step",.75},
                {"min_normal_z",.7},{"clearance_heights",{.5,1.2,1.75}},{"nearest_radius_columns",2},{"nearest_max_dz",2.5},
                {"astar_budget",6000},{"margin_xy",40},{"margin_z_below",15},{"margin_z_above",35}}},
            {"grid",{{"nx",m_nx},{"ny",m_ny},{"cell",m_cell},{"min_x",m_min.x},{"min_y",m_min.y},{"min_z",m_min.z},{"max_z",m_max.z}}},
            {"water_level",gEnv->p3DEngine?gEnv->p3DEngine->GetWaterLevel():0.f},
            {"counts",{{"nodes",m_nodes.size()},{"edges",m_edges.size()}}},
            {"memory",{{"file",memoryPath.filename().u8string()},{"matched",matched}}}};
        doc["nodes"]=json::array();doc["spawns"]=json::array();
        auto& edges=doc["edges"];
        for (const char* key:{"first","count","to","cost"}) edges[key]=json::array();
        for (const Node& n:m_nodes)
        {
            doc["nodes"].push_back({rounded(n.pos.x,2),rounded(n.pos.y,2),rounded(n.pos.z,2),n.column});
            edges["first"].push_back(n.firstEdge);edges["count"].push_back(n.edgeCount);
        }
        for (const Edge& e:m_edges) { edges["to"].push_back(e.to);edges["cost"].push_back(rounded(e.cost,3)); }
        if (CGameRules* rules=g_pGame->GetGameRules())
            for (int i=0;i<rules->GetSpawnLocationCount();++i)
                if (IEntity* spawn=gEnv->pEntitySystem->GetEntity(rules->GetSpawnLocation(i)))
                { const Vec3 p=spawn->GetWorldPos();doc["spawns"].push_back({spawn->GetId(),rounded(p.x,2),rounded(p.y,2),rounded(p.z,2)}); }
        const std::string bytes=doc.dump();
        temp=path;temp+=L".tmp";
        FILE* f=_wfopen(temp.c_str(),L"wb");
        if (!f) throw std::runtime_error("cannot open temporary export");
        const bool written=fwrite(bytes.data(),1,bytes.size(),f)==bytes.size();
        const bool closed=fclose(f)==0;
        if (!written || !closed) throw std::runtime_error("incomplete export write");
        std::filesystem::rename(temp,path); // Atomic replacement; preserve old export if publication fails.
        WB_LOG(0,"[WarsBots] NAV export: %s nodes=%d edges=%d bytes=%u ms=%.2f",path.u8string().c_str(),NodeCount(),EdgeCount(),unsigned(bytes.size()),
            (gEnv->pTimer->GetAsyncTime()-started).GetMilliSeconds());
        return true;
    }
    catch (const std::exception& e)
    {
        std::error_code ignored;if (!temp.empty()) std::filesystem::remove(temp,ignored);
        WB_LOG(0,"[WarsBots] NAV export failed: %s",e.what());return false;
    }
}

void BotNav::ReportStuck(const Vec3& pos, std::uint32_t actor)
{
	if (m_phase != Phase::Ready) return;
	const int id = Nearest(pos);
	if (id < 0) return;
    WB_LOG(2,"[WarsBots] NAV stuck report actor=%u node=%d pos=(%.2f,%.2f,%.2f)",actor,id,pos.x,pos.y,pos.z);
	m_memory[id].stuck = min(m_memory[id].stuck + 1.0f, 20.0f);
	m_dirty = true; ++m_events;
}
void BotNav::ReportKill(const Vec3& pos)
{
	if (m_phase != Phase::Ready) return;
	const int id = Nearest(pos);
	if (id >= 0) { m_memory[id].kills += 1.0f; m_dirty = true; ++m_events; }
}
void BotNav::ReportDeath(const Vec3& pos)
{
	if (m_phase != Phase::Ready) return;
	const int id = Nearest(pos);
	if (id >= 0) { m_memory[id].deaths += 1.0f; m_dirty = true; ++m_events; }
}

bool BotNav::PickHotspot(unsigned seed, bool preferSafe, Vec3& out, const Vec3* from, std::uint32_t actor) const
{
    if(m_auditOnly)return false;
	if (m_phase != Phase::Ready || m_memory.empty()) return false;
	// Reservoir-style weighted pick over nodes with fight history; cheap enough at 10 Hz/bots.
    const ICVar* all=gEnv->pConsole->GetCVar("wb_hotspot_all_nodes");
    const std::size_t stride=all && all->GetIVal()==1 ? 1 : 2;
    const int fromNode=from && GoalFilterEnabled()?NodeAt(*from):-1;
    unsigned skipped=0;int first=-1;
    const int chosen=BotHotspot::Pick(seed,m_memory.size(),stride,[&](std::size_t i) {
        const Memory& m=m_memory[i];
        const float w=(preferSafe ? m.kills*1.5f-m.deaths*.7f : m.kills+m.deaths)-m.stuck*.5f;
        if(w>.5f && fromNode>=0 && !m_reachability.Closed(fromNode) && !m_reachability.Reaches(fromNode,int(i))){++skipped;if(first<0)first=int(i);return 0.f;}
        return w;
    });
    if(skipped) WB_LOG(2,"[WarsBots] GOAL SKIP unreachable kind=hotspot actor=%u id=%d fromScc=%d toScc=%d skipped=%u",actor,first,Component(fromNode),Component(first),skipped);
	if (chosen < 0) return false;
	out = m_nodes[chosen].pos;
	return true;
}

bool BotNav::GoalFilterEnabled() const
{
    const ICVar* flag=gEnv->pConsole->GetCVar("wb_reachable_goals");
    return flag && flag->GetIVal()==1 && m_reachability.Ready();
}
bool BotNav::KnownUnreachable(int from,const Vec3& goal,BotGoalProjection& trace) const
{
    trace=BotGoalProjection();
    if(!GoalFilterEnabled() || from<0 || m_reachability.Closed(from))return false;
    const int cx=int((goal.x-m_min.x)/m_cell),cy=int((goal.y-m_min.y)/m_cell);
    for(int dx=-2;dx<=2;++dx)for(int dy=-2;dy<=2;++dy) {
        const auto column=m_columns.find(Key(cx+dx,cy+dy));if(column==m_columns.end())continue;
        for(int id:column->second) {
            const Vec3& p=m_nodes[id].pos;const float dz=fabs_tpl(p.z-goal.z);
            if(dz>2.5f)continue;
            // Item-origin LOS can hit the item's own geometry. This is a conservative
            // topology test, not FindPath: retain if ANY plausible nearby floor is reachable.
            trace.Consider(id,p.z,(p-goal).GetLengthSquared2D()+4.f*dz*dz,m_reachability.Reaches(from,id));
        }
    }
    return trace.Unreachable();
}
void BotNav::Tick(float dt)
{
	if (m_phase != Phase::Ready) return;
    const ICVar* filter=gEnv->pConsole->GetCVar("wb_reachable_goals");
    const ICVar* escape=gEnv->pConsole->GetCVar("wb_sink_escape");
    if(!m_componentsAttempted && ((filter && filter->GetIVal()==1) || (escape && escape->GetIVal()==1)))
    {
        m_componentsAttempted=true;const CTimeValue start=gEnv->pTimer->GetAsyncTime();
        std::vector<std::vector<int>> graph(m_nodes.size());
        for(std::size_t i=0;i<m_nodes.size();++i)for(unsigned e=m_nodes[i].firstEdge;e<m_nodes[i].firstEdge+m_nodes[i].edgeCount;++e)graph[i].push_back(m_edges[e].to);
        const bool ok=m_reachability.Build(graph);std::vector<int> sizes;
        for(int c=0;c<m_reachability.Count();++c)sizes.push_back(m_reachability.Size(c));
        std::sort(sizes.rbegin(),sizes.rend());
        WB_LOG(1,"[WarsBots] NAV SCC ready=%d components=%d largest=%d second=%d ms=%.3f",ok?1:0,m_reachability.Count(),sizes.empty()?0:sizes[0],sizes.size()<2?0:sizes[1],(gEnv->pTimer->GetAsyncTime()-start).GetMilliSeconds());
    }
	m_saveTimer += dt;
	if (m_dirty && m_saveTimer > 120.0f) { Save(); m_saveTimer = 0; }
}

void BotNav::Load(const char* level)
{
	m_level = level ? level : "";
	if (m_level.empty()) return;
	const std::filesystem::path path = MemoryPath(m_level);
	FILE* f = _wfopen(path.c_str(), L"rb");
	if (!f) { WB_LOG(0, "[WarsBots] MAP MEMORY: none yet for %s (learning from scratch).", m_level.c_str()); return; }
	char magic[8] = {};
	int nx = 0, ny = 0, count = 0;
	float cell = 0, mx = 0, my = 0;
	bool ok = fread(magic, 1, 8, f) == 8 && !memcmp(magic, "WBMEM01", 7) &&
		fread(&nx, 4, 1, f) == 1 && fread(&ny, 4, 1, f) == 1 && fread(&cell, 4, 1, f) == 1 &&
		fread(&mx, 4, 1, f) == 1 && fread(&my, 4, 1, f) == 1 && fread(&count, 4, 1, f) == 1;
	// Only reuse memory for the identical graph (same map geometry and build parameters).
	ok = ok && nx == m_nx && ny == m_ny && cell == m_cell && mx == m_min.x && my == m_min.y && count == int(m_memory.size());
	if (ok) ok = fread(m_memory.data(), sizeof(Memory), m_memory.size(), f) == m_memory.size();
	fclose(f);
	if (!ok) { m_memory.assign(m_nodes.size(), Memory()); WB_LOG(0, "[WarsBots] MAP MEMORY: %s does not match this graph; starting fresh.", m_level.c_str()); return; }
	float stuck = 0, kills = 0, deaths = 0;
	for (const Memory& m : m_memory) { stuck += m.stuck; kills += m.kills; deaths += m.deaths; }
	WB_LOG(0, "[WarsBots] MAP MEMORY: loaded %s (stuck %.0f, kills %.0f, deaths %.0f).", m_level.c_str(), stuck, kills, deaths);
}

void BotNav::Save() const
{
    if(m_auditOnly)return;
	if (m_level.empty() || m_memory.empty()) return;
	const std::filesystem::path path = MemoryPath(m_level);
	FILE* f = _wfopen(path.c_str(), L"wb");
	if (!f) return;
	const int count = int(m_memory.size());
	fwrite("WBMEM01", 1, 8, f);
	fwrite(&m_nx, 4, 1, f); fwrite(&m_ny, 4, 1, f); fwrite(&m_cell, 4, 1, f);
	fwrite(&m_min.x, 4, 1, f); fwrite(&m_min.y, 4, 1, f); fwrite(&count, 4, 1, f);
	fwrite(m_memory.data(), sizeof(Memory), m_memory.size(), f);
	fclose(f);
	const_cast<BotNav*>(this)->m_dirty = false;
	WB_LOG(1, "[WarsBots] MAP MEMORY: saved %s (%u new events).", m_level.c_str(), m_events);
}
}
