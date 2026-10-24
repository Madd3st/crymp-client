# Steelmill Lab merged into Triage (2026-10-24)

`warsbots/research/steelmill-lab` (ad34d283) was a parallel Claude session. Its unique parts now live here:

| Lab file | Here | Note |
| --- | --- | --- |
| `level_data.py` | `wblab/level_data.py`, command `level` | schema renamed `steelmill-triage/map/1` |
| `ml_validate.py` | `wblab/ml_validate.py`, command `episodes` | unchanged |
| `navgraph.py` (botnav-graph/1 validator) | `wblab/navgraph.py` | Triage version is stricter and BotNav-exact; fingerprints agree |
| `wbmem.py` | `wblab/mem.py` | same WBMEM01 parsing |
| `botlog.py`, `lab.py`, `collect.py`, viewer | `wblab/logparse.py`, `analysis.py`, `render.py`, Steelmill Nav Atlas | overlap; not ported |
| `BOTNAV_EXPORT_CONTRACT.md` | stays on the Lab branch, still the contract reference | |

Tests for the ported modules: `tests/test_ported_lab.py`. The Lab branch is retired: no new work there.
