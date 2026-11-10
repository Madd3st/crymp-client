
#define WARSBOTS_EXPERIMENTAL_SPAWNER 1
#define WB_LOG(...) ((void)0)
#include <cmath>
#include <cstdio>
#include <algorithm>
using std::max;
float cry_floorf(float f){return std::floor(f);} float cry_fabsf(float f){return std::fabs(f);}
struct Vec3 {float x=0,y=0,z=0;Vec3(){} Vec3(float a,float b,float c):x(a),y(b),z(c){}
 Vec3 operator-(const Vec3&v)const{return Vec3(x-v.x,y-v.y,z-v.z);}
 float len2()const{return x*x+y*y+z*z;} float len()const{return std::sqrt(len2());}
 void NormalizeSafe(){float n=len();if(n>0)*this/=n;}
 Vec3& operator/=(float a){x/=a;y/=a;z/=a;return *this;}
 Vec3& operator*=(float a){x*=a;y*=a;z*=a;return *this;}
 Vec3& operator+=(const Vec3&v){x+=v.x;y+=v.y;z+=v.z;return *this;}
};
enum {STANCE_STAND,ACTION_SPRINT=1,ACTION_JUMP=2,NANOMODE_SPEED=3,eCMT_Fly=4};
struct Timer{float GetFrameTime(){return .016f;}};
struct Renderer{void Draw2dLabel(...) {}};
struct Env{bool bServer=true;Timer timer;Renderer renderer;Timer*pTimer=&timer;Renderer*pRenderer=&renderer;}env,*gEnv=&env;
struct Cvars{int pl_debug_ladders=0;}cvars,*g_pGameCVars=&cvars;
namespace WarsBots{struct BotConfig{int ladder=1;static BotConfig&Get(){static BotConfig c;return c;}};}
struct Manager{bool owns=true;bool Owns(unsigned){return owns;}};
struct Game{Manager mgr;Manager&GetBotManager(){return mgr;}}game,*g_pGame=&game;
struct Suit{int GetMode(){return 0;}float GetSprintMultiplier(bool){return 1;}};
struct CPlayer{bool bot=true,ready=false,frozen=false;int health=100;Suit suit;
 enum {eLS_Climb,eLDIR_Up,eLDIR_Down,eLAT_Jump}; using ELadderDirection=int;
 bool IsWarsBotActor()const{return bot;}unsigned GetEntityId()const{return 7;}
 int GetHealth(){return health;}bool IsFrozen(){return frozen;}float GetStanceMaxSpeed(int){return 4.5f;}
 Suit*GetNanoSuit(){return &suit;}void RequestLeaveLadder(int){}
 bool UpdateLadderAnimation(int,int,float){return ready;}
};
static bool WarsBotLadderServer(const CPlayer& player)
{
	return gEnv && gEnv->bServer && g_pGame && player.IsWarsBotActor() && WarsBots::BotConfig::Get().ladder == 1 &&
		g_pGame->GetBotManager().Owns(player.GetEntityId());
}
struct Movement{
 CPlayer&m_player;struct{bool isRagDoll=false;Vec3 ladderTop{0,0,20},ladderBottom{0,0,10};}m_stats;
 struct{Vec3 desiredVelocity{0,1,0};}m_movement;
 struct{int type=0;Vec3 velocity;}m_request;
 int m_actions=0;
 Movement(CPlayer&p):m_player(p){}
 void AdjustPlayerPositionOnLadder(CPlayer&){}
 void Climb(CPlayer&player){float bottomDist=0;
	Vec3 move(m_stats.ladderTop - m_stats.ladderBottom);
	move.NormalizeSafe();
	move *= m_movement.desiredVelocity.y * 0.5f;// * (dirDot>0.0f?1.0f:-1.0f) * min(1.0f,fabs(dirDot)*5);

	//cap the movement vector to max 1
	float moveModule(move.len());

	if (moveModule > 1.0f)
		move /= moveModule;

	move *= m_player.GetStanceMaxSpeed(STANCE_STAND) * 0.5f;

	//player.m_stats.bSprinting = false;		//Manual update here (if not suit doensn't decrease energy and so on...)
	if (m_actions & ACTION_SPRINT)
	{
		if ((move.len2() > 0.1f))
		{
			move *= 1.2f;
			if (player.GetNanoSuit() && (player.GetNanoSuit()->GetMode() == NANOMODE_SPEED))
				move *= (max(1.2f, player.GetNanoSuit()->GetSprintMultiplier(cry_fabsf(m_movement.desiredVelocity.x) > 0.01f) * 0.5f));
			//player.m_stats.bSprinting = true;
		}
	}
	if (m_actions & ACTION_JUMP)
	{
		player.RequestLeaveLadder(CPlayer::eLAT_Jump);
		move += Vec3(0.0f, 0.0f, 3.0f);
	}

	if (g_pGameCVars->pl_debug_ladders != 0)
	{
		float white[] = { 1,1,1,1 };
		gEnv->pRenderer->Draw2dLabel(50, 100, 2.0f, white, false, "Move (%.2f, %.2f, %.2f)", move.x, move.y, move.z);
	}

	move *= 0.8f;
	//Animation and movement synch
	{
		AdjustPlayerPositionOnLadder(player);
		bottomDist += (move.z * gEnv->pTimer->GetFrameTime());
		float animTime = bottomDist - cry_floorf(bottomDist);
		CPlayer::ELadderDirection eLDir = (m_movement.desiredVelocity.y >= 0.0f ? CPlayer::eLDIR_Up : CPlayer::eLDIR_Down);
		const bool animationReady = player.UpdateLadderAnimation(CPlayer::eLS_Climb, eLDir, animTime);
		bool authoritativeClimb = false;
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
		// DS112: an owned, live server bot has no client-authoritative climb position. Native physics input
		// must not wait for the cosmetic manual-animation FIFO. Keep the stock native axis/speed/top ray.
		authoritativeClimb = WarsBotLadderServer(player) && player.GetHealth()>0 && !player.IsFrozen() && !m_stats.isRagDoll;
		if (authoritativeClimb)
			WB_LOG(2,"[WarsBots] LADDER NATIVE actor=%u input=(%.3f,%.3f,%.3f) velocity=(%.3f,%.3f,%.3f) animationReady=%d.",
				player.GetEntityId(),m_movement.desiredVelocity.x,m_movement.desiredVelocity.y,m_movement.desiredVelocity.z,
				move.x,move.y,move.z,animationReady?1:0);
#endif
		if (!animationReady && !authoritativeClimb)
			return;
	}

	m_request.type = eCMT_Fly;
	m_request.velocity = move;

	//m_velocity.Set(0,0,0);
}
};
bool run(CPlayer&p,bool expected){Movement m(p);m.Climb(p);return (m.m_request.type==eCMT_Fly && m.m_request.velocity.z>0)==expected;}
int main(){CPlayer p;
 if(!run(p,true)){std::puts("FAIL: owned native climb waits for cosmetic FIFO");return 1;}
 p.ready=true;if(!run(p,true))return 2;p.ready=false;
 p.bot=false;if(!run(p,false))return 3;p.bot=true;
 game.mgr.owns=false;if(!run(p,false))return 4;game.mgr.owns=true;
 env.bServer=false;if(!run(p,false))return 5;env.bServer=true;
 WarsBots::BotConfig::Get().ladder=0;if(!run(p,false))return 6;WarsBots::BotConfig::Get().ladder=1;
 p.health=0;if(!run(p,false))return 7;p.health=100;
 p.frozen=true;if(!run(p,false))return 8;p.frozen=false;
 Movement dead(p);dead.m_stats.isRagDoll=true;dead.Climb(p);if(dead.m_request.type!=0)return 9;
 Movement hold(p);hold.m_movement.desiredVelocity={0,0,0};hold.Climb(p);if(hold.m_request.velocity.len2()!=0)return 10;
 Movement down(p);down.m_movement.desiredVelocity={0,-1,0};down.Climb(p);if(down.m_request.velocity.z>=0)return 11;
 p.bot=false;p.ready=true;if(!run(p,true))return 12;
 std::puts("Actual native climb/animation gate controls PASS");return 0;
}
