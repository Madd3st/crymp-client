"""Self-contained HTML map viewer for one or more analysed runs (no external data, no server).

Writes page content without <html>/<body> wrappers so the same file opens in a
browser locally and can be published as a claude.ai artifact unchanged.
"""
from __future__ import annotations

import json
import random

from . import analysis

MAX_TRACK_POINTS = 24000


def build_payload(label, run, summary, stall_rows, pins, grid, graph=None, memory=None):
    tele = [(t, d) for k, t, d in run.events if k == "telemetry" and d["health"] > 0]
    rnd = random.Random(7)
    if len(tele) > MAX_TRACK_POINTS:
        tele = rnd.sample(tele, MAX_TRACK_POINTS)
    track = [[round(d["pos"][0], 1), round(d["pos"][1], 1), round(d["pos"][2], 1), d["state"][:2]] for t, d in tele]
    # First DEAD telemetry sample after a live one = approximate death position.
    deaths, dead = [], set()
    for k, t, d in run.events:
        if k != "telemetry":
            continue
        if d["health"] <= 0 and d["entity"] not in dead:
            dead.add(d["entity"]); deaths.append([round(v, 1) for v in d["pos"]] + [round(t)])
        elif d["health"] > 0:
            dead.discard(d["entity"])
    revives = {}
    for k, t, d in run.events:
        if k == "revive":
            p = tuple(round(v, 2) for v in d["pos"])
            revives.setdefault(p, [0, d["spawn"]])[0] += 1
    fb = analysis.fallback_profile(run)
    names = {b.entity: f"#{b.slot} {b.style}/{b.skill}" for b in run.bots.values()}
    payload = dict(
        label=label, summary=summary, grid=grid,
        track=track,
        stalls=[[*[round(v, 2) for v in s["pos"]], s["seconds"], s["state"], names.get(s["entity"], s["entity"]), round(s["start"])]
                for s in stall_rows],
        pins=[[*[round(v, 2) for v in p["pos"]], p["seconds"], p["state"], names.get(p["entity"], p["entity"]), round(p["start"]),
               p.get("blocked_when_requested_percent")] for p in pins],
        traps=[[*[round(v, 2) for v in p["pos"]], p["seconds"], p["state"], names.get(p["entity"], p["entity"]), round(p["start"]),
                p.get("blocked_when_requested_percent")] for p in analysis.traps(run)],
        deaths=deaths[:6000],
        spawns=[[*p, n, sid] for p, (n, sid) in revives.items()],
        fb_goals=[[*g["pos"], g["queries"]] for g in fb["top_goals"]],
        fb_starts=[[*g["pos"], g["queries"]] for g in fb["top_starts"]],
        graph=None,
    )
    if graph is not None:
        nodes = [[round(x, 2), round(y, 2), round(z, 2)] for x, y, z in graph.nodes]
        edges = []
        for i in range(len(graph.nodes)):
            for e in graph.out(i):
                j = graph.to[e]
                if i < j: edges.append([i, j])
        stuck = memory.stuck if memory and memory.count == len(nodes) else None
        payload["graph"] = dict(nodes=nodes, edges=edges, stuck=stuck)
    return payload


def write_html(path, payloads, title="Steelmill Triage"):
    data = json.dumps(payloads, separators=(",", ":"))
    html = TEMPLATE.replace("__TITLE__", title).replace("__DATA__", data.replace("</", "<\\/"))
    with open(path, "w", encoding="utf-8") as f:
        f.write(html)
    return path


TEMPLATE = r"""<title>__TITLE__</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@500;600;700&family=IBM+Plex+Sans:wght@400;500;600&family=IBM+Plex+Mono:wght@400;500&display=swap">
<style>
/* Layout: control-room console. Map canvas left, findings rail right; stacks on phones. */
:root {
  --bg: #eef0f1; --panel: #ffffff; --ink: #1d2328; --muted: #5d6870; --line: #d3d8dc;
  --heat: #d2541c; --trap: #b78900; --pin: #c0262d; --nav: #2b6f9e; --ok: #2f7d4f;
  --mapbg: #e3e7ea;
  --display: "Barlow Condensed", "Arial Narrow", sans-serif;
  --body: "IBM Plex Sans", "Segoe UI", system-ui, sans-serif;
  --mono: "IBM Plex Mono", ui-monospace, Consolas, monospace;
}
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --bg: #14181b; --panel: #1c2226; --ink: #e5e9ec; --muted: #94a0a8; --line: #2e373d;
  --heat: #ff7a3d; --trap: #e6b422; --pin: #ff5a5f; --nav: #5fa9dd; --ok: #5cc185; --mapbg: #101417; color-scheme: dark } }
:root[data-theme="dark"] {
  --bg: #14181b; --panel: #1c2226; --ink: #e5e9ec; --muted: #94a0a8; --line: #2e373d;
  --heat: #ff7a3d; --trap: #e6b422; --pin: #ff5a5f; --nav: #5fa9dd; --ok: #5cc185; --mapbg: #101417; color-scheme: dark }
body { background: var(--bg); color: var(--ink); font: 14px/1.5 var(--body);
  padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); padding-block: 20px 40px; }
h1 { font: 700 30px/1 var(--display); letter-spacing: .02em; text-transform: uppercase; margin: 0; text-wrap: balance }
h2 { font: 600 17px/1.2 var(--display); letter-spacing: .06em; text-transform: uppercase; margin: 0 0 8px; color: var(--muted) }
.top { display: flex; flex-wrap: wrap; gap: 12px 24px; align-items: baseline; margin-bottom: 14px }
.sub { color: var(--muted); font-family: var(--mono); font-size: 12px; min-width: 0 }
.runpick { display: flex; gap: 6px; align-items: center; max-width: 100% }
.runpick select { min-width: 0; flex: 1 }
select, input { max-width: 100%; font: 13px var(--mono); color: var(--ink); background: var(--panel); border: 1px solid var(--line); border-radius: 4px; padding: 4px 6px }
.wrap { display: grid; grid-template-columns: minmax(0, 1fr) 360px; gap: 16px; align-items: start }
@media (max-width: 900px) { .wrap { grid-template-columns: minmax(0,1fr) } }
.mapbox { background: var(--panel); border: 1px solid var(--line); border-radius: 6px; padding: 10px; min-width: 0 }
canvas { width: 100%; display: block; background: var(--mapbg); border-radius: 3px; cursor: crosshair }
.layers { display: flex; flex-wrap: wrap; gap: 6px 14px; margin: 0 0 8px; font-size: 13px }
.layers label { display: inline-flex; gap: 6px; align-items: center; cursor: pointer }
.sw { width: 10px; height: 10px; border-radius: 50%; display: inline-block }
.zrow { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; margin-top: 8px; font-size: 13px; color: var(--muted) }
.zrow input[type=number] { width: 70px }
.rail { display: grid; gap: 12px; min-width: 0 }
.card { background: var(--panel); border: 1px solid var(--line); border-radius: 6px; padding: 12px 14px; min-width: 0 }
.kv { display: grid; grid-template-columns: 1fr auto; gap: 3px 12px; font-size: 13px }
.kv b { font: 500 13px var(--mono); font-variant-numeric: tabular-nums; text-align: right }
.flag { display: grid; grid-template-columns: 4px 1fr; gap: 10px; margin: 8px 0 }
.flag i { border-radius: 2px }
.flag p { margin: 0; font-size: 13px }
table { border-collapse: collapse; width: 100%; font-size: 12px; font-variant-numeric: tabular-nums }
td, th { padding: 3px 6px; border-bottom: 1px solid var(--line); text-align: left; font-family: var(--mono) }
th { font: 600 11px var(--body); letter-spacing: .06em; text-transform: uppercase; color: var(--muted) }
tbody tr { cursor: pointer } tbody tr:hover { background: var(--mapbg) }
.scroll { overflow-x: auto; max-height: 260px; overflow-y: auto }
#tip { position: fixed; pointer-events: none; background: var(--panel); border: 1px solid var(--line); border-radius: 4px;
  padding: 6px 8px; font: 12px var(--mono); box-shadow: 0 4px 14px rgba(0,0,0,.18); max-width: 320px }
.note { font-size: 12px; color: var(--muted); margin-top: 8px }
:focus-visible { outline: 2px solid var(--nav); outline-offset: 2px }
</style>
<div class="top">
  <h1>Steelmill Triage</h1>
  <span class="sub" id="meta"></span>
  <label class="sub runpick">run <select id="run"></select></label>
</div>
<div class="wrap">
  <div class="mapbox">
    <div class="layers" id="layers"></div>
    <canvas id="map" width="1200" height="820" aria-label="Top-down Steelmill map of bot positions and navigation problems"></canvas>
    <div class="zrow">
      <span>Floor filter, z</span><input type="number" id="zmin" step="1"> – <input type="number" id="zmax" step="1">
      <span id="zinfo"></span>
    </div>
    <p class="note">World X right, Y up, 3 m BotNav grid. Small dots are sampled live bot positions (every ~2 s), blue low to orange high.
      Graph layer appears only when a warsbots/botnav-graph/1 export is loaded; the .mem file alone has no coordinates.</p>
  </div>
  <div class="rail">
    <div class="card"><h2>Summary</h2><div class="kv" id="kv"></div></div>
    <div class="card"><h2>Attention</h2><div id="flags"></div></div>
    <div class="card"><h2>Worst traps</h2><div class="scroll"><table><thead><tr><th>s</th><th>bot</th><th>state</th><th title="Share of samples where the bot asked to move and BotMovement refused">blocked</th><th>x, y, z</th></tr></thead><tbody id="traps"></tbody></table></div></div>
  </div>
</div>
<div id="tip" hidden></div>
<script>
const RUNS = __DATA__;
const css = n => getComputedStyle(document.documentElement).getPropertyValue(n).trim();
const LAYERS = [
  ["track", "Bot positions", "--muted", true], ["deaths", "Deaths", "--heat", false],
  ["spawns", "Revive points", "--ok", true], ["stalls", "Stalls ≥4 s", "--trap", true],
  ["pins", "Pinned lives", "--pin", true], ["fb", "NAV fallback hotspots", "--nav", true],
  ["graph", "BotNav graph", "--nav", false]];
const on = {}; let R = RUNS[0], view, hits = [];
const cv = document.getElementById("map"), cx = cv.getContext("2d");
const sel = document.getElementById("run");
RUNS.forEach((r, i) => sel.add(new Option(r.label, i)));
sel.onchange = () => { R = RUNS[+sel.value]; setup(); };
const lay = document.getElementById("layers");
LAYERS.forEach(([k, name, col, def]) => {
  on[k] = def; const l = document.createElement("label");
  l.innerHTML = `<input type="checkbox" id="ly-${k}" ${def ? "checked" : ""}><span class="sw" style="background:var(${col})"></span>${name}`;
  l.querySelector("input").onchange = e => { on[k] = e.target.checked; draw(); };
  lay.appendChild(l);
});
["zmin", "zmax"].forEach(id => document.getElementById(id).oninput = draw);
function bounds() {
  const g = R.grid || {}, pts = R.track;
  let x0 = g.min_x, y0 = g.min_y, x1 = g.min_x + g.nx * g.cell, y1 = g.min_y + g.ny * g.cell;
  if (!(x1 > x0)) { x0 = Math.min(...pts.map(p => p[0])); x1 = Math.max(...pts.map(p => p[0])); y0 = Math.min(...pts.map(p => p[1])); y1 = Math.max(...pts.map(p => p[1])); }
  const zs = pts.map(p => p[2]);
  return { x0, y0, x1, y1, z0: Math.floor(Math.min(...zs)), z1: Math.ceil(Math.max(...zs)) };
}
function setup() {
  view = bounds();
  const w = cv.clientWidth || 900; cv.width = w * devicePixelRatio;
  cv.height = cv.width * (view.y1 - view.y0) / (view.x1 - view.x0);
  document.getElementById("zmin").value = view.z0; document.getElementById("zmax").value = view.z1;
  const s = R.summary;
  document.getElementById("meta").textContent = `${s.build || "unknown build"} · ${s.bots} bots · ${Math.round(s.seconds / 60)} min · ${s.profiles.join(", ")}`;
  const kv = [["Live bot-minutes", s.live_bot_minutes], ["Telemetry period", (s.telemetry_period_s || 0).toFixed(2) + " s"],
    ["NAV graph", s.nav ? `${s.nav.nodes} / ${s.nav.edges}` : "—"],
    ["Pinned (never moved)", `${s.pinned.bot_minutes} min · ${s.pinned.live_percent}%`],
    ["Traps (stall ≥60 s)", `${s.traps.count} · ${s.traps.live_percent}%`],
    ["Stalls ≥4 s", `${s.stalls.count} · ${s.stalls.per_bot_minute}/bot-min`],
    ["NAV fallbacks", `${s.nav_fallback.counts.total || 0} · ${s.nav_fallback.per_bot_minute}/bot-min`],
    ["Fallback, goal on other floor", s.nav_fallback.counts["dz_over_2.5"] || 0],
    ["Fallback, under 7.5 m", (s.nav_fallback.counts["under_3m"] || 0) + (s.nav_fallback.counts["3_to_7.5m"] || 0)],
    ["Weapon unreachable", s.events.unreachable || 0], ["Kills / deaths", `${s.events.kill || 0} / ${s.events.death || 0}`]];
  document.getElementById("kv").innerHTML = kv.map(([k, v]) => `<span>${k}</span><b>${v}</b>`).join("");
  const flags = [];
  if (s.pinned.live_percent >= 1) flags.push(["--pin", `${s.pinned.live_percent}% of live bot time spent pinned to one spot, mostly at ${fmt(s.pinned.clusters[0]?.pos)}.`]);
  if (s.traps.live_percent >= 1) flags.push(["--trap", `${s.traps.count} traps longer than 60 s cost ${s.traps.bot_minutes} bot-minutes.`]);
  const fbt = s.nav_fallback.counts.total || 0;
  if (fbt) flags.push(["--nav", `${Math.round(100 * (s.nav_fallback.counts["dz_over_2.5"] || 0) / fbt)}% of failed routes target another floor.`]);
  (s.dataset.issues || []).forEach(t => flags.push(["--muted", "Dataset: " + t]));
  document.getElementById("flags").innerHTML = flags.map(([c, t]) => `<div class="flag"><i style="background:var(${c})"></i><p>${t}</p></div>`).join("") || "<p>No flags.</p>";
  const rows = [...R.pins.map(p => [...p, "pin"]), ...R.traps.map(p => [...p, "trap"])].sort((a, b) => b[3] - a[3]).slice(0, 40);
  const tb = document.getElementById("traps"); tb.innerHTML = "";
  rows.forEach(r => { const tr = document.createElement("tr");
    tr.innerHTML = `<td>${Math.round(r[3])}</td><td>${r[5]}</td><td>${r[8] === "pin" ? "pinned, " : ""}${r[4].replace(/_/g, " ").toLowerCase()}</td><td>${r[7] == null ? "—" : r[7] + "%"}</td><td>${fmt(r)}</td>`;
    tr.onclick = () => { document.getElementById("zmin").value = Math.floor(r[2] - 2); document.getElementById("zmax").value = Math.ceil(r[2] + 2); draw(r); };
    tb.appendChild(tr); });
  draw();
}
const fmt = p => p ? `${(+p[0]).toFixed(1)}, ${(+p[1]).toFixed(1)}, ${(+p[2]).toFixed(1)}` : "—";
function P(x, y) { return [(x - view.x0) / (view.x1 - view.x0) * cv.width, (1 - (y - view.y0) / (view.y1 - view.y0)) * cv.height]; }
function zRamp(z) { const t = Math.max(0, Math.min(1, (z - view.z0) / Math.max(1, view.z1 - view.z0))); return `hsl(${210 - 190 * t} 55% ${48 + 6 * t}%)`; }
function draw(focus) {
  const zl = +document.getElementById("zmin").value, zh = +document.getElementById("zmax").value;
  const inZ = z => z >= zl && z <= zh; const d = devicePixelRatio; hits = [];
  cx.clearRect(0, 0, cv.width, cv.height);
  if (R.grid && R.grid.cell) { cx.strokeStyle = css("--line"); cx.lineWidth = 1;
    for (let x = R.grid.min_x; x <= view.x1; x += R.grid.cell * 10) { const [a] = P(x, 0); cx.beginPath(); cx.moveTo(a, 0); cx.lineTo(a, cv.height); cx.stroke(); }
    for (let y = R.grid.min_y; y <= view.y1; y += R.grid.cell * 10) { const [, b] = P(0, y); cx.beginPath(); cx.moveTo(0, b); cx.lineTo(cv.width, b); cx.stroke(); } }
  if (on.graph && R.graph) { const g = R.graph; cx.strokeStyle = css("--nav"); cx.globalAlpha = .35; cx.lineWidth = d;
    cx.beginPath(); g.edges.forEach(([i, j]) => { const a = g.nodes[i], b = g.nodes[j]; if (!inZ(a[2]) && !inZ(b[2])) return;
      const [x1, y1] = P(a[0], a[1]), [x2, y2] = P(b[0], b[1]); cx.moveTo(x1, y1); cx.lineTo(x2, y2); }); cx.stroke(); cx.globalAlpha = 1;
    if (g.stuck) g.nodes.forEach((n, i) => { if (g.stuck[i] > 0 && inZ(n[2])) { const [x, y] = P(n[0], n[1]); cx.fillStyle = css("--pin");
      cx.beginPath(); cx.arc(x, y, (2 + Math.sqrt(g.stuck[i]) * 1.5) * d, 0, 7); cx.fill(); hits.push([x, y, `node ${i} stuck ${g.stuck[i]}`]); } }); }
  if (on.track) { R.track.forEach(p => { if (!inZ(p[2])) return; const [x, y] = P(p[0], p[1]); cx.fillStyle = zRamp(p[2]); cx.globalAlpha = .5; cx.fillRect(x - d, y - d, 2 * d, 2 * d); }); cx.globalAlpha = 1; }
  if (on.deaths) { cx.fillStyle = css("--heat"); cx.globalAlpha = .55; R.deaths.forEach(p => { if (!inZ(p[2])) return; const [x, y] = P(p[0], p[1]); cx.beginPath(); cx.arc(x, y, 2.2 * d, 0, 7); cx.fill(); }); cx.globalAlpha = 1; }
  if (on.fb) { cx.lineWidth = 1.5 * d;
    R.fb_goals.forEach(p => { if (!inZ(p[2])) return; const [x, y] = P(p[0], p[1]); cx.strokeStyle = css("--nav"); cx.beginPath(); cx.arc(x, y, (3 + Math.sqrt(p[3]) * .6) * d, 0, 7); cx.stroke(); hits.push([x, y, `fallback goal ${fmt(p)} · ${p[3]} failed queries`]); });
    R.fb_starts.forEach(p => { if (!inZ(p[2])) return; const [x, y] = P(p[0], p[1]); cx.strokeStyle = css("--nav"); cx.setLineDash([3 * d, 3 * d]); cx.beginPath(); cx.arc(x, y, (3 + Math.sqrt(p[3]) * .6) * d, 0, 7); cx.stroke(); cx.setLineDash([]); hits.push([x, y, `fallback start ${fmt(p)} · ${p[3]} failed queries`]); }); }
  if (on.spawns) R.spawns.forEach(p => { if (!inZ(p[2])) return; const [x, y] = P(p[0], p[1]); cx.fillStyle = css("--ok"); cx.fillRect(x - 3 * d, y - 3 * d, 6 * d, 6 * d); hits.push([x, y, `revive point spawnId=${p[4]} · ${p[3]} revives · ${fmt(p)}`]); });
  if (on.stalls) R.stalls.forEach(p => { if (!inZ(p[2])) return; const [x, y] = P(p[0], p[1]); cx.strokeStyle = css("--trap"); cx.lineWidth = 2 * d; cx.beginPath(); cx.arc(x, y, (2.5 + Math.sqrt(p[3]) * .45) * d, 0, 7); cx.stroke(); hits.push([x, y, `stall ${p[3]} s · ${p[5]} · ${p[4]} · t=${p[6]} · ${fmt(p)}`]); });
  if (on.pins) R.pins.forEach(p => { if (!inZ(p[2])) return; const [x, y] = P(p[0], p[1]); const r = (5 + Math.sqrt(p[3]) * .5) * d; cx.strokeStyle = css("--pin"); cx.lineWidth = 2.5 * d;
    cx.beginPath(); cx.moveTo(x - r, y - r); cx.lineTo(x + r, y + r); cx.moveTo(x + r, y - r); cx.lineTo(x - r, y + r); cx.stroke(); hits.push([x, y, `PINNED ${p[3]} s · ${p[5]} · ${p[4]} · t=${p[6]} · ${fmt(p)}`]); });
  if (focus) { const [x, y] = P(focus[0], focus[1]); cx.strokeStyle = css("--ink"); cx.lineWidth = 1.5 * d; cx.beginPath(); cx.arc(x, y, 18 * d, 0, 7); cx.stroke(); }
  document.getElementById("zinfo").textContent = `samples in range: ${R.track.filter(p => inZ(p[2])).length}`;
}
const tip = document.getElementById("tip");
cv.onmousemove = e => { const b = cv.getBoundingClientRect(), k = cv.width / b.width, mx = (e.clientX - b.left) * k, my = (e.clientY - b.top) * k;
  let best = null, bd = 14 * k * k * 14; hits.forEach(h => { const dd = (h[0] - mx) ** 2 + (h[1] - my) ** 2; if (dd < bd) { bd = dd; best = h; } });
  const wx = view.x0 + mx / cv.width * (view.x1 - view.x0), wy = view.y0 + (1 - my / cv.height) * (view.y1 - view.y0);
  tip.hidden = false; tip.style.left = (e.clientX + 14) + "px"; tip.style.top = (e.clientY + 10) + "px";
  tip.textContent = (best ? best[2] + "\n" : "") + `x ${wx.toFixed(1)}  y ${wy.toFixed(1)}`; tip.style.whiteSpace = "pre"; };
cv.onmouseleave = () => tip.hidden = true;
addEventListener("resize", () => { const f = +sel.value; R = RUNS[f]; setup(); });
matchMedia("(prefers-color-scheme: dark)").addEventListener?.("change", () => draw());
setup();
</script>
"""
