// Optional server diagnostics. Exit: permanent opt-in recorder, never an AI policy.
#pragma once
#include <memory>
#include <cstdint>
struct IGameFramework;struct HitInfo;class CPlayer;
namespace WarsBots {
class BotManager;
class BotDemoRecorder {
public:
 BotDemoRecorder();~BotDemoRecorder();
 void Initialize(IGameFramework*,BotManager*);
 void Shutdown();void EndLevel();
 void Update(bool enabled,int hz);
 bool Start(const char* label,int hz);void Stop();void Mark(const char* text);
 bool Active()const;
 void Hit(const HitInfo&);void Kill(uint32_t victim,uint32_t killer);
 void Revive(uint32_t id,uint32_t spawn);
 void Pickup(uint32_t id,uint32_t item);
 void Jump(uint32_t id,bool strength);void Land(uint32_t id,float height);
 float AverageMs()const;float WorstMs()const;
private:
 struct Impl;std::unique_ptr<Impl> m_impl;
};
}
