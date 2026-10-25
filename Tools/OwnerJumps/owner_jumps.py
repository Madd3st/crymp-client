#!/usr/bin/env python3
"""Owner MOVE TRACE -> Detour off-mesh jump links (normal / Strength / drop).

Reads WarsBots `MOVE TRACE ... kind=human` lines from one or more Client `.bots.log`
files, detects airborne episodes per actor, classifies each jump, filters out
teleport/revive artifacts and physically impossible (merged) episodes, and writes
OWNER_JUMPS.json (kept + rejected) + OWNER_JUMPS.md (summary).

Stdlib only.
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

G = 9.81
VH = 6.75          # max horizontal velocity of a jump (m/s), per OC-023
V0_NORMAL = 4.43   # normal jump vertical launch (m/s)
V0_STRENGTH = 9.60 # Strength jump vertical launch (m/s)
REACH_MARGIN = 1.2 # 250 ms sampling tolerance
TELEPORT_DIST = 5.0   # position jump between consecutive records = teleport/revive (m)
TELEPORT_WINDOW = 1.0 # reject jumps within this many seconds of a teleport
LINK_RADIUS = 2.0     # candidate-link cluster radius (m)


def _vec(s):
    return [float(x) for x in s.split(",")]


def parse(path):
    """Return (per-actor records, per-actor teleport times)."""
    actors = defaultdict(list)
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = TRACE_RE.search(line)
            if not m:
                continue
            rec = {
                "actor": int(m.group(1)),
                "t": float(m.group(3)),
                "pos": _vec(m.group(5)),
                "vz": float(m.group(8)),
                "mode": int(m.group(9)),
                "energy": float(m.group(10)),
                "grounded": int(m.group(12)),
                "flying": int(m.group(13)),
                "jump": int(m.group(14)),
                "peakZ": float(m.group(21)),
            }
            actors[rec["actor"]].append(rec)
    teleports = defaultdict(list)
    for a, recs in actors.items():
        recs.sort(key=lambda r: r["t"])
        for i in range(1, len(recs)):
            p0 = recs[i - 1]["pos"]
            p1 = recs[i]["pos"]
            d = math.dist(p0, p1)
            if d > TELEPORT_DIST:
                teleports[a].append(recs[i]["t"])
    return actors, teleports


def episodes(records):
    """Split a single actor's records into airborne episodes (takeoff -> landing)."""
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
        else:
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
    return out


def classify(ep):
    to = ep["takeoff"]["pos"]
    ld = ep["landing"]["pos"]
    dz = ld[2] - to[2]
    dist = math.hypot(ld[0] - to[0], ld[1] - to[1])
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
        "takeoff_t": ep["takeoff"]["t"],
        "landing_t": ep["landing"]["t"],
    }


def reach_for(j):
    """Ballistic horizontal reach (m) for a jump with this dz/type; -1 = physically impossible."""
    dz = j["dz"]
    if j["type"] == "drop":
        return VH * math.sqrt(max(0.0, 2.0 * (-dz) / G))
    v0 = V0_STRENGTH if j["type"] == "strength" else V0_NORMAL
    d = v0 * v0 - 2.0 * G * dz
    if dz >= 0.0 and d < 0.0:
        return -1.0  # landing higher than apex
    t = (v0 + math.sqrt(max(0.0, d))) / G
    return VH * t


def reject_reason(j, teleports):
    reach = reach_for(j)
    if reach < 0.0:
        return "dz exceeds jump apex"
    if j["dist"] > reach * REACH_MARGIN:
        return f"dist {j['dist']:.1f}m > reach {reach:.1f}m (merged/sample gap)"
    for tp in teleports:
        if abs(j["takeoff_t"] - tp) <= TELEPORT_WINDOW or abs(j["landing_t"] - tp) <= TELEPORT_WINDOW:
            return "near teleport/revive"
    return None


def cluster(points, radius):
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

    kept, rejected = [], []
    for path in args.logs:
        actors, teleports = parse(path)
        for actor, records in actors.items():
            for ep in episodes(records):
                j = classify(ep)
                j["actor"] = actor
                j["source"] = path
                if j["type"] == "other":
                    continue
                reason = reject_reason(j, teleports.get(actor, []))
                if reason:
                    j["reason"] = reason
                    rejected.append(j)
                else:
                    kept.append(j)

    json_path = args.out.rstrip("/\\") + "/OWNER_JUMPS.json"
    with open(json_path, "w", encoding="utf-8") as fh:
        json.dump({"jumps": [j for j in kept], "rejected": rejected}, fh, indent=2)

    counts = defaultdict(int)
    for j in kept:
        counts[j["type"]] += 1
    climbs = sorted(kept, key=lambda j: j["peak"], reverse=True)[:15]
    links = [c for c in cluster([j["takeoff"] for j in kept], LINK_RADIUS) if len(c) >= 2]
    links = sorted(links, key=len, reverse=True)

    md = args.out.rstrip("/\\") + "/OWNER_JUMPS.md"
    lines = ["# Owner jumps -> Detour off-mesh links", "",
             "Parsed owner MOVE TRACE (`kind=human side=client`). Filtered: ballistic reach "
             f"(vh<=6.75 m/s, v0 normal {V0_NORMAL}/Strength {V0_STRENGTH}, x{REACH_MARGIN} margin) + "
             f"teleport/revive within +/-{TELEPORT_WINDOW}s.", "",
             "## Counts", "", "| type | kept | rejected |", "| --- | --- | --- |"]
    rj = defaultdict(int)
    for j in rejected:
        rj[j["type"]] += 1
    total_kept = len(kept)
    total_rej = len(rejected)
    for t in ("strength", "normal", "drop"):
        lines.append(f"| {t} | {counts[t]} | {rj[t]} |")
    lines.append(f"| **total** | **{total_kept}** | **{total_rej}** |")
    lines.append("")
    lines.append("## Candidate links (>= 2 independent take-offs within 2 m)")
    lines.append("")
    lines.append("| size | centroid (x,y) |")
    lines.append("| --- | --- |")
    for c in links:
        cx = sum(q[0] for q in c) / len(c)
        cy = sum(q[1] for q in c) / len(c)
        lines.append(f"| {len(c)} | ({cx:.1f},{cy:.1f}) |")
    lines.append("")
    lines.append("## Biggest climbs (kept)")
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
    lines.append("## Rejected (first 40)")
    lines.append("")
    lines.append("| reason | type | dist m | dz m | takeoff | landing |")
    lines.append("| --- | --- | --- | --- | --- | --- |")
    for j in rejected[:40]:
        lines.append(
            f"| {j['reason']} | {j['type']} | {j['dist']:.1f} | {j['dz']:.1f} | "
            f"({j['takeoff'][0]:.1f},{j['takeoff'][1]:.1f},{j['takeoff'][2]:.1f}) | "
            f"({j['landing'][0]:.1f},{j['landing'][1]:.1f},{j['landing'][2]:.1f}) |"
        )
    lines.append("")
    with open(md, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")

    print(f"{total_kept} kept / {total_rej} rejected -> {json_path}")
    print(f"kept: {dict(counts)}; rejected: {dict(rj)}; links: {len(links)}")


if __name__ == "__main__":
    main()
