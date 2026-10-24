// Bounded per-life diagnostic schedule; never applies a physics action.
#pragma once
namespace WarsBots {
class BotPhysicsProbe {
public:
 void Cancel() { m_active=false;m_taken=0; }
 void Start(float now) { m_start=now;m_taken=0;m_active=true; }
 float Elapsed(float now) const { return now-m_start; }
 unsigned Take(float now) {
  if (!m_active) return 0;
  if (now<m_start) { Cancel();return 0; }
  const unsigned due=(now-m_start>=1.f?1u:0u)|(now-m_start>=5.f?2u:0u);
  const unsigned fresh=due&~m_taken;m_taken|=due;
  if(m_taken==3u)m_active=false;
  return fresh;
 }
private:
 float m_start=0;unsigned m_taken=0;bool m_active=false;
};
}
