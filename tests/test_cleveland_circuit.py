"""Cleveland Historic Circuit citypack invariants."""
from __future__ import annotations

import json
import math
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / "citypacks" / "cleveland" / "burke_gp_1997"
OFFICIAL_M = 3389.0
EARTH_R = 6_371_000.0


def haversine_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dlat = p2 - p1
    dlon = math.radians(lon2 - lon1)
    h = math.sin(dlat / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dlon / 2) ** 2
    return 2 * EARTH_R * math.asin(math.sqrt(min(1.0, h)))


@pytest.fixture(scope="module")
def racing_line():
    path = PACK / "racing_line.json"
    assert path.is_file(), path
    return json.loads(path.read_text())


@pytest.fixture(scope="module")
def checkpoints():
    path = PACK / "checkpoints.json"
    assert path.is_file(), path
    return json.loads(path.read_text())


def test_closed_loop(racing_line):
    assert racing_line.get("closed") is True
    samples = racing_line["samples"]
    assert len(samples) >= 50
    first, last = samples[0], samples[-1]
    gap = haversine_m(first["lat"], first["lon"], last["lat"], last["lon"])
    # Ring stored without duplicating S/F; last sample should sit one spacing (~5–10 m) from start.
    assert gap < 25.0, f"start/end gap {gap:.1f} m"


def test_ten_turns(racing_line):
    turns = {s.get("turn_index") for s in racing_line["samples"] if s.get("turn_index") is not None}
    assert turns == set(range(1, 11)), turns


def test_length_within_5_percent(racing_line):
    samples = racing_line["samples"]
    length = 0.0
    for i, s in enumerate(samples):
        n = samples[(i + 1) % len(samples)]
        length += haversine_m(s["lat"], s["lon"], n["lat"], n["lon"])
    assert abs(length - OFFICIAL_M) / OFFICIAL_M <= 0.05, length
    if "measured_length_m" in racing_line:
        assert abs(racing_line["measured_length_m"] - length) < 2.0


def test_checkpoint_s_monotonic_and_start_finish(checkpoints):
    assert checkpoints["track_id"] == "cleveland_burke_gp_1997"
    gates = checkpoints["gates"]
    assert 10 <= len(gates) <= 14
    names = " ".join(g["name"].lower() for g in gates)
    assert "start" in names and "finish" in names
    assert gates[0]["s"] == 0 or gates[0]["s"] == 0.0
    assert "start" in gates[0]["name"].lower()
    assert "start" in gates[-1]["name"].lower() or "wrap" in gates[-1]["name"].lower()
    ss = [g["s"] for g in gates]
    assert ss == sorted(ss), ss
    for a, b in zip(ss, ss[1:]):
        assert b >= a
