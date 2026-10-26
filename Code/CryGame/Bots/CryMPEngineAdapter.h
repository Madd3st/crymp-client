#pragma once
#include "CryCommon/CryMath/Cry_Math.h"
struct IGameFramework;
struct IActor;
namespace WarsBots {
// CryMP integration boundary. No connection registration or binary addresses here.
class CryMPEngineAdapter {
public:
    static bool LocalIAGate(IGameFramework* framework, bool levelReady, int optIn, bool diagnostic, bool allowPowerStruggle = false, bool allowPublic = false);
    // Read-only exports (wb_geom_export): the same offline loopback gate, but a PowerStruggle level may be loaded with
    // its own rules (X-066: PS maps fail to start under InstantAction). Bots keep LocalIAGate, IA only.
    static bool LocalExportGate(IGameFramework* framework, bool levelReady, int optIn, bool diagnostic)
    { return LocalIAGate(framework, levelReady, optIn, diagnostic, true); }
    static IActor* CreateServerActor(IGameFramework& framework, const Vec3& position, const char* name);
};
}
