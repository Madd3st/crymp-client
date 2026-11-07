"""SPEC-007 log cost wiring guard (pure Python, reads the real sources; not a runtime measurement).
Codex 06.11 (f9d33a47 pair): trace 89 lines/s, server MOVE TRACE 59/s, the engine flushes the main log per line."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOTS = ROOT / 'Code/CryGame/Bots'
fails = 0


def check(ok, what):
    global fails
    print(('PASS ' if ok else 'FAIL ') + what)
    fails += not ok


def main():
    log = (BOTS / 'BotLog.cpp').read_text(encoding='utf-8-sig')
    mgr = (BOTS / 'BotManager.cpp').read_text(encoding='utf-8-sig')
    trace = (BOTS / 'BotMoveTrace.cpp').read_text(encoding='utf-8-sig')
    cfg = (BOTS / 'BotConfig.cpp').read_text(encoding='utf-8-sig')
    check('setvbuf(g_trace' in log, 'trace file is block-buffered')
    check('TraceFlushDue(level,++g_unflushed,now-g_lastFlush)' in log and 'level<=kLogEvent ||' not in log, 'events no longer flush the trace one by one')
    check('if (level <= console)' in log, 'main-log copy still only up to wb_log (default 1: no telemetry in the main log)')
    check('LOGCOST lines=' in mgr and 'TakeLogCost()' in mgr, 'LOGCOST next to FRAME for the A/B')
    check('::WarsBots::FlushTrace();' in mgr, 'trace flushed at level end')
    check('MoveTraceServerHz(BotConfig::Get().moveTraceServerHz)' in trace and '"wb_move_trace_server_hz"' in cfg, 'server MOVE TRACE rate is a cvar')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
