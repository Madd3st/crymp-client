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
from wblab import pace  # noqa: E402
from wblab import navmesh  # noqa: E402


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


def cmd_pace(a):
    """MOVE TRACE pace per kind (human/bot) + C-013 acceptance for bots."""
    res = {}
    for path in a.logs:
        with open(path, encoding="utf-8", errors="replace") as f:
            res[path] = pace.profile(pace.parse(f, side=a.side))
    if a.json:
        print(json.dumps(res, indent=1)); return
    for path, kinds in res.items():
        print(f"== {path}")
        for kind, p in sorted(kinds.items()):
            print(f"  {kind}: actors={p['actors']} samples={p['samples']} moving={p['moving_pct']}% "
                  f"median={p['moving_median']} p90={p['p90']} max={p['max']} >4={p['over4_pct']}% "
                  f"sprint={p['sprint_pct']}% modes={p['modes_pct']} energy={p['energy_median']} "
                  f"takeoffs={p['takeoffs_total']}")
            if kind == "bot":
                ok, notes = pace.accept(p)
                print("    C-013 " + ("PASS" if ok else "FAIL") + ": " + "; ".join(notes))


def _points(path, key):
    """(id, x, y, z) from a level_data map JSON (spawns/items with pos) or a C-019 <map>.spawns/items.json."""
    if not path:
        return []
    with open(path, encoding="utf-8") as f:
        d = json.load(f)
    rows = d.get(key, []) if isinstance(d, dict) else d
    out = []
    for r in rows:
        p = r.get("pos") or [r.get("x"), r.get("y"), r.get("z")]
        out.append((r.get("id"), float(p[0]), float(p[1]), float(p[2])))
    return out


def cmd_navmesh(a):
    """Recast-style walkability model from exported geometry (OBJ) + the graph metrics (D-006)."""
    v, f = navmesh.load_obj(a.obj, up=a.up)
    bounds = None
    if a.bounds:
        x0, y0, x1, y1 = (float(t) for t in a.bounds.split(","))
        bounds = ((x0, y0), (x1, y1))
    m = navmesh.WalkModel(v, f, bounds=bounds, cs=a.cs, climb=a.climb, agent_r=a.radius,
                          agent_h=a.height, max_drop=a.max_drop, max_slope=a.slope)
    route = []
    for log in a.trace or []:
        with open(log, encoding="utf-8", errors="replace") as fh:
            for line in fh:
                if "MOVE TRACE" in line and "kind=human" in line and "health=0" not in line:
                    try:
                        t = float(line.split(" t=")[1].split()[0])
                        x, y, z = (float(c) for c in line.split(" pos=(")[1].split(")")[0].split(","))
                        route.append((t, x, y, z))
                    except (IndexError, ValueError):
                        pass
    sp, it = _points(a.points or a.spawns, "spawns"), _points(a.points or a.items, "items")
    res = navmesh.profile(m, spawns=sp, items=it, route=route)
    if not a.no_jumps:
        # Crysis is vertical: compare walk-only with normal jumps and Strength jumps added
        # (v0 4.43 / 9.60 m/s, PlayerMovement.cpp:1134,1150-1170; horizontal speed = run 4.5 m/s by default)
        res = {"walk": res}
        n = m.add_jumps("jump", v0=4.43, vh=a.jump_vh)
        res["walk+jump"] = dict(navmesh.profile(m, spawns=sp, items=it, route=route), links=n)
        n = m.add_jumps("strength", v0=9.60, vh=a.jump_vh)
        res["walk+jump+strength"] = dict(navmesh.profile(m, spawns=sp, items=it, route=route), links=n)
    if a.png:
        navmesh.render_png(m, a.png, spawns=_points(a.points or a.spawns, "spawns"), route=route)
        res["png"] = a.png
    res["input"] = dict(obj=a.obj, vertices=int(len(v)), triangles=int(len(f)), spans=int(len(m.col)),
                        columns=[m.nx, m.ny])
    print(json.dumps(res, indent=1, default=lambda o: o.item() if hasattr(o, "item") else str(o)))


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
    s = sub.add_parser("pace"); s.add_argument("logs", nargs="+"); s.add_argument("--side", default="server")
    s.add_argument("--json", action="store_true"); s.set_defaults(fn=cmd_pace)
    s = sub.add_parser("navmesh"); s.add_argument("obj"); s.add_argument("--up", default="z")
    s.add_argument("--bounds", help="x0,y0,x1,y1"); s.add_argument("--points", help="level_data map JSON")
    s.add_argument("--spawns"); s.add_argument("--items"); s.add_argument("--trace", nargs="*")
    s.add_argument("--png", help="top-view PNG of the walk model (with all links)")
    s.add_argument("--no-jumps", action="store_true"); s.add_argument("--jump-vh", type=float, default=4.5)
    s.add_argument("--cs", type=float, default=0.3); s.add_argument("--climb", type=float, default=0.5)
    s.add_argument("--radius", type=float, default=0.4); s.add_argument("--height", type=float, default=1.8)
    s.add_argument("--max-drop", type=float, default=4.0); s.add_argument("--slope", type=float, default=45.0)
    s.set_defaults(fn=cmd_navmesh)
    s = sub.add_parser("movement"); s.add_argument("logs", nargs="+"); s.set_defaults(fn=cmd_movement)
    a = p.parse_args(argv)
    a.fn(a)


if __name__ == "__main__":
    main()
