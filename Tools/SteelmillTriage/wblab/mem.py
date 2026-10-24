"""Reader for BotNav map memory files (WBMEM01), as written by BotNav::Save().

Layout (little-endian, see Code/CryGame/Bots/BotNav.cpp Save/Load):
    char[8]  magic "WBMEM01\\0"
    int32    nx, ny          grid columns
    float32  cell            metres
    float32  min_x, min_y    grid origin
    int32    count           node count
    count x { float32 stuck, kills, deaths }

The file holds per-node counters ONLY. It has no node coordinates, no columns and
no edges, so it cannot be drawn on a map by itself: node i is "the i-th accepted
floor" in column-scan order, and how many floors each column produced is not stored.
Use a graph export (NAV_EXPORT_CONTRACT.md) to place the counters.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path

MAGIC = b"WBMEM01\x00"
HEADER = struct.Struct("<8siifffi")
RECORD = struct.Struct("<fff")
STUCK_CAP = 20.0  # BotNav::ReportStuck clamps at 20
PENALTY_CAP = 6.0  # FindPath: 1 + 0.6 * min(stuck, 6)


class MemError(ValueError):
    pass


@dataclass
class MapMemory:
    nx: int
    ny: int
    cell: float
    min_x: float
    min_y: float
    stuck: list = field(default_factory=list)
    kills: list = field(default_factory=list)
    deaths: list = field(default_factory=list)

    @property
    def count(self) -> int:
        return len(self.stuck)

    def fingerprint(self) -> dict:
        """The fields BotNav::Load compares before reusing memory."""
        return dict(nx=self.nx, ny=self.ny, cell=self.cell, min_x=self.min_x,
                    min_y=self.min_y, count=self.count)

    def summary(self) -> dict:
        n = self.count
        st, k, d = self.stuck, self.kills, self.deaths
        stuck_nodes = [i for i in range(n) if st[i] > 0]
        # BotNav::PickHotspot iterates `i += 2`: odd nodes are never candidates.
        def hot_weight(i, safe):
            w = (k[i] * 1.5 - d[i] * 0.7) if safe else (k[i] + d[i])
            return w - st[i] * 0.5
        eligible = lambda i, safe: hot_weight(i, safe) > 0.5
        out = dict(
            fingerprint=self.fingerprint(),
            columns=self.nx * self.ny,
            nodes_per_column=round(n / (self.nx * self.ny), 3) if self.nx * self.ny else None,
            totals=dict(stuck=sum(st), kills=sum(k), deaths=sum(d)),
            nodes_with=dict(stuck=len(stuck_nodes), kills=sum(1 for x in k if x > 0),
                            deaths=sum(1 for x in d if x > 0)),
            stuck_at_cap=sum(1 for x in st if x >= STUCK_CAP),
            stuck_at_full_penalty=sum(1 for x in st if x >= PENALTY_CAP),
            max_route_multiplier=round(1 + 0.6 * min(max(st, default=0), PENALTY_CAP), 2),
            top_stuck=sorted(((i, st[i]) for i in stuck_nodes), key=lambda t: -t[1])[:15],
            hotspot_audit={},
        )
        for safe in (False, True):
            all_ok = [i for i in range(n) if eligible(i, safe)]
            seen = [i for i in all_ok if i % 2 == 0]
            wall = sum(hot_weight(i, safe) for i in all_ok)
            wseen = sum(hot_weight(i, safe) for i in seen)
            out["hotspot_audit"]["safe" if safe else "fight"] = dict(
                eligible_nodes=len(all_ok), visible_to_PickHotspot=len(seen),
                weight_ignored_percent=round(100 * (1 - wseen / wall), 1) if wall else None)
        return out


def read(path) -> MapMemory:
    raw = Path(path).read_bytes()
    return parse(raw)


def parse(raw: bytes) -> MapMemory:
    if len(raw) < HEADER.size:
        raise MemError(f"file too short: {len(raw)} bytes")
    magic, nx, ny, cell, mx, my, count = HEADER.unpack_from(raw, 0)
    # BotNav::Load only compares the first 7 bytes.
    if magic[:7] != MAGIC[:7]:
        raise MemError(f"bad magic {magic!r}")
    if count < 0 or nx <= 0 or ny <= 0:
        raise MemError(f"bad header nx={nx} ny={ny} count={count}")
    need = HEADER.size + count * RECORD.size
    if len(raw) != need:
        raise MemError(f"size {len(raw)} != header-implied {need}")
    m = MapMemory(nx, ny, cell, mx, my)
    for s, k, d in RECORD.iter_unpack(raw[HEADER.size:]):
        m.stuck.append(s); m.kills.append(k); m.deaths.append(d)
    return m


def write(m: MapMemory) -> bytes:
    """Byte-identical to BotNav::Save (used by tests and for synthetic fixtures)."""
    out = bytearray(HEADER.pack(MAGIC, m.nx, m.ny, m.cell, m.min_x, m.min_y, m.count))
    for s, k, d in zip(m.stuck, m.kills, m.deaths):
        out += RECORD.pack(s, k, d)
    return bytes(out)


def diff(a: MapMemory, b: MapMemory) -> dict:
    """Per-node growth from snapshot a to b (e.g. before/after a QA run)."""
    if a.fingerprint() != b.fingerprint():
        raise MemError("snapshots belong to different graphs")
    grow = lambda x, y: [j - i for i, j in zip(x, y)]
    ds, dk, dd = grow(a.stuck, b.stuck), grow(a.kills, b.kills), grow(a.deaths, b.deaths)
    return dict(stuck=sum(ds), kills=sum(dk), deaths=sum(dd),
                negative_nodes=sum(1 for v in ds + dk + dd if v < 0),
                new_stuck_nodes=[i for i, v in enumerate(ds) if v > 0 and a.stuck[i] == 0])
