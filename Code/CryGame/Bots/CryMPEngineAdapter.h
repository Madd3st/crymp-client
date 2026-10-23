#pragma once
#include "CryCommon/CryMath/Cry_Math.h"
struct IGameFramework;
struct IActor;
namespace WarsBots {
// CryMP integration boundary. No connection registration or binary addresses here.
class CryMPEngineAdapter {
public:
    static bool LocalIAGate(IGameFramework* framework, bool levelReady, int optIn, bool diagnostic);
    static IActor* CreateServerActor(IGameFramework& framework, const Vec3& position);
};
}
