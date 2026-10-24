#!/usr/bin/env python3
"""Owner MOVE TRACE -> Detour off-mesh jump links (normal / Strength / drop).

Reads WarsBots `MOVE TRACE ... kind=human` lines from one or more Client `.bots.log`
files, detects airborne episodes per actor, classifies each jump, and writes
OWNER_JUMPS.json (positions only) + OWNER_JUMPS.md (summary) into --out.

Stdlib only. Classification rules (from PLAYER_MOVEMENT.md OC-001 numbers):
  - Strength: mode=1 (NANOMODE_STRENGTH) during the episode, or energy drop >= 50
    (native Strength jump costs 70), or peak height >= 2.0 m.
  - normal: native jump flag was set, 0.3 <= peak < 2.0 m, energy drop < 50.
  - drop:    no native jump flag and dz < -1.0 m (fell off a ledge).
Everything else is ignored (walking off a curb, respawn pops, etc.).
"""
import re
import sys
import json
import math
import argparse
from collections import defaultdict

TRACE_RE = re.compile(
    r"MOVE TRACE actor=(\d+) kind=human side=client local=(\d+) "
    r"t=([\d.eE+-]+) sampleDt=([\d.eE+-]+) "
    r"pos=\(([^)]+)\) vel=\(([^)]+)\) "
    r"speed=([\d.eE+-]+) vz=([\d.eE+-]+) mode=(\d+) energy=([\d.eE+-]+) "
    r"sprint=(\d+) grounded=(\d+) flying=([\d-]+) jump=(\d+) land=(\d+) "
    r"takeoffs=(\d+) jumpEdges=(\d+) landEdges=(\d+) "
    r"jumpTime=([\d.eE+-]+) landTime=([\d.eE+-]+) peakZ=([\d.eE+-]+) health=(\d+)"
)


def _vec(s):
    return [float(x) for x in s.split(",")]


def parse(path):
    """Return per-actor list of record dicts, sorted by t."""
    actors = defaultdict(list)
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = TRACE_RE.search(line)
            if not m:
                continue
            pos = _vec(m.group(5))
            vel = _vec(m.group(6))
            rec = {
                "actor": int(m.group(1)),
                "t": float(m.group(3)),
                "pos": pos,
                "vel": vel,
                "vz": float(m.group(8)),
                "mode": int(m.group(9)),
                "energy": float(m.group(10)),
                "grounded": int(m.group(12)),
                "flying": int(m.group(13)),
                "jump": int(m.group(14)),
                "peakZ": float(m.group(21)),
            }
            actors[rec["actor"]].append(rec)
    for a in actors:
        actors[a].sort(key=lambda r: r["t"])
    return actors


def episodes(records):
    """Split a single actor's records into airborne episodes.

    Take-off: last grounded record before going airborne.
    Landing: first grounded record after being airborne.
    """
    out = []
    cur = None
    last_grounded = None
    for r in records:
        if r["grounded"]:
            last_grounded = r
            if cur is not None:
                cur["landing"] = r
                out.append(cur)
                cur = None
        else:  # airborne
            if cur is None:
                cur = {
                    "takeoff": last_grounded if last_grounded else r,
                    "max_peak": -1e9,
                    "jump": 0,
                    "modes": set(),
                    "min_energy": 1e9,
                    "pre_energy": last_grounded["energy"] if last_grounded else r["energy"],
                }
            cur["max_peak"] = max(cur["max_peak"], r["peakZ"])
            cur["jump"] = max(cur["jump"], r["jump"])
            cur["modes"].add(r["mode"])
            cur["min_energy"] = min(cur["min_energy"], r["energy"])
    if cur is not None:
        # trace ends mid-air; no landing
        pass
    return out


def classify(ep):
    to = ep["takeoff"]["pos"]
    ld = ep["landing"]["pos"]
    dz = ld[2] - to[2]
    dx = ld[0] - to[0]
    dy = ld[1] - to[1]
    dist = math.hypot(dx, dy)
    peak = ep["max_peak"] - to[2]
    energy_drop = ep["pre_energy"] - ep["min_energy"]
    mode = 1 if 1 in ep["modes"] else (min(ep["modes"]) if ep["modes"] else 3)

    if peak >= 2.0 or energy_drop >= 50.0:
        typ = "strength"
    elif ep["jump"] and peak >= 0.3:
        typ = "normal"
    elif not ep["jump"] and dz < -1.0:
        typ = "drop"
    else:
        typ = "other"

    return {
        "type": typ,
        "takeoff": [round(v, 3) for v in to],
        "landing": [round(v, 3) for v in ld],
        "dz": round(dz, 3),
        "dist": round(dist, 3),
        "peak": round(peak, 3),
        "mode": mode,
        "energyDrop": round(energy_drop, 1),
    }


def cluster(points, radius=5.0):
    """Greedy spatial clustering of takeoff positions (x,y only)."""
    clusters = []
    for p in points:
        placed = False
        for c in clusters:
            cx = sum(q[0] for q in c) / len(c)
            cy = sum(q[1] for q in c) / len(c)
            if math.hypot(p[0] - cx, p[1] - cy) <= radius:
                c.append(p)
                placed = True
                break
        if not placed:
            clusters.append([p])
    return clusters


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs", nargs="+", help="Client .bots.log files")
    ap.add_argument("--out", default=".", help="output dir")
    args = ap.parse_args()

    jumps = []
    for path in args.logs:
        for actor, records in parse(path).items():
            for ep in episodes(records):
                j = classify(ep)
                j["actor"] = actor
                j["source"] = path
                jumps.append(j)

    jumps = [j for j in jumps if j["type"] != "other"]

    json_path = args.out.rstrip("/\\") + "/OWNER_JUMPS.json"
    with open(json_path, "w", encoding="utf-8") as fh:
        json.dump({"jumps": jumps}, fh, indent=2)

    counts = defaultdict(int)
    for j in jumps:
        counts[j["type"]] += 1
    by_type = defaultdict(list)
    for j in jumps:
        by_type[j["type"]].append(j)

    climbs = sorted(jumps, key=lambda j: j["peak"], reverse=True)[:15]
    takeoff_pts = [j["takeoff"] for j in jumps]
    cl = cluster(takeoff_pts, radius=6.0)
    cl = sorted(cl, key=len, reverse=True)

    md = args.out.rstrip("/\\") + "/OWNER_JUMPS.md"
    lines = []
    lines.append("# Owner jumps -> Detour off-mesh links")
    lines.append("")
    lines.append("Parsed owner MOVE TRACE (`kind=human side=client`). Classification: strength / normal / drop.")
    lines.append("")
    lines.append("## Counts")
    lines.append("")
    lines.append("| type | count |")
    lines.append("| --- | --- |")
    for t in ("strength", "normal", "drop"):
        lines.append(f"| {t} | {counts[t]} |")
    lines.append(f"| **total** | **{len(jumps)}** |")
    lines.append("")
    lines.append("## Biggest climbs (by peak height above take-off)")
    lines.append("")
    lines.append("| peak m | dz m | dist m | type | takeoff | landing |")
    lines.append("| --- | --- | --- | --- | --- | --- |")
    for j in climbs:
        lines.append(
            f"| {j['peak']:.2f} | {j['dz']:.2f} | {j['dist']:.2f} | {j['type']} | "
            f"({j['takeoff'][0]:.1f},{j['takeoff'][1]:.1f},{j['takeoff'][2]:.1f}) | "
            f"({j['landing'][0]:.1f},{j['landing'][1]:.1f},{j['landing'][2]:.1f}) |"
        )
    lines.append("")
    lines.append("## Clusters (take-off positions, radius 6 m)")
    lines.append("")
    lines.append("| size | centroid (x,y) |")
    lines.append("| --- | --- |")
    for c in cl:
        cx = sum(q[0] for q in c) / len(c)
        cy = sum(q[1] for q in c) / len(c)
        lines.append(f"| {len(c)} | ({cx:.1f},{cy:.1f}) |")
    lines.append("")
    lines.append("## Caveats")
    lines.append("")
    lines.append("- 250 ms sampling can merge rapid bunny-hops into one episode (landing missed):")
    lines.append("  episodes with `dist > ~15 m` or `dz > ~5 m` are merged, not single jumps.")
    lines.append("- `strength` = peak >= 2.0 m **or** energy drop >= 50 (native cost 70); a merged episode")
    lines.append("  can mask the 70-cost with in-flight regen, so a few `strength` rows may be merged normals.")
    lines.append("- `drop` = no native jump flag and dz < -1.0 m.")
    lines.append("")
    with open(md, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")

    print(f"{len(jumps)} jumps -> {json_path}")
    print(f"counts: {dict(counts)}")


if __name__ == "__main__":
    main()
