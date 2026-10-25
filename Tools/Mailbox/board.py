#!/usr/bin/env python3
"""Mailbox board: every open C-/DS- item with the receiver's last state (PROTOCOL v4, D-017).

Usage: python3 Tools/Mailbox/board.py [--repo PATH] [--last N]
Fetches the three branches, reads the outboxes with git show, prints a table and each agent's NOW line.
"""
import argparse
import re
import subprocess

TRIAGE = "warsbots/research/steelmill-triage"
LANES = [
    # (agent, item prefix, inbox path on triage, outbox branch, outbox path)
    ("Codex", "C", "Docs/Research/Coordination/MAILBOX/TO_CODEX.md",
     "warsbots/feature/weapon-goals", "Docs/Coordination/TO_CLAUDE.md"),
    ("DeepSeek", "DS", "Docs/Research/Coordination/MAILBOX/TO_DEEPSEEK.md",
     "warsbots/research/deepseek", "Docs/Coordination/DEEPSEEK_TO_CLAUDE.md"),
]
STATES = ["REJECTED", "BLOCKED", "DONE", "PROGRESS", "STARTED", "ACK"]
WORDS = {  # loose words used before v4
    "DONE": ["complete", "done", "pass", "report", "verdict"],
    "BLOCKED": ["blocked", "fail"],
    "ACK": ["acked", "started"],
}


def git(repo, *args):
    return subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True).stdout


def show(repo, branch, path):
    return git(repo, "show", f"origin/{branch}:{path}")


def entries(text):
    """Split a mailbox file into (header, body) entries, newest first."""
    parts = re.split(r"^(#{2,3} .*)$", text, flags=re.M)
    return [(parts[i].strip(), parts[i + 1]) for i in range(1, len(parts) - 1, 2)]


def item_ids(text, prefix):
    out = []
    for header, _ in entries(text):
        m = re.match(rf"#{{2,3}} ({prefix}-\d+)\s*[·|]\s*([^·|]*)[·|]?\s*(.*)", header)
        if m:
            out.append((m.group(1), m.group(2).strip(), m.group(3).strip()[:60]))
    return out


def id_pattern(item):
    prefix, num = item.split("-")
    return re.compile(rf"\b{prefix}-?0*{int(num)}\b")


def classify(header, body):
    text = header + " " + body[:400]
    for s in STATES:
        if re.search(rf"\b{s}\b", text):
            return s
    low = text.lower()
    for s, words in WORDS.items():
        if any(w in low for w in words):
            return s
    return "mentioned"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--last", type=int, default=8, help="items per agent")
    a = ap.parse_args()
    git(a.repo, "fetch", "-q", "origin", *[f"+refs/heads/{b}:refs/remotes/origin/{b}" for b in [TRIAGE] + [l[3] for l in LANES]])
    for agent, prefix, inbox, branch, outbox in LANES:
        sent = item_ids(show(a.repo, TRIAGE, inbox), prefix)[: a.last]
        out = show(a.repo, branch, outbox)
        now = re.search(r"^NOW:.*$", out, re.M)
        last = git(a.repo, "log", "-1", "--format=%ad", "--date=format:%d.%m %H:%M", f"origin/{branch}").strip()
        print(f"\n== {agent}  (last push {last})")
        print("   " + (now.group(0) if now else "NOW: <not written - PROTOCOL v4 asks for it>"))
        replies = entries(out)
        for item, when, title in sent:
            pat = id_pattern(item)
            hit = next(((h, b) for h, b in replies if pat.search(h) or pat.search(b[:300])), None)
            state = classify(*hit) if hit else "NO REPLY"
            ref = re.match(r"#+ (\S+)", hit[0]).group(1) if hit else ""
            print(f"   {item:7} {when:18} {state:10} {ref:8} {title}")


if __name__ == "__main__":
    main()
