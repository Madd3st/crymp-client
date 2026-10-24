#!/usr/bin/env python3
"""Read-only analyzer for WarsBots map memory files (format WBMEM01).

Format, from Code/CryGame/Bots/BotNav.cpp (BotNav::Save / BotNav::Load), little endian:

    offset size  field
    0      8     magic  "WBMEM01\\0"  (Load compares only the first 7 bytes)
    8      4     int32  nx            grid columns along X
    12     4     int32  ny            grid columns along Y
    16     4     float  cell          column size in metres (3.0)
    20     4     float  min_x         grid origin X (spawn bbox - 40 m)
    24     4     float  min_y         grid origin Y
    28     4     int32  count         number of graph nodes
    32     12*n  {float stuck, kills, deaths} per node, in BotNav node order

Node index -> position is NOT recoverable from this file: nodes are created column by column
with 0..5 floor layers each, so the index depends on the full graph of that build. Use
--graph with a graph exported by the same BotNav build (see BOTNAV_EXPORT_CONTRACT.md).

The tool never writes to the input file.
"""
import argparse
import hashlib
import json
import math
import struct
import sys
from pathlib import Path

MAGIC = b"WBMEM01"
HEADER = struct.Struct("<8siifffi")  # magic, nx, ny, cell, min_x, min_y, count (32 bytes)
RECORD = struct.Struct("<fff")
MAX_FILE = 64 * 1024 * 1024          # far above any plausible graph (5 layers * 4096^2 columns)
STUCK_CAP = 20.0                     # BotNav::ReportStuck clamps to 20


class MemError(ValueError):
    pass


def parse_bytes(data, source="<bytes>"):
    """Return a dict with header, records (list of tuples) and validation issues."""
    issues = []
    if len(data) > MAX_FILE:
        raise MemError(f"file too large ({len(data)} bytes > {MAX_FILE})")
    if len(data) < HEADER.size:
        raise MemError(f"truncated header: {len(data)} < {HEADER.size} bytes")
    magic, nx, ny, cell, min_x, min_y, count = HEADER.unpack_from(data, 0)
    if magic[:7] != MAGIC:
        raise MemError(f"bad magic {magic!r}")
    if magic[7:8] != b"\0":
        issues.append(dict(severity="warning", code="magic_terminator", detail=repr(magic)))
    if nx <= 0 or ny <= 0 or nx > 100000 or ny > 100000:
        raise MemError(f"implausible grid {nx}x{ny}")
    if not (math.isfinite(cell) and 0.1 <= cell <= 100):
        raise MemError(f"implausible cell size {cell}")
    if not (math.isfinite(min_x) and math.isfinite(min_y)):
        raise MemError("non-finite grid origin")
    if count < 0:
        raise MemError(f"negative node count {count}")
    max_nodes = nx * ny * 5          # BotNav::ScanColumn keeps at most 5 layers per column
    if count > max_nodes:
        raise MemError(f"node count {count} exceeds {nx}*{ny}*5 = {max_nodes}")
    expected = HEADER.size + count * RECORD.size
    if len(data) < expected:
        raise MemError(f"truncated body: {len(data)} bytes, expected {expected}")
    if len(data) > expected:
        issues.append(dict(severity="error", code="trailing_bytes", detail=len(data) - expected))
    records = [RECORD.unpack_from(data, HEADER.size + i * RECORD.size) for i in range(count)]
    for i, (s, k, d) in enumerate(records):
        for name, v in (("stuck", s), ("kills", k), ("deaths", d)):
            if not math.isfinite(v):
                issues.append(dict(severity="error", code="non_finite", node=i, field=name, value=repr(v)))
            elif v < 0:
                issues.append(dict(severity="error", code="negative", node=i, field=name, value=v))
            elif v != int(v):
                issues.append(dict(severity="warning", code="fractional", node=i, field=name, value=v))
        if math.isfinite(s) and s > STUCK_CAP:
            issues.append(dict(severity="error", code="stuck_above_cap", node=i, value=s))
    return dict(source=source, size=len(data),
                header=dict(magic=magic[:7].decode(), nx=nx, ny=ny, cell=cell, min_x=min_x, min_y=min_y,
                            count=count, max_x=min_x + nx * cell, max_y=min_y + ny * cell),
                records=records, issues=issues)


def distribution(values):
    vals = sorted(values)
    nz = [v for v in vals if v > 0]
    def q(p):
        return vals[min(len(vals) - 1, int(p * (len(vals) - 1)))] if vals else None
    hist = {}
    for v in vals:
        key = str(int(v)) if v < 10 else ("10-19" if v < 20 else ("20-49" if v < 50 else "50+"))
        hist[key] = hist.get(key, 0) + 1
    return dict(total=round(sum(vals), 3), nonzero_nodes=len(nz), max=vals[-1] if vals else None,
                mean=round(sum(vals) / len(vals), 4) if vals else None,
                p50=q(.5), p90=q(.9), p99=q(.99), histogram=hist)


def summarize(parsed, top=15):
    recs = parsed["records"]
    fields = ("stuck", "kills", "deaths")
    stats = {f: distribution([r[i] for r in recs]) for i, f in enumerate(fields)}
    stats["stuck"]["at_cap"] = sum(1 for r in recs if r[0] >= STUCK_CAP)
    tops = {f: sorted(((r[i], n) for n, r in enumerate(recs) if r[i] > 0), reverse=True)[:top]
            for i, f in enumerate(fields)}
    # PickHotspot only samples even indices and skips weights <= 0.5 (BotNav.cpp:297-302).
    hotspot_fight = sum(1 for n in range(0, len(recs), 2) if recs[n][1] + recs[n][2] - recs[n][0] * .5 > .5)
    hotspot_safe = sum(1 for n in range(0, len(recs), 2)
                       if recs[n][1] * 1.5 - recs[n][2] * .7 - recs[n][0] * .5 > .5)
    errors = [i for i in parsed["issues"] if i["severity"] == "error"]
    return dict(source=parsed["source"], size=parsed["size"], header=parsed["header"],
                valid=not errors, error_count=len(errors),
                warning_count=len(parsed["issues"]) - len(errors), issues=parsed["issues"][:200],
                stats=stats, top_nodes={f: [dict(node=n, value=v) for v, n in t] for f, t in tops.items()},
                hotspot_candidates=dict(fight=hotspot_fight, safe=hotspot_safe),
                node_positions="unknown: requires a graph export of the same build (--graph)")


def link_graph(parsed, graph):
    """Verify that a graph export belongs to the same BotNav build and attach memory to nodes."""
    h = parsed["header"]
    g = graph.get("build", {})
    problems = []
    checks = (("nx", h["nx"]), ("ny", h["ny"]), ("cell", h["cell"]), ("min_x", h["min_x"]), ("min_y", h["min_y"]))
    for key, value in checks:
        if key not in g:
            problems.append(f"graph.build.{key} missing")
        elif not math.isclose(float(g[key]), float(value), rel_tol=0, abs_tol=1e-4):
            problems.append(f"{key}: memory {value} != graph {g[key]}")
    nodes = graph.get("nodes", [])
    if len(nodes) != h["count"]:
        problems.append(f"node count: memory {h['count']} != graph {len(nodes)}")
    if graph.get("schema") != "warsbots/botnav-graph/1":
        problems.append(f"unsupported graph schema {graph.get('schema')!r}")
    for i, node in enumerate(nodes[:h["count"]]):
        if node[0] != i:
            problems.append(f"graph node at index {i} has id {node[0]} (ids must equal BotNav order)")
            break
    if problems:
        return dict(linked=False, problems=problems)
    out = []
    for node, (s, k, d) in zip(nodes, parsed["records"]):
        if s or k or d:
            out.append(dict(id=node[0], pos=node[1:4], layer=node[6], stuck=s, kills=k, deaths=d))
    return dict(linked=True, problems=[], nodes_with_memory=out)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mem", type=Path)
    ap.add_argument("--json", type=Path, help="write the report as JSON")
    ap.add_argument("--graph", type=Path, help="graph export JSON of the SAME BotNav build, for node linking")
    ap.add_argument("--top", type=int, default=15)
    a = ap.parse_args(argv)
    data = a.mem.read_bytes()                     # read once; the file is never opened for writing
    try:
        parsed = parse_bytes(data, str(a.mem))
    except MemError as e:
        print(f"INVALID {a.mem}: {e}", file=sys.stderr)
        if a.json:
            a.json.write_text(json.dumps(dict(source=str(a.mem), valid=False, fatal=str(e)), indent=2))
        return 2
    report = summarize(parsed, a.top)
    report["sha256"] = hashlib.sha256(data).hexdigest()
    if a.graph:
        report["graph_link"] = link_graph(parsed, json.loads(a.graph.read_text(encoding="utf-8")))
    h = report["header"]
    print(f"{a.mem.name}: {'VALID' if report['valid'] else 'INVALID'} sha256={report['sha256'][:16]}")
    print(f"  grid {h['nx']}x{h['ny']} cell {h['cell']} origin ({h['min_x']:.2f}, {h['min_y']:.2f})"
          f" -> ({h['max_x']:.2f}, {h['max_y']:.2f}); nodes {h['count']}")
    for f in ("stuck", "kills", "deaths"):
        s = report["stats"][f]
        print(f"  {f:6}: total {s['total']:.0f}, nodes>0 {s['nonzero_nodes']}, max {s['max']}, p99 {s['p99']}")
    print(f"  stuck at cap(20): {report['stats']['stuck']['at_cap']}; issues: {report['error_count']} errors,"
          f" {report['warning_count']} warnings")
    if a.graph:
        gl = report["graph_link"]
        print(f"  graph link: {'OK' if gl['linked'] else 'REFUSED: ' + '; '.join(gl['problems'])}")
    if a.json:
        a.json.parent.mkdir(parents=True, exist_ok=True)
        a.json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["valid"] else 1


if __name__ == "__main__":
    sys.exit(main())
