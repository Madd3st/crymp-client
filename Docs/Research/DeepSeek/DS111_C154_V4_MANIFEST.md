# DS111 · C154 v1 readiness abort: V4 raw-CRLF manifest vs runtime CR-stripped FNV

Status: **diagnosis / INVALID_DATA_PACKAGING** (no game-code defect proved, no SHA substitution).

## Observed archive

`QA/build/c154-route-3cfd8596-v1` (exact source `3cfd8596`, CI `40855444355`, review head `16b8f2a9`):

- `session.json`: `phase=archived_restored`, `client_pid=null`, `client_log=null`, `server_pid=4092`; no client.
- `runtime-result.json`: `runtime_criteria_pass=false`, `quality_pass=false`, `failure="Readiness error"`, `evidence=null`.
- `C154-3cfd8596-v1.log`:
  - `158: NAVMESH loaded Multiplayer/PS/Shore: 46830 polys, 3857 parts, 2 links, 11329588 bytes.`
  - `159: NAVMESH data links for Shore does not match its manifest (ece81253207f6be5, manifest 6e03169869301531).`
  - `160: NAVMESH link kinds 0/2 ...`
  - `165-172`: native registered-545 `DEBUG REVIVE`, `revive result ... sameEntity=1 alive=1 health=100`; setup worked.
  - `178: gate=1 levelReady=1 bots=2/2`; `188: #1 ENTITY_EVENT_DONE`.
  - Bot trace only `t=0.265` and `t=0.790`; no `#2 LADDER start/done`, no `NAVMESH links ON`, no accepted ladder execution.
- `demo-archive.json`: one `Multiplayer_PS_Shore-...wbdemo`, `bytes=0`, SHA256 `E3B0...`; no `WBDM` header.
- `review-checks.jsonl`: two fresh fetches, `MATCH` approved/observed `16b8f2a9`; no stale-review launch.
- Physical attempt count: **1** (v1); no retry, no unchanged game.

## Root cause

Actual `Code/CryGame/Bots/BotNavMesh.cpp` at 3cfd:

- `Fnv64` lines 24-28: FNV-1a 64 over raw bytes.
- `ManifestFnv` lines 30-41: reads `"fnv64"` from `<leaf>.manifest.json`.
- `ManifestOk` lines 44-56, especially 49-52: for every non-`navmesh` kind it removes `\r` before hashing:
  `if (std::strcmp(kind, "navmesh")) { for (char c : data) if (c != '\r') lf.push_back(c); }`
  `const std::string& h = std::strcmp(kind, "navmesh") ? lf : data;`
- Links path line 100: failed `ManifestOk(...,"links",...)` clears the text; line 101 leaves `s.kinds=0`; no link mask/ladder is enabled.

V4 files:

| file | bytes | SHA256 | raw FNV | CR-stripped FNV |
| --- | ---: | --- | --- | --- |
| `Shore.links.txt` | 118 (2 CRLF) | `e7204de3…` | `6e03169869301531` (manifest) | `ece81253207f6be5` (runtime) |
| `Shore.navmesh` | 11329588 | `b60178d7…` | `f9e8cb2d6cc174bb` | n/a (binary) |
| `Shore.spots.txt` | 62 (LF) | `d95e1e11…` | `9bce2faedebbca34` | same |
| `Shore.manifest.json` | — | `c7e96b3d…` | — | — |

The V4 manifest `links.fnv64` is `6e03169869301531` (raw CRLF), while actual `ManifestOk` computes `ece81253207f6be5`; mismatch refuses the links as another bake. This is the exact runtime log pair. Navmesh (warning-only) and spots (already LF) still match.

## Why the offline checks missed it

- `Tools/Diagnostics/ShoreLowerRouteProbe.cpp:11-13` calls `NavMeshCore::LoadFile` / `LoadLinkKinds` / `SetLinkMask` directly.
- `build/c154-endpoint/proof.cpp:6` does the same (`m.LoadFile`, `m.LoadLinkKinds`, `m.SetLinkMask(14)`).
- Neither executes `BotNavMesh.cpp` `ManifestOk`/`Fnv64`/`ManifestFnv`; the actual load-gate (`CheckData` equivalent) was not in the offline core query. The data otherwise parses, which is why the offline 12/12 and 2-link results passed.

## V5 draft (present; not yet final-published)

`build/c154-route-data-v5` (created 13:29) is consistent with the contract:

- `Shore.links.txt`: canonical LF, **116 bytes**, SHA256 `0230a0c3e59bb818888684012291ee39efaddb542917ac0bc5e02c8e13d32dcb`, FNV `ece81253207f6be5`.
- `Shore.navmesh`: unchanged `b60178d7…` (raw FNV `f9e8cb2d6cc174bb`).
- `Shore.spots.txt`: unchanged `d95e1e11…` (LF, FNV `9bce2faedebbca34`).
- `Shore.manifest.json`: SHA256 `a95a0503813e394161bf961f6556e26db672b46041a03079baac99d8191b13e2`; links `bytes=116`, `fnv64=ece81253207f6be5`; explicit `manifest_fnv_contract` (navmesh raw; links/spots remove CR).
- `build/c154-manifest-caller`: literal actual `ManifestOk`/`Fnv64`/`ManifestFnv` contract control: V4 links=`0` (exit 1), V5 links=`1` (exit 0); tamper rejected; CRLF text normalized for V5. Reproduced read-only.

V2 QA harness draft (local commit `c71aacee`, **not yet on origin**; remote weapon-goals is `efc081eb`) changes only data path/hashes to V5, `out/label` to `-v2`, and quality scope to `c154-lower-route-v2` (old V1 quality refused). `PrepareOnly` reproduces **1460/1506** command bytes and V5 `data_hashes=PASS`. No V2 quality receipt exists yet; current `build/ci-3cfd8596/c154-quality.json` is still V1-scope.

## Restore/audit limits

- Archive restore receipts: `memory-before == memory-restored` and `memory-after == before`; `nav-restored` exact, `nav-after` contained the staged V4 Shore files and restore removed them. Receipts are internally consistent.
- Current plain `%LOCALAPPDATA%/WarsBots/Navmesh` matches the archive restored set (16 files, no Shore); current plain `Memory` does **not** match the archive Memory receipt (2 files present vs 4; Shore hash differs). This is consistent with the known C153 **MSIX internal vs external plain AppData** distinction and is not by itself proof of a restore failure; the archive receipts are the authoritative internal evidence. No live-process claim.

## Conclusion

- v1 is **INVALID_DATA_PACKAGING**: the readiness gate correctly refused a V4 manifest whose raw-CRLF FNV disagrees with the runtime CR-stripped contract. No game-code fault, no SHA substitution, no ladder attempt, no full demo.
- Do not retry v1 or bypass `ManifestOk`; preserve v1 as the single physical attempt.
- V5 canonical-LF data + matching FNV, if published with a fresh V2 harness/quality and a new post-CI review, is the correct data-only fix. Same `3cfd` green CI can be reused only for this data fix; fresh V5/harness pins and V2 quality are mandatory. The postarchive checker still references `build/c154-route-data-v4` and must be pinned to V5 (or explicitly justified) before the post-run archive/Core audit.
