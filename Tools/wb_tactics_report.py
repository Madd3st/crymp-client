#!/usr/bin/env python3
"""Controlled QA comparison and bounded combat episode export (health loss, not raw hit damage)."""
import argparse
import collections
import hashlib
import json
import re
import statistics
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from wb_ai_metrics import measure


def parse(path, emit):
    actors = {}
    slots = {}
    skills = collections.defaultdict(collections.Counter)
    episodes = {}
    pending = collections.defaultdict(list)
    context = None
    positions = {}
    windows = {}
    recovery = collections.defaultdict(list)
    end = 0.0

    def info(actor):
        return actors.get(actor)

    def finish(actor, reason):
        ep = episodes.pop(actor, None)
        if ep:
            ep['end_seconds'] = end
            ep['context_complete']=bool(ep['weapons'] and ep['suit_modes'] and ep['first_sampled_distance'] is not None)
            if ep['outcome'] == 'open': ep['outcome'] = reason
            ep['weapons'] = sorted(ep['weapons'])
            ep['suit_modes'] = sorted(ep['suit_modes'])
            ep['actions'] = dict(ep['actions'])
            emit(ep)

    def encounter(actor, target):
        if actor not in actors: return None
        if actor in episodes and episodes[actor]['target'] != target: finish(actor, 'target_changed')
        if actor not in episodes:
            slot, style, skill = actors[actor]
            episodes[actor] = dict(actor=actor, bot=slot, skill=skill, style=style, target=target,
                start_seconds=end, outcome='open', damage_dealt_hp=0, damage_taken_hp=0,
                weapons=set(), suit_modes=set(), actions=collections.Counter(), first_sampled_distance=None,first_sample_seconds=None,
                damage_kind='native_health_loss_only')
        return episodes[actor]

    def close_life(actor):
        if actor in slots:
            data = slots[actor]
            tier = actors[actor][2]
            skills[tier]['entity_seconds'] += end - data['spawn']
            if data['alive'] is not None: skills[tier]['alive_seconds'] += end-data['alive']
            del slots[actor]

    for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
        t = re.match(r'^([\d.]+) ', line)
        if t: end = float(t[1])
        m = re.search(r'#(\d+) created EntityId=(\d+) name=\S+ profile=(\w+)/(\w+)', line)
        if m:
            slot, actor, style, skill = int(m[1]), int(m[2]), m[3], m[4]
            actors[actor] = (slot, style, skill)
            slots[actor] = dict(spawn=end, alive=end)
        m = re.search(r'ENTITY_EVENT_DONE EntityId=(\d+)', line)
        if m:
            actor=int(m[1]); finish(actor,'entity_done'); close_life(actor)
            if actor in actors: skills[actors[actor][2]]['progress_recovery_interrupted']+=len(recovery.pop(actor,[]))
        m = re.search(r'PROGRESS ESCAPE actor=(\d+) kind=(\w+) goal=(\d+) pos=\(([^)]+)\)',line)
        if m and int(m[1]) in actors:
            actor,kind,goal=int(m[1]),m[2],int(m[3])
            c=skills[actors[actor][2]]; c['progress_escapes']+=1; c['progress_escape_'+kind]+=1
            xyz=[float(v) for v in m[4].split(',')]
            recovery[actor].append((end,xyz[0],xyz[1]))
            if kind=='combat' and actor in episodes and episodes[actor]['target']==goal:
                finish(actor,'no_progress')
        m = re.search(r'AI telemetry EntityId=(\d+) state=(\w+).*health=(-?\d+) weapon=(\d+) ammo=(-?\d+) target=(\d+)',line)
        if m:
            context=int(m[1]); target=int(m[6]); health=int(m[3])
            if context in actors:
                c=skills[actors[context][2]]
                c['samples']+=1; c['state_'+m[2]]+=1
                pos=re.search(r'pos=\(([-\d.]+), ([-\d.]+), ([-\d.]+)\)',line)
                if pos: positions[context]=(end,float(pos[1]),float(pos[2]),m[2],target,health,float(pos[3]))
                if health<=0:
                    c['progress_recovery_interrupted']+=len(recovery.pop(context,[]))
                elif pos:
                    keep=[]
                    for at,x,y in recovery[context]:
                        if end-at>8: c['progress_no_horizontal_recovery_8s']+=1
                        elif (float(pos[1])-x)**2+(float(pos[2])-y)**2>=4: c['progress_horizontal_recoveries_8s']+=1
                        else: keep.append((at,x,y))
                    recovery[context]=keep
                if health>0: c['live_samples']+=1
                if context in episodes and (target==0 or health<=0): finish(context,'dead' if health<=0 else 'lost_target')
        m = re.search(r'ABILITY PHYSICS: .*sprint=(\d) mode=(\d+).*clients=',line)
        if m and context in actors and context in slots and slots[context]['alive'] is not None:
            c=skills[actors[context][2]]; c['suit_samples']+=1; c['mode_'+m[2]]+=1; c['sprint_samples']+=int(m[1])
        m = re.search(r'MOTION INTENT: .*requestedBase=([\d.]+).*blocked=(\d)',line)
        if m and context in actors and float(m[1])>0:
            c=skills[actors[context][2]]; c['moving_samples']+=1; c['blocked_samples']+=int(m[2])
            point=positions.get(context)
            if point and point[5]>0:
                window=windows.setdefault(context,[])
                if window and point[0]-window[-1][0]>3: window.clear()
                window.append(point)
                if len(window)>=4 and point[0]-window[0][0]>=6:
                    walked=sum(((b[1]-a[1])**2+(b[2]-a[2])**2+(b[6]-a[6])**2)**0.5 for a,b in zip(window,window[1:]))
                    net=((point[1]-window[0][1])**2+(point[2]-window[0][2])**2+(point[6]-window[0][6])**2)**0.5
                    c['progress_windows']+=1
                    c['stationary_windows']+=int(walked<0.8)
                    c['loop_suspect_windows']+=int(walked>=3 and net<1.5)
                    c['window_displacement_metres']+=net
                    windows[context]=[point]
        m = re.search(r'Bot (\d+) acquired enemy id=(\d+)',line)
        if m and int(m[1]) in actors:
            actor,target=int(m[1]),int(m[2]); skills[actors[actor][2]]['acquisitions']+=1; encounter(actor,target)
        m = re.search(r'TARGET CHOICE actor=(\d+) old=(\d+) target=(\d+)',line)
        if m and int(m[1]) in actors and int(m[2])!=0:
            skills[actors[int(m[1])][2]]['switch_events']+=1
        m = re.search(r'TACTIC SAMPLE actor=(\d+) skill=(\w+) style=(\w+) weapon=(\S+) suit=(\d+) distance=([\d.]+) action=(\w+) target=(\d+) visible=(\d)',line)
        if m and int(m[8])!=0 and int(m[1]) in slots and slots[int(m[1])]['alive'] is not None:
            actor,target=int(m[1]),int(m[8]); ep=encounter(actor,target)
            if ep:
                ep['weapons'].add(m[4]); ep['suit_modes'].add(int(m[5])); ep['actions'][m[7]]+=1
                if ep['first_sampled_distance'] is None:
                    ep['first_sampled_distance']=float(m[6]); ep['first_sample_seconds']=end
        m = re.search(r'TACTIC actor=(\d+).*action=withdraw ',line)
        if m and int(m[1]) in actors: skills[actors[int(m[1])][2]]['withdraw_events']+=1
        m = re.search(r'Hit (before-Lua|after-Lua) target=(\d+) shooter=(\d+).*weapon=(\d+).*health=(-?\d+)',line)
        if m:
            phase,target,shooter,weapon,health=m[1],int(m[2]),int(m[3]),int(m[4]),int(m[5])
            key=(target,shooter,weapon)
            if phase=='before-Lua': pending[key].append(health)
            elif pending[key]:
                before=pending[key].pop()
                loss=max(0,max(0,before)-max(0,health))
                if shooter in actors and shooter!=target:
                    skills[actors[shooter][2]]['damage_dealt_hp']+=loss
                    skills[actors[shooter][2]]['accepted_hit_events']+=1
                    ep=episodes.get(shooter)
                    if ep and ep['target']==target: ep['damage_dealt_hp']+=loss
                if target in actors:
                    skills[actors[target][2]]['damage_taken_hp']+=loss
                    if target in episodes: episodes[target]['damage_taken_hp']+=loss
        m = re.search(r'combat kill shooter=(\d+) target=(\d+)',line)
        if m:
            shooter,target=int(m[1]),int(m[2])
            if shooter in actors:
                c=skills[actors[shooter][2]]
                c['suicides' if shooter==target else 'kills']+=1
                ep=episodes.get(shooter)
                if ep and ep['target']==target: ep['outcome']='suicide' if shooter==target else 'kill'
        m = re.search(r'death observed EntityId=(\d+)',line)
        if m and int(m[1]) in actors:
            actor=int(m[1]); c=skills[actors[actor][2]]; c['deaths']+=1
            c['progress_recovery_interrupted']+=len(recovery.pop(actor,[]))
            if actor in slots and slots[actor]['alive'] is not None:
                life=end-slots[actor]['alive']; c['completed_life_seconds']+=life; c['completed_lives']+=1
                c['alive_seconds']+=life; slots[actor]['alive']=None
            finish(actor,'death')
        m = re.search(r'revive result EntityId=(\d+).*sameEntity=1 alive=1',line)
        if m and int(m[1]) in slots:
            actor=int(m[1]); slots[actor]['alive']=end
            skills[actors[actor][2]]['progress_recovery_interrupted']+=len(recovery.pop(actor,[]))
            windows.pop(actor,None)
    for actor in list(episodes): finish(actor,'log_end')
    for actor in list(slots): close_life(actor)
    for actor,events in recovery.items():
        if actor in actors: skills[actors[actor][2]]['progress_recovery_censored']+=len(events)
    result={}
    for skill,c in skills.items():
        v=dict(c)
        for key in ('kills','deaths','damage_dealt_hp','damage_taken_hp','acquisitions','accepted_hit_events',
                    'withdraw_events','switch_events','progress_windows','stationary_windows','loop_suspect_windows',
                    'progress_escapes','progress_horizontal_recoveries_8s','progress_no_horizontal_recovery_8s',
                    'progress_recovery_interrupted','progress_recovery_censored'):
            v.setdefault(key,0)
        exposure=c['entity_seconds']/60
        v['kills_per_bot_minute']=round(c['kills']/exposure,3) if exposure else None
        v['hp_damage_per_bot_minute']=round(c['damage_dealt_hp']/exposure,2) if exposure else None
        v['kd']=round(c['kills']/c['deaths'],3) if c['deaths'] else None
        v['alive_fraction']=round(c['alive_seconds']/c['entity_seconds'],3) if c['entity_seconds'] else None
        v['mean_completed_life_seconds']=round(c['completed_life_seconds']/c['completed_lives'],2) if c['completed_lives'] else None
        result[skill]=v
    return result


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('runs',type=Path)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--episodes',type=Path,required=True)
    ap.add_argument('--max-episodes',type=int,default=10000)
    ap.add_argument('--max-bytes',type=int,default=8*1024*1024)
    args=ap.parse_args()
    if args.max_episodes<1 or args.max_bytes<1024: ap.error('Invalid dataset limits')
    runs=json.loads(args.runs.read_text(encoding='utf-8-sig'))
    if isinstance(runs,dict): runs=[runs]
    rows=written=0
    truncated=False
    with args.episodes.open('wb') as f:
        result=[]
        for run in runs:
            path=args.runs.parent/run['log']
            def emit(ep):
                nonlocal rows,written,truncated
                ep.update(schema=2,run=run['tag'],source_sha=run['source_sha'])
                blob=(json.dumps(ep,separators=(',',':'))+'\n').encode()
                if rows>=args.max_episodes or written+len(blob)>args.max_bytes:
                    truncated=True; return
                f.write(blob); rows+=1; written+=len(blob)
            raw=path.read_text(encoding='utf-8',errors='replace')
            observed={(int(n),style,skill) for n,style,skill in re.findall(r'#(\d+) created EntityId=\d+ name=\S+ profile=(\w+)/(\w+)',raw)}
            if 'roster' in run:
                expected={(int(p['number']),p['skill']) for p in run['roster']}
                if {(n,skill) for n,style,skill in observed}!=expected:
                    raise ValueError('Observed profiles disagree with QA roster: '+run['tag'])
                style_expected={(int(p['number']),p['style'],p['skill']) for p in run['roster'] if p.get('style','auto')!='auto'}
                if style_expected and not style_expected.issubset(observed):
                    raise ValueError('Observed styles disagree with QA roster: '+run['tag'])
            metrics=measure(path)
            cpu=[float(v) for v in re.findall(r'PROFILE bots=\d+ avgSum=([\d.]+)ms/frame',raw)]
            metrics['median_reported_window_ms']=statistics.median(cpu) if cpu else None
            metrics['profile_windows']=len(cpu)
            metrics['observed_profiles']=[dict(number=n,style=style,skill=skill) for n,style,skill in sorted(observed)]
            spawns={}
            for n,actor,x,y,z in re.findall(r'#(\d+) created EntityId=(\d+).*position=\(([-\d.]+), ([-\d.]+), ([-\d.]+)\)',raw):
                spawns.setdefault(int(n),dict(number=int(n),actor=int(actor),position=[float(x),float(y),float(z)]))
            metrics['initial_spawns']=[spawns[n] for n in sorted(spawns)]
            result.append(dict(metadata=run,metrics=metrics,skills=parse(path,emit)))
    args.output.write_text(json.dumps(dict(schema=2,runs=result,episode_rows=rows,
        episode_bytes=written,episode_limit_reached=truncated),indent=2)+'\n',encoding='utf-8')
    print(f'{len(result)} runs; {rows} episodes; {written} bytes; capped={truncated}')


if __name__=='__main__': main()
