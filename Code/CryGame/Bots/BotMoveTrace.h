// Opt-in MOVE TRACE. Never writes actor state or network aspects.
#pragma once
#include "BotMoveTraceState.h"
#include <unordered_map>
struct IGameFramework;
class CPlayer;
namespace WarsBots {
class BotManager;
class BotMoveTrace {
 struct Record {BotMoveTraceState state;float logged=-1;unsigned seen=0;};
 std::unordered_map<unsigned,Record> m_records;
 BotMoveTraceClock m_clock;unsigned m_frame=0,m_lines=0;bool m_capped=false;
 void Visit(CPlayer& actor,bool bot,bool due,float now);
public:
 void Update(IGameFramework* framework,const BotManager& manager);
 void Reset(){m_records.clear();m_clock=BotMoveTraceClock();m_frame=m_lines=0;m_capped=false;}
};
}
