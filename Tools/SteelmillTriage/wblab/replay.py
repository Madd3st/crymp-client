"""2D match replay from WarsBots server logs (MOVE TRACE + AI telemetry + kills) -> one self-contained HTML page.

Owner request 2026-10-24: "demos like in CS". This is the offline, log-based first step: top-down playback on the
minimap with every player's position, heading, suit mode, sprint, AI state, current target and the kill feed.
Needs `wb_move_trace 1` in the run (4 Hz). In-game demo recording is a later Codex task (OC-017 research).
"""
import json
import re
from pathlib import Path

_KV = re.compile(r'(\w+)=(\([^)]*\)|\S+)')
_T = re.compile(r'^([\d.]+) ')
_STATE = re.compile(r'AI telemetry EntityId=(\d+) state=(\w+)')
_KILL = re.compile(r'GameRules::KillPlayer target=(\d+) killer=(\d+) name=(\S+?);')
_TARGET = re.compile(r'(?:Bot (\d+) acquired enemy id=(\d+)|TARGET CHOICE actor=(\d+) old=\d+ target=(\d+))')
_NAME = re.compile(r'acquired enemy id=(\d+) name=(\S+) kind=')
_SCORE = re.compile(r'IA SCORE EntityId=(\d+) kills=(-?\d+) deaths=(-?\d+)')


def build(log_path, label=None):
    tracks, kinds, names, states, targets, kills, score = {}, {}, {}, {}, {}, [], {}
    t_last = 0.0
    with open(log_path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = _T.match(line)
            if not m:
                continue
            t = float(m.group(1))
            i = line.find('MOVE TRACE ')
            if i >= 0:
                kv = dict(_KV.findall(line[i + 11:]))
                if kv.get('side') != 'server':
                    continue
                try:
                    a = kv['actor']; x, y, z = (float(c) for c in kv['pos'].strip('()').split(','))
                    vx, vy, _ = (float(c) for c in kv['vel'].strip('()').split(','))
                    tracks.setdefault(a, []).append([round(float(kv['t']), 2), round(x, 2), round(y, 2), round(z, 2),
                                                     round(vx, 2), round(vy, 2), int(kv.get('mode', 3)),
                                                     int(kv.get('sprint', 0)), int(float(kv.get('health', 100))),
                                                     int(float(kv.get('energy', 0)))])
                    kinds[a] = kv.get('kind', 'bot')
                    t_last = max(t_last, float(kv['t']))
                except (KeyError, ValueError):
                    pass
                continue
            s = _STATE.search(line)
            if s:
                lst = states.setdefault(s.group(1), [])
                if not lst or lst[-1][1] != s.group(2):
                    lst.append([round(t, 2), s.group(2)])
                continue
            k = _KILL.search(line)
            if k:
                kills.append([round(t, 2), k.group(2), k.group(1)]); continue
            n = _NAME.search(line)
            if n:
                names[n.group(1)] = n.group(2)
            g = _TARGET.search(line)
            if g:
                a, b = (g.group(1), g.group(2)) if g.group(1) else (g.group(3), g.group(4))
                targets.setdefault(a, []).append([round(t, 2), b]); continue
            sc = _SCORE.search(line)
            if sc:
                score[sc.group(1)] = [int(sc.group(2)), int(sc.group(3))]
    # a map restart re-creates every entity with new ids: keep the last (longest-running) segment only
    span = {a: (tr[0][0], tr[-1][0]) for a, tr in tracks.items()}
    finals = sorted(st for st, en in span.values() if en >= t_last - 10)
    t0 = finals[len(finals) // 2] if finals else 0.0
    keep = [a for a, (st, en) in span.items() if en > t0 + 5]
    actors = sorted(keep, key=lambda a: (kinds[a] != 'human', names.get(a, a)))
    kills = [k for k in kills if k[0] >= t0 - 1]
    return dict(schema='warsbots-replay/1', label=label or Path(log_path).name, start=round(max(0.0, t0 - 0.5), 2),
                duration=round(t_last, 2),
                actors=[dict(id=a, kind=kinds[a], name=names.get(a, 'You' if kinds[a] == 'human' else 'bot ' + a),
                             track=tracks[a], states=states.get(a, []), targets=targets.get(a, []),
                             score=score.get(a)) for a in actors],
                kills=kills)


def render(data, out_path, minimap=None):
    """minimap: dict(uri,row0,col0,kx,ky,start_x,start_y) as in the Nav Atlas data (CHUDRadar convention)."""
    data = dict(data, minimap=minimap)
    html = TEMPLATE.replace('/*__DATA__*/null', json.dumps(data, separators=(',', ':')))
    Path(out_path).write_text(html, encoding='utf-8')
    return out_path


TEMPLATE = r'''<title>Steelmill Replay</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@500;600&family=IBM+Plex+Sans:wght@400;500&family=IBM+Plex+Mono:wght@400;500&display=swap">
<style>
/* Layout: spectator console — map fills the left, roster + kill feed on the right, transport bar under the map. */
:root{
  --bg:#0d1114; --panel:#141a1f; --line:#25303a; --ink:#d9e2e8; --dim:#7f8e99;
  --armor:#5b9bd5; --speed:#e8c547; --strength:#e05a4f; --cloak:#62d6d0; --you:#f2f2f2; --dead:#4a545c;
  --f-display:"Chakra Petch", "Segoe UI", system-ui, sans-serif;
  --f-body:"IBM Plex Sans", "Segoe UI", system-ui, sans-serif;
  --f-mono:"IBM Plex Mono", ui-monospace, Consolas, monospace;
  color-scheme:dark;
}
html,body{height:100%}
body{background:var(--bg);color:var(--ink);font:14px/1.45 var(--f-body);
  padding-left:max(16px,env(safe-area-inset-left,0px));padding-right:max(16px,env(safe-area-inset-right,0px));padding-block:12px;box-sizing:border-box}
.app{display:grid;grid-template-columns:minmax(0,1fr) 300px;grid-template-rows:auto minmax(0,1fr) auto;gap:10px;height:100%;min-height:560px}
header{grid-column:1/-1;display:flex;align-items:baseline;gap:14px;flex-wrap:wrap}
h1{font:600 20px/1.1 var(--f-display);letter-spacing:.04em;margin:0;text-transform:uppercase}
.meta{color:var(--dim);font:12px var(--f-mono)}
.stage{position:relative;background:#0a0d10;border:1px solid var(--line);border-radius:6px;overflow:hidden;min-height:320px}
canvas{display:block;width:100%;height:100%;cursor:grab}
canvas:active{cursor:grabbing}
.hud{position:absolute;left:10px;top:8px;font:12px var(--f-mono);color:var(--dim);pointer-events:none}
aside{display:flex;flex-direction:column;gap:10px;min-height:0}
.box{background:var(--panel);border:1px solid var(--line);border-radius:6px;padding:10px;min-height:0}
.box h2{font:600 12px var(--f-display);letter-spacing:.12em;text-transform:uppercase;color:var(--dim);margin:0 0 8px}
#roster{display:flex;flex-direction:column;gap:2px;overflow:auto;max-height:46vh}
.p{display:grid;grid-template-columns:10px minmax(0,1fr) auto;gap:8px;align-items:center;padding:4px 6px;border-radius:4px;cursor:pointer;border:1px solid transparent}
.p:hover{border-color:var(--line)} .p[aria-pressed="true"]{border-color:var(--ink)}
.p .sw{width:10px;height:10px;border-radius:50%}
.p .n{white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.p .st{font:11px var(--f-mono);color:var(--dim);text-align:right}
.p.dead .n{color:var(--dead);text-decoration:line-through}
#feed{display:flex;flex-direction:column;gap:3px;overflow:auto;flex:1;font:12px var(--f-mono)}
#feed div{color:var(--dim)} #feed b{color:var(--ink);font-weight:500}
.bar{grid-column:1/2;display:flex;gap:10px;align-items:center;flex-wrap:wrap}
button,select{font:500 13px var(--f-body);color:var(--ink);background:var(--panel);border:1px solid var(--line);border-radius:4px;padding:5px 10px;cursor:pointer}
button:focus-visible,select:focus-visible,input:focus-visible{outline:2px solid var(--speed);outline-offset:1px}
#seek{flex:1;min-width:160px;accent-color:var(--speed)}
#clock{font:500 13px var(--f-mono);font-variant-numeric:tabular-nums;min-width:96px}
.legend{display:flex;gap:12px;flex-wrap:wrap;font:12px var(--f-mono);color:var(--dim)}
.legend i{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:5px;vertical-align:-1px}
label.t{display:flex;gap:5px;align-items:center;font-size:13px;color:var(--dim)}
@media (max-width:820px){.app{grid-template-columns:1fr;grid-template-rows:auto 60vh auto auto;height:auto}.bar{grid-column:1}aside{order:4}#roster{max-height:none}}
@media (prefers-reduced-motion:reduce){*{scroll-behavior:auto}}
</style>
<div class="app">
  <header><h1>Steelmill Replay</h1><span class="meta" id="label"></span>
    <span class="legend"><span><i style="background:var(--armor)"></i>Armor</span><span><i style="background:var(--speed)"></i>Speed</span><span><i style="background:var(--strength)"></i>Strength</span><span><i style="background:var(--cloak)"></i>Cloak</span><span>▲ sprint · line = target</span></span></header>
  <div class="stage"><canvas id="cv"></canvas><div class="hud" id="hud">drag to pan · wheel to zoom · click a player to follow</div></div>
  <aside>
    <div class="box"><h2>Players</h2><div id="roster"></div></div>
    <div class="box" style="display:flex;flex-direction:column;flex:1"><h2>Kill feed</h2><div id="feed"></div></div>
  </aside>
  <div class="bar">
    <button id="play" type="button">Play</button>
    <select id="rate" aria-label="Playback speed"><option>1</option><option selected>2</option><option>4</option><option>8</option></select>
    <input id="seek" type="range" min="0" step="0.05" aria-label="Time">
    <span id="clock">0:00</span>
    <label class="t"><input type="checkbox" id="trails" checked>Trails</label>
    <label class="t"><input type="checkbox" id="labels" checked>Names</label>
  </div>
</div>
<script>
const D=/*__DATA__*/null;
const $=id=>document.getElementById(id);
const css=n=>getComputedStyle(document.documentElement).getPropertyValue(n).trim();
const MODE=["--speed","--strength","--cloak","--armor"];
const HUE=a=>`hsl(${(D.actors.indexOf(a)*360/D.actors.length+200)%360} 70% 62%)`;
const LEN=D.duration-(D.start||0);$("label").textContent=`${D.label} · ${Math.floor(LEN/60)}:${String(Math.round(LEN%60)).padStart(2,"0")} · ${D.actors.length} players · ${D.kills.length} kills`;
// bounds from tracks
let x0=1e9,y0=1e9,x1=-1e9,y1=-1e9;
for(const a of D.actors) for(const s of a.track){x0=Math.min(x0,s[1]);x1=Math.max(x1,s[1]);y0=Math.min(y0,s[2]);y1=Math.max(y1,s[2]);}
x0-=15;y0-=15;x1+=15;y1+=15;
const st={t:0,play:false,scale:1,ox:0,oy:0,follow:null,fitted:false};
const cv=$("cv"),ctx=cv.getContext("2d");let W=0,H=0,dpr=1;
const MAP={img:new Image(),ok:false}; if(D.minimap){MAP.img.onload=()=>{MAP.ok=true;draw()};MAP.img.src=D.minimap.uri;}
function fit(){const r=cv.getBoundingClientRect();dpr=devicePixelRatio||1;W=r.width;H=r.height;cv.width=W*dpr;cv.height=H*dpr;
  if(!st.fitted){st.scale=Math.min(W/(x1-x0),H/(y1-y0))*0.95;st.ox=(W-(x1-x0)*st.scale)/2;st.oy=(H-(y1-y0)*st.scale)/2;st.fitted=true;}draw();}
const sx=x=>st.ox+(x-x0)*st.scale, sy=y=>st.oy+(y1-y)*st.scale;
// sample lookup: last track sample <= t (binary search)
function at(arr,t,k=0){let lo=0,hi=arr.length-1,r=-1;while(lo<=hi){const m=(lo+hi)>>1;if(arr[m][k]<=t){r=m;lo=m+1}else hi=m-1}return r;}
function pose(a,t){const i=at(a.track,t);if(i<0)return null;const s=a.track[i],n=a.track[i+1];
  if(!n||n[0]-s[0]>1.0||s[8]<=0)return {s,x:s[1],y:s[2],alive:s[8]>0&&t-s[0]<1.0};
  const f=(t-s[0])/(n[0]-s[0]);return {s,x:s[1]+(n[1]-s[1])*f,y:s[2]+(n[2]-s[2])*f,alive:true};}
const stateAt=(a,t)=>{const i=at(a.states,t);return i<0?"":a.states[i][1]};
const targetAt=(a,t)=>{const i=at(a.targets,t);return i<0||t-a.targets[i][0]>12?null:a.targets[i][1]};
const byId=Object.fromEntries(D.actors.map(a=>[a.id,a]));
function draw(){
  if(!W)return;ctx.setTransform(dpr,0,0,dpr,0,0);ctx.fillStyle="#0a0d10";ctx.fillRect(0,0,W,H);
  if(MAP.ok){const M=D.minimap;ctx.save();ctx.globalAlpha=.55;
    ctx.setTransform(0,dpr*(-M.ky*st.scale),dpr*(M.kx*st.scale),0,dpr*(st.ox+(M.start_x+M.row0*M.kx-x0)*st.scale),dpr*(st.oy+(y1-(M.start_y+M.col0*M.ky))*st.scale));
    ctx.drawImage(MAP.img,0,0);ctx.restore();ctx.setTransform(dpr,0,0,dpr,0,0);}
  const t=st.t, R=Math.max(4,Math.min(9,st.scale*1.6)), P={};
  for(const a of D.actors){const p=pose(a,t);if(p)P[a.id]=p;}
  if(st.follow&&P[st.follow]&&st.play){const p=P[st.follow];st.ox+=W/2-sx(p.x);st.oy+=H/2-sy(p.y);}
  if($("trails").checked){ctx.lineWidth=1.5;for(const a of D.actors){const i=at(a.track,t);if(i<1)continue;ctx.strokeStyle=a.kind==="human"?css("--you"):HUE(a);ctx.globalAlpha=.35;ctx.beginPath();
    let started=false;for(let j=Math.max(0,i-20);j<=i;j++){const s=a.track[j];if(s[8]<=0){started=false;continue}if(!started){ctx.moveTo(sx(s[1]),sy(s[2]));started=true}else ctx.lineTo(sx(s[1]),sy(s[2]));}ctx.stroke();}ctx.globalAlpha=1;}
  // target lines
  ctx.setLineDash([4,4]);ctx.lineWidth=1;
  for(const a of D.actors){const p=P[a.id];if(!p||!p.alive)continue;const sta=stateAt(a,t);if(!/ATTACK|CHAS/.test(sta))continue;const tg=targetAt(a,t);const q=tg&&P[tg];if(!q||!q.alive)continue;
    ctx.strokeStyle=sta.startsWith("ATTACK")?css("--strength"):css("--dim");ctx.beginPath();ctx.moveTo(sx(p.x),sy(p.y));ctx.lineTo(sx(q.x),sy(q.y));ctx.stroke();}
  ctx.setLineDash([]);
  ctx.font=`500 11px ${css("--f-mono")}`;
  for(const a of D.actors){const p=P[a.id];if(!p)continue;const X=sx(p.x),Y=sy(p.y),s=p.s;
    if(!p.alive){ctx.strokeStyle=css("--dead");ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(X-R*.7,Y-R*.7);ctx.lineTo(X+R*.7,Y+R*.7);ctx.moveTo(X+R*.7,Y-R*.7);ctx.lineTo(X-R*.7,Y+R*.7);ctx.stroke();continue;}
    ctx.fillStyle=a.kind==="human"?css("--you"):HUE(a);ctx.beginPath();ctx.arc(X,Y,R,0,7);ctx.fill();
    ctx.strokeStyle=css(MODE[s[6]]||"--armor");ctx.lineWidth=2.5;ctx.beginPath();ctx.arc(X,Y,R+2.5,0,7);ctx.stroke();
    const v=Math.hypot(s[4],s[5]);if(v>0.4){const ux=s[4]/v,uy=-s[5]/v,L=R+6+Math.min(14,v*2);
      ctx.strokeStyle=css("--ink");ctx.lineWidth=1.5;ctx.beginPath();ctx.moveTo(X+ux*(R+2),Y+uy*(R+2));ctx.lineTo(X+ux*L,Y+uy*L);ctx.stroke();
      if(s[7]){ctx.fillStyle=css("--speed");ctx.beginPath();ctx.moveTo(X+ux*(L+5),Y+uy*(L+5));ctx.lineTo(X+ux*L-uy*3,Y+uy*L+ux*3);ctx.lineTo(X+ux*L+uy*3,Y+uy*L-ux*3);ctx.fill();}}
    if(st.follow===a.id){ctx.strokeStyle=css("--ink");ctx.lineWidth=1;ctx.beginPath();ctx.arc(X,Y,R+8,0,7);ctx.stroke();}
    if($("labels").checked){ctx.fillStyle=css("--ink");ctx.fillText(`${a.name}  ${(stateAt(a,t)||"").replace(/_/g," ").toLowerCase()}`,X+R+6,Y-R-2);
      ctx.fillStyle=css("--dim");ctx.fillText(`${D.gaze?'':v.toFixed(1)+' m/s · '}z ${s[3].toFixed(0)} · hp ${s[8]} · e ${s[9]}`,X+R+6,Y+R+10);}
  }
  $("clock").textContent=`${Math.floor(t/60)}:${(t%60).toFixed(1).padStart(4,"0")}`;$("seek").value=t;
  roster(P,t);feed(t);
}
function roster(P,t){const el=$("roster");if(!el.children.length){for(const a of D.actors){const d=document.createElement("div");d.className="p";d.dataset.id=a.id;
    d.innerHTML=`<span class="sw" style="background:${a.kind==="human"?css("--you"):HUE(a)}"></span><span class="n">${a.name}${a.score?` <span class="meta">${a.score[0]}/${a.score[1]}</span>`:""}</span><span class="st"></span>`;
    d.onclick=()=>{st.follow=st.follow===a.id?null:a.id;[...el.children].forEach(c=>c.setAttribute("aria-pressed",c.dataset.id===st.follow));const p=P[a.id];if(p&&st.follow){st.ox+=W/2-sx(p.x);st.oy+=H/2-sy(p.y);}draw();};el.append(d);}}
  for(const d of el.children){const a=byId[d.dataset.id],p=P[a.id];d.classList.toggle("dead",!p||!p.alive);d.querySelector(".st").textContent=(stateAt(a,t)||"—").replace(/_/g," ").toLowerCase();}}
let feedN=-1;function feed(t){const n=at(D.kills,t);if(n===feedN)return;feedN=n;const el=$("feed");el.innerHTML="";
  for(let i=n;i>=0&&i>n-40;i--){const[k,ki,ta]=D.kills[i];const d=document.createElement("div");
    d.innerHTML=`${Math.floor(k/60)}:${String(Math.floor(k%60)).padStart(2,"0")} <b>${byId[ki]?.name||ki}</b> → ${byId[ta]?.name||ta}`;el.append(d);}}
let last=0;function tick(ts){if(st.play){const dt=Math.min(.1,(ts-last)/1000);st.t=Math.min(D.duration,st.t+dt*+$("rate").value);if(st.t>=D.duration)toggle(false);draw();}last=ts;requestAnimationFrame(tick);}
function toggle(v){st.play=v;$("play").textContent=v?"Pause":"Play";}
$("play").onclick=()=>toggle(!st.play);$("seek").max=D.duration;$("seek").min=D.start||0;$("seek").oninput=e=>{st.t=+e.target.value;draw()};
$("trails").onchange=$("labels").onchange=draw;
addEventListener("keydown",e=>{if(e.code==="Space"&&e.target.tagName!=="SELECT"){e.preventDefault();toggle(!st.play)}});
let drag=null;cv.onpointerdown=e=>{drag={x:e.clientX,y:e.clientY,ox:st.ox,oy:st.oy,moved:false};cv.setPointerCapture(e.pointerId)};
cv.onpointermove=e=>{if(!drag)return;const dx=e.clientX-drag.x,dy=e.clientY-drag.y;if(Math.abs(dx)+Math.abs(dy)>3)drag.moved=true;st.ox=drag.ox+dx;st.oy=drag.oy+dy;draw()};
cv.onpointerup=e=>{if(drag&&!drag.moved){const r=cv.getBoundingClientRect(),mx=e.clientX-r.left,my=e.clientY-r.top;let best=null,bd=18;
  for(const a of D.actors){const p=pose(a,st.t);if(!p)continue;const d=Math.hypot(sx(p.x)-mx,sy(p.y)-my);if(d<bd){bd=d;best=a.id}}
  st.follow=best;[...$("roster").children].forEach(c=>c.setAttribute("aria-pressed",c.dataset.id===st.follow));draw();}drag=null};
cv.onwheel=e=>{e.preventDefault();const r=cv.getBoundingClientRect(),mx=e.clientX-r.left,my=e.clientY-r.top,k=e.deltaY<0?1.15:1/1.15;
  st.ox=mx-(mx-st.ox)*k;st.oy=my-(my-st.oy)*k;st.scale*=k;draw()};
new ResizeObserver(fit).observe(cv);
// open on the first kill so the first frame shows a fight
st.t=D.kills.length?Math.max(D.start||0,D.kills[0][0]-3):Math.min((D.start||0)+30,D.duration);
requestAnimationFrame(tick);
</script>
'''


def build_from_demo(d, label=None):
    """Same replay data from a wbdemo/2 file (wblab.demo.read). The arrow shows where the player LOOKS (yaw),
    not where it moves — that is what a demo adds over MOVE TRACE."""
    import math
    tracks, states, targets = {}, {}, {}
    for t, rows in d['frames']:
        for r in rows:
            aid, x, y, z, yaw, mode, energy, health = str(r[0]), r[1], r[2], r[3], r[4] / 100.0, r[8], r[9], r[10]
            yr = math.radians(yaw)
            tracks.setdefault(aid, []).append([round(t, 2), round(x, 2), round(y, 2), round(z, 2),
                                               round(4 * math.sin(yr), 2), round(4 * math.cos(yr), 2),
                                               int(mode), 0, int(health), int(energy)])
    for b in d['brain']:
        a = str(b['id']); lst = states.setdefault(a, [])
        if not lst or lst[-1][1] != b['state']:
            lst.append([round(b['t'], 2), b['state']])
        if b['target']:
            tg = targets.setdefault(a, [])
            if not tg or tg[-1][1] != str(b['target']):
                tg.append([round(b['t'], 2), str(b['target'])])
    kills = [[round(e['t'], 2), str(e['actor']), str(e['other'])] for e in d['events'] if e['kind'] == 'kill']
    acts = [a for a in d['actors'].values() if str(a['id']) in tracks]
    acts.sort(key=lambda a: (a['kind'] != 'human', a['name']))
    dur = d['frames'][-1][0] if d['frames'] else 0
    return dict(schema='warsbots-replay/1', gaze=True, label=label or 'demo', start=0.0, duration=round(dur, 2),
                actors=[dict(id=str(a['id']), kind=a['kind'], name=a['name'] + ('' if a['kind'] == 'human' else ' · ' + a.get('profile', '')),
                             track=tracks[str(a['id'])], states=states.get(str(a['id']), []),
                             targets=targets.get(str(a['id']), []), score=None) for a in acts],
                kills=kills)
