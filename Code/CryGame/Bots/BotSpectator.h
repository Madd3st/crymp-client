// WarsBots: pure selection policy; entity validation and native RMIs stay at the call site.
#pragma once
#include <vector>
#include <algorithm>
namespace WarsBots { namespace BotSpectator {
inline bool CanInspect(bool client, bool instantAction, bool spectator) {
    return client && instantAction && spectator;
}
inline bool Eligible(bool bot, bool alive, bool hidden, bool spectator) {
    return bot && alive && !hidden && !spectator;
}
template<class Id> Id Cycle(const std::vector<Id>& ids, Id current, int direction) {
    if (ids.empty()) return Id(0);
    const auto it=std::find(ids.begin(),ids.end(),current);
    if (it==ids.end()) return direction<0 ? ids.back() : ids.front();
    const auto i=std::size_t(it-ids.begin());
    return ids[(i+ids.size()+(direction<0 ? -1 : 1))%ids.size()];
}
}}
