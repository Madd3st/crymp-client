#!/usr/bin/env python3
"""Steelmill Triage: offline inspection of WarsBots map memory, trace logs and (future) graph exports.

  steelmill_triage.py mem  <file.mem> [--against older.mem]       memory statistics / growth
  steelmill_triage.py analyze <run.bots.log>... [--mem f.mem] [--graph nav.json] [--out DIR]
  steelmill_triage.py graph <nav.json> [--mem f.mem] [--replay run.bots.log]
  steelmill_triage.py movement <run.bots.log>...                   pace profile, bots vs humans

Read-only: never writes next to its inputs. Stdlib only (Python 3.9+).
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from wblab import analysis, logparse, mem, movement, navgraph, render  # noqa: E402


def cmd_mem(a):
    m = mem.read(a.file)
    out = m.summary()
    if a.against:
        out["growth_since"] = dict(file=str(a.against), **mem.diff(mem.read(a.against), m))
    print(json.dumps(out, indent=2))


def cmd_analyze(a):
    memory = mem.read(a.mem) if a.mem else None
    graph = navgraph.load(a.graph) if a.graph else None
    if graph and memory:
        navgraph.validate(graph.data, memory)
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    results, payloads = [], []
    for path in a.logs:
        raw = Path(path).read_bytes()
        for run in logparse.parse_lines(raw.decode("utf-8", errors="replace").splitlines()):
            grid = memory.fingerprint() if memory else None
            if grid is None and run.grid:
                # A log alone gives nx/ny/cell but not min_x/min_y; leave the viewer to fit bounds.
                grid = None
            summary, st, osc = analysis.summarize(run, grid)
            summary["log"] = Path(path).name
            summary["log_sha256"] = hashlib.sha256(raw).hexdigest()
            if graph:
                counts, rows = navgraph.replay_fallbacks(graph, run, memory.stuck if memory else None, limit=a.replay_limit)
                summary["fallback_replay"] = counts
            results.append(summary)
            label = f"{Path(path).name} · run {run.index}"
            payloads.append(render.build_payload(label, run, summary, st, analysis.pinned(run), grid, graph, memory))
    report = dict(schema="steelmill-triage/1", memory=memory.summary() if memory else None, runs=results)
    (out / "triage_report.json").write_text(json.dumps(report, indent=2, default=list) + "\n", encoding="utf-8")
    render.write_html(out / "steelmill_triage.html", payloads)
    for s in results:
        print(f"{s['log']} run {s['run']}: {s['bots']} bots {s['seconds']:.0f}s build={s['build']} "
              f"pinned={s['pinned']['live_percent']}% traps={s['traps']['count']} stalls={s['stalls']['count']} "
              f"fallbacks={s['nav_fallback']['counts'].get('total', 0)}")
    print(f"wrote {out / 'triage_report.json'} and {out / 'steelmill_triage.html'}")


def cmd_graph(a):
    g = navgraph.load(a.file)
    memory = mem.read(a.mem) if a.mem else None
    if memory: navgraph.validate(g.data, memory)
    strong = g.components(); weak = g.components(undirected=True)
    out = dict(nodes=len(g.nodes), edges=len(g.to), one_way_edges=g.one_way_edges(),
               strong_components=len(strong), largest_strong=len(strong[0]) if strong else 0,
               weak_components=len(weak), largest_weak=len(weak[0]) if weak else 0,
               isolated_nodes=sum(1 for c in weak if len(c) == 1))
    if a.replay:
        for run in logparse.parse_file(a.replay):
            counts, _ = navgraph.replay_fallbacks(g, run, memory.stuck if memory else None, limit=a.replay_limit)
            out.setdefault("fallback_replay", []).append(counts)
    print(json.dumps(out, indent=2))


def cmd_movement(a):
    out = [movement.profile(p) for p in a.logs]
    for o in out:
        o["log"] = Path(o["log"]).name
    print(json.dumps(dict(schema="steelmill-triage-movement/1", runs=out), indent=2))


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("mem"); s.add_argument("file"); s.add_argument("--against"); s.set_defaults(fn=cmd_mem)
    s = sub.add_parser("analyze"); s.add_argument("logs", nargs="+"); s.add_argument("--mem"); s.add_argument("--graph")
    s.add_argument("--out", default="steelmill-triage-out"); s.add_argument("--replay-limit", type=int, default=20000)
    s.set_defaults(fn=cmd_analyze)
    s = sub.add_parser("graph"); s.add_argument("file"); s.add_argument("--mem"); s.add_argument("--replay")
    s.add_argument("--replay-limit", type=int, default=20000); s.set_defaults(fn=cmd_graph)
    s = sub.add_parser("movement"); s.add_argument("logs", nargs="+"); s.set_defaults(fn=cmd_movement)
    a = p.parse_args(argv)
    a.fn(a)


if __name__ == "__main__":
    main()
