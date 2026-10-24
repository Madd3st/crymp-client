// Position-only progress window. A legitimate detour away from a goal still counts as movement.
#pragma once
namespace WarsBots
{
class BotProgress
{
public:
    void Reset() { m_active=false; }
    float Age(float now) const { return m_active?now-m_since:0.0f; }
    bool Stalled(float x,float y,float z,float now,float timeout,float radius=1.5f)
    {
        const float dx=x-m_x,dy=y-m_y,dz=z-m_z;
        if (!m_active || now<m_since || dx*dx+dy*dy+dz*dz>=radius*radius)
        { m_x=x; m_y=y; m_z=z; m_since=now; m_active=true; return false; }
        return now-m_since>=timeout;
    }
private:
    bool m_active=false;
    float m_x=0,m_y=0,m_z=0,m_since=0;
};
}
