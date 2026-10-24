#!/usr/bin/env python3
"""Extract verifiable Steelmill map facts from the installed level files (read-only, local).

Inputs (copies in the lab folder, never the game install):
  level.pak      mission_mission0.xml: SpawnPoint, weapon/ammo entities (Layer "Weapons"),
                 spectator points, AreaBox/AreaSphere; *.bai CryAI navigation exports.
  <Level>.xml    MiniMap bounds (startX/startY/endX/endY).
  <Level>.dds    top-down minimap texture (optional; converted to PNG if Pillow is present).

Output: one map JSON for the viewer and the log analyzer. It contains entity names, classes,
EntityIds and positions only: facts, no asset bytes. It is written under build/ (git-ignored)
and must not be committed, together with the PNG.

Mission EntityIds are kept at runtime: e.g. bots.log "revive ... previousSpawn=195" equals
SpawnPoint107 EntityId=195 in the mission; check_ids() reports this agreement per log.

Minimap orientation follows the engine's own mapping, CHUDRadar::GetPosOnMap
(Code/CryGame/HUD/HUDRadar.cpp:1583-1598): image u comes from world Y, image v from world X:
    u = (Y - startY) / (endY - startY),  v = (X - startX) / (endX - startX)
"""
import argparse
import hashlib
import json
import re
import sys
import zipfile
import xml.etree.ElementTree as ET
from pathlib import Path

ITEM_LAYERS = {"Weapons"}
ITEM_CLASSES = {"SCAR", "FY71", "SMG", "Shotgun", "SOCOM", "DSG1", "GaussRifle", "LAW", "Hurricane",
                "Claymore", "C4", "CustomAmmoPickup", "AVMine", "Binoculars", "AlienMount", "MOARAttach"}


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def vec(s):
    try:
        return [round(float(v), 3) for v in s.split(",")]
    except (AttributeError, ValueError):
        return None


def extract(pak, meta_xml=None, minimap=None, png_out=None):
    z = zipfile.ZipFile(pak)
    names = {i.filename.lower(): i for i in z.infolist()}
    mission = next(n for n in names if re.match(r"mission_.*\.xml$", n))
    root = ET.fromstring(z.read(names[mission].filename))
    out = dict(schema="steelmill-triage/map/1", provenance=dict(level_pak=dict(path=str(pak), sha256=sha256(pak)),
                                                             mission=mission),
               spawns=[], items=[], spectators=[], areas=[], other_counts={},
               navigation_exports={n: i.file_size for n, i in names.items() if n.endswith(".bai")})
    counts = {}
    for e in root.iter("Entity"):
        cls = e.get("EntityClass")
        if not cls:
            continue
        counts[cls] = counts.get(cls, 0) + 1
        pos = vec(e.get("Pos"))
        eid = int(e.get("EntityId")) if (e.get("EntityId") or "").isdigit() else None
        rec = dict(id=eid, name=e.get("Name"), pos=pos)
        if cls == "SpawnPoint":
            out["spawns"].append(rec)
        elif cls == "SpectatorPoint":
            out["spectators"].append(rec)
        elif e.get("Layer") in ITEM_LAYERS or cls in ITEM_CLASSES:
            out["items"].append(dict(rec, **{"class": cls, "layer": e.get("Layer")}))
        elif cls in ("AreaBox", "AreaSphere"):
            p = e.find("Properties")
            props = dict(p.attrib) if p is not None else {}
            out["areas"].append(dict(rec, **{"class": cls, "properties": {k: v for k, v in props.items()
                                                                        if k in ("BoxSizeX", "BoxSizeY", "BoxSizeZ", "Radius")}}))
    out["other_counts"] = dict(sorted(counts.items(), key=lambda kv: -kv[1]))
    nav_has_data = any(size >= 1024 for size in out["navigation_exports"].values())
    out["cryai_navigation"] = "present" if nav_has_data else "absent (all *.bai exports < 1 KiB)"
    if meta_xml:
        mm = ET.parse(meta_xml).getroot().find("MiniMap")
        if mm is not None:
            out["minimap"] = dict(file=mm.get("Filename"), start_x=float(mm.get("startX")),
                                  start_y=float(mm.get("startY")), end_x=float(mm.get("endX")),
                                  end_y=float(mm.get("endY")),
                                  orientation="u=(Y-startY)/(endY-startY), v=(X-startX)/(endX-startX)",
                                  orientation_source="Code/CryGame/HUD/HUDRadar.cpp CHUDRadar::GetPosOnMap",
                                  orientation_status="engine convention; visually plausible, not verified in game")
            out["provenance"]["level_xml"] = dict(path=str(meta_xml), sha256=sha256(meta_xml))
    if minimap and png_out:
        try:
            from PIL import Image
            img = Image.open(minimap).convert("RGB")
            img.save(png_out)
            out["minimap_png"] = dict(file=Path(png_out).name, size=list(img.size))
            out["provenance"]["minimap_dds"] = dict(path=str(minimap), sha256=sha256(minimap))
        except Exception as exc:          # Pillow missing or unsupported DDS: viewer still works
            out["minimap_png"] = dict(error=str(exc))
    out["spawns"].sort(key=lambda r: r["id"] or 0)
    out["items"].sort(key=lambda r: r["id"] or 0)
    return out


def check_ids(map_data, log_path):
    """Agreement of runtime ids in a bots.log with mission EntityIds (evidence for id stability)."""
    text = Path(log_path).read_text(encoding="utf-8", errors="replace")
    spawn_ids = {s["id"] for s in map_data["spawns"]}
    item_ids = {i["id"] for i in map_data["items"]}
    used_spawns = {int(v) for v in re.findall(r"spawnId=(\d+)", text)} | {int(v) for v in re.findall(r"previousSpawn=(\d+)", text)}
    used_spawns.discard(0)
    used_items = {int(v) for v in re.findall(r"(?:item=|weapon )(\d+)(?: |\.|$| unreachable)", text)}
    return dict(log=str(log_path), spawn_ids_seen=len(used_spawns), spawn_ids_in_mission=len(used_spawns & spawn_ids),
                item_ids_seen=len(used_items), item_ids_in_mission=len(used_items & item_ids),
                unknown_item_ids=sorted(used_items - item_ids)[:50])


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pak", type=Path, required=True)
    ap.add_argument("--level-xml", type=Path)
    ap.add_argument("--minimap", type=Path)
    ap.add_argument("--out", type=Path, required=True, help="output map JSON (under build/)")
    ap.add_argument("--check-log", type=Path, action="append", default=[])
    a = ap.parse_args(argv)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    png = a.out.with_name("minimap.png") if a.minimap else None
    data = extract(a.pak, a.level_xml, a.minimap, png)
    data["id_checks"] = [check_ids(data, p) for p in a.check_log]
    a.out.write_text(json.dumps(data, indent=1) + "\n", encoding="utf-8")
    print(f"map: {len(data['spawns'])} spawns, {len(data['items'])} items, {len(data['areas'])} areas, "
          f"CryAI nav {data['cryai_navigation']}")
    for c in data["id_checks"]:
        print(f"  ids {Path(c['log']).name}: spawns {c['spawn_ids_in_mission']}/{c['spawn_ids_seen']}, "
              f"items {c['item_ids_in_mission']}/{c['item_ids_seen']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
