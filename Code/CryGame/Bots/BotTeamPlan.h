// PS-0 #2: team assignment before the first spawn. In PowerStruggle a bot must have a team before
// GetSpawnLocationTeam / SetPlayerSpawnGroup / EquipPlayer, otherwise it is a neutral channel-0 actor.
// Pure logic, no engine types; Tests/BotTeamPlanTests.cpp.
#pragma once

namespace WarsBots {

// Alternating slot assignment (1-based): odd -> tan (1), even -> black (2).
inline int TeamForSlot(int slot)
{
    return (slot & 1) ? 1 : 2;
}
// Balance a new slot by current team sizes; tie goes to the slot's natural team so a fresh 3v3 does not drift.
inline int TeamForCounts(int slot, int tanCount, int blackCount)
{
    if (tanCount < blackCount) return 1;
    if (blackCount < tanCount) return 2;
    return TeamForSlot(slot);
}

} // namespace WarsBots
