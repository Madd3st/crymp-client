// DS-076: MOVE TRACE diagnostic eligibility (engine-free).
// C145 preflight X-218: BotMoveTrace.cpp used to require the rules class name "InstantAction", so PowerStruggle
// returned before Visit and the native water fields could never be observed. The engine maps the existing
// BotMatch policy to MoveTraceMode; this predicate keeps the opt-in flag and the local/server + client/multiplayer
// guards unchanged. No new wb_* flag and no default change.
#pragma once

namespace WarsBots
{
enum class MoveTraceMode
{
	Unsupported = 0,
	InstantAction,
	PowerStruggle,
};

// flag: wb_move_trace (1 or 2); server: gEnv->bServer; localAllowed: BotManager::CanRunLocalIA();
// multiplayer: gEnv->bMultiplayer; mode: BotMatch policy for the live game rules.
inline bool MoveTraceEligible(int flag, bool server, bool localAllowed, bool multiplayer, MoveTraceMode mode)
{
	if (flag != 1 && flag != 2) return false;
	if (mode != MoveTraceMode::InstantAction && mode != MoveTraceMode::PowerStruggle) return false;
	return server ? localAllowed : multiplayer;
}
}
