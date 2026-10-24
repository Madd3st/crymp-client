#!/usr/bin/env python3
"""Copy real Steelmill data into the lab folder WITHOUT touching the originals.

    python Tools/SteelmillLab/collect.py ^
        --level-dir "E:/Steam/steamapps/common/Crysis/Game/Levels/Multiplayer/IA/Steelmill" ^
        --mem "%USERPROFILE%/Documents/My Games/Crysis/WarsBots/Multiplayer_IA_Steelmill.mem" ^
        --logs "%USERPROFILE%/Documents/My Games/Crysis/WarsBots-Tactics-*.bots.log" ^
        --dest build/steelmill-lab/sources

Originals are only opened for reading; copies are made read-only and listed with SHA256 in
<dest>/../SOURCES.sha256 (appended, with the original path). A running game may still be
writing a log: such a copy is a snapshot and its hash identifies exactly what was analysed.
"""
import argparse
import glob
import hashlib
import os
import shutil
import stat
import sys
import time
from pathlib import Path


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--level-dir", type=Path, help="Game/Levels/Multiplayer/IA/Steelmill")
    ap.add_argument("--mem", action="append", default=[])
    ap.add_argument("--logs", action="append", default=[], help="glob, repeatable")
    ap.add_argument("--dest", type=Path, default=Path("build/steelmill-lab/sources"))
    a = ap.parse_args(argv)
    files = []
    if a.level_dir:
        for name in ("level.pak", "Steelmill.xml", "Steelmill.dds"):
            p = a.level_dir / name
            if p.exists():
                files.append(p)
    files += [Path(os.path.expandvars(m)) for m in a.mem]
    for pattern in a.logs:
        files += [Path(p) for p in glob.glob(os.path.expandvars(pattern))]
    if not files:
        ap.error("nothing to copy")
    a.dest.mkdir(parents=True, exist_ok=True)
    manifest = a.dest.parent / "SOURCES.sha256"
    with open(manifest, "a", encoding="utf-8") as out:
        for src in files:
            dst = a.dest / src.name
            if dst.exists():
                os.chmod(dst, stat.S_IWRITE | stat.S_IREAD)
            shutil.copyfile(src, dst)                      # copyfile never modifies the source
            os.chmod(dst, stat.S_IREAD)
            digest = hashlib.sha256(dst.read_bytes()).hexdigest()
            out.write(f"{digest}  {dst.name}  <- {src}  copied {time.strftime('%Y-%m-%dT%H:%M:%S')}\n")
            print(f"{digest[:16]}  {dst.name}")
    print(f"manifest: {manifest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
