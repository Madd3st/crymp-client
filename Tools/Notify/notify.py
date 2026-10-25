#!/usr/bin/env python3
"""WarsBots -> Telegram: tells the owner when an agent finished, got blocked, went silent, or a decision needs him.

Tiny on purpose (one shallow git fetch every run; no game, no build). Run from cron every 10 minutes:
    */10 * * * * cd /opt/warsbots-notify && /usr/bin/python3 notify.py >> notify.log 2>&1
Config: environment variables or a `notify.env` file next to this script (KEY=VALUE lines):
    TG_TOKEN   bot token from @BotFather              (required)
    TG_CHAT    your chat id (send /start to the bot, then see README)  (required)
    REPO_URL   default https://github.com/Madd3st/crymp-client.git (a private repo needs a read-only token URL)
    SILENT_MIN minutes without a push while STARTED/PROGRESS before a "silent" alert (default 45)
State lives in `state.json` next to the script; the clone in `repo/`. Python 3.8+, git; no pip packages.
"""
import json
import os
import re
import subprocess
import sys
import time
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
TRIAGE = "warsbots/research/steelmill-triage"
LANES = [  # agent, outbox branch, outbox path
    ("Codex", "warsbots/feature/weapon-goals", "Docs/Coordination/TO_CLAUDE.md"),
    ("DeepSeek", "warsbots/research/deepseek", "Docs/Coordination/DEEPSEEK_TO_CLAUDE.md"),
]
DECISIONS = "Docs/Research/Coordination/MAILBOX/DECISIONS.md"
FINAL = re.compile(r"\b(DONE|BLOCKED|REJECTED)\b")


def load_env():
    path = os.path.join(HERE, "notify.env")
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                os.environ.setdefault(k.strip(), v.strip())


def git(*args):
    r = subprocess.run(["git", "-C", os.path.join(HERE, "repo"), *args], capture_output=True, text=True)
    return r.stdout


def sync(url):
    repo = os.path.join(HERE, "repo")
    if not os.path.isdir(os.path.join(repo, ".git")):
        subprocess.run(["git", "init", "-q", repo], check=True)
        subprocess.run(["git", "-C", repo, "remote", "add", "origin", url], check=True)
    branches = [TRIAGE] + [l[1] for l in LANES]
    specs = [f"+refs/heads/{b}:refs/remotes/origin/{b}" for b in branches]
    r = subprocess.run(["git", "-C", repo, "fetch", "-q", "--depth", "50", "origin", *specs], capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError("git fetch failed: " + r.stderr.strip()[:300])


def show(branch, path):
    return git("show", f"origin/{branch}:{path}")


def push_time(branch):
    out = git("log", "-1", "--format=%ct", f"origin/{branch}").strip()
    return int(out) if out.isdigit() else 0


def entries(text):
    parts = re.split(r"^(#{2,3} .*)$", text, flags=re.M)
    return [(parts[i].strip(), parts[i + 1]) for i in range(1, len(parts) - 1, 2)]


def send(text):
    token, chat = os.environ.get("TG_TOKEN"), os.environ.get("TG_CHAT")
    if not token or not chat:
        print("TG_TOKEN/TG_CHAT not set; would send:\n" + text)
        return
    data = urllib.parse.urlencode({"chat_id": chat, "text": text, "disable_web_page_preview": "true"}).encode()
    urllib.request.urlopen(f"https://api.telegram.org/bot{token}/sendMessage", data=data, timeout=20).read()


def short(header, body, limit=260):
    h = re.sub(r"^#+\s*", "", header)
    b = " ".join(body.split())
    return (h + " — " + b)[:limit]


def main():
    load_env()
    url = os.environ.get("REPO_URL", "https://github.com/Madd3st/crymp-client.git")
    silent_min = int(os.environ.get("SILENT_MIN", "45"))
    state_path = os.path.join(HERE, "state.json")
    state = json.load(open(state_path)) if os.path.exists(state_path) else {}
    first_run = not state
    sync(url)
    msgs, now = [], time.time()

    for agent, branch, path in LANES:
        out = show(branch, path)
        seen = set(state.get(agent + ".seen", []))
        heads = entries(out)
        ids = [h for h, _ in heads]
        for header, body in heads:
            if header in seen:
                continue
            if FINAL.search(header) or FINAL.search(body[:200]):
                icon = "🔴" if re.search(r"\b(BLOCKED|REJECTED)\b", header + body[:200]) else "✅"
                msgs.append(f"{icon} {agent}: {short(header, body)}")
        state[agent + ".seen"] = ids[:200]
        m = re.search(r"^NOW:(.*)$", out, re.M)
        now_line = m.group(1).strip() if m else ""
        last = push_time(branch)
        busy = re.search(r"\b(STARTED|PROGRESS)\b", now_line)
        key = agent + ".silentAlerted"
        if busy and last and now - last > silent_min * 60:
            if state.get(key) != last:
                msgs.append(f"🟡 {agent} молчит {int((now - last) / 60)} мин, а в NOW: «{now_line[:150]}»")
                state[key] = last
        if re.search(r"\bidle\b", now_line, re.I) and state.get(agent + ".now") != now_line:
            msgs.append(f"💤 {agent} свободен: «{now_line[:150]}»")
        state[agent + ".now"] = now_line

    # decisions waiting for the owner: a D- row whose last column has no "yes"/"no"
    pending = []
    for line in show(TRIAGE, DECISIONS).splitlines():
        m = re.match(r"\|\s*(D-\d+)\s*\|(.*)\|\s*([^|]*)\|\s*$", line)
        if m and not re.search(r"\b(yes|no|да|нет)\b", m.group(3), re.I):
            pending.append(m.group(1) + ": " + " ".join(m.group(2).split())[:120])
    old = set(state.get("pending", []))
    for p in pending:
        if p.split(":")[0] not in {o.split(":")[0] for o in old}:
            msgs.append(f"❓ Нужно твоё решение — {p}")
    state["pending"] = pending

    json.dump(state, open(state_path, "w"), ensure_ascii=False, indent=1)
    if first_run:
        send("WarsBots-уведомления подключены. Буду писать, когда агент закончил или упёрся, молчит дольше "
             f"{silent_min} мин или нужно твоё решение.")
        return
    if msgs:
        send("\n\n".join(msgs[:10]) + ("\n\n(+ ещё %d)" % (len(msgs) - 10) if len(msgs) > 10 else ""))


if __name__ == "__main__":
    try:
        main()
    except Exception as e:  # cron logs it; never spam Telegram with tracebacks
        print(time.strftime("%F %T"), "error:", e, file=sys.stderr)
        sys.exit(1)
