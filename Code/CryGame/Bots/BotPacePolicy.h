// C018 energy policy only: native physics/energy costs remain authoritative.
#pragma once
namespace WarsBots {
inline bool PaceSprintEnergy(bool sprinting,float energy){return sprinting?energy>20.f:energy>=40.f;}
inline bool PaceSpeedBurst(bool active,float distance,float energy){return active?(energy>60.f && distance>=8.f):(energy>=130.f && distance>=20.f);}
}
