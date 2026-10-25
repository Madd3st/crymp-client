#!/usr/bin/env python3
"""Next free mailbox ID (D-021 #4): python3 Tools/Mailbox/next_id.py C|DS|D [--check]

Fetches steelmill-triage first, so two writers do not hand out the same number. --check exits 1 if any ID is used
twice in TO_CODEX.md / TO_DEEPSEEK.md / DECISIONS.md (run before every push).
"""
import re, subprocess, sys
REPO = "."
FILES = {"C": "Docs/Research/Coordination/MAILBOX/TO_CODEX.md", "DS": "Docs/Research/Coordination/MAILBOX/TO_DEEPSEEK.md",
         "D": "Docs/Research/Coordination/MAILBOX/DECISIONS.md"}
PAT = {"C": r"^### (C-\d+)", "DS": r"^### (DS-\d+)", "D": r"^\| (D-\d+) \|"}

def ids(kind):
    text = subprocess.run(["git", "show", f"origin/warsbots/research/steelmill-triage:{FILES[kind]}"], capture_output=True, text=True).stdout
    local = open(FILES[kind], encoding="utf-8").read()
    return re.findall(PAT[kind], text, re.M), re.findall(PAT[kind], local, re.M)

def main():
    subprocess.run(["git", "fetch", "-q", "origin", "+refs/heads/warsbots/research/steelmill-triage:refs/remotes/origin/warsbots/research/steelmill-triage"])
    if "--check" in sys.argv:
        bad = []
        for k in FILES:
            _, local = ids(k)
            seen = set()
            bad += [i for i in local if i in seen or seen.add(i)]
        if bad: print("DUPLICATE IDs:", ", ".join(sorted(set(bad)))); sys.exit(1)
        print("mailbox IDs OK"); return
    kind = sys.argv[1] if len(sys.argv) > 1 else "C"
    remote, local = ids(kind)
    n = max([int(i.split("-")[1]) for i in remote + local] or [0]) + 1
    print(f"{kind}-{n:03d}")

if __name__ == "__main__":
    main()
