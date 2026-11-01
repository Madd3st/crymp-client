# C146: owner videos and native Shore route, 2026-10-28

**DS085 corrections independently checked:** near the pool rim client35.395–35.898 reports local waterZ89.37 while swim0; after submersion waterZ changes to99.5/swim1. The complete native log (outside the supplied73sclip) ALSO contains an upper Strength jump: t156.644 from2107.373,1883.286,103.125/mode1 -> apex106.682 -> t158.147 landing2102.234,1882.349,106.017 (rise2.892). This supports the real upper route, west counterpart of candidate east link; it does not prove bot traversal. DeepSeek2762ba5b/461326e9; rawtrace independently inspected byCodex. Earlier pool-plane wording below is superseded.

Evidence analysis only; no new game, code change or Playable acceptance. Native recording uses exact claude-sprint b06681eaa4365afe5bf7309994012dce13a7ac83, green CI40649771503 and independent DS081 review dd68f2ce. Zero bots, navmesh0, jump_links0. Owner supplied two local MP4s; original paths, sizes and SHA256 are in [receipt](C146_VIDEO_ANALYSIS.json). Videos remain local, outside Git.

## What the videos show

The 25.10 clip (261.13 s) shows an older **Steelmill InstantAction** bot playtest. Console at approximately260s identifies `v26-159-gb521feeb`, not current b06681ea. Chronological sampled frames show spectator combat/deaths and weapon changes. Historical reference only: no current-code regression or fix inferred from this clip.

The 28.10 clip (73.13 s) shows the native human **Shore lower-deck exit**:

| Approximate video time | Observed action |
| --- | --- |
| 12–14 s | Dry lower metal compartment; approach rectangular indoor pool |
| 16–24 s | Enter water, dive below hull, then ascend outside |
| 25–29 s | Surface alongside submarine/inflatable boat; boarding prompt visible |
| 30–55 s | Board and drive boat toward shore |
| 60–70 s | On land, enter building and emerge outside |

This demonstrates lower-compartment exit without the proposed upper Strength link. The sea-to-shore leg is a **boat journey**, not evidence of uninterrupted infantry swimming. It neither validates nor rejects the separate E223 upper Strength candidate. No new boat/ladder controller is implied by the recording.

## Native trace correlation

Client clock differs from server clock; the following times are CLIENT trace seconds, approximately video time+20s, not interchangeable with server timestamps. F8 marks at client37.959/wall15:16:58 and41.120/wall15:17:01 anchor the underwater interval.

| Client t | Position / native state |
| --- | --- |
| 31.696 | Spawn2124.500,1880.625,90.340; swim0; settles on dry floor z90.125, native no-water sentinel -1000000 |
| 35.144 | 2123.632,1883.959,89.575; still swim0/no-water |
| 36.150 | 2120.467,1884.376,87.806; swim1, waterZ99.50 |
| 37.959 | F8 route mark2118.77,1884.45,86.75 |
| 41.120 | F8 route mark2114.75,1880.58,84.60 |
| 42.149 | 2112.903,1876.227,83.656; minimum depth on first clean swim interval |
| 44.150 | 2100.992,1869.773,92.326; ascending vz6.038 |
| 45.143 | 2092.572,1871.142,100.112; swim1, vz7.602 |
| 45.643 | 2087.302,1872.278,100.776; swim0, suit mode0 |

The pool uses the ocean plane99.5 while the enclosed deck suppresses native water. A constant surface-water plane cannot represent the dry floor, below-hull descent and outside ascent. Most first underwater interval is native mode3/Armor; video does not show an upper Strength jump. Do not infer exact held keys merely from velocity. Once aboard, actor physics speed can be zero while world position moves; later `ShouldSwim` also cannot alone classify on-foot traversal.

Later in the SAME native log, outside the supplied73s clip, t139.65–147.65 stays at x~2116.125,y~1880.625 while z90.586->98.720 with vz~1.077 then.729; t148.9 exits to2116.245,1879.727,99.721. This strongly matches the independently decoded native shaft ladder centre (asset translation2116.125,1880.125 plus native Yoffset.5) and top exit animation. It is **telemetry inference, not visual confirmation**. Any future ladder design must prove physical exit; blind elapsed-time detach is not validated.

## Archive and limits

Local archive: `C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build/owner-playtests/C146-native-Shore-b06681ea-20261028-151609` contains session/arguments, client/server logs and traces, memory-before/after, restore receipt and demo. WBDM2 parser verifies6214frames, one human actor65486, header exactb066/Shore/20Hz/navmeshnone, times.064–310.770. Demo SHA2566bbf02547ea826b0aed50fdb7a13e32c96bd46eac1d523525408b48ca2ef42ce verified. Trace captures999client/1003server samples; requested demo20Hz is distinct from trace cadence.

Before-memory archive hashes verify and launcher records `archived_memory_restored`. **Independent current-memory comparison does not match that receipt**: Shore hash differs and current directory also contains Plantation.mem and Steelmill.nav.json; cause unestablished. This analysis does not overwrite/delete current memory or certify current-byte restoration. Full hashes are in receipt. No game processes active at inspection.

Raw local analysis: `build/c146-video-analysis` contains reproducible frame extraction/contact sheets, videos.json, Client-trace.json, Server-trace.json, player-critical-route.json, native-summary.json and archive-verification.json. Chronological contact sheets and detailed underwater frames inspected; no claim of frame-by-frame full-video review. No raw video/game resources added to Git.

Next: DS085 jointly analyse this route versus actual water steering/topology and propose a bounded repair batch with offline red/green oracles. Preserve staged B1 and hold speculative Strength speed patch. Lower12spawns remain outside current surface-limited mesh. Shore Playable and infantry-to-land exit remain unproved; no purposeless repeat run.
