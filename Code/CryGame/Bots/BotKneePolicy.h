// Export-only knee experiment; normal navigation always uses variant0.
#pragma once
namespace WarsBots {
inline float KneeRayHeight(int variant){return variant==1?.8f:.5f;}
inline bool KneeHitIsStep(int variant,float groundAboveBase,float groundAboveHit,float normalZ){
 return variant==2 && groundAboveBase<=.75f && groundAboveBase>=0.f && groundAboveHit>=-.05f && normalZ>=.7f;
}
}
