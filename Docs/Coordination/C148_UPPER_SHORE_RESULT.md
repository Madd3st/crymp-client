# C148: upper submarine exit -> sea -> dry shore PASS

Tested exact claude-sprint **def8a65d99e4640c9c765375f5739d010835e67e**, green CI40703800474, independent code reviewDS09080144e6077050ad098c27517eea445cdb7941262. CI client/server hashes verified before launch. One native US#2 observation,85.03wallseconds, temporary E2235993 oneStrength data/matchingmanifest, nav1/jump_links1/ps_capture1, doors0/vehicles0. Actorless effective-recipe preflight passed; no forced spawn/resample/teleport/client/local game build. No second bot observation.

**PASS for the requested complete upper exit to shore route.** Native trajectory contains all phases:

| Trace time | Evidence |
| --- | --- |
| .27 | Native target654 selected (1794.54,1690.64,101.53), Strength link starts |
| 3.10 | LINK done at2114.85,1885.48,105.92 on upper ring |
| 11.520–25.274 | First sea swim; starts2054.840,1880.623,97.855 |
| 25.548–54.778 | Intermediate native dry segment; continued toward shore, not accepted as final |
| 55.025–76.276 | Second sea swim; ends1871.201,1689.218,98.103 |
| 76.520–82.546 | Final native dry segment; last1822.856,1685.845,100.967, grounded1/swim0/flying0, running6.75m/s |

Endpoint is **28.725m from the native shore target654**; after the final water exit it stays out of swimming for6.026s. Actual committed-core postcheck on the exact E223 data: endpoint projects1822.856,1685.845,101.081 onto a **non-water polygon**, remains connected to target654, remaining path3corners/0links/enddistance.001m. This supports genuine dry shore near the target, not merely the first intermediate pier/islet. Raw postcheck `build/c148-proof/proof-run.log`; native ground motion is the physical evidence, mesh connectivity is supporting data.

Health100 throughout330native samples;142swim samples, no runtime Lua/script/RMI/errors, no link failures. One transient `NAV fallback reason=no_route` at25.55, immediately after native swimming switches off while feet are still98.25; route proceeds successfully. No `start_unprojected` entries (previous C147 had127). Only target654 is reported throughout; a full capture is NOT claimed (observation stops~29m short).

Archive `C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build/c148-route-def8a65d-v1`: logs/traces, args/effective recipe, snapshot-before/after/restored receipts, summary.json and trace.json. DemoWBDM2 parsed: exactdef8/Shore,1642frames, one recorded ownedbot65481; SHA256fd409447353bd903f5b71f2607d3a7d5bc99e4a1408aa2fe968f363c217b5189 verified. **Current memory and nav directory filenames AND byte hashes independently equal fresh pre-run snapshots**, including removal of temporary Shoredata. No game process retained.

[Machine-readable result](C148_UPPER_SHORE_RESULT.json). The upper route is confirmed with def8+E223; this does not promote defaults/data or certify every Shore spawn/capture area, historical41waterholes, lower-deck diving/ladder route, a full PS match or visual animations. DeepSeek DS091 independently audits this same archive; no further game requested.
