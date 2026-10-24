# WarsBots mailbox automation (Windows, PowerShell 5.1+)

Two layers so agents read `MAILBOX/TO_CODEX.md` / `TO_OPENCODE.md` without the owner relaying messages.

1. **Mail loop inside a live session** (main mechanism). When an agent finishes a task it does **not** end its
   turn; it runs
   `powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Mailbox\wait_mail.ps1 -Agent codex -Repo <clone>`
   which polls `origin/warsbots/research/steelmill-triage` every 60 s for up to 9 min and exits 0 with the new
   items printed (state in `.git\warsbots-mail-<agent>.last`) or 1 on timeout → call again. `-Peek` lists open
   items without waiting. Only `git fetch` + `git show`; never checkout/merge.
2. **Wake-up for stopped sessions** (safety net). `register_wake_task.ps1 -CodexRepo <clone> -OpenCodeRepo <clone>`
   registers the scheduled task "WarsBots Mailbox Wake" (every 10 min). `wake_agents.ps1` starts
   `codex exec --full-auto` / `opencode run` with a "read your mailbox" prompt only when there is an open item
   newer than the last wake **and** the agent's clone looks idle (no commit, no edited file for 20 min), and never
   while a run it started is alive. Log: `%LOCALAPPDATA%\WarsBots\Mailbox\wake.log`. `-DryRun` to test,
   `register_wake_task.ps1 -Remove` to undo. Needs the CLIs on PATH (`codex`, `opencode`); without them it only logs.

Install: copy `Tools\Mailbox\*` to a stable folder (e.g. `%LOCALAPPDATA%\WarsBots\Mailbox\bin`) so branch
switches do not move the scripts:
`git show origin/warsbots/research/steelmill-triage:Tools/Mailbox/wait_mail.ps1 > wait_mail.ps1` (same for the others).

Cost note: each wake is a full agent run. OpenCode on a free tier may hit its limit; the task then just logs.
