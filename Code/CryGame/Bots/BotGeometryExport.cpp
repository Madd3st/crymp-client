// WarsBots diagnostics. See LICENSE.txt for Crytek SDK terms.
#include "PortIncludes.h"
#include "BotGeometryExport.h"
#include "BotGeometryMesh.h"
#include "BotDataPath.h"
#include "BotLog.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryCommon/Cry3DEngine/I3DEngine.h"
#include "CryCommon/CryPhysics/physinterface.h"
#include "CryCommon/CryAction/IItemSystem.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "nlohmann/json.hpp"
#include "version.h"
#include <filesystem>
#include <fstream>
#include <locale>
#include <locale.h>
#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>
namespace WarsBots {
namespace {
using nlohmann::json;
using File=std::unique_ptr<FILE,decltype(&fclose)>;
struct NumericLocale {
 _locale_t handle=_create_locale(LC_NUMERIC,"C");
 ~NumericLocale(){if(handle)_free_locale(handle);}
 operator _locale_t()const{return handle;}
};
struct Writer {
 File file;NumericLocale numeric;AABB bounds;uint64_t faces=0,vertices=0,rejected=0;
 Writer(const std::filesystem::path& path,const AABB& b,const char* map):file(_wfopen(path.c_str(),L"wb"),fclose),bounds(b){
  if(!numeric)throw std::runtime_error("numeric locale unavailable");
  if(!file)throw std::runtime_error("cannot open temporary OBJ");
  setvbuf(file.get(),nullptr,_IOFBF,65536);
  _fprintf_l(file.get(),"# WarsBots static collision export; Z-up; metres\n# map=%s sha=%s\n# bounds=%.3f %.3f %.3f %.3f %.3f %.3f\n",numeric,map,CRYMP_GIT_SHA,b.min.x,b.min.y,b.min.z,b.max.x,b.max.y,b.max.z);
 }
 void Triangle(const Vec3& a,const Vec3& b,const Vec3& c){
  if(!a.IsValid() || !b.IsValid() || !c.IsValid())throw std::runtime_error("nonfinite geometry");
  if((b-a).Cross(c-a).len2()<1e-12f){++rejected;return;}
  AABB box(a,a);box.Add(b);box.Add(c);
  if(!bounds.IsIntersectBox(box))return;
  if(faces>=15000000)throw std::runtime_error("15M triangle safety limit reached");
  // Stream, bounded memory; duplicate vertices intentionally avoid a map-sized hash table.
  const int result=_fprintf_l(file.get(),"v %.6f %.6f %.6f\nv %.6f %.6f %.6f\nv %.6f %.6f %.6f\nf %llu %llu %llu\n",numeric,a.x,a.y,a.z,b.x,b.y,b.z,c.x,c.y,c.z,vertices+1,vertices+2,vertices+3);
  if(result<0)throw std::runtime_error("OBJ write failed");
  vertices+=3;++faces;
 }
 void Finish(){FILE* p=file.release();if(fclose(p)!=0)throw std::runtime_error("OBJ close failed");}
};
Vec3 V(const GeometryMesh::Point& p){return Vec3(p.x,p.y,p.z);}
json Position(const Vec3& p){return {p.x,p.y,p.z};}
void Save(const std::filesystem::path& path,const json& doc){
 std::ofstream stream(path,std::ios::binary);stream.exceptions(std::ios::failbit|std::ios::badbit);stream<<doc.dump(2)<<'\n';stream.close();
}
void Publish(const std::filesystem::path& temp,const std::filesystem::path& path){std::filesystem::rename(temp,path);}
}
bool ExportLevelGeometry(const char* requested){
 std::vector<std::filesystem::path> temporary;
 try{
  if(!gEnv->pPhysicalWorld || !gEnv->p3DEngine || !g_pGame)throw std::runtime_error("level physics unavailable");
  auto* framework=g_pGame->GetIGameFramework();auto* level=framework->GetILevelSystem()->GetCurrentLevel();
  if(!level)throw std::runtime_error("no level");
  const char* name=level->GetLevelInfo()->GetName();std::string key=name;
  for(char& c:key)if(!std::isalnum(static_cast<unsigned char>(c)) && c!='-')c='_';
  const auto dir=DataDirectory(L"Geometry");
  std::filesystem::path path=requested && *requested?std::filesystem::u8path(requested):dir/(key+".obj");
  if(path.extension()!=L".obj")throw std::runtime_error(".obj destination required");
  auto stem=path;stem.replace_extension();
  const auto sibling=[&](const char* suffix){auto p=stem;p+=suffix;return p;};
  auto objTemp=path;objTemp+=L".tmp";temporary.push_back(objTemp);
  auto spawnPath=sibling(".spawns.json"),itemPath=sibling(".items.json"),metaPath=sibling(".geometry.json");
  for(const auto& existing:{path,spawnPath,itemPath,metaPath})if(std::filesystem::exists(existing))throw std::runtime_error("destination exists; choose another .obj name");
  for(const auto& p:{spawnPath,itemPath,metaPath}){auto t=p;t+=L".tmp";temporary.push_back(t);}
  const int terrainSize=gEnv->p3DEngine->GetTerrainSize();
  if(terrainSize<=0 || terrainSize>16384)throw std::runtime_error("invalid terrain bounds");
  // Whole terrain footprint, not the old spawn-centred BotNav grid. Z spans engine map space.
  AABB bounds(Vec3(0,0,-4096),Vec3(float(terrainSize),float(terrainSize),4096));
  Writer writer(objTemp,bounds,name);
  IPhysicalEntity** found=nullptr;
  // Static brushes + terrain, plus immovable entity geometry (mass 0 rigid bodies): Power Struggle HQs, factories,
  // spawn bunkers and other structures are entities, not brushes (C-073 Shore crop had only terrain holes there).
  const int count=gEnv->pPhysicalWorld->GetEntitiesInBox(bounds.min,bounds.max,found,ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid|ent_independent|ent_ignore_noncolliding);
  std::vector<IPhysicalEntity*> entities;if(count>0)entities.assign(found,found+count);
  // X-070 diagnosis: how are the Power Struggle structures physicalized (if at all) in an actorless export?
  json psStructures=json::array();
  if(gEnv->pEntitySystem){
   IEntityItPtr it=gEnv->pEntitySystem->GetEntityIterator();
   while(IEntity* e=it->Next()){
    const char* c=e->GetClass()?e->GetClass()->GetName():"";
    static const char* kinds[]={"HQ","Factory","SpawnGroup","AlienEnergyPoint","BuyZone","AutoTurret"};
    bool ps=false;for(const char* k:kinds)if(!_stricmp(c,k))ps=true;
    if(!ps || psStructures.size()>=64)continue;
    IPhysicalEntity* pe=e->GetPhysics();int pt=pe?pe->GetType():-1;float mass=-1;
    if(pe){pe_status_dynamics dyn;if(pe->GetStatus(&dyn))mass=dyn.mass;}
    int slots=0;for(int sl=0;sl<e->GetSlotCount();++sl)if(e->GetStatObj(sl))++slots;
    psStructures.push_back({{"class",c},{"name",e->GetName()},{"physics_type",pt},{"mass",mass},{"statobj_slots",slots},{"pos",Position(e->GetWorldPos())}});
   }
  }
  uint64_t parts=0,skipped=0,proxy=0,unsupported=0,terrain=0,structures=0;json types=json::object(),structureClasses=json::object(),skippedClasses=json::object();
  for(auto* entity:entities){
   if(!entity)continue;
   const int peType=entity->GetType();
   if(peType!=PE_STATIC){
    // immovable structures only: mass 0, not doors (open/close at runtime), not items/vehicles. X-070: Shore had 0
    // structures and Plantation skipped 764 rigid bodies - record why each non-static body was skipped.
    pe_status_dynamics dyn;const bool hasDyn=entity->GetStatus(&dyn)!=0;
    IEntity* owner=gEnv->pEntitySystem?gEnv->pEntitySystem->GetEntityFromPhysics(entity):nullptr;
    const char* cls=owner&&owner->GetClass()?owner->GetClass()->GetName():"<no owner>";
    {
     const std::string key=std::string(cls)+" type="+std::to_string(peType)+(hasDyn&&dyn.mass>0.0f?" moving":" mass0");
     skippedClasses[key]=skippedClasses.value(key,uint64_t(0))+1;
    }
    if(peType!=PE_RIGID || !hasDyn || dyn.mass>0.0f)continue;
    if(!owner || !_stricmp(cls,"Door") || (g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(owner->GetId())) ||
       g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(owner->GetId()))continue;
    ++structures;structureClasses[cls]=structureClasses.value(cls,uint64_t(0))+1;
   }
   // PE_STATIC: brushes and terrain
   pe_status_nparts np;const int n=entity->GetStatus(&np);
   for(int part=0;part<n;++part){
    pe_status_pos pose;pose.ipart=part;if(!entity->GetStatus(&pose)){++skipped;continue;}
    // Do not discard all foliage/proxy flags: solid parts can carry those bits too.
    if(!(pose.flagsOR&(geom_colltype0|geom_colltype_player)) || (pose.flagsOR&geom_no_coll_response)){++skipped;continue;}
    IGeometry* geom=pose.pGeomProxy?pose.pGeomProxy:pose.pGeom;if(!geom){++skipped;continue;}
    if(pose.pGeomProxy)++proxy;++parts;
    const auto world=[&](const Vec3& p){return pose.pos+pose.q*(p*pose.scale);};
    const auto* data=geom->GetData();const int type=geom->GetType();
    const auto typeKey=std::to_string(type);types[typeKey]=types.value(typeKey,uint64_t(0))+1;
    switch(type){
    case GEOM_TRIMESH:{
     const auto* mesh=static_cast<const mesh_data*>(data);
     if(!mesh || mesh->nVertices<0 || mesh->nTris<0)throw std::runtime_error("invalid mesh");
     for(int i=0;i<mesh->nTris;++i){
      const auto a=mesh->pIndices[i*3],b=mesh->pIndices[i*3+1],c=mesh->pIndices[i*3+2];
      if(a>=mesh->nVertices || b>=mesh->nVertices || c>=mesh->nVertices)throw std::runtime_error("invalid mesh index");
      writer.Triangle(world(mesh->pVertices[a]),world(mesh->pVertices[b]),world(mesh->pVertices[c]));
     }break;}
    case GEOM_HEIGHTFIELD:{
     const auto* h=static_cast<const primitives::heightfield*>(data);
     if(!h || !h->fpGetHeightCallback || !h->fpGetSurfTypeCallback || h->size.x<=0 || h->size.y<=0 || uint64_t(h->size.x)*h->size.y>8000000)throw std::runtime_error("invalid heightfield");
     const auto point=[&](int x,int y){Vec3 p(x*h->step.x,y*h->step.y,h->getheight(x,y));if(h->bOriented)p=h->Basis.GetTransposed()*p;return world(h->origin+p);};
     for(int y=0;y<h->size.y;++y)for(int x=0;x<h->size.x;++x){
      if(h->gettype(x,y)<0)continue; // native gettype maps holes to -1, not typehole.
      Vec3 a=point(x,y),b=point(x+1,y),c=point(x+1,y+1),d=point(x,y+1);
      writer.Triangle(a,b,c);writer.Triangle(a,c,d);++terrain;
     }break;}
    case GEOM_BOX:{
     const auto* b=static_cast<const primitives::box*>(data);if(!b)throw std::runtime_error("null box");
     GeometryMesh::Box([&](const GeometryMesh::Triangle& t){Vec3 v[3];for(int i=0;i<3;++i){Vec3 p(t[i].x*b->size.x,t[i].y*b->size.y,t[i].z*b->size.z);if(b->bOriented)p=b->Basis.GetTransposed()*p;v[i]=world(b->center+p);}writer.Triangle(v[0],v[1],v[2]);});break;}
    case GEOM_CYLINDER:case GEOM_CAPSULE:{
     const auto* c=static_cast<const primitives::cylinder*>(data);if(!c || c->r<=0 || c->hh<0)throw std::runtime_error("invalid axial primitive");
     Vec3 z=c->axis.GetNormalizedSafe(Vec3(0,0,1)),x=z.GetOrthogonal().GetNormalized(),y=z.Cross(x);
     const auto sink=[&](const GeometryMesh::Triangle& t){Vec3 v[3];for(int i=0;i<3;++i)v[i]=world(c->center+x*t[i].x+y*t[i].y+z*t[i].z);writer.Triangle(v[0],v[1],v[2]);};
     if(type==GEOM_CAPSULE)GeometryMesh::Capsule(c->r,c->hh,sink);else GeometryMesh::Cylinder(c->r,c->hh,sink);break;}
    case GEOM_SPHERE:{const auto* s=static_cast<const primitives::sphere*>(data);if(!s || s->r<=0)throw std::runtime_error("invalid sphere");GeometryMesh::Capsule(s->r,0,[&](const GeometryMesh::Triangle& t){writer.Triangle(world(s->center+V(t[0])),world(s->center+V(t[1])),world(s->center+V(t[2])));});break;}
    default:++unsupported;break;
    }
   }
  }
  if(!writer.faces || !terrain)throw std::runtime_error("incomplete export: player collision triangles/heightfield missing");
  json spawns=json::array(),items=json::array();
  if(auto* rules=g_pGame->GetGameRules())for(int i=0;i<rules->GetSpawnLocationCount();++i)if(auto* e=gEnv->pEntitySystem->GetEntity(rules->GetSpawnLocation(i)))spawns.push_back({{"id",e->GetId()},{"position",Position(e->GetWorldPos())}});
  auto iterator=gEnv->pEntitySystem->GetEntityIterator();
  for(iterator->MoveFirst();!iterator->IsEnd();iterator->Next())if(auto* e=iterator->This()){
   IItem* item=framework->GetIItemSystem()->GetItem(e->GetId());std::string cls=e->GetClass()->GetName(),lower=cls;
   for(char& c:lower)c=char(std::tolower(static_cast<unsigned char>(c)));
   if((item && !item->GetOwnerId()) || lower.find("ammo")!=std::string::npos)items.push_back({{"id",e->GetId()},{"class",cls},{"name",e->GetName()},{"position",Position(e->GetWorldPos())},{"kind",item && item->GetIWeapon()?"weapon":"item_or_ammo"}});
  }
  json meta={{"schema","wbgeometry/1"},{"map",name},{"build",CRYMP_VERSION_STRING},{"sha",CRYMP_GIT_SHA},{"up_axis","Z"},{"units","metres"},{"bounds",{Position(bounds.min),Position(bounds.max)}},{"static_entities",entities.size()},{"parts",parts},{"collision_proxy_parts",proxy},{"skipped_parts",skipped},{"unsupported_parts",unsupported},{"geometry_types",types},{"vertices",writer.vertices},{"triangles",writer.faces},{"degenerate_skipped",writer.rejected},{"terrain_cells",terrain},{"structure_entities",structures},{"structure_classes",structureClasses},{"nonstatic_bodies",skippedClasses},{"ps_structures",psStructures},{"primitive_segments",16},{"heightmap_unit",gEnv->p3DEngine->GetHeightMapUnitSize()}};
  writer.Finish();Save(temporary[1],{{"schema","wbspawns/1"},{"map",name},{"spawns",spawns}});Save(temporary[2],{{"schema","wbitems/1"},{"map",name},{"items",items}});Save(temporary[3],meta);
  // Publish metadata last; its presence is the completion marker for the bundle.
  Publish(objTemp,path);Publish(temporary[1],spawnPath);Publish(temporary[2],itemPath);Publish(temporary[3],metaPath);
  WB_LOG(0,"[WarsBots] GEOM export path=%s triangles=%llu vertices=%llu bytes=%llu unsupported=%llu terrainCells=%llu",path.u8string().c_str(),writer.faces,writer.vertices,uint64_t(std::filesystem::file_size(path)),unsupported,terrain);
  return true;
 }catch(const std::exception& e){for(const auto& t:temporary){std::error_code ec;std::filesystem::remove(t,ec);}WB_LOG(0,"[WarsBots] GEOM export FAILED: %s",e.what());return false;}
}
}
