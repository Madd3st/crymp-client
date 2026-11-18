
#include <cstdio>
#include <cmath>
#include <vector>
#include "CryGame/Bots/BotPsHold.h"
#include "CryGame/Bots/BotPSCapture.h"
#include "CryGame/Bots/BotTeamOrders.h"
using namespace WarsBots;
struct Vec3 { float x = 0, y = 0, z = 0; Vec3() {} Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& b) const { return Vec3(x + b.x, y + b.y, z + b.z); }
    Vec3 operator-(const Vec3& b) const { return Vec3(x - b.x, y - b.y, z - b.z); }
    Vec3 operator*(float a) const { return Vec3(x * a, y * a, z * a); }
    float GetLength() const { return std::sqrt(x * x + y * y + z * z); }
    void NormalizeSafe(const Vec3& f) { const float l = GetLength(); if (l > .001f) { x /= l; y /= l; z /= l; } else *this = f; } };
struct CPlayer { unsigned GetEntityId() const { return 7; } };
struct PsHoldInfo { bool contested = false, approach = false; float ax = 0, ay = 0, az = 0; };
bool PSHoldInfo(int, int, PsHoldInfo&) { return true; }
void PSHoldClaim(int, unsigned, const float*) {}
void PSHoldOthers(int, unsigned, std::vector<float>&) {}
struct NavMeshCore { bool Ready() { return true; } bool Project(const float*, float*, float = 0, float = 0, float = 0) { return false; }
    bool FindPath(const float*, const float*, std::vector<float>&) { return false; } } g_mesh;
NavMeshCore* CurrentNavMesh() { return &g_mesh; }
bool EyeRayClear(CPlayer&, const Vec3&, const Vec3&) { return true; }
float Rand01() { return .5f; }
static int g_logs = 0;
#define WB_LOG(...) (++g_logs)
#define WBI(field, value) (m_intent.field = (value))
#define WBI_MOVETO(d_, l_) (m_intent.move = true, m_intent.destination = (d_))
struct Intent { bool move = false; Vec3 aim, destination; };
class BotBrain { public:
    int m_number = 1, m_psHoldTarget = 64; unsigned m_actorId = 7; float m_now = 10;
    bool m_psHoldCapture = false, m_psHoldHas = true, m_psHoldBack = false;
    HoldTimer m_psHoldTimer; Vec3 m_psHoldPos = Vec3(17.5f, 0, 103); float m_psHoldDir[2] = {1, 0};
    struct { float damagedUntil = -1; } m_blackboard; Intent m_intent; int steers = 0; Vec3 steerGoal;
    BotBrain() { m_psHoldTimer.Assign(10, 30); }
    void ResetPSHold(int) { m_psHoldTarget = 0; m_psHoldHas = false; m_psHoldTimer.Reset(); }
    Vec3 Steer(const Vec3&, const Vec3& g) { ++steers; steerGoal = g; return g; }
    void TravelGait(float) {}
    bool UpdatePSHold(CPlayer&, const PsCapturePoint&, int, const Vec3&);
};
bool BotBrain::UpdatePSHold(CPlayer& actor,const PsCapturePoint& point,int team,const Vec3& feet)
{
	const unsigned self=unsigned(actor.GetEntityId());
	if(m_psHoldTarget!=point.id){ResetPSHold(team);m_psHoldTarget=point.id;}
	PsHoldInfo info; PSHoldInfo(team,point.id,info);
	if(info.contested)
	{
		if(!m_psHoldCapture)WB_LOG(1,"[WarsBots] #%d PS HOLD bot=%u target=%d pos=(%.1f,%.1f,%.1f) reason=capture t=%.1f.",m_number,self,point.id,point.x,point.y,point.z,m_now);
		m_psHoldCapture=true;
		return false;
	}
	const HoldConfig cfg;
	const float hitAt=m_blackboard.damagedUntil>0.0f?m_blackboard.damagedUntil-3.0f:-1e9f;   // damagedUntil = hit + 3 s
	HoldReason why=m_psHoldTimer.Due(m_now,hitAt);
	if(m_psHoldCapture){m_psHoldCapture=false;why=HoldReason::Back;}
	else if(why==HoldReason::None&&m_psHoldBack&&m_psHoldHas){m_psHoldBack=false;
		WB_LOG(1,"[WarsBots] #%d PS HOLD bot=%u target=%d pos=(%.1f,%.1f,%.1f) reason=back t=%.1f.",m_number,self,point.id,m_psHoldPos.x,m_psHoldPos.y,m_psHoldPos.z,m_now);}
	if(why!=HoldReason::None)
	{
		m_psHoldBack=false;
		NavMeshCore* mesh=CurrentNavMesh();
		const Vec3 c(point.x,point.y,point.z);
		const HoldApproach ap=info.approach?MakeApproach(c.x,c.y,info.ax,info.ay):HoldApproach();
		std::vector<HoldCandidate> cands;
		const float c0[3]={c.x,c.y,c.z}; float cs[3]={c.x,c.y,c.z};
		const bool centerOn=mesh&&mesh->Ready()&&mesh->Project(c0,cs,2.0f,2.0f,4.0f);
		if(centerOn)
			for(float ang:SectorDirections(ap,cfg.sectorDeg))
				for(float r:{cfg.ringMin,(cfg.ringMin+cfg.ringMax)*0.5f,cfg.ringMax})
				{
					const float q[3]={c.x+std::cos(ang)*r,c.y+std::sin(ang)*r,c.z};
					float s[3]; HoldCandidate h;
					if(!mesh->Project(q,s,1.5f,6.0f,6.0f))continue;
					h.onMesh=true; h.x=s[0]; h.y=s[1]; h.z=s[2];
					std::vector<float> xyz;
					if(mesh->FindPath(cs,s,xyz))
					{
						float len=0.0f; Vec3 prev(cs[0],cs[1],cs[2]);
						for(size_t i=0;i+2<xyz.size();i+=3){const Vec3 p(xyz[i],xyz[i+1],xyz[i+2]);len+=(p-prev).GetLength();prev=p;}
						h.path=len;
					}
					if(h.path>cfg.pathMax)continue;
					const Vec3 p(s[0],s[1],s[2]);
					Vec3 d=ap.valid?Vec3(ap.x,ap.y,0):Vec3(p.x-c.x,p.y-c.y,0); d.NormalizeSafe(Vec3(1,0,0));
					const bool lowBlocked=!EyeRayClear(actor,p+Vec3(0,0,0.8f),p+Vec3(0,0,0.8f)+d*3.0f);
					const bool highFree=EyeRayClear(actor,p+Vec3(0,0,1.6f),p+Vec3(0,0,1.6f)+d*30.0f);
					h.cover=lowBlocked&&highFree;
					h.seesCenter=EyeRayClear(actor,p+Vec3(0,0,1.6f),c+Vec3(0,0,1.0f));
					cands.push_back(h);
				}
		std::vector<float> flat; PSHoldOthers(team,self,flat);
		std::vector<HoldPos> others;
		for(size_t i=0;i+2<flat.size();i+=3)others.push_back(HoldPos{flat[i],flat[i+1],flat[i+2]});
		const int pick=PickHold(cands,c.x,c.y,c.z,ap,point.kind==int(PsKind::Energy),others,cfg);
		m_psHoldTimer.Assign(m_now,HoldPeriod(Rand01(),cfg));
		if(pick<0)
		{
			if(m_psHoldHas||why==HoldReason::Start)
				WB_LOG(1,"[WarsBots] #%d PS HOLD bot=%u target=%d pos=none candidates=%d sector=%d reason=%s t=%.1f.",m_number,self,point.id,int(cands.size()),ap.valid?1:0,HoldReasonName(why),m_now);
			m_psHoldHas=false; PSHoldClaim(team,self,nullptr);
		}
		else
		{
			const HoldCandidate& h=cands[size_t(pick)];
			m_psHoldPos=Vec3(h.x,h.y,h.z); m_psHoldHas=true;
			m_psHoldDir[0]=ap.valid?ap.x:(h.x-c.x); m_psHoldDir[1]=ap.valid?ap.y:(h.y-c.y);
			const float pos[3]={h.x,h.y,h.z}; PSHoldClaim(team,self,pos);
			WB_LOG(1,"[WarsBots] #%d PS HOLD bot=%u target=%d pos=(%.1f,%.1f,%.1f) score=%d cover=%d up=%.1f sector=%d reason=%s t=%.1f.",
				m_number,self,point.id,h.x,h.y,h.z,HoldScore(h,c.z),h.cover?1:0,h.z-c.z,ap.valid?1:0,HoldReasonName(why),m_now);
		}
	}
	if(!m_psHoldHas)return false;
	const Vec3 off(m_psHoldPos.x-feet.x,m_psHoldPos.y-feet.y,m_psHoldPos.z-feet.z);
	if(!HoldArrived(off.x,off.y,off.z))   // Codex #44: the upper floor is only reached at its height, keep the path
	{
		const Vec3 step=Steer(feet,m_psHoldPos);
		WBI_MOVETO(step,step+Vec3(0,0,1.2f));
		TravelGait(off.GetLength());
	}
	else
	{
		WBI(move,false);   // on the position: watch the approach at eye height (p. 6)
		Vec3 d(m_psHoldDir[0],m_psHoldDir[1],0); d.NormalizeSafe(Vec3(1,0,0));
		WBI(aim,m_psHoldPos+d*20.0f+Vec3(0,0,1.5f));
	}
	return true;
}

int main() {
    int fails = 0;
    auto check = [&](bool c, const char* m) { if (!c) { std::printf("FAIL: %s\n", m); ++fails; } };
    CPlayer a; PsCapturePoint point; point.id = 64; point.kind = int(PsKind::Energy); point.hasDestination = true;
    // 1. same XY, upper position 3 m above: keep the route
    { BotBrain b; b.UpdatePSHold(a, point, 1, Vec3(17.5f, 0, 100));
      check(b.steers == 1 && b.m_intent.move && std::fabs(b.steerGoal.z - 103.f) < 0.01f, "under the upper position: Steer to it, no stand"); }
    // 2. at its height: hold and watch
    { BotBrain b; b.UpdatePSHold(a, point, 1, Vec3(17.5f, 0, 103));
      check(b.steers == 0 && !b.m_intent.move, "on the position: stop and watch"); }
    // 3. two floors with a detour: from beside the building the route goes to the upper position
    { BotBrain b; b.UpdatePSHold(a, point, 1, Vec3(25.f, 4.f, 100));
      check(b.steers == 1 && b.m_intent.move && std::fabs(b.steerGoal.x - 17.5f) < 0.01f && std::fabs(b.steerGoal.z - 103.f) < 0.01f, "beside the building: route to the upper floor");
      b.m_intent = Intent(); b.UpdatePSHold(a, point, 1, Vec3(18.0f, 0.5f, 100.2f));   // on the stairs below
      check(b.steers == 2 && b.m_intent.move, "below on the way: still routing");
      b.m_intent = Intent(); b.UpdatePSHold(a, point, 1, Vec3(17.8f, 0.3f, 102.7f));   // arrived upstairs
      check(b.steers == 2 && !b.m_intent.move, "upstairs: arrived, hold"); }
    // 4. back after combat under the upper position: Steer again until the height is reached
    { BotBrain b; b.m_psHoldBack = true; const int logs = g_logs;
      b.UpdatePSHold(a, point, 1, Vec3(17.5f, 0, 100));
      check(g_logs == logs + 1 && !b.m_psHoldBack, "reason=back logged once");
      check(b.steers == 1 && b.m_intent.move, "back after combat below: route up");
      b.m_intent = Intent(); b.UpdatePSHold(a, point, 1, Vec3(17.5f, 0, 103));
      check(b.steers == 1 && !b.m_intent.move, "back on the position: hold"); }
    // 5. above the position (a roof over it): not arrived
    { BotBrain b; b.UpdatePSHold(a, point, 1, Vec3(17.5f, 0, 106));
      check(b.steers == 1 && b.m_intent.move, "on a roof over the position: route down to it"); }
    // 6. one floor, 0.4 m snap difference: arrived
    { BotBrain b; b.UpdatePSHold(a, point, 1, Vec3(17.0f, 0.6f, 102.6f));
      check(b.steers == 0 && !b.m_intent.move, "one floor, small snap offset: arrived"); }
    if (fails) { std::printf("SPEC-005b hold arrival caller FAILED (%d)\n", fails); return 1; }
    std::printf("SPEC-005b hold arrival caller PASS\n");
    return 0;
}
