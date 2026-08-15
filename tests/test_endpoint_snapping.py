#!/usr/bin/env python3
"""Tests for compile-time junction endpoint snapping in road_network."""

import sys
import time
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools" / "universal-city-compiler"))

from road_network import SNAP_TOLERANCE_M, build_road_graph, _snap_road_endpoints

# 1 m of latitude/longitude at the test site (lat 41.5)
M_LAT = 1.0 / 110540.0
M_LON = 1.0 / (111320.0 * 0.749)  # cos(41.5 deg) ~ 0.749


def _write_osm(tmp_path: Path, body: str) -> Path:
    osm_path = tmp_path / "fixture.osm"
    osm_path.write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<osm version="0.6">\n'
        f"{body}\n"
        "</osm>\n",
        encoding="utf-8",
    )
    return osm_path


def _roads_by_id(graph: dict) -> dict:
    return {r["id"]: r for r in graph["roads"]}


class TestEndpointSnapping:
    def test_clustered_endpoints_snap_and_connect(self, tmp_path):
        # Road B's start sits 0.5 m east of road A's end: a broken junction.
        osm_path = _write_osm(tmp_path, f"""
  <node id="1" lat="41.5000" lon="-81.6900"/>
  <node id="2" lat="41.5010" lon="-81.6900"/>
  <node id="3" lat="{41.5010}" lon="{-81.6900 + 0.5 * M_LON}"/>
  <node id="4" lat="41.5020" lon="-81.6890"/>
  <way id="100">
    <nd ref="1"/><nd ref="2"/>
    <tag k="highway" v="residential"/>
  </way>
  <way id="200">
    <nd ref="3"/><nd ref="4"/>
    <tag k="highway" v="residential"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.5, -81.69)
        roads = _roads_by_id(graph)
        a_end = roads["100"]["points"][-1]
        b_start = roads["200"]["points"][0]
        assert (a_end["lat"], a_end["lon"]) == (b_start["lat"], b_start["lon"])
        # Snapped junction must appear as an intersection connecting both.
        joint = [i for i in graph["intersections"]
                 if "100" in i["road_ids"] and "200" in i["road_ids"]]
        assert len(joint) == 1
        assert joint[0].get("from_snap") is True
        assert joint[0]["layer"] == 0
        report = graph["endpoint_snap"]
        assert report["pairs_snapped"] == 1
        assert report["endpoints_moved"] >= 1
        # All-singleton cluster snaps to the centroid: each endpoint of a
        # 0.5 m near-miss pair moves half the gap.
        assert report["max_distance_moved_m"] == pytest.approx(0.25, abs=0.1)

    def test_beyond_tolerance_pairs_do_not_snap(self, tmp_path):
        # 5 m apart: outside the 1.5 m tolerance.
        osm_path = _write_osm(tmp_path, f"""
  <node id="1" lat="41.5000" lon="-81.6900"/>
  <node id="2" lat="41.5010" lon="-81.6900"/>
  <node id="3" lat="41.5010" lon="{-81.6900 + 5.0 * M_LON}"/>
  <node id="4" lat="41.5020" lon="-81.6890"/>
  <way id="100">
    <nd ref="1"/><nd ref="2"/>
    <tag k="highway" v="residential"/>
  </way>
  <way id="200">
    <nd ref="3"/><nd ref="4"/>
    <tag k="highway" v="residential"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.5, -81.69)
        roads = _roads_by_id(graph)
        assert roads["100"]["points"][-1]["lon"] == -81.6900
        assert roads["200"]["points"][0]["lon"] == pytest.approx(-81.6900 + 5.0 * M_LON)
        assert graph["intersections"] == []
        report = graph["endpoint_snap"]
        assert report["pairs_snapped"] == 0
        assert report["endpoints_moved"] == 0

    def test_cross_layer_pairs_never_snap(self, tmp_path):
        # Bridge endpoint (layer 1) 0.5 m from a surface endpoint (layer 0):
        # within tolerance horizontally, but vertically separated — no snap.
        osm_path = _write_osm(tmp_path, f"""
  <node id="1" lat="41.5000" lon="-81.6900"/>
  <node id="2" lat="41.5010" lon="-81.6900"/>
  <node id="3" lat="41.5010" lon="{-81.6900 + 0.5 * M_LON}"/>
  <node id="4" lat="41.5020" lon="-81.6890"/>
  <way id="100">
    <nd ref="1"/><nd ref="2"/>
    <tag k="highway" v="residential"/>
  </way>
  <way id="200">
    <nd ref="3"/><nd ref="4"/>
    <tag k="highway" v="primary"/>
    <tag k="bridge" v="yes"/>
    <tag k="layer" v="1"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.5, -81.69)
        roads = _roads_by_id(graph)
        assert roads["100"]["points"][-1]["lon"] == -81.6900
        assert roads["200"]["points"][0]["lon"] == pytest.approx(-81.6900 + 0.5 * M_LON)
        assert graph["intersections"] == []
        report = graph["endpoint_snap"]
        assert report["pairs_snapped"] == 0
        assert report["endpoints_moved"] == 0
        assert report["cross_layer_pairs_skipped"] == 1

    def test_exact_matches_do_not_move(self, tmp_path):
        # Roads 100+200 share OSM node 2 (exact junction). Road 300 starts
        # 0.5 m away: it must snap ONTO node 2's coords, and node 2's coords
        # must remain the exact original values.
        osm_path = _write_osm(tmp_path, f"""
  <node id="1" lat="41.5000" lon="-81.6900"/>
  <node id="2" lat="41.5010" lon="-81.6900"/>
  <node id="3" lat="41.5020" lon="-81.6900"/>
  <node id="4" lat="41.5010" lon="{-81.6900 + 0.5 * M_LON}"/>
  <node id="5" lat="41.5010" lon="-81.6880"/>
  <way id="100">
    <nd ref="1"/><nd ref="2"/>
    <tag k="highway" v="residential"/>
  </way>
  <way id="200">
    <nd ref="2"/><nd ref="3"/>
    <tag k="highway" v="residential"/>
  </way>
  <way id="300">
    <nd ref="4"/><nd ref="5"/>
    <tag k="highway" v="residential"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.5, -81.69)
        roads = _roads_by_id(graph)
        # Exact pair untouched:
        assert (roads["100"]["points"][-1]["lat"], roads["100"]["points"][-1]["lon"]) == (41.5010, -81.6900)
        assert (roads["200"]["points"][0]["lat"], roads["200"]["points"][0]["lon"]) == (41.5010, -81.6900)
        # Outlier snapped onto the exact junction:
        assert (roads["300"]["points"][0]["lat"], roads["300"]["points"][0]["lon"]) == (41.5010, -81.6900)
        # The existing OSM-node intersection was augmented, not duplicated:
        joint = [i for i in graph["intersections"] if i["lat"] == 41.5010 and i["lon"] == -81.6900]
        assert len(joint) == 1
        assert set(joint[0]["road_ids"]) == {"100", "200", "300"}
        assert "from_snap" not in joint[0]
        report = graph["endpoint_snap"]
        assert report["intersections_augmented"] == 1
        assert report["intersections_added"] == 0

    def test_snap_report_fields_present(self, tmp_path):
        osm_path = _write_osm(tmp_path, """
  <node id="1" lat="41.5000" lon="-81.6900"/>
  <node id="2" lat="41.5010" lon="-81.6900"/>
  <way id="100">
    <nd ref="1"/><nd ref="2"/>
    <tag k="highway" v="residential"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.5, -81.69)
        report = graph["endpoint_snap"]
        for key in ("enabled", "tolerance_m", "endpoints_total", "pairs_snapped",
                    "cross_layer_pairs_skipped", "clusters_merged",
                    "endpoints_moved", "max_distance_moved_m",
                    "intersections_added", "intersections_augmented"):
            assert key in report
        assert report["enabled"] is True
        assert report["tolerance_m"] == SNAP_TOLERANCE_M
        assert report["endpoints_total"] == 2
        # Untouched single road: nothing snapped, no phantom junctions.
        assert report["endpoints_moved"] == 0
        assert graph["intersections"] == []

    def test_snapping_can_be_disabled(self, tmp_path):
        osm_path = _write_osm(tmp_path, f"""
  <node id="1" lat="41.5000" lon="-81.6900"/>
  <node id="2" lat="41.5010" lon="-81.6900"/>
  <node id="3" lat="41.5010" lon="{-81.6900 + 0.5 * M_LON}"/>
  <node id="4" lat="41.5020" lon="-81.6890"/>
  <way id="100">
    <nd ref="1"/><nd ref="2"/>
    <tag k="highway" v="residential"/>
  </way>
  <way id="200">
    <nd ref="3"/><nd ref="4"/>
    <tag k="highway" v="residential"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.5, -81.69, snap_tolerance_m=0.0)
        assert graph["endpoint_snap"]["enabled"] is False
        assert graph["endpoint_snap"]["endpoints_moved"] == 0
        assert graph["intersections"] == []


class TestSnapPerformance:
    def test_akron_scale_50k_endpoints(self):
        # 25k roads / 50k endpoints arranged as 12.5k road pairs whose joints
        # alternate between exact matches and 0.8 m near-misses; pairs sit
        # 30 m apart so no unintended clustering. Must complete in seconds.
        roads = []
        pairs = 12500
        for k in range(pairs):
            row, col = divmod(k, 112)  # ~3.4 km x 3.4 km extent
            jlat = 41.0 + row * 30 * M_LAT
            jlon = -82.0 + col * 30 * M_LON
            # Even road ends at the junction point; odd road starts there.
            roads.append({
                "id": str(2 * k),
                "points": [
                    {"lat": jlat - 10 * M_LAT, "lon": jlon - 10 * M_LON},
                    {"lat": jlat, "lon": jlon},
                ],
                "layer": 0,
            })
            # Every 4th pair is a 0.8 m near-miss; the rest are exact.
            jitter = 0.8 * M_LAT if k % 4 == 0 else 0.0
            roads.append({
                "id": str(2 * k + 1),
                "points": [
                    {"lat": jlat + jitter, "lon": jlon},
                    {"lat": jlat + 10 * M_LAT + jitter, "lon": jlon + 10 * M_LON},
                ],
                "layer": 1 if k % 11 == 0 else 0,  # some cross-layer joints
            })
        t0 = time.time()
        report = _snap_road_endpoints(roads, SNAP_TOLERANCE_M)
        dt = time.time() - t0
        assert dt < 10.0
        assert report["endpoints_total"] == 4 * pairs  # 2 roads x 2 endpoints per pair
        # ~1/4 of same-layer pairs are near-misses that must snap.
        assert report["pairs_snapped"] > 2000
        # Layer-1 joints were within tolerance but must be skipped.
        assert report["cross_layer_pairs_skipped"] > 0
