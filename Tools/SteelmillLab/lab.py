#!/usr/bin/env python3
"""Steelmill AI Lab: one command that runs every offline tool and builds the viewer.

    python Tools/SteelmillLab/lab.py --sources build/steelmill-lab/sources

Expects COPIES of real data in --sources (never point it at the game install or at the
live %USER%/WarsBots folder):
    level.pak, Steelmill.xml, Steelmill.dds      (from Game/Levels/Multiplayer/IA/Steelmill)
    *.mem                                         (WBMEM01 map memory copy)
    *.bots.log                                    (WarsBots bot logs)
    *.graph.json                                  (optional future BotNav export)
Everything is written to --out (default build/steelmill-lab/out, git-ignored):
    SOURCES.sha256, steelmill_map.json, minimap.png, mem_report*.json, movement/*.json,
    viewer/index.html + viewer/data.js + viewer/minimap.png  -> open viewer/index.html
"""
import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(HERE))
import botlog      # noqa: E402
import level_data  # noqa: E402
import navgraph    # noqa: E402
import wbmem       # noqa: E402


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sources", type=Path, required=True)
    ap.add_argument("--out", type=Path, default=Path("build/steelmill-lab/out"))
    ap.add_argument("--viewer-runs", default="*", help="glob of movement runs to embed in the viewer")
    ap.add_argument("--max-embed-mb", type=float, default=40.0)
    a = ap.parse_args(argv)
    src, out = a.sources, a.out
    out.mkdir(parents=True, exist_ok=True)
    files = sorted(p for p in src.iterdir() if p.is_file())
    manifest = [f"{sha(p)}  {p.name}" for p in files]
    (out / "SOURCES.sha256").write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print(f"[sources] {len(files)} files hashed -> {out / 'SOURCES.sha256'}")

    map_data = None
    pak = src / "level.pak"
    if pak.exists():
        xml = next(iter(src.glob("*.xml")), None)
        dds = next(iter(src.glob("*.dds")), None)
        map_data = level_data.extract(pak, xml, dds, out / "minimap.png" if dds else None)
        logs = sorted(src.glob("*.bots.log"))
        map_data["id_checks"] = [level_data.check_ids(map_data, p) for p in logs]
        (out / "steelmill_map.json").write_text(json.dumps(map_data, indent=1) + "\n", encoding="utf-8")
        print(f"[map] {len(map_data['spawns'])} spawns, {len(map_data['items'])} items; CryAI nav "
              f"{map_data['cryai_navigation']}")
    graph = None
    for gpath in sorted(src.glob("*.graph.json")):
        g = navgraph.load(gpath)
        err, warn = navgraph.validate(g)
        print(f"[graph] {gpath.name}: {'VALID' if not err else 'INVALID'} ({len(err)} errors, {len(warn)} warnings)")
        if not err:
            graph = g
    mem_report = None
    for mpath in sorted(src.glob("*.mem")):
        raw = mpath.read_bytes()
        try:
            parsed = wbmem.parse_bytes(raw, mpath.name)
        except wbmem.MemError as e:
            print(f"[mem] {mpath.name}: INVALID {e}")
            continue
        rep = wbmem.summarize(parsed)
        rep["sha256"] = hashlib.sha256(raw).hexdigest()
        if graph:
            rep["graph_link"] = wbmem.link_graph(parsed, graph)
        (out / f"mem_report_{mpath.stem}.json").write_text(json.dumps(rep, indent=1) + "\n", encoding="utf-8")
        mem_report = mem_report or rep
        print(f"[mem] {mpath.name}: {'VALID' if rep['valid'] else 'INVALID'}, nodes {rep['header']['count']}, "
              f"stuck {rep['stats']['stuck']['total']:.0f} kills {rep['stats']['kills']['total']:.0f} "
              f"deaths {rep['stats']['deaths']['total']:.0f}")
    items = {int(i["id"]): {"class": i["class"], "pos": i["pos"]} for i in (map_data or {}).get("items", []) if i.get("id")}
    (out / "movement").mkdir(exist_ok=True)
    runs = []
    for log in sorted(src.glob("*.bots.log")):
        r = botlog.analyze(log, items)
        name = log.name.replace(".bots.log", "")
        r["_name"] = name
        (out / "movement" / f"{name}.movement.json").write_text(json.dumps(r, separators=(",", ":")), encoding="utf-8")
        c = r["summary"]["counts"]
        print(f"[log] {name}: bots {r['summary']['bots']}, samples {r['summary']['samples']}, "
              f"A* failed {c.get('astar_failed', 0)}, stall_unblocked {c.get('stall_unblocked', 0)}")
        if Path(name).match(a.viewer_runs):
            runs.append(r)
    viewer = out / "viewer"
    viewer.mkdir(exist_ok=True)
    shutil.copyfile(HERE / "viewer" / "steelmill_viewer.html", viewer / "index.html")
    if (out / "minimap.png").exists():
        shutil.copyfile(out / "minimap.png", viewer / "minimap.png")
    runs.sort(key=lambda r: -r["summary"]["samples"])
    payload = dict(map=map_data, runs=[], graph=graph, mem=mem_report,
                   minimap="minimap.png" if (viewer / "minimap.png").exists() else None)
    budget = a.max_embed_mb * 1024 * 1024
    used = len(json.dumps(dict(payload, runs=[])))
    for r in runs:
        size = len(json.dumps(r, separators=(",", ":")))
        if used + size > budget:
            print(f"[viewer] skipped {r['_name']} ({size / 1e6:.1f} MB): over --max-embed-mb; load it via the file picker")
            continue
        payload["runs"].append(r)
        used += size
    (viewer / "data.js").write_text("window.LAB_DATA=" + json.dumps(payload, separators=(",", ":")) + ";\n",
                                    encoding="utf-8")
    print(f"[viewer] {viewer / 'index.html'} ({len(payload['runs'])} runs embedded, {used / 1e6:.1f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
