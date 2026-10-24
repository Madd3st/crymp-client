// Native map-memory weighted reservoir, shared by the opt-in full scan and legacy stride.
#pragma once
#include <cstddef>
namespace WarsBots { namespace BotHotspot {
template<class Weight> int Pick(unsigned seed,std::size_t count,std::size_t stride,Weight weight) {
 if (!stride) return -1;
 float total=0;int chosen=-1;unsigned rng=seed*2654435761u+12345u;
 for (std::size_t i=0;i<count;i+=stride) {
  const float w=weight(i);if (w<=.5f) continue;
  total+=w;rng=rng*1664525u+1013904223u;
  if (float(rng>>8)/float(1u<<24)*total<w) chosen=int(i);
 }
 return chosen;
}
}}
