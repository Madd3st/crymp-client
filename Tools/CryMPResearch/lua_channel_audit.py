#!/usr/bin/env python3
"""
CryMP Lua channel audit (offline, read-only; no game, no dependencies).

Resolves the *effective* script set the way CryMP loads it, parses every Lua function,
builds an approximate call graph and, starting from Power Struggle entry points, reports
every place where gameplay depends on a network channel or on Actor:IsPlayer() — the two
properties a channel-0 WarsBots actor does not have.

Effective file resolution (verified in source):
  * Code/CryScriptSystem/ScriptSystem.cpp ExecuteFile: "CryMP/<path>" overrides "<path>".
  * Code/CrySystem/CryPak.cpp: CryMP internal Pak.zip is CLIENT_PAK priority (> NORMAL), so
    repo Pak/Scripts/* overrides game paks; among game paks (same NORMAL priority) the
    later-opened ZPatch1.pak overrides GameData.pak.

Usage:
  lua_channel_audit.py --repo <crymp-client checkout> --pak-scripts <ZPatch1/Scripts> \
      --pak-scripts <GameData/Scripts> [--entry Name ...] [--markdown out.md]
Pass extracted game script folders in precedence order (highest first). Game scripts are
read locally only; this tool never copies them anywhere.
"""
import argparse
import collections
import os
import re
import sys

# ----------------------------------------------------------------------------- sinks
# Each rule: (category, regex, explanation). Categories are what a channel-0 bot breaks.
RULES = [
    ("CLIENT_RMI",  re.compile(r"\b(self|g_gameRules|g_localActor|[\w\.]+)\.onClient:(\w+)\s*\(\s*([^,\)]+)"),
     "Lua RMI addressed to ONE client channel (first argument). For a bot the channel is 0."),
    ("BROADCAST",   re.compile(r"\.(allClients|otherClients):(\w+)\s*\("),
     "Broadcast RMI to all/other clients: no per-actor channel, safe for bots."),
    ("GET_CHANNEL", re.compile(r"(\w+)(?:\.actor)?:GetChannel\s*\(\s*\)"),
     "Reads an actor's channel id (0 for bots)."),
    ("CHANNEL_API", re.compile(r"\bgame:(GetPlayerByChannelId|GetChannelTeam|GetTeamChannelCount|IsChannelInGame|GetChannelId|SendTextMessage|SendChatMessage|ForbiddenAreaWarning|SetSynchedGlobalValue|RenamePlayer|ChangePlayerClass|SpawnPlayer)\s*\("),
     "GameRules script binding that takes or returns a channel id."),
    ("CHANNEL_TABLE", re.compile(r"\b(\w*[cC]hannel\w*)\s*\[\s*([^\]]+)\]"),
     "Lua table keyed by channel id: all channel-0 bots would share ONE slot."),
    ("ISPLAYER",    re.compile(r"\.actor:IsPlayer\s*\(\s*\)"),
     "Actor:IsPlayer() is false for channel-0 bots: the branch silently skips them."),
    ("PLAYER_ENUM", re.compile(r"\bgame:(GetPlayers|GetPlayerCount|GetTeamPlayers|GetTeamPlayerCount|GetSpectatorCount)\s*\("),
     "Player enumeration: connection-based unless WarsBots participant APIs include bots."),
    ("ENTITY_KEY",  re.compile(r"\[\s*(\w+)\.id\s*\]|\[\s*playerId\s*\]|\[\s*(\w*Id)\s*\]"),
     "Table keyed by EntityId: works for bots as-is."),
]
LUA_KEYWORDS_OPEN = re.compile(r"\b(function|if|for|while|repeat|do)\b")
FUNC_DEF = re.compile(r"^\s*(?:local\s+)?function\s+([\w\.:]+)\s*\(|^\s*([\w\.]+)\s*=\s*function\s*\(", re.M)
CALL = re.compile(r"(?:(\w+(?:\.\w+)*)[:\.])?(\w+)\s*\(")

def strip_comments_and_strings(src):
    """Replace comments/strings with spaces (keep length and newlines) so regexes see code only."""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if src.startswith("--[[", i) or src.startswith("--[=[", i):
            close = "]]" if src.startswith("--[[", i) else "]=]"
            j = src.find(close, i); j = n if j < 0 else j + len(close)
            out.append(re.sub(r"[^\n]", " ", src[i:j])); i = j; continue
        if src.startswith("--", i):
            j = src.find("\n", i); j = n if j < 0 else j
            out.append(" " * (j - i)); i = j; continue
        if c in "\"'":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            j = min(j + 1, n)
            # keep string content visible for API names? no: blank it but keep quotes
            out.append(c + " " * max(0, j - i - 2) + (c if j - i >= 2 else "")); i = j; continue
        if src.startswith("[[", i):
            j = src.find("]]", i); j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", src[i:j])); i = j; continue
        out.append(c); i += 1
    return "".join(out)

def block_end(code, start):
    """Given index just after a function header, return index after its matching 'end'."""
    depth, i = 1, start
    tok = re.compile(r"\b(function|if|for|while|repeat|do|end|until)\b")
    pending_loop = 0  # 'for/while' are closed by their 'do'
    for m in tok.finditer(code, start):
        w = m.group(1)
        if w in ("function", "if", "repeat"):
            depth += 1
        elif w in ("for", "while"):
            depth += 1; pending_loop += 1
        elif w == "do":
            if pending_loop: pending_loop -= 1
            else: depth += 1
        elif w in ("end", "until"):
            depth -= 1
            if depth == 0:
                return m.end()
    return len(code)

class Function:
    def __init__(self, name, path, line, body, raw):
        self.name, self.path, self.line, self.body, self.raw = name, path, line, body, raw
        self.short = re.split(r"[\.:]", name)[-1]
        self.sinks, self.calls = [], set()

def resolve_effective(repo, pak_roots):
    """virtual path ('Scripts/...') -> (real path, source label), highest precedence first."""
    layers = [(os.path.join(repo, "Pak", "CryMP", "Scripts"), "CryMP"),
              (os.path.join(repo, "Pak", "Scripts"), "CryMP-pak")]
    layers += [(p, os.path.basename(os.path.dirname(os.path.abspath(p))) or p) for p in pak_roots]
    effective = {}
    for root, label in layers:
        for dirpath, _, files in os.walk(root):
            for f in files:
                if not f.lower().endswith(".lua"): continue
                real = os.path.join(dirpath, f)
                virt = "Scripts/" + os.path.relpath(real, root).replace(os.sep, "/")
                effective.setdefault(virt.lower(), (virt, real, label))
    return effective

def parse(effective):
    funcs = []
    for key, (virt, real, label) in sorted(effective.items()):
        try:
            raw = open(real, encoding="latin-1").read()
        except OSError:
            continue
        code = strip_comments_and_strings(raw)
        for m in FUNC_DEF.finditer(code):
            name = m.group(1) or m.group(2)
            start = code.find("(", m.start())
            close = code.find(")", start)
            end = block_end(code, close + 1)
            line = code.count("\n", 0, m.start()) + 1
            fn = Function(name, f"{virt} [{label}]", line, code[m.start():end], raw[m.start():end])
            for cat, rx, _ in RULES:
                for s in rx.finditer(fn.body):
                    sline = line + fn.body.count("\n", 0, s.start())
                    text = raw.splitlines()[sline - 1].strip() if sline - 1 < len(raw.splitlines()) else ""
                    fn.sinks.append((cat, sline, text[:140]))
            for c in CALL.finditer(fn.body):
                callee = c.group(2)
                if callee not in ("function", "if", "for", "while", "return", "and", "or", "not", "local"):
                    fn.calls.add(callee)
            funcs.append(fn)
    return funcs

def reachable(funcs, entries, max_depth=6):
    by_short = collections.defaultdict(list)
    for f in funcs: by_short[f.short].append(f)
    seen, frontier, parent = {}, [], {}
    for e in entries:
        for f in by_short.get(e, []):
            seen[id(f)] = (f, 0); frontier.append(f)
    while frontier:
        f = frontier.pop(0)
        d = seen[id(f)][1]
        if d >= max_depth: continue
        for callee in f.calls:
            for g in by_short.get(callee, []):
                if id(g) not in seen:
                    seen[id(g)] = (g, d + 1); parent[id(g)] = f; frontier.append(g)
    return seen, parent

ENTRIES_DEFAULT = {
    "join / team":       ["OnClientConnect", "OnClientEnteredGame", "SvRequestTeam", "RequestTeam", "SetTeam", "OnSetTeam", "AutoTeamBalance"],
    "spawn group":       ["RequestSpawnGroup", "SvRequestSpawnGroup", "GetPlayerSpawnGroup", "SetPlayerSpawnGroup"],
    "revive / waves":    ["RequestRevive", "QueueRevive", "UpdateReviveQueue", "RevivePlayerInQueue", "RevivePlayer", "CanRevive", "CommitRevivePurchases"],
    "death / score":     ["OnPlayerKilled", "ProcessScores", "ProcessDeath", "Award", "AwardPPCount", "AwardCPCount", "ProcessScoreXP"],
    "buying":            ["SvBuy", "BuyItem", "BuyVehicle", "BuyAmmo", "SvBuyAmmo", "EnterBuyZone", "LeaveBuyZone", "OnEnterBuyZone"],
    "capture":           ["UpdateCapture", "OnCapture", "Capture", "OnUncapture", "OnEnterArea", "OnLeaveArea", "MakeCapturable"],
    "factory / vehicle": ["Buy", "Queue", "OnVehicleBuilt", "BuildVehicle", "CancelProduction", "OnVehicleSpawned", "ClaimVehicle"],
    "win":               ["OnHQDestroyed", "OnGameEnd", "EndGame", "OnTimer", "CheckTimeLimit"],
}

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", required=True)
    ap.add_argument("--pak-scripts", action="append", default=[], help="extracted <pak>/Scripts, highest precedence first")
    ap.add_argument("--markdown")
    ap.add_argument("--depth", type=int, default=4)
    ap.add_argument("--scope", default=r"GameRules/(PowerStruggle|TeamInstantAction|InstantAction)|Entities/Multiplayer/|Entities/Actor/(Player|BasicActor)\.lua|Entities/[Vv]ehicles/VehicleBase",
                    help="regex of script paths that take part in the PS call graph (others are not loaded by PS)")
    ap.add_argument("--show-pak-code", action="store_true",
                    help="print code text of game-PAK lines too (LOCAL USE ONLY; never commit such output)")
    args = ap.parse_args()

    effective = resolve_effective(args.repo, args.pak_scripts)
    funcs = [f for f in parse(effective) if re.search(args.scope, f.path)]
    out = []
    w = out.append
    w("# Lua channel audit (generated by Tools/CryMPResearch/lua_channel_audit.py)\n")
    w(f"Effective Lua files: {len(effective)}; functions parsed: {len(funcs)}.\n")
    ps_files = [v for v in effective.values() if re.search(r"GameRules/|Multiplayer/", v[0])]
    w("## Effective sources for Power Struggle\n")
    for virt, real, label in sorted(ps_files):
        if re.search(r"PowerStruggle|TeamInstantAction|InstantAction|Factory|HQ|SpawnGroup|Bunker|AlienEnergy|Buy", virt):
            w(f"- `{virt}` ← **{label}**")
    w("")
    legend = {c: e for c, _, e in RULES}
    w("## Sink categories\n")
    for c, e in legend.items(): w(f"- **{c}** — {e}")
    w("")
    for group, entries in ENTRIES_DEFAULT.items():
        seen, parent = reachable(funcs, entries, args.depth)
        w(f"## {group}\n")
        w(f"Entry points: {', '.join(entries)}. Reachable functions (depth ≤ {args.depth}): {len(seen)}.\n")
        rows = []
        for f, d in sorted(seen.values(), key=lambda x: (x[1], x[0].path, x[0].line)):
            bad = [s for s in f.sinks if s[0] not in ("BROADCAST", "ENTITY_KEY")]
            if not bad: continue
            chain = [f.name]; p = parent.get(id(f))
            while p is not None and len(chain) < 6:
                chain.append(p.name); p = parent.get(id(p))
            for cat, line, text in sorted(set(bad)):
                if not args.show_pak_code and not re.search(r"\[CryMP(-pak)?\]$", f.path):
                    text = "(game PAK line: not reproduced, see local extract)"
                rows.append(f"| {cat} | `{f.name}` | {f.path}:{line} | `{text.replace('|','/')}` | {' ← '.join(chain[1:]) or '(entry)'} |")
        if rows:
            w("| Sink | Function | Location | Code | Reached from |\n|---|---|---|---|---|")
            out.extend(rows)
        else:
            w("No channel/IsPlayer sinks reached.")
        w("")
    text = "\n".join(out)
    if args.markdown:
        open(args.markdown, "w", encoding="utf-8").write(text)
    else:
        sys.stdout.write(text)

if __name__ == "__main__":
    main()
