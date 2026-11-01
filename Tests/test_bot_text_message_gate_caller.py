"""Compile/link the real SendTextMessage pointer branch with experimental bots OFF/ON.

OFF deliberately has only BotMatch::IsBot's declaration, no implementation: any
unguarded dependency must fail at LINK, not count a compile error as reproduction.
ON checks suppression of bot-only per-client messages and unchanged human routing.
Engine boundaries are mocked; this is not a full game or network runtime test.
"""
from pathlib import Path
import argparse
import subprocess


def block(text,marker):
    start=text.index(marker);left=text.index('{',start);depth=0
    for i in range(left,len(text)):
        depth+=(text[i]=='{')-(text[i]=='}')
        if not depth:return text[start:i+1]
    raise ValueError('Unclosed actual source block')


CPP=r"""
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
 @ACTUAL@
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
"""


def main():
    ap=argparse.ArgumentParser();root=Path(__file__).resolve().parents[1]
    ap.add_argument('--revision');ap.add_argument('--out',type=Path,default=root/'units/text-message-gate')
    ap.add_argument('--expect-off-link-failure',action='store_true');a=ap.parse_args()
    path='Code/CryGame/ScriptBind_GameRules.cpp'
    source=subprocess.check_output(['git','-C',str(root),'show',a.revision+':'+path]).decode('utf-8-sig') if a.revision else (root/path).read_text(encoding='utf-8-sig')
    method=source[source.index('int CScriptBind_GameRules::SendTextMessage('):]
    actual=block(method,'if (pH->GetParamType(4) == svtPointer)')
    for enabled in [False,True]:
        out=a.out.resolve()/('ON' if enabled else 'OFF');out.mkdir(parents=True,exist_ok=True)
        src=out/'caller.cpp';obj=out/'caller.obj';exe=out/'caller.exe';src.write_text(CPP.replace('@ACTUAL@',actual),encoding='utf-8')
        flags=['/DWARSBOTS_EXPERIMENTAL_SPAWNER'] if enabled else []
        comp=subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/c',*flags,str(src),'/Fo:'+str(obj)],capture_output=True)
        (out/'compile.log').write_bytes(comp.stdout+comp.stderr)
        if comp.returncode:raise RuntimeError('Compilation must succeed: '+(comp.stdout+comp.stderr).decode(errors='replace'))
        link=subprocess.run(['cl','/nologo',str(obj),'/Fe:'+str(exe)],capture_output=True)
        payload=link.stdout+link.stderr;(out/'link.log').write_bytes(payload)
        if not enabled and a.expect_off_link_failure:
            assert link.returncode and b'BotMatch' in payload and (b'LNK2019' in payload or b'LNK2001' in payload), 'Expected specific unresolved BotMatch::IsBot link error'
            print('OFF compiled; expected BotMatch::IsBot link failure reproduced')
            continue
        if link.returncode:raise RuntimeError(payload.decode(errors='replace'))
        run=subprocess.run([str(exe)],capture_output=True);(out/'run.log').write_bytes(run.stdout+run.stderr)
        print(('ON' if enabled else 'OFF')+': '+run.stdout.decode(errors='replace').strip())
        if run.returncode:raise RuntimeError('Actual pointer branch behavior failed')
    return 0


if __name__=='__main__':raise SystemExit(main())
