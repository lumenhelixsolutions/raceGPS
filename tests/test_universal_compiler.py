#!/usr/bin/env python3
"""Tests for the universal city compiler."""

import json
import math
import sys
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools" / "universal-city-compiler"))

from geocode import bounds_from_center, expand_bounds
from road_network import build_road_graph, _parse_maxspeed
from route_engine import generate_routes, place_checkpoints, _haversine
from poi_engine import classify_pois, _classify_tags
from building_extractor import extract_buildings, _estimate_height


class TestGeocode:
    def test_bounds_from_center(self):
        b = bounds_from_center(41.08, -81.52, 5.0)
        assert b["south"] < 41.08 < b["north"]
        assert b["west"] < -81.52 < b["east"]
        # 5km ≈ 0.045 degrees
        assert abs(b["north"] - b["south"]) - 0.09 < 0.01

    def test_expand_bounds(self):
        b = {"south": 40.0, "north": 41.0, "west": -82.0, "east": -81.0}
        e = expand_bounds(b, 10.0)
        assert e["south"] < b["south"]
        assert e["north"] > b["north"]
        assert e["west"] < b["west"]
        assert e["east"] > b["east"]


class TestRoadNetwork:
    def test_parse_maxspeed(self):
        assert _parse_maxspeed("50") == 50
        assert _parse_maxspeed("30 mph") == pytest.approx(48, abs=1)
        assert _parse_maxspeed("none") == 250
        assert _parse_maxspeed("") == 50

    def test_build_road_graph_from_sample(self):
        osm_path = PROJECT_ROOT / "citypacks" / "akron-oh-beta-001" / "akron_raw.osm"
        if not osm_path.exists():
            pytest.skip("akron_raw.osm not available")
        graph = build_road_graph(osm_path, 41.08, -81.52)
        assert graph["road_count"] > 0
        assert "roads" in graph
        assert "intersections" in graph
        assert "bounds" in graph
        # Check road structure
        for road in graph["roads"][:5]:
            assert "id" in road
            assert "points" in road
            assert len(road["points"]) >= 2
            assert "width" in road
            assert "lane_count" in road
            assert "max_speed" in road


class TestRouteEngine:
    def test_haversine(self):
        a = {"lat": 41.08, "lon": -81.52}
        b = {"lat": 41.09, "lon": -81.52}
        dist = _haversine(a, b)
        # ~1.11 km per 0.01 degree lat
        assert 1000 < dist < 1200

    def test_generate_routes_basic(self):
        osm_path = PROJECT_ROOT / "citypacks" / "akron-oh-beta-001" / "akron_raw.osm"
        if not osm_path.exists():
            pytest.skip("akron_raw.osm not available")
        graph = build_road_graph(osm_path, 41.08, -81.52)
        routes = generate_routes(graph, "test_city", mode="cruise_sprint", count=2, seed=42)
        assert isinstance(routes, list)
        for r in routes:
            assert "route_id" in r
            assert "mode" in r
            assert "distance_meters" in r
            assert r["distance_meters"] >= 800
            assert "points" in r
            assert len(r["points"]) >= 2

    def test_place_checkpoints(self):
        route = {
            "route_id": "test_001",
            "points": [
                {"lat": 41.08, "lon": -81.52},
                {"lat": 41.085, "lon": -81.52},
                {"lat": 41.09, "lon": -81.52},
                {"lat": 41.095, "lon": -81.52},
            ]
        }
        cps = place_checkpoints(route, spacing_meters=300.0)
        assert isinstance(cps, list)
        assert len(cps) >= 1
        for cp in cps:
            assert "id" in cp
            assert "lat" in cp
            assert "lon" in cp
            assert "radius_meters" in cp


class TestPOIEngine:
    def test_classify_tags(self):
        assert _classify_tags({"tourism": "museum"}) == "landmark"
        assert _classify_tags({"amenity": "restaurant"}) == "food"
        assert _classify_tags({"amenity": "fuel"}) == "service"
        assert _classify_tags({"shop": "supermarket"}) == "shop"
        assert _classify_tags({"random": "tag"}) is None

    def test_classify_pois_from_sample(self):
        osm_path = PROJECT_ROOT / "citypacks" / "akron-oh-beta-001" / "akron_raw.osm"
        if not osm_path.exists():
            pytest.skip("akron_raw.osm not available")
        pois = classify_pois(osm_path)
        assert isinstance(pois, list)
        for poi in pois[:10]:
            assert "id" in poi
            assert "lat" in poi
            assert "lon" in poi
            assert "type" in poi


class TestBuildingExtractor:
    def test_estimate_height(self):
        assert _estimate_height("yes", "", "") == 8.0
        assert _estimate_height("house", "", "") == 8.0
        assert _estimate_height("skyscraper", "", "") == 120.0
        assert _estimate_height("church", "", "") == 15.0
        assert _estimate_height("", "5", "") == 17.5
        assert _estimate_height("", "", "20m") == 20.0

    def test_extract_buildings_from_sample(self):
        osm_path = PROJECT_ROOT / "citypacks" / "akron-oh-beta-001" / "akron_raw.osm"
        if not osm_path.exists():
            pytest.skip("akron_raw.osm not available")
        buildings = extract_buildings(osm_path, 41.08, -81.52)
        assert isinstance(buildings, list)
        for b in buildings[:5]:
            assert "id" in b
            assert "lat" in b
            assert "lon" in b
            assert "footprint" in b
            assert "height_meters" in b


class TestExportBundle:
    def test_export_bundle_structure(self):
        from export_bundle import export_bundle
        import tempfile
        with tempfile.TemporaryDirectory() as td:
            citypack_dir = Path(td)
            bounds = {"south": 40.0, "north": 41.0, "west": -82.0, "east": -81.0}
            routes = [{"route_id": "test_001", "mode": "cruise_sprint", "start": {"lat": 40.5, "lon": -81.5}, "points": [{"lat": 40.5, "lon": -81.5}, {"lat": 40.6, "lon": -81.5}]}]
            pois = [{"id": "p1", "lat": 40.5, "lon": -81.5, "type": "landmark", "name": "Test"}]
            buildings = [{"id": "b1", "lat": 40.5, "lon": -81.5, "footprint": [], "height_meters": 10.0}]
            road_graph = {"road_count": 5, "intersection_count": 3}

            manifest = export_bundle(citypack_dir, "test_city", bounds, routes, pois, buildings, road_graph)
            assert manifest["city_id"] == "test_city"
            assert manifest["route_count"] == 1
            assert manifest["poi_count"] == 1
            assert manifest["building_count"] == 1
            assert "files" in manifest
            assert (citypack_dir / "test_city_semantic_manifest.json").exists()
