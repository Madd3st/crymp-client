#!/usr/bin/env python3
"""Run recipe of a C- item (D-021 #9): python3 Tools/Mailbox/recipe.py C-098 [--ps]

A C- item that asks for a run carries a fenced block:
    ```recipe
    build 9ac55001
    preset owner-c080            # feature set of Tools/Play-WarsBotsC080.ps1
    wb_bot_difficulty 1
    wb_bot_finish 1
    bots 15  seconds 660
    ```
This prints the block as JSON (or as PowerShell `+set` arguments with --ps) so Codex's driver sets exactly these
values and compares them with the effective console values before the run starts (fail-closed).
"""
import json, re, sys
item = sys.argv[1] if len(sys.argv) > 1 else ""
text = open("Docs/Research/Coordination/MAILBOX/TO_CODEX.md", encoding="utf-8").read()
m = re.search(rf"^### {re.escape(item)} .*?(?=^### C-|\Z)", text, re.M | re.S)
if not m: sys.exit(f"{item} not found")
b = re.search(r"```recipe\n(.*?)```", m.group(0), re.S)
if not b: sys.exit(f"{item} has no ```recipe block")
r = {"cvars": {}}
for line in b.group(1).splitlines():
    line = line.split("#", 1)[0].strip()
    if not line: continue
    tok = line.split()
    if tok[0] in ("build", "preset"): r[tok[0]] = tok[1]
    elif tok[0] == "bots": r["bots"] = int(tok[1]); r["seconds"] = int(tok[3]) if len(tok) > 3 else 660
    else: r["cvars"][tok[0]] = tok[1]
if "--ps" in sys.argv: print(" ".join(f"+{k} {v}" for k, v in r["cvars"].items()))
else: print(json.dumps(r, indent=1))
