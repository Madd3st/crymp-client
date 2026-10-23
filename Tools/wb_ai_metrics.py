#!/usr/bin/env python3
"""Export observed AI QA counters, not learning or a navigation completion rate."""
import argparse
import collections
import hashlib
import json
import re
from pathlib import Path


def measure(path):
    raw = path.read_bytes()
    text = raw.decode("utf-8", errors="replace")
    states = collections.Counter(re.findall(r"AI telemetry EntityId=\d+ state=(\w+)", text))
    total = sum(states.values())
    moves = re.findall(r"MOTION INTENT: .*?requestedBase=([\d.]+).*?blocked=(\d)", text)
    moving = [int(b) for speed, b in moves if float(speed) > 0]
    profiles = re.findall(r"PROFILE bots=(\d+) avgSum=([\d.]+)ms/frame worstFrame=([\d.]+)ms", text)
    times = [float(t) for t in re.findall(r"(?m)^([\d.]+) \[WarsBots\]", text)]
    failures = collections.Counter(re.findall(r"NAV fallback start=\([^)]*\) goal=\(([^)]*)\)", text))
    count = lambda pattern: len(re.findall(pattern, text))
    return {
        "log": str(path), "log_sha256": hashlib.sha256(raw).hexdigest(),
        "observed_seconds": round(max(times) - min(times), 2) if times else None,
        "telemetry_samples": total,
        "state_samples": dict(states),
        "state_percent": {k: round(100 * v / total, 2) for k, v in states.items()} if total else {},
        "moving_samples": len(moving),
        "blocked_percent": round(100 * sum(moving) / len(moving), 2) if moving else None,
        "pickups_acquired": count(r"REAL PICKUP .*acquired=1"),
        "weapon_timeouts": count(r"unreachable; skipping"),
        "route_fallback_queries": count(r"NAV fallback start="),
        "frequent_fallback_goals": [
            {"position": [float(v) for v in goal.split(",")], "queries": n}
            for goal, n in failures.most_common(5)
        ],
        "weapon_cancel_unavailable": count(r"weapon goal cancelled .*reason=unavailable"),
        "weapon_cancel_no_route": count(r"weapon goal cancelled .*reason=no_route"),
        "strength_takeoffs": count(r"JUMP TAKEOFF: strength=1"),
        "strength_landings": count(r"JUMP LANDING: strength=1"),
        "jump_clearance_refusals": count(r"ACTION REFUSED: jump_clearance_or_landing"),
        "kills": count(r"combat kill shooter="),
        "deaths": count(r"death observed EntityId="),
        "revives_alive": count(r"revive result .*sameEntity=1 alive=1"),
        "max_reported_avg_ms": max((float(p[1]) for p in profiles), default=None),
        "worst_reported_frame_ms": max((float(p[2]) for p in profiles), default=None),
        "crash_marker": bool(re.search(r"CRASH DETECTED|Exception", text)),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", type=Path, nargs="+")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = {"schema": 1, "runs": [measure(p) for p in args.logs]}
    data = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.write_text(data, encoding="utf-8")
    else:
        print(data, end="")


if __name__ == "__main__":
    main()
