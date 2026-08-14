#!/usr/bin/env python3
"""Tests for OSM bridge/tunnel/layer handling in the universal city compiler."""

import sys
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools" / "universal-city-compiler"))

from road_network import LAYER_HEIGHT_M, build_road_graph, _parse_layer


def _write_osm(tmp_path: Path, body: str) -> Path:
    """Write a minimal OSM XML fixture and return its path."""
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


class TestParseLayer:
    def test_defaults_to_zero(self):
        assert _parse_layer({}) == 0
        assert _parse_layer({"bridge": "no"}) == 0
        assert _parse_layer({"tunnel": "no"}) == 0

    def test_bridge_and_tunnel_inference(self):
        assert _parse_layer({"bridge": "yes"}) == 1
        assert _parse_layer({"bridge": "viaduct"}) == 1
        assert _parse_layer({"tunnel": "yes"}) == -1
        assert _parse_layer({"tunnel": "culvert"}) == -1

    def test_explicit_layer_tag_wins(self):
        assert _parse_layer({"layer": "2"}) == 2
        assert _parse_layer({"layer": "-2"}) == -2
        assert _parse_layer({"bridge": "yes", "layer": "3"}) == 3
        assert _parse_layer({"tunnel": "yes", "layer": "-1"}) == -1

    def test_invalid_layer_tag_falls_back(self):
        assert _parse_layer({"layer": "high"}) == 0
        assert _parse_layer({"layer": "high", "bridge": "yes"}) == 1


class TestBridgeElevation:
    def test_bridge_way_gets_elevation(self, tmp_path):
        osm_path = _write_osm(tmp_path, """
  <node id="1" lat="41.4993" lon="-81.6944"/>
  <node id="2" lat="41.5003" lon="-81.6944"/>
  <node id="3" lat="41.5013" lon="-81.6944"/>
  <way id="100">
    <nd ref="1"/><nd ref="2"/><nd ref="3"/>
    <tag k="highway" v="primary"/>
    <tag k="bridge" v="yes"/>
    <tag k="layer" v="1"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.5, -81.69)
        road = _roads_by_id(graph)["100"]
        assert road["is_bridge"] is True
        assert road["is_tunnel"] is False
        assert road["layer"] == 1
        assert road["elevation_m"] == pytest.approx(LAYER_HEIGHT_M)

    def test_bridge_without_layer_tag_implies_layer_1(self, tmp_path):
        osm_path = _write_osm(tmp_path, """
  <node id="1" lat="41.4993" lon="-81.6944"/>
  <node id="2" lat="41.5003" lon="-81.6944"/>
  <way id="100">
    <nd ref="1"/><nd ref="2"/>
    <tag k="highway" v="primary"/>
    <tag k="bridge" v="viaduct"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.5, -81.69)
        road = _roads_by_id(graph)["100"]
        assert road["is_bridge"] is True
        assert road["layer"] == 1
        assert road["elevation_m"] == pytest.approx(LAYER_HEIGHT_M)

    def test_tunnel_way_gets_negative_elevation(self, tmp_path):
        osm_path = _write_osm(tmp_path, """
  <node id="1" lat="41.4993" lon="-81.6944"/>
  <node id="2" lat="41.5003" lon="-81.6944"/>
  <way id="200">
    <nd ref="1"/><nd ref="2"/>
    <tag k="highway" v="secondary"/>
    <tag k="tunnel" v="yes"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.5, -81.69)
        road = _roads_by_id(graph)["200"]
        assert road["is_tunnel"] is True
        assert road["is_bridge"] is False
        assert road["layer"] == -1
        assert road["elevation_m"] == pytest.approx(-LAYER_HEIGHT_M)

    def test_explicit_multi_level_layer(self, tmp_path):
        osm_path = _write_osm(tmp_path, """
  <node id="1" lat="41.4993" lon="-81.6944"/>
  <node id="2" lat="41.5003" lon="-81.6944"/>
  <way id="300">
    <nd ref="1"/><nd ref="2"/>
    <tag k="highway" v="motorway"/>
    <tag k="layer" v="2"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.5, -81.69)
        road = _roads_by_id(graph)["300"]
        assert road["layer"] == 2
        assert road["elevation_m"] == pytest.approx(2 * LAYER_HEIGHT_M)


class TestLayeredCrossings:
    def test_different_layer_crossing_does_not_junction(self, tmp_path):
        # Bridge (layer 1) and surface street (layer 0) share node 2 in OSM
        # but do not physically connect.
        osm_path = _write_osm(tmp_path, """
  <node id="1" lat="41.4993" lon="-81.6944"/>
  <node id="2" lat="41.5003" lon="-81.6944"/>
  <node id="3" lat="41.5013" lon="-81.6944"/>
  <node id="4" lat="41.5003" lon="-81.6954"/>
  <node id="5" lat="41.5003" lon="-81.6934"/>
  <way id="100">
    <nd ref="1"/><nd ref="2"/><nd ref="3"/>
    <tag k="highway" v="primary"/>
    <tag k="bridge" v="yes"/>
    <tag k="layer" v="1"/>
  </way>
  <way id="200">
    <nd ref="4"/><nd ref="2"/><nd ref="5"/>
    <tag k="highway" v="residential"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.5, -81.69)
        assert graph["road_count"] == 2
        for inter in graph["intersections"]:
            assert not ("100" in inter["road_ids"] and "200" in inter["road_ids"])
        # No valid same-layer grouping exists at node 2 -> no intersection
        assert graph["intersection_count"] == 0

    def test_same_layer_crossing_still_junctions(self, tmp_path):
        osm_path = _write_osm(tmp_path, """
  <node id="1" lat="41.4993" lon="-81.6944"/>
  <node id="2" lat="41.5003" lon="-81.6944"/>
  <node id="3" lat="41.5013" lon="-81.6944"/>
  <node id="4" lat="41.5003" lon="-81.6954"/>
  <node id="5" lat="41.5003" lon="-81.6934"/>
  <way id="100">
    <nd ref="1"/><nd ref="2"/><nd ref="3"/>
    <tag k="highway" v="primary"/>
  </way>
  <way id="200">
    <nd ref="4"/><nd ref="2"/><nd ref="5"/>
    <tag k="highway" v="residential"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.5, -81.69)
        assert graph["intersection_count"] == 1
        inter = graph["intersections"][0]
        assert inter["node_id"] == "2"
        assert set(inter["road_ids"]) == {"100", "200"}
        assert inter["layer"] == 0

    def test_bridge_deck_junction_at_same_layer(self, tmp_path):
        # Two bridge segments meeting on the same elevated deck DO junction.
        osm_path = _write_osm(tmp_path, """
  <node id="1" lat="41.4993" lon="-81.6944"/>
  <node id="2" lat="41.5003" lon="-81.6944"/>
  <node id="3" lat="41.5013" lon="-81.6944"/>
  <node id="4" lat="41.5003" lon="-81.6954"/>
  <way id="100">
    <nd ref="1"/><nd ref="2"/><nd ref="3"/>
    <tag k="highway" v="primary"/>
    <tag k="bridge" v="yes"/>
    <tag k="layer" v="1"/>
  </way>
  <way id="200">
    <nd ref="4"/><nd ref="2"/>
    <tag k="highway" v="primary_link"/>
    <tag k="bridge" v="yes"/>
    <tag k="layer" v="1"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.5, -81.69)
        assert graph["intersection_count"] == 1
        inter = graph["intersections"][0]
        assert set(inter["road_ids"]) == {"100", "200"}
        assert inter["layer"] == 1


class TestBackwardCompatibility:
    def test_missing_tags_default_to_ground_level(self, tmp_path):
        osm_path = _write_osm(tmp_path, """
  <node id="1" lat="41.0814" lon="-81.5190"/>
  <node id="2" lat="41.0824" lon="-81.5190"/>
  <way id="100">
    <nd ref="1"/><nd ref="2"/>
    <tag k="highway" v="residential"/>
    <tag k="name" v="Main St"/>
  </way>""")
        graph = build_road_graph(osm_path, 41.08, -81.52)
        road = _roads_by_id(graph)["100"]
        assert road["layer"] == 0
        assert road["elevation_m"] == 0.0
        assert road["is_bridge"] is False
        assert road["is_tunnel"] is False

    def test_akron_citypack_all_ground_level(self):
        osm_path = PROJECT_ROOT / "citypacks" / "akron-oh-beta-001" / "akron_raw.osm"
        if not osm_path.exists():
            pytest.skip("akron_raw.osm not available")
        graph = build_road_graph(osm_path, 41.08, -81.52)
        assert graph["road_count"] > 0
        for road in graph["roads"]:
            assert road["layer"] == 0
            assert road["elevation_m"] == 0.0
            assert "is_bridge" in road
            assert "is_tunnel" in road
        # Existing intersection behavior is preserved for untagged data.
        assert graph["intersection_count"] > 0
        for inter in graph["intersections"]:
            assert inter["layer"] == 0
