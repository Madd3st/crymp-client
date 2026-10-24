#!/usr/bin/env python3
"""Independent validator for WarsBots combat episodes (schema 2 of Tools/wb_tactics_report.py).

Does not import or modify the exporter. Reads an episodes JSONL file and, optionally, the
source bots.log files, then reports errors, warnings and a readiness summary for a
contextual-bandit dataset.

    ml_validate.py episodes.jsonl [--runs runs.json] [--json report.json]

--runs is the same runs.json given to the exporter; with it the validator re-reads the logs to
check life boundaries (death / revive inside an episode), kill outcomes and actor identity.

Field roles (see ML_DATA_READINESS.md):
  pre-decision context  skill, style, target (id only), first_sampled_distance*, first_sample_visible*,
                        distance_source*   (* sampled up to ~2 s AFTER the episode started)
  aggregated over the episode (post-decision, NOT usable as context)
                        weapons, suit_modes, actions
  outcome / reward      outcome, damage_dealt_hp, damage_taken_hp, end_seconds
  bookkeeping           actor, bot, run, source_sha, schema, start_seconds, context_complete,
                        damage_kind, progress_experiment_states
"""
import argparse
import collections
import json
import re
import sys
from pathlib import Path

REQUIRED = {"actor": int, "bot": int, "skill": str, "style": str, "target": int, "start_seconds": (int, float),
            "end_seconds": (int, float), "outcome": str, "damage_dealt_hp": int, "damage_taken_hp": int,
            "weapons": list, "suit_modes": list, "actions": dict, "context_complete": bool, "schema": int,
            "run": str, "source_sha": str}
OPTIONAL_NULLABLE = ("first_sampled_distance", "first_sample_seconds", "first_sample_visible", "distance_source")
OUTCOMES = {"kill", "death", "dead", "target_changed", "lost_target", "no_progress", "entity_done", "log_end", "suicide"}
CENSORED = {"log_end", "entity_done"}
SKILLS = {"beginner", "normal", "veteran", "elite"}
POST_DECISION = ("weapons", "suit_modes", "actions")
MAX_HP_PER_EPISODE = 400   # far above 100 HP; anything larger is a join error


def lives_from_log(path):
    """death/revive/done times per actor and kill lines, from a bots.log."""
    deaths, revives, kills = collections.defaultdict(list), collections.defaultdict(list), []
    hits = collections.defaultdict(list)   # (shooter, target) -> times of accepted hits
    killers = {}                           # (victim, time) -> last hit shooter
    t = 0.0
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.match(r"([\d.]+) ", line)
            if m:
                t = float(m[1])
            m = re.search(r"death observed EntityId=(\d+) health=-?\d+ lastHitShooter=(\d+)", line)
            if m:
                deaths[int(m[1])].append(t)
                killers[(int(m[1]), t)] = int(m[2])
            m = re.search(r"revive result EntityId=(\d+).*alive=1", line)
            if m:
                revives[int(m[1])].append(t)
            m = re.search(r"combat kill shooter=(\d+) target=(\d+)", line)
            if m:
                kills.append((t, int(m[1]), int(m[2])))
            m = re.search(r"Hit after-Lua target=(\d+) shooter=(\d+)", line)
            if m:
                hits[(int(m[2]), int(m[1]))].append(t)
    return deaths, revives, kills, hits, killers


def validate(rows, logs=None):
    issues = collections.Counter()
    examples = collections.defaultdict(list)

    rowflags = collections.defaultdict(set)

    def flag(code, i, detail=""):
        issues[code] += 1
        rowflags[i].add(code)
        if len(examples[code]) < 5:
            examples[code].append(dict(row=i, detail=detail))

    seen = set()
    by_actor = collections.defaultdict(list)
    for i, r in enumerate(rows):
        for k, typ in REQUIRED.items():
            if k not in r:
                flag("E_missing_" + k, i)
            elif not isinstance(r[k], typ) or (typ is int and isinstance(r[k], bool)):
                flag("E_type_" + k, i, repr(r[k])[:40])
        for k in OPTIONAL_NULLABLE:
            if k not in r:
                flag("E_missing_" + k, i)
        if any(c.startswith("E_missing") or c.startswith("E_type") for c in issues if examples[c] and examples[c][-1]["row"] == i):
            continue
        if r["schema"] != 2:
            flag("E_schema", i, r["schema"])
        if r["outcome"] not in OUTCOMES:
            flag("E_outcome_unknown", i, r["outcome"])
        if r["end_seconds"] < r["start_seconds"]:
            flag("E_end_before_start", i)
        if r["actor"] == r["target"] or r["target"] == 0:
            flag("E_bad_target", i)
        if r["skill"] not in SKILLS:
            flag("E_skill_unknown", i, r["skill"])
        for k in ("damage_dealt_hp", "damage_taken_hp"):
            if r[k] < 0 or r[k] > MAX_HP_PER_EPISODE:
                flag("E_damage_range", i, f"{k}={r[k]}")
        fs = r.get("first_sample_seconds")
        if fs is not None and not (r["start_seconds"] - 1e-6 <= fs <= r["end_seconds"] + 1e-6):
            flag("E_first_sample_outside_episode", i)
        complete = bool(r["weapons"] and r["suit_modes"] and r.get("first_sampled_distance") is not None)
        if complete != r["context_complete"]:
            flag("E_context_complete_inconsistent", i)
        key = (r["run"], r["actor"], r["target"], r["start_seconds"], r["end_seconds"])
        if key in seen:
            flag("E_duplicate", i)
        seen.add(key)
        by_actor[(r["run"], r["actor"])].append((r["start_seconds"], r["end_seconds"], i))
        # warnings: quality / leakage
        if fs is None:
            flag("W_no_context_sample", i)
        elif fs - r["start_seconds"] > 1.0:
            flag("W_context_sampled_late_gt1s", i, round(fs - r["start_seconds"], 2))
        if r.get("distance_source") == "last_seen_target":
            flag("W_distance_from_last_seen", i)
        if len(r["weapons"]) > 1:
            flag("W_weapon_changed_during_episode", i, r["weapons"])
        if len(r["suit_modes"]) > 1:
            flag("W_suit_changed_during_episode", i, r["suit_modes"])
        if r["outcome"] in CENSORED:
            flag("W_censored_outcome", i, r["outcome"])
        if not re.fullmatch(r"[0-9a-f]{7,40}", r["source_sha"]):
            flag("W_source_sha_not_a_commit", i, r["source_sha"])
        if r["end_seconds"] - r["start_seconds"] < 0.2:
            flag("W_episode_shorter_than_0.2s", i)
    for key, spans in by_actor.items():
        spans.sort()
        for (s0, e0, i0), (s1, e1, i1) in zip(spans, spans[1:]):
            if s1 < e0 - 1e-6:
                flag("E_overlapping_episodes_same_actor", i1, f"rows {i0},{i1}")
    if logs:
        for run, path in logs.items():
            deaths, revives, kills, hits, killers = lives_from_log(path)
            kill_set = collections.defaultdict(list)
            for t, s, v in kills:
                kill_set[(s, v)].append(t)
            for i, r in enumerate(rows):
                if r.get("run") != run:
                    continue
                inner = [t for t in deaths.get(r["actor"], []) + revives.get(r["actor"], [])
                         if r["start_seconds"] + 1e-6 < t < r["end_seconds"] - 1e-6]
                if inner:
                    flag("E_crosses_life_boundary", i, inner[:2])
                if r["outcome"] == "kill" and not any(r["start_seconds"] - 1e-6 <= t <= r["end_seconds"] + 2.5
                                                     for t in kill_set.get((r["actor"], r["target"]), [])):
                    flag("E_kill_outcome_without_kill_line", i)
                fs = r.get("first_sample_seconds")
                if fs is not None:
                    pair_hits = hits.get((r["actor"], r["target"]), []) + hits.get((r["target"], r["actor"]), [])
                    if any(r["start_seconds"] - 1e-6 <= t < fs for t in pair_hits):
                        flag("W_context_sampled_after_first_hit", i)
                if r["outcome"] == "death":
                    match = [t for t in deaths.get(r["actor"], []) if abs(t - r["end_seconds"]) < 0.05]
                    if not match:
                        flag("E_death_outcome_without_death_line", i)
                    elif killers.get((r["actor"], match[0])) != r["target"]:
                        flag("W_death_caused_by_other_than_target", i, killers.get((r["actor"], match[0])))
    return issues, examples, rowflags


CONTEXT_EXCLUDE = {"W_no_context_sample", "W_context_sampled_late_gt1s", "W_context_sampled_after_first_hit",
                   "W_censored_outcome", "W_episode_shorter_than_0.2s"}


def readiness(rows, rowflags):
    usable = [r for i, r in enumerate(rows) if r.get("context_complete") and not (rowflags.get(i, set()) & CONTEXT_EXCLUDE)
              and not any(c.startswith("E") for c in rowflags.get(i, set()))]
    pairs = collections.Counter((r["actor"], r["target"], r["start_seconds"]) for r in rows)
    mirrored = sum(1 for r in rows if (r["target"], r["actor"], r["start_seconds"]) in pairs)
    return dict(rows=len(rows), usable_for_offline_analysis=len(usable),
                outcomes=dict(collections.Counter(r["outcome"] for r in rows)),
                by_skill=dict(collections.Counter(r["skill"] for r in rows)),
                by_style=dict(collections.Counter(r["style"] for r in rows)),
                runs=len({r["run"] for r in rows}), source_shas=dict(collections.Counter(r["source_sha"] for r in rows)),
                mirrored_rows_same_start=mirrored,
                bandit_blockers=[
                    "no explicit action/arm field: `actions` is a count of FSM states over the whole episode",
                    "no action propensity / exploration probability: off-policy evaluation impossible",
                    "weapons, suit_modes, actions aggregate post-decision behaviour (leak if used as context)",
                    "context distance is sampled up to ~2 s after the episode start (telemetry period)",
                    "no life/episode id: lives must be reconstructed from the log",
                    "no profile parameter values (only skill/style names); no immutable policy id",
                    "mirrored bot-vs-bot episodes are not independent; split by run, never by row",
                ])


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("episodes", type=Path)
    ap.add_argument("--runs", type=Path, help="runs.json used for the export (enables log cross-checks)")
    ap.add_argument("--json", type=Path)
    a = ap.parse_args(argv)
    rows, bad = [], 0
    with open(a.episodes, encoding="utf-8") as f:
        for n, line in enumerate(f):
            if not line.strip():
                continue
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError:
                bad += 1
    logs = None
    if a.runs:
        runs = json.loads(a.runs.read_text(encoding="utf-8-sig"))
        runs = [runs] if isinstance(runs, dict) else runs
        logs = {r.get("session", a.runs.parent.name) + ":" + r["tag"]: a.runs.parent / r["log"] for r in runs}
    issues, examples, rowflags = validate(rows, logs)
    if bad:
        issues["E_unparseable_line"] += bad
    rep = dict(schema="steelmill-lab/ml-validate/1", episodes=str(a.episodes), errors={k: v for k, v in issues.items() if k.startswith("E")},
               warnings={k: v for k, v in issues.items() if k.startswith("W")}, examples=examples,
               readiness=readiness(rows, rowflags), log_checks=bool(logs))
    print(f"{a.episodes.name}: {len(rows)} rows; errors {sum(rep['errors'].values())}, warnings {sum(rep['warnings'].values())}")
    for k, v in sorted(issues.items()):
        print(f"  {k}: {v}")
    print(f"  usable (complete, uncensored, context <=1 s and before any hit, no errors): {rep['readiness']['usable_for_offline_analysis']}")
    if a.json:
        a.json.write_text(json.dumps(rep, indent=1) + "\n", encoding="utf-8")
    return 1 if rep["errors"] else 0


if __name__ == "__main__":
    sys.exit(main())
