
#include <cstdio>
using EntityId=unsigned;
const int svtPointer=1,eRMI_ToClientChannel=2;
struct ScriptHandle{unsigned n=0;};
struct Handler {
 unsigned id=0;int ended=0;
 int GetParamType(int){return svtPointer;}
 void GetParam(int,ScriptHandle& value){value.n=id;}
 int EndFunction(){++ended;return -99;}
};
struct CGameRules {int channels=0;int GetChannelId(EntityId id){++channels;return int(id)+100;}};
namespace WarsBots {struct BotMatch {static bool IsBot(const CGameRules&,EntityId);};}
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
bool WarsBots::BotMatch::IsBot(const CGameRules&,EntityId id){return id==7;}
#endif
int Dispatch(Handler* pH,CGameRules* pGameRules,int to){
 int channelId=-1;
 if (pH->GetParamType(4) == svtPointer)
		{
			ScriptHandle playerId;
			pH->GetParam(4, playerId);

#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
			// WarsBots C-126: a bot has no client; a per-client text message (e.g. stock PS BuyItem "cannot carry
			// more") would be an RMI to channel 0. Drop it here once for every Lua path.
			if ((to & eRMI_ToClientChannel) && WarsBots::BotMatch::IsBot(*pGameRules, (EntityId)playerId.n))
				return pH->EndFunction();
#endif

			channelId = pGameRules->GetChannelId((EntityId)playerId.n);
		}
 return channelId;
}
int main(){
 CGameRules rules;Handler h;h.id=7;
 const int bot=Dispatch(&h,&rules,eRMI_ToClientChannel);
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
 if(bot!=-99||h.ended!=1||rules.channels!=0)return 1;
#else
 if(bot!=107||h.ended||rules.channels!=1)return 2;
#endif
 rules.channels=0;h.ended=0;h.id=8;
 if(Dispatch(&h,&rules,eRMI_ToClientChannel)!=108||h.ended||rules.channels!=1)return 3;
 rules.channels=0;h.ended=0;h.id=7;
 if(Dispatch(&h,&rules,0)!=107||h.ended||rules.channels!=1)return 4;
 std::puts("Actual SendTextMessage caller PASS");return 0;
}
