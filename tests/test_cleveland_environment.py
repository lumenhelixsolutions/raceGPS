"""Cleveland M5 environment dressing invariants (offline, no Unreal)."""
from __future__ import annotations

import json
import math
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / "citypacks" / "cleveland" / "burke_gp_1997"


def haversine_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    r = 6_371_000.0
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dlat = p2 - p1
    dlon = math.radians(lon2 - lon1)
    h = math.sin(dlat / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dlon / 2) ** 2
    return 2 * r * math.asin(math.sqrt(min(1.0, h)))


@pytest.fixture(scope="module")
def racing_line():
    return json.loads((PACK / "racing_line.json").read_text())


@pytest.fixture(scope="module")
def water():
    return json.loads((PACK / "water.json").read_text())


@pytest.fixture(scope="module")
def skyline():
    return json.loads((PACK / "skyline.json").read_text())


@pytest.fixture(scope="module")
def dressing():
    return json.loads((PACK / "track_dressing.json").read_text())


@pytest.fixture(scope="module")
def environment():
    return json.loads((PACK / "environment.json").read_text())


def test_json_files_parse(racing_line, water, skyline, dressing, environment):
    for name in (
        "environment.json",
        "water.json",
        "skyline.json",
        "track_dressing.json",
        "racing_line.json",
        "checkpoints.json",
        "metadata.json",
        "manifest.json",
    ):
        path = PACK / name
        assert path.is_file(), path
        json.loads(path.read_text())
    assert environment["cesium_required"] is False
    assert environment["carla_required"] is False
    assert environment["offline"] is True


def test_ten_turn_dressing_still_consistent(racing_line, dressing):
    turns = {s.get("turn_index") for s in racing_line["samples"] if s.get("turn_index") is not None}
    assert turns == set(range(1, 11)), turns
    assert racing_line.get("closed") is True
    samples = racing_line["samples"]
    length = 0.0
    for i, s in enumerate(samples):
        n = samples[(i + 1) % len(samples)]
        length += haversine_m(s["lat"], s["lon"], n["lat"], n["lon"])
    assert abs(length - 3389.0) / 3389.0 <= 0.05, length
    assert dressing["generated_from"] == "racing_line.json"
    skip = dressing["skip"]["t1_vortex_s"]
    assert skip[0] < 751.229 < skip[1]


def test_lake_polygon_north_of_circuit_centroid(racing_line, water):
    samples = racing_line["samples"]
    clat = sum(s["lat"] for s in samples) / len(samples)
    clon = sum(s["lon"] for s in samples) / len(samples)
    pts = water["points"]
    assert len(pts) >= 3
    lake_lats = [p["lat"] for p in pts]
    assert min(lake_lats) > clat, (min(lake_lats), clat)
    # centroid of lake should also sit north
    lake_c = sum(lake_lats) / len(lake_lats)
    assert lake_c > clat
    # one polygon, not an 811-pond dump
    assert len(pts) < 32
    assert "polygons" not in water or len(water.get("polygons") or []) <= 1
    assert "inner_shoreline" in water
    # unused
    _ = clon


def test_skyline_south_of_circuit(racing_line, skyline):
    min_circuit_lat = min(s["lat"] for s in racing_line["samples"])
    buildings = skyline["buildings"]
    assert 25 <= len(buildings) <= 60
    names = {b["name"] for b in buildings}
    for required in ("Key Tower", "Terminal Tower", "200 Public Square"):
        assert required in names, required
    for b in buildings:
        assert b["lat"] < min_circuit_lat, (b["name"], b["lat"], min_circuit_lat)
        assert b["height_m"] > 10.0
        assert "width_m" in b or "footprint" in b
    assert skyline.get("tier") == 0
    assert any("photoreal" in a.lower() or "cadastral" in a.lower() or "centroid" in a.lower() for a in skyline["assumptions"])


def test_barrier_count_and_grid(dressing):
    assert len(dressing["barriers"]) > 50
    types = {b["type"] for b in dressing["barriers"]}
    assert "concrete" in types and "tire" in types
    slots = dressing["grid_slots"]
    assert len(slots) == 3
    assert [s["role"] for s in slots] == ["PLAYER", "AI", "AI"]
    assert dressing["start_finish"]["s"] == 0.0
    assert len(dressing["cones"]) > 10
    assert 8 <= len(dressing["airport_boxes"]) <= 16
    assert 1 <= len(dressing["infield_grass"]) <= 6
    assert dressing["runway_regions"][0]["name"].startswith("06L")
    assert "G" in dressing["taxiway_regions"][0]["name"]
    assert dressing["pit_visual"]["modeled_in_xodr"] is False


def test_manifest_lists_env_layers(environment):
    ids = {layer["id"] for layer in environment["layers"]}
    for needed in ("water", "skyline", "barriers", "start_finish", "airport_boxes"):
        assert needed in ids


def test_airport_hangars_and_markings(dressing, environment):
    boxes = dressing["airport_boxes"]
    names = {b["name"] for b in boxes}
    for required in ("hangar_west", "hangar_mid", "hangar_fsdo", "hangar_fbo", "hangar_t_row_w"):
        assert required in names, required
    # Geo stays on Burke, south of origin, same yaw as 06/24.
    for b in boxes:
        assert 41.511 < b["lat"] < 41.516, b
        assert -81.691 < b["lon"] < -81.674, b
        assert abs(b["yaw_deg"] - 58.0) < 1.0
        assert b.get("mesh_path", "").startswith("/Game/Carla/")
        assert b["height_m"] <= 16.0  # hangars not terminals
    assert dressing.get("prefer_carla_hangar_mesh") is True
    cands = dressing["hangar_mesh_candidates"]
    assert any("SM_Hangar" in c for c in cands)
    marks = dressing["taxiway_markings"]
    assert 50 <= len(marks) <= 240
    types = {m["type"] for m in marks}
    assert "centerline" in types and "hold_short" in types
    assert len(dressing["cones"]) <= 360
    assert len(dressing["barriers"]) <= 1100
    ids = {layer["id"] for layer in environment["layers"]}
    assert "taxiway_markings" in ids
    assert environment["carla_required"] is False


def test_origin_matches_env_actor():
    env = json.loads((PACK / "environment.json").read_text())
    assert abs(env["geo"]["origin"]["lat"] - 41.51722) < 1e-6
    assert abs(env["geo"]["origin"]["lon"] - (-81.68306)) < 1e-6
