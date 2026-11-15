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
// Package adb16bb8 (Codex 15.11): the old fixed cap of 200000 rows cut MOVE TRACE at t3464.99 of a 3643.66 s Shore
// level (~58 rows/s, ~496 bytes a row, ~103 MB an hour), so the last 178.67 s had no positions. Budget now: every due
// row up to `full` rows (a full Shore-15 level is ~211k), then every `thin`-th due row of each actor (2 Hz instead of
// 4 Hz), and nothing after `cap` rows. Returns 1 = write the row, 0 = thinned out, -1 = capped.
struct MoveTraceBudget
{
	unsigned full = 400000;
	unsigned cap = 1000000;
	unsigned thin = 2;
};

inline int MoveTraceKeep(unsigned lines, unsigned actorDue, const MoveTraceBudget& b = MoveTraceBudget())
{
	if (lines >= b.cap) return -1;
	if (lines < b.full || b.thin <= 1) return 1;
	return actorDue % b.thin == 0 ? 1 : 0;
}
}
