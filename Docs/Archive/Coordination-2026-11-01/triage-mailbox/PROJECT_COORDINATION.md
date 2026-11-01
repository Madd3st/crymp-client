# WarsBots coordination (maintained by Claude)

Updated 2026-10-24 14:50 MSK. Current truth is `../STATUS.md`; this file keeps branch SHAs and the component
table. Older versions of this file (with the full history of the day) are in git.

## Branches

| Branch | SHA | Owner | State |
| --- | --- | --- | --- |
| `warsbots/feature/weapon-goals` | `fb578d52` | Codex | active: watchdog, route reasons, nav export, body probes, reachability (B), sink escape (C), nav explain, revive_at/teleport, summary pipeline — all behaviour flags default OFF |
| `warsbots/main` | `caf1a815` | integrator | far behind weapon-goals |
| `warsbots/research/steelmill-triage` | this branch | Claude | tools, findings, mailbox, coordination |
| `warsbots/research/fps-ai-planning` | `995672b4` | Claude | FPS AI study, decision-layer prototype, A/B contract |
| `warsbots/research/crymp-native-audit` | `47ccfacf` | Claude | done |
| `warsbots/research/steelmill-lab` | `4c8bac9` | Claude | **retired**, merged into Triage; keeps BOTNAV_EXPORT_CONTRACT.md |

## Components

| Component | State | Evidence |
| --- | --- | --- |
| Fire control / NetShoot replay | fixed for owned bots, holds at 15 bots | unit + server |
| Spectator (`wb_watch`, F1) | native, works in the owner's client | server + client |
| Revive | spawn 195 freezes 19/19 (only that point); cause in the bot revive path, not geometry | server + client |
| BotNav graph | 7633 nodes / 41618 edges; split: outer ring vs factory, 25 % of nodes in sinks; stairs/railings, ledges, roofs missing | graph + client |
| Routing | `no_route` 90–92 % of fallbacks, from the split graph | server + graph |
| Trap recovery | watchdog works; only the 45 s self-kill helps in sinks/195 | server |
| Weapon goals (B) | parked until graph repair | server + graph |
| Pace | bots 2.5 m/s vs owner 4.3 m/s; sprint rare | server + client |
| Map memory | half ignored by PickHotspot (fix behind `wb_hotspot_all_nodes`), no decay | src |
| Decision stability | CHASING↔MOVING_TO_WEAPON flips (4,579 in 48 min) | server |
| Power Struggle | design only | docs |

## Risks

1. Single-run comparisons are noise: paired runs, same roster/memory, ≥2 repeats per arm.
2. Behaviour flags pile up default OFF; promotion needs the owner (D-002).
3. Graph repair touches every bot route: needs a paired soak and the owner's eyes before default ON.
4. One PC runs Codex, servers and owner play; reboots and live sessions stop QA.
