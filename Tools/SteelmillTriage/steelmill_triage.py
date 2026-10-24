#!/usr/bin/env python3
"""Steelmill Triage: offline inspection of WarsBots map memory, trace logs and (future) graph exports.

  steelmill_triage.py mem  <file.mem> [--against older.mem]       memory statistics / growth
  steelmill_triage.py analyze <run.bots.log>... [--mem f.mem] [--graph level.graph.json] [--out DIR]
  steelmill_triage.py graph <level.graph.json> [--mem f.mem] [--replay run.bots.log]   (warsbots/botnav-graph/1)
  steelmill_triage.py movement <run.bots.log>...                   pace profile, bots vs humans
  steelmill_triage.py level --pak level.pak --level-xml Steelmill.xml --minimap Steelmill.dds --out map.json
  steelmill_triage.py episodes episodes.jsonl [--runs runs.json]      ML dataset readiness
  steelmill_triage.py summary <run.bots.log> [--graph g.json] [--mem f.mem] [--out summary.json]   (warsbots-run-summary/1)

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
from wblab import summary as runsummary  # noqa: E402
from wblab import level_data, ml_validate  # noqa: E402


def cmd_mem(a):
    m = mem.read(a.file)
    out = m.summary()
    if a.against:
        out["growth_since"] = dict(file=str(a.against), **mem.diff(mem.read(a.against), m))
    print(json.dumps(out, indent=2))


def cmd_analyze(a):
    memory = mem.read(a.mem) if a.mem else None
    graph = navgraph.load(a.graph, memory) if a.graph else None
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
    memory = mem.read(a.mem) if a.mem else None
    data = navgraph.read_json(a.file)
    errors, warnings = navgraph.check(data, memory)
    out = dict(schema="steelmill-triage-graph/1", file=Path(a.file).name, valid=not errors,
               errors=errors, warnings_total=len(warnings), warnings=warnings[:20])
    if not errors:
        g = navgraph.NavGraph(data)
        out.update(g.summary())
        out["scc"] = g.scc_profile()
        if memory:
            out["memory_link"] = "OK"
        if a.replay:
            for run in logparse.parse_file(a.replay):
                counts, _ = navgraph.replay_fallbacks(g, run, memory.stuck if memory else None, limit=a.replay_limit)
                out.setdefault("fallback_replay", []).append(dict(run=run.index, **counts))
            text = Path(a.replay).read_text(encoding="utf-8", errors="replace").splitlines()
            out["native_routes"] = g.classify_native_routes(text)
    print(json.dumps(out, indent=2))
    if errors:
        raise SystemExit(2)


def cmd_summary(a):
    memory = mem.read(a.mem) if a.mem else None
    graph = navgraph.load(a.graph, memory) if a.graph else None
    out = runsummary.build(a.log, graph, memory, a.label)
    text = json.dumps(out, indent=2) + "\n"
    if a.out:
        Path(a.out).write_text(text, encoding="utf-8")
    print(text, end="")


def cmd_level(a):
    """Map facts from the installed level files (copies): spawns, items, areas, minimap bounds/PNG."""
    sub = ["--pak", a.pak, "--out", a.out] + (["--level-xml", a.level_xml] if a.level_xml else []) \
        + (["--minimap", a.minimap] if a.minimap else []) + sum((["--check-log", c] for c in a.check_log), [])
    return level_data.main([str(x) for x in sub])


def cmd_episodes(a):
    """Contextual-bandit readiness of Codex episodes.jsonl (wb_tactics_report.py schema 2)."""
    sub = [a.episodes] + (["--runs", a.runs] if a.runs else []) + (["--json", a.json] if a.json else [])
    return ml_validate.main([str(x) for x in sub])


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
    s = sub.add_parser("summary"); s.add_argument("log"); s.add_argument("--graph"); s.add_argument("--mem")
    s.add_argument("--label"); s.add_argument("--out"); s.set_defaults(fn=cmd_summary)
    s = sub.add_parser("level"); s.add_argument("--pak", required=True); s.add_argument("--level-xml")
    s.add_argument("--minimap"); s.add_argument("--out", required=True); s.add_argument("--check-log", action="append", default=[])
    s.set_defaults(fn=cmd_level)
    s = sub.add_parser("episodes"); s.add_argument("episodes"); s.add_argument("--runs"); s.add_argument("--json")
    s.set_defaults(fn=cmd_episodes)
    s = sub.add_parser("movement"); s.add_argument("logs", nargs="+"); s.set_defaults(fn=cmd_movement)
    a = p.parse_args(argv)
    a.fn(a)


if __name__ == "__main__":
    main()
