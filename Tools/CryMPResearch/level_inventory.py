#!/usr/bin/env python3
"""
CryMP level inventory (offline, read-only; no game, stdlib only).

Opens an installed multiplayer level.pak in place and reports facts a bot designer needs:
  * which CryAI navigation exports exist and how large they are (an empty *.bai export
    means CryAI pathfinding/hidespots have no data on that map, whatever the AI system can do);
  * counts of gameplay entity classes (SpawnGroup, Factory, HQ, AlienEnergyPoint, turrets, ...);
  * per objective entity: name, class, position and the area ids it references.

Nothing is extracted or copied: archive contents stay local. Output contains only names,
counts and coordinates, never script/asset bytes.

Usage:
  level_inventory.py <path/to/level.pak> [--mission mission_mission0.xml] [--markdown out.md]
"""
import argparse
import collections
import re
import sys
import zipfile
import xml.etree.ElementTree as ET

OBJECTIVES = ("SpawnGroup", "Factory", "HQ", "AlienEnergyPoint", "BuyZone", "ServiceZone",
              "ForbiddenArea", "AutoTurret", "AutoTurretAA", "SpectatorPoint", "Flag")
AI_HINTS = ("AIAnchor", "TagPoint", "AIPath", "AINavigationModifier", "AIHorizontalOcclusionPlane",
            "AIReinforcementSpot", "SmartObject", "CoverSurface")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pak")
    ap.add_argument("--mission", default=None, help="mission xml inside the pak (default: first mission_*.xml)")
    ap.add_argument("--markdown")
    a = ap.parse_args()

    z = zipfile.ZipFile(a.pak)
    names = {i.filename.lower(): i for i in z.infolist()}
    out = []
    w = out.append
    w(f"# Level inventory: `{a.pak.replace(chr(92), '/').split('/Levels/')[-1]}`\n")

    w("## CryAI navigation exports\n")
    bai = sorted((n, i.file_size) for n, i in names.items() if n.endswith(".bai"))
    if not bai:
        w("No *.bai exports: CryAI navigation is absent.")
    for n, size in bai:
        verdict = "EMPTY/header only" if size < 1024 else "has data"
        w(f"- `{n}`: {size} bytes ({verdict})")
    w("")

    mission = a.mission or next((n for n in names if re.match(r"mission_.*\.xml$", n)), None)
    if not mission:
        w("No mission xml found.")
    else:
        root = ET.fromstring(z.read(names[mission.lower()].filename))
        counts = collections.Counter()
        objectives = []
        for e in root.iter("Entity"):
            cls = e.get("EntityClass", "?")
            counts[cls] += 1
            if cls in OBJECTIVES:
                props = e.find("Properties")
                areas = []
                if props is not None:
                    for k, v in props.attrib.items():
                        if "area" in k.lower() and v not in ("", "0"):
                            areas.append(f"{k}={v}")
                objectives.append((cls, e.get("Name", "?"), e.get("Pos", "?"), ", ".join(areas)))
        w(f"## Entity classes in `{mission}`\n")
        w("| Class | Count |\n|---|---|")
        for cls, c in counts.most_common():
            w(f"| {cls} | {c} |")
        w("")
        w("CryAI hint entities present: " + (", ".join(f"{h}={counts[h]}" for h in AI_HINTS if counts[h]) or "none") + "\n")
        w("## Objective entities\n")
        w("| Class | Name | Pos | Area properties |\n|---|---|---|---|")
        for row in sorted(objectives):
            w("| " + " | ".join(row) + " |")
    text = "\n".join(out) + "\n"
    if a.markdown:
        open(a.markdown, "w", encoding="utf-8").write(text)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
