#pragma once
#include "CryCommon/CryMath/Cry_Math.h"
struct IGameFramework;
struct IActor;
namespace WarsBots {
// CryMP integration boundary. No connection registration or binary addresses here.
class CryMPEngineAdapter {
public:
    // allowLan: wb_lan_test may widen the bind to a private LAN address (bots only); the export gate passes false.
    static bool LocalIAGate(IGameFramework* framework, bool levelReady, int optIn, bool diagnostic, bool allowPowerStruggle = false, bool allowPublic = false, bool allowLan = true);
    // Read-only exports (wb_geom_export): the same offline loopback gate, but a PowerStruggle level may be loaded with
    // its own rules (X-066: PS maps fail to start under InstantAction). Bots keep LocalIAGate, IA only.
    static bool LocalExportGate(IGameFramework* framework, bool levelReady, int optIn, bool diagnostic)
    { return LocalIAGate(framework, levelReady, optIn, diagnostic, true, false, false); }   // loopback only, never wb_lan_test
    static IActor* CreateServerActor(IGameFramework& framework, const Vec3& position, const char* name);
};
}
