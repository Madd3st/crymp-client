#!/usr/bin/env python3
"""Movement / stuck analyzer for WarsBots `.bots.log` files (read-only, stdlib only).

Line formats come from the current sources (cited so a format change is easy to track):
  BotBrain.cpp:213-237  telemetry block, emitted every 2 s per verbose bot, in this order:
                        AI telemetry (EntityId) -> TACTIC SAMPLE (actor) -> AI decision ->
                        Motion heading -> ABILITY PHYSICS -> MOTION INTENT. The last four lines carry
                        no actor id; they are attached to the open block of the same timestamp
                        (single-threaded emission inside one function call).
  BotBrain.cpp:44       "#N AI state=S."          BotBrain.cpp:103  "#N NAV fallback start=.. goal=.."
  BotBrain.cpp:300,349  "weapon goal cancelled"   BotBrain.cpp:342  "weapon I unreachable"
  BotBrain.cpp:532      "chase abandoned" (a ReportStuck call)
  BotBrain.cpp:566      "PROGRESS ESCAPE" (a ReportStuck call, experiment only)
  BotInstance/BotManager: "#N created EntityId", "combat kill", "death observed", "revive requested".

Evidence levels in the output:
  confirmed  the engine/bot logged the event itself (e.g. NAV fallback == FindPath returned false)
  measured   derived only from logged engine values of one sample (e.g. requested speed > 0,
             actual speed ~ 0, blocked = 0)
  inferred   derived across samples or joined by time (stalls, loops, no-approach, kill positions)

Not logged at all (cannot be recovered): the roam goal, the route actually followed, the
ReportStuck call of a blocked roam leg (BotBrain.cpp:418), per-bot jump lines, human positions.
"""
import argparse
import bisect
import collections
import hashlib
import json
import math
import re
import sys
from pathlib import Path

NUM = r"-?\d+(?:\.\d+)?"
TS = re.compile(r"^(\d+(?:\.\d+)?) ")
RX = {
    "created": re.compile(rf"#(\d+) created EntityId=(\d+) name=(\S+) profile=(\w+)/(\w+) position=\(({NUM}), ({NUM}), ({NUM})\)"),
    "telemetry": re.compile(rf"AI telemetry EntityId=(\d+) state=(\w+) pos=\(({NUM}), ({NUM}), ({NUM})\) health=(-?\d+) weapon=(\d+) ammo=(-?\d+) target=(\d+)"),
    "tactic": re.compile(rf"TACTIC SAMPLE actor=(\d+) .*?weapon=(\S+) suit=(\d+) distance=({NUM}) action=(\w+) target=(\d+) visible=(\d)"),
    "decision": re.compile(rf"AI decision target=(\d+) visible=(\d)"),
    "heading": re.compile(rf"Motion heading=\(({NUM}),({NUM})\) physicalSpeed=({NUM})"),
    "physics": re.compile(rf"ABILITY PHYSICS: speed=({NUM}) vz=({NUM}) grounded=(\d) sprint=(\d) mode=(\d+) energy=({NUM})"),
    "intent": re.compile(rf"MOTION INTENT: gait=(\d+) strafe=(\d) requestedBase=({NUM}) actual=({NUM}) lateral=({NUM}) facingDot=({NUM}) blocked=(\d)"),
    "state": re.compile(r"#(\d+) AI state=(\w+)"),
    "fallback": re.compile(rf"#(\d+) NAV fallback start=\(({NUM}),({NUM}),({NUM})\) goal=\(({NUM}),({NUM}),({NUM})\)"),
    "unreachable": re.compile(r"#(\d+) weapon (\d+) unreachable"),
    "cancelled": re.compile(r"#(\d+) weapon goal cancelled item=(\d+) reason=(\w+)"),
    "pickup": re.compile(r"REAL PICKUP actor=(\d+) item=(\d+) acquired=(\d)"),
    "chase_abandoned": re.compile(r"#(\d+) chase abandoned target=(\d+)"),
    "escape": re.compile(rf"PROGRESS ESCAPE actor=(\d+) kind=(\w+) goal=(\d+) pos=\(({NUM}),({NUM}),({NUM})\)"),
    "kill": re.compile(r"combat kill shooter=(\d+) target=(\d+)"),
    "death": re.compile(r"#(\d+) death observed EntityId=(\d+) health=(-?\d+) lastHitShooter=(\d+)"),
    "revive": re.compile(rf"#(\d+) revive requested EntityId=(\d+) spawnId=(\d+) previousSpawn=(\d+) position=\(({NUM}),({NUM}),({NUM})\)"),
    "done": re.compile(r"ENTITY_EVENT_DONE EntityId=(\d+)"),
    "nav_ready": re.compile(r"NAV ready: (\d+) nodes, (\d+) edges"),
    "nav_build": re.compile(rf"NAV build started: (\d+)x(\d+) columns, cell ({NUM}) m, z ({NUM})\.\.({NUM})"),
    "memory": re.compile(rf"MAP MEMORY: loaded (\S+) \(stuck ({NUM}), kills ({NUM}), deaths ({NUM})\)"),
    "build": re.compile(r"BotManager initialized \(build ([^)]+)\)"),
}

# Thresholds (documented in NAVIGATION_FINDINGS.md); telemetry period is 2 s.
WINDOW_S = 6.0          # same as the exporter's progress windows (wb_tactics_report.py)
GAP_S = 3.5             # a longer gap between samples breaks a track (dead/respawn/log gap)
STALL_PATH_M = 0.8
LOOP_PATH_M = 3.0
LOOP_NET_M = 1.5
APPROACH_GAIN_M = 1.0   # must get at least this much closer over the window
STILL_SPEED = 0.15      # m/s treated as standing still
JOIN_TARGET_S = 1.5     # max time offset when joining another bot's position


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def dist2d(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


class Analysis:
    def __init__(self, items=None):
        self.items = items or {}                 # mission EntityId -> {class, pos}
        self.segment = 0
        self.last_t = -1.0
        self.meta = dict(builds=[], nav=[], memory_loaded=[], lines=0, unparsed_blocks=0)
        self.slot_entity = {}                    # (segment, slot) -> entity
        self.profiles = {}                       # entity key -> {...}
        self.samples = collections.defaultdict(list)   # entity key -> samples
        self.events = []
        self.block = None
        self.counts = collections.Counter()

    def key(self, entity):
        return f"{self.segment}:{entity}"

    def entity_of_slot(self, slot):
        return self.slot_entity.get((self.segment, slot))

    def event(self, etype, t, entity, confidence, pos=None, **detail):
        self.counts[etype] += 1
        ev = dict(type=etype, t=t, segment=self.segment, bot=self.key(entity) if entity else None,
                  confidence=confidence)
        if pos is not None:
            ev["pos"] = [round(v, 2) for v in pos]
        ev.update(detail)
        self.events.append(ev)

    def last_pos(self, entity_key, t, max_age=2.5):
        s = self.samples.get(entity_key)
        if not s:
            return None, None
        i = bisect.bisect_right([x["t"] for x in s[-8:]], t) - 1
        if i < 0:
            return None, None
        x = s[-8:][i]
        return (x["pos"], round(t - x["t"], 2)) if t - x["t"] <= max_age else (None, None)

    def feed(self, line):
        self.meta["lines"] += 1
        m = TS.match(line)
        if not m:
            return
        t = float(m[1])
        if t + 1.0 < self.last_t:                # the engine log clock restarted: new session
            self.segment += 1
            self.block = None
        self.last_t = t
        if "[WarsBots" not in line:
            return
        for name in ("telemetry", "tactic", "decision", "heading", "physics", "intent"):
            mm = RX[name].search(line)
            if mm:
                getattr(self, "on_" + name)(t, mm)
                return
        if self.block and self.block["t"] != t:
            self.close_block(incomplete=True)
        for name in ("state", "fallback", "unreachable", "cancelled", "pickup", "chase_abandoned", "escape",
                     "kill", "death", "revive", "done", "created", "nav_ready", "nav_build", "memory", "build"):
            mm = RX[name].search(line)
            if mm:
                getattr(self, "on_" + name)(t, mm)
                return

    # ---- telemetry block
    def on_telemetry(self, t, m):
        if self.block:
            self.close_block(incomplete=True)
        e = int(m[1])
        self.block = dict(t=t, entity=e, state=m[2], pos=(float(m[3]), float(m[4]), float(m[5])),
                          health=int(m[6]), weapon=int(m[7]), ammo=int(m[8]), target=int(m[9]))

    def _attach(self, t, fields):
        if not self.block or self.block["t"] != t:
            self.meta["unparsed_blocks"] += 1
            return False
        self.block.update(fields)
        return True

    def on_tactic(self, t, m):
        if self.block and int(m[1]) != self.block["entity"]:
            self.close_block(incomplete=True)
            return
        self._attach(t, dict(weapon_class=m[2], suit=int(m[3]), target_distance=float(m[4]), action=m[5],
                             visible=int(m[7])))

    def on_decision(self, t, m):
        self._attach(t, dict(decision_visible=int(m[2])))

    def on_heading(self, t, m):
        self._attach(t, dict(heading=(float(m[1]), float(m[2])), physical_speed=float(m[3])))

    def on_physics(self, t, m):
        self._attach(t, dict(speed=float(m[1]), vz=float(m[2]), grounded=int(m[3]), sprint=int(m[4]),
                             mode=int(m[5]), energy=float(m[6])))

    def on_intent(self, t, m):
        if self._attach(t, dict(gait=int(m[1]), strafe=int(m[2]), requested=float(m[3]), actual=float(m[4]),
                                lateral=float(m[5]), facing=float(m[6]), blocked=int(m[7]))):
            self.close_block()

    def close_block(self, incomplete=False):
        b, self.block = self.block, None
        if not b:
            return
        if incomplete:
            self.meta["unparsed_blocks"] += 1
        k = self.key(b["entity"])
        b["complete"] = "requested" in b
        self.samples[k].append(b)
        if b["complete"] and b["health"] > 0 and b["requested"] > 0 and b["actual"] < STILL_SPEED:
            kind = "blocked_no_motion" if b["blocked"] else "unblocked_no_motion"
            self.counts["sample_" + kind] += 1

    # ---- discrete events
    def on_state(self, t, m):
        e = self.entity_of_slot(int(m[1]))
        if e:
            self.samples_state(e, t, m[2])

    def samples_state(self, e, t, state):
        self.profiles.setdefault(self.key(e), {}).setdefault("state_changes", []).append((t, state))

    def on_fallback(self, t, m):
        slot = int(m[1])
        e = self.entity_of_slot(slot)
        start = tuple(float(m[i]) for i in (2, 3, 4))
        goal = tuple(float(m[i]) for i in (5, 6, 7))
        self.event("astar_failed", t, e, "confirmed", start, goal=[round(v, 2) for v in goal], slot=slot,
                   straight_distance=round(dist(start, goal), 2))

    def on_unreachable(self, t, m):
        slot, item = int(m[1]), int(m[2])
        e = self.entity_of_slot(slot)
        info = self.items.get(item)
        pos, age = self.last_pos(self.key(e), t) if e else (None, None)
        self.event("weapon_unreachable", t, e, "confirmed", pos, item=item, slot=slot,
                   item_class=info and info["class"], item_pos=info and info["pos"], pos_age=age,
                   meaning="upgrade target not picked up within 10 s (BotBrain.cpp:340)")

    def on_cancelled(self, t, m):
        slot, item = int(m[1]), int(m[2])
        e = self.entity_of_slot(slot)
        info = self.items.get(item)
        self.event("weapon_goal_cancelled", t, e, "confirmed", None, item=item, slot=slot, reason=m[3],
                   item_class=info and info["class"], item_pos=info and info["pos"])

    def on_pickup(self, t, m):
        e, item = int(m[1]), int(m[2])
        info = self.items.get(item)
        self.event("pickup", t, e, "confirmed", info and info["pos"], item=item, acquired=int(m[3]),
                   item_class=info and info["class"], pos_source="mission item position" if info else None)

    def on_chase_abandoned(self, t, m):
        slot = int(m[1])
        e = self.entity_of_slot(slot)
        pos, age = self.last_pos(self.key(e), t) if e else (None, None)
        self.event("stuck_reported", t, e, "confirmed", pos, cause="chase_abandoned", slot=slot,
                   target=int(m[2]), pos_age=age, pos_source="last telemetry (inferred position)")

    def on_escape(self, t, m):
        e = int(m[1])
        self.event("stuck_reported", t, e, "confirmed", tuple(float(m[i]) for i in (4, 5, 6)),
                   cause="progress_escape_" + m[2], goal=int(m[3]), pos_source="logged")

    def on_kill(self, t, m):
        s, v = int(m[1]), int(m[2])
        pos, age = self.last_pos(self.key(s), t)
        vpos, vage = self.last_pos(self.key(v), t)
        self.event("kill", t, s, "confirmed", pos, victim=self.key(v), pos_age=age,
                   victim_pos=vpos and [round(c, 2) for c in vpos], victim_pos_age=vage,
                   pos_source="last telemetry of each bot (inferred, up to 2.5 s old)")

    def on_death(self, t, m):
        e = int(m[2])
        pos, age = self.last_pos(self.key(e), t)
        self.event("death", t, e, "confirmed", pos, health=int(m[3]), shooter=int(m[4]), pos_age=age,
                   pos_source="last telemetry (inferred, up to 2.5 s old)")

    def on_revive(self, t, m):
        e = int(m[2])
        self.event("revive", t, e, "confirmed", tuple(float(m[i]) for i in (5, 6, 7)), spawn=int(m[3]),
                   previous_spawn=int(m[4]))

    def on_done(self, t, m):
        self.event("entity_done", t, int(m[1]), "confirmed")

    def on_created(self, t, m):
        slot, e = int(m[1]), int(m[2])
        self.slot_entity[(self.segment, slot)] = e
        self.profiles[self.key(e)] = dict(entity=e, slot=slot, name=m[3], style=m[4], skill=m[5],
                                          segment=self.segment, created=t, state_changes=[])
        self.event("created", t, e, "confirmed", tuple(float(m[i]) for i in (6, 7, 8)), slot=slot)

    def on_nav_ready(self, t, m):
        self.meta["nav"].append(dict(t=t, segment=self.segment, nodes=int(m[1]), edges=int(m[2])))

    def on_nav_build(self, t, m):
        self.meta["nav"].append(dict(t=t, segment=self.segment, columns=[int(m[1]), int(m[2])],
                                     cell=float(m[3]), z=[float(m[4]), float(m[5])]))

    def on_memory(self, t, m):
        self.meta["memory_loaded"].append(dict(t=t, segment=self.segment, level=m[1], stuck=float(m[2]),
                                               kills=float(m[3]), deaths=float(m[4])))

    def on_build(self, t, m):
        self.meta["builds"].append(dict(t=t, segment=self.segment, build=m[1]))

    # ---- derived analysis
    def finish(self):
        if self.block:
            self.close_block(incomplete=True)
        index = {k: ([s["t"] for s in v], v) for k, v in self.samples.items()}

        def pos_at(key, t):
            ts, s = index.get(key, ([], []))
            i = bisect.bisect_left(ts, t)
            best = None
            for j in (i - 1, i):
                if 0 <= j < len(ts) and abs(ts[j] - t) <= JOIN_TARGET_S and s[j]["health"] > 0:
                    if best is None or abs(ts[j] - t) < abs(best["t"] - t):
                        best = s[j]
            return best["pos"] if best else None

        fallbacks = collections.defaultdict(list)
        for ev in self.events:
            if ev["type"] == "astar_failed" and ev["bot"]:
                fallbacks[ev["bot"]].append(ev)
        for key, s in self.samples.items():
            seg = key.split(":")[0]
            alive = [x for x in s if x["health"] > 0 and x.get("complete")]
            tracks, cur = [], []
            for x in alive:
                if cur and x["t"] - cur[-1]["t"] > GAP_S:
                    tracks.append(cur)
                    cur = []
                cur.append(x)
            if cur:
                tracks.append(cur)
            for tr in tracks:
                self.windows(key, seg, tr, pos_at, fallbacks.get(key, []))

    def windows(self, key, seg, tr, pos_at, fallbacks):
        i = 0
        while i < len(tr):
            j = i
            while j + 1 < len(tr) and tr[j + 1]["t"] - tr[i]["t"] <= WINDOW_S:
                j += 1
            w = tr[i:j + 1]
            if len(w) < 3 or w[-1]["t"] - w[0]["t"] < WINDOW_S - 2.1:
                i += 1
                continue
            self.counts["windows"] += 1
            wants = all(x["requested"] > 0 for x in w)
            path = sum(dist(a["pos"], b["pos"]) for a, b in zip(w, w[1:]))
            net = dist(w[0]["pos"], w[-1]["pos"])
            blocked = sum(x["blocked"] for x in w)
            states = sorted({x["state"] for x in w})
            common = dict(t_end=w[-1]["t"], duration=round(w[-1]["t"] - w[0]["t"], 2), samples=len(w),
                          path_m=round(path, 2), net_m=round(net, 2), blocked_samples=blocked, states=states)
            found = None
            if wants and path < STALL_PATH_M:
                found = ("stall_unblocked" if blocked == 0 else "stall_blocked", common)
            elif wants and path >= LOOP_PATH_M and net < LOOP_NET_M:
                found = ("loop_suspect", common)
            elif wants and all(x["state"] == "CHASING_ENEMY" for x in w) and len({x["target"] for x in w}) == 1:
                tgt = f"{seg}:{w[0]['target']}"
                a, b = pos_at(tgt, w[0]["t"]), pos_at(tgt, w[-1]["t"])
                if a and b:
                    d0, d1 = dist2d(w[0]["pos"], a), dist2d(w[-1]["pos"], b)
                    if d0 - d1 < APPROACH_GAIN_M and blocked == 0:
                        found = ("chase_no_approach", dict(common, target=tgt, distance_start=round(d0, 2),
                                                          distance_end=round(d1, 2)))
            if not found:
                # straight-line fallback after a failed A*: is the bot closing on the logged goal?
                fb = [f for f in fallbacks if w[0]["t"] - 1.0 <= f["t"] <= w[0]["t"] + 0.5]
                if wants and fb:
                    g = fb[-1]["goal"]
                    d0, d1 = dist(w[0]["pos"], g), dist(w[-1]["pos"], g)
                    if d0 - d1 < APPROACH_GAIN_M and d1 > 2.0:
                        found = ("fallback_no_approach", dict(common, goal=g, distance_start=round(d0, 2),
                                                             distance_end=round(d1, 2)))
            if found:
                kind, detail = found
                x = w[-1]
                self.event(kind, w[0]["t"], int(key.split(":")[1]), "inferred", x["pos"], **detail)
                self.events[-1]["segment"] = int(seg)
                i = j            # windows do not overlap once reported
            i += 1

    def result(self, source, sha):
        tracks = []
        for key, s in self.samples.items():
            prof = self.profiles.get(key, {})
            tracks.append(dict(bot=key, entity=int(key.split(":")[1]), segment=int(key.split(":")[0]),
                               slot=prof.get("slot"), style=prof.get("style"), skill=prof.get("skill"),
                               columns=["t", "x", "y", "z", "state", "health", "target", "requested", "actual",
                                        "blocked", "weapon_class"],
                               samples=[[x["t"], *[round(v, 2) for v in x["pos"]], x["state"], x["health"],
                                         x["target"], x.get("requested"), x.get("actual"), x.get("blocked"),
                                         x.get("weapon_class")] for x in s]))
        tracks.sort(key=lambda d: (d["segment"], d["slot"] or 0, d["entity"]))
        summary = dict(counts=dict(sorted(self.counts.items())),
                       bots=len(tracks), samples=sum(len(x["samples"]) for x in tracks))
        moving = [x for v in self.samples.values() for x in v
                  if x.get("complete") and x["health"] > 0 and x["requested"] > 0]
        if moving:
            summary["moving_samples"] = len(moving)
            summary["share_unblocked_no_motion"] = round(
                sum(1 for x in moving if not x["blocked"] and x["actual"] < STILL_SPEED) / len(moving), 4)
            summary["share_blocked"] = round(sum(x["blocked"] for x in moving) / len(moving), 4)
        fails = collections.Counter(tuple(round(c / 3) * 3 for c in e["goal"]) for e in self.events
                                    if e["type"] == "astar_failed")
        summary["top_failed_goals_3m"] = [dict(goal=list(g), count=c) for g, c in fails.most_common(15)]
        return dict(schema="steelmill-lab/botlog/1", source=source, sha256=sha, meta=self.meta,
                    thresholds=dict(window_s=WINDOW_S, gap_s=GAP_S, stall_path_m=STALL_PATH_M,
                                    loop_path_m=LOOP_PATH_M, loop_net_m=LOOP_NET_M,
                                    approach_gain_m=APPROACH_GAIN_M, still_speed=STILL_SPEED),
                    summary=summary, tracks=tracks, events=self.events)


def load_items(path):
    if not path:
        return {}
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    return {int(i["id"]): {"class": i["class"], "pos": i["pos"]}
            for i in data.get("items", []) if i.get("id") is not None}


def analyze(path, items=None):
    raw = Path(path).read_bytes()
    a = Analysis(items)
    for line in raw.decode("utf-8", errors="replace").splitlines():
        a.feed(line)
    a.finish()
    return a.result(str(path), hashlib.sha256(raw).hexdigest())


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logs", nargs="+", type=Path)
    ap.add_argument("--map", type=Path, help="map JSON from level_data.py (mission item ids/positions)")
    ap.add_argument("--out", type=Path, required=True, help="output directory")
    a = ap.parse_args(argv)
    items = load_items(a.map)
    a.out.mkdir(parents=True, exist_ok=True)
    for log in a.logs:
        r = analyze(log, items)
        name = log.name.replace(".bots.log", "").replace(".log", "")
        (a.out / f"{name}.movement.json").write_text(json.dumps(r, separators=(",", ":")), encoding="utf-8")
        c = r["summary"]["counts"]
        print(f"{log.name}: bots={r['summary']['bots']} samples={r['summary']['samples']} "
              f"A*fail={c.get('astar_failed', 0)} unreachable={c.get('weapon_unreachable', 0)} "
              f"stuck_reported={c.get('stuck_reported', 0)} stall_unblocked={c.get('stall_unblocked', 0)} "
              f"stall_blocked={c.get('stall_blocked', 0)} loop={c.get('loop_suspect', 0)} "
              f"chase_no_approach={c.get('chase_no_approach', 0)} fallback_no_approach={c.get('fallback_no_approach', 0)} "
              f"unblocked_no_motion_share={r['summary'].get('share_unblocked_no_motion')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
