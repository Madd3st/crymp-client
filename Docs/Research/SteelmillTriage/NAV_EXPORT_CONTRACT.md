# BotNav export: use the Lab contract; this file keeps only the log addendum

**Superseded 2026-10-24 04:50.** The parallel Lab session published
`Docs/Research/SteelmillLab/BOTNAV_EXPORT_CONTRACT.md` (`warsbots/botnav-graph/1`, branch
`warsbots/research/steelmill-lab`). It is the single contract to implement: it is more precise
(per-node `ix, iy, layer`; this file wrongly suggested exporting `Node::column`, which keeps only
the low 32 bits of `Key(ix, iy)`, i.e. `iy`), and its validator already exists. The Triage
`navgraph.py` reader will be adapted to that schema instead of `wbnav/1`.

What remains here and is **not** in the Lab contract is the log addendum below; it is part of the
same small Codex patch.

---

## Log fields wanted together with the export (same small patch)

These make existing traces decisive instead of ambiguous. All are additive, level 2.

| Line today | Add | Why |
| --- | --- | --- |
| `#N NAV fallback start=.. goal=..` | `actor=<EntityId> reason=<start_unprojected\|goal_unprojected\|budget\|no_route> from=<node> to=<node> expanded=<n>` | 55% of failed queries target another floor; cannot separate projection from topology now |
| `BOT SUIT`, `JUMP TAKEOFF/LANDING`, `BOT ACTION`, `ACTION REFUSED` | `actor=<EntityId>` | per-bot suit and jump features cannot be joined |
| new: `NAV stuck report actor= node= pos=` in `ReportStuck` | — | memory growth is not traceable to a location in logs |

