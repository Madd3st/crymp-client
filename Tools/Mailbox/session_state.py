"""Read only OpenCode activity metadata; never reads messages or credentials."""
import argparse,json,sqlite3,time
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--session',required=True);ap.add_argument('--latest',action='store_true');a=ap.parse_args()
c=sqlite3.connect((Path.home()/'.local/share/opencode/opencode.db').as_uri()+'?mode=ro',uri=True,timeout=2)
if a.latest:
    anchor=c.execute('select project_id,directory from session_v2 where id=?',(a.session,)).fetchone()
    if not anchor:raise SystemExit('Anchor session missing')
    candidates=c.execute('select id,model from session_v2 where project_id=? and directory=? and parent_id is null and time_archived is null and time_suspended is null order by time_created desc,id desc',anchor)
    for session,model in candidates:
        if 'deepseek' in str(json.loads(model or '{}').get('id','')).lower():
            a.session=session;break
    else:raise SystemExit('No current DeepSeek session for anchored project/directory')
r=c.execute('select time_idle,idle_outcome from session_v2 where id=?',(a.session,)).fetchone()
if not r:raise SystemExit('Session missing')
msg=c.execute('select max(time_updated) from session_message where session_id=?',(a.session,)).fetchone()[0] or 0
created=c.execute('select max(time_created) from session_message where session_id=?',(a.session,)).fetchone()[0] or 0
pending=c.execute('select count(*) from session_pending where session_id=?',(a.session,)).fetchone()[0]
last=max(r[0] or 0,msg);print(json.dumps(dict(session=a.session,idle=bool(r[0] and r[1] and not pending and created<=r[0]),last_activity=last,age_seconds=(time.time()*1000-last)/1000,pending=pending,outcome=r[1])))
