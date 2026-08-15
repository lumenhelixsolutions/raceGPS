#!/usr/bin/env python3
"""Tests for water extraction (water_extractor.py) and the water clauses in the
Overpass query builder (fetch_overpass._build_query).

Uses synthetic OSM XML fixtures plus an optional integration pass against the
cached Cleveland extract (skips gracefully when the cache is absent).
"""

import sys
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools" / "universal-city-compiler"))

from water_extractor import extract_water, _join_rings
from fetch_overpass import _build_query

CLEVELAND_CACHE = PROJECT_ROOT / "citypacks" / "cleveland_5.0km" / "cleveland_5.0km_raw.osm"

BBOX = (41.37, -81.90, 41.62, -81.51)  # south, west, north, east


def _osm(body: str) -> str:
    return f'<?xml version="1.0" encoding="UTF-8"?>\n<osm version="0.6">\n{body}\n</osm>\n'


def _write(tmp_path: Path, body: str) -> Path:
    p = tmp_path / "fixture.osm"
    p.write_text(_osm(body), encoding="utf-8")
    return p


# --- Synthetic fixtures -----------------------------------------------------

SQUARE_NODES = """
  <node id="1" lat="41.50" lon="-81.70"/>
  <node id="2" lat="41.50" lon="-81.69"/>
  <node id="3" lat="41.51" lon="-81.69"/>
  <node id="4" lat="41.51" lon="-81.70"/>
"""

RIVERBANK_WAY = SQUARE_NODES + """
  <way id="100">
    <nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="4"/><nd ref="1"/>
    <tag k="waterway" v="riverbank"/>
    <tag k="name" v="Test River"/>
  </way>
"""

LAKE_WAY = SQUARE_NODES + """
  <way id="101">
    <nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="4"/><nd ref="1"/>
    <tag k="natural" v="water"/>
    <tag k="water" v="lake"/>
    <tag k="name" v="Test Lake"/>
  </way>
"""

STREAM_WAY = """
  <node id="1" lat="41.50" lon="-81.70"/>
  <node id="2" lat="41.51" lon="-81.70"/>
  <node id="3" lat="41.52" lon="-81.71"/>
  <way id="102">
    <nd ref="1"/><nd ref="2"/><nd ref="3"/>
    <tag k="waterway" v="stream"/>
    <tag k="name" v="Test Creek"/>
  </way>
"""

COASTLINE_WAY = """
  <node id="1" lat="41.55" lon="-81.80"/>
  <node id="2" lat="41.56" lon="-81.79"/>
  <node id="3" lat="41.57" lon="-81.78"/>
  <way id="103">
    <nd ref="1"/><nd ref="2"/><nd ref="3"/>
    <tag k="natural" v="coastline"/>
  </way>
"""

# Multipolygon river: ring split across two untagged member ways.
RIVER_RELATION = SQUARE_NODES + """
  <way id="110"><nd ref="1"/><nd ref="2"/><nd ref="3"/></way>
  <way id="111"><nd ref="3"/><nd ref="4"/><nd ref="1"/></way>
  <relation id="200">
    <member type="way" ref="110" role="outer"/>
    <member type="way" ref="111" role="outer"/>
    <tag k="type" v="multipolygon"/>
    <tag k="natural" v="water"/>
    <tag k="water" v="river"/>
    <tag k="name" v="Relation River"/>
  </relation>
"""

NO_WATER = SQUARE_NODES + """
  <way id="300">
    <nd ref="1"/><nd ref="2"/><nd ref="3"/>
    <tag k="highway" v="residential"/>
  </way>
"""


# --- Extractor tests --------------------------------------------------------

class TestWaterExtractor:
    def test_riverbank_polygon_extracted(self, tmp_path):
        water = extract_water(_write(tmp_path, RIVERBANK_WAY))
        assert water["river_polygon_count"] == 1
        poly = water["river_polygons"][0]
        assert poly["name"] == "Test River"
        assert poly["type"] == "riverbank"
        assert poly["closed"] is True
        assert len(poly["points"]) == 5
        assert poly["area_approx_m2"] > 0

    def test_lake_polygon_extracted(self, tmp_path):
        water = extract_water(_write(tmp_path, LAKE_WAY))
        assert water["lake_count"] == 1
        lake = water["lakes"][0]
        assert lake["name"] == "Test Lake"
        assert lake["type"] == "lake"
        assert lake["area_approx_m2"] > 0

    def test_stream_polyline_extracted(self, tmp_path):
        water = extract_water(_write(tmp_path, STREAM_WAY))
        assert water["river_count"] == 1
        stream = water["rivers"][0]
        assert stream["type"] == "stream"
        assert len(stream["points"]) == 3
        assert stream["points"][0] == {"lat": 41.50, "lon": -81.70}

    def test_coastline_extracted(self, tmp_path):
        water = extract_water(_write(tmp_path, COASTLINE_WAY))
        assert water["coastline_count"] == 1
        coast = water["coastlines"][0]
        assert len(coast["points"]) == 3

    def test_multipolygon_relation_assembled(self, tmp_path):
        water = extract_water(_write(tmp_path, RIVER_RELATION))
        assert water["river_polygon_count"] == 1
        poly = water["river_polygons"][0]
        assert poly["source"] == "relation"
        assert poly["name"] == "Relation River"
        assert poly["closed"] is True
        # Ring reassembled from two member ways: 5 coords incl. closure
        assert len(poly["points"]) == 5
        assert poly["area_approx_m2"] > 0

    def test_no_water_input_empty_but_valid(self, tmp_path):
        water = extract_water(_write(tmp_path, NO_WATER))
        assert water["rivers"] == []
        assert water["river_polygons"] == []
        assert water["lakes"] == []
        assert water["coastlines"] == []
        assert water["river_count"] == 0
        assert water["river_polygon_count"] == 0
        assert water["lake_count"] == 0
        assert water["coastline_count"] == 0
        assert water["water_count"] == 0

    def test_counts_are_consistent(self, tmp_path):
        water = extract_water(_write(
            tmp_path, RIVERBANK_WAY + LAKE_WAY.replace(SQUARE_NODES, "")
            + STREAM_WAY.replace(SQUARE_NODES, "") + COASTLINE_WAY.replace(SQUARE_NODES, "")
        ))
        assert water["water_count"] == (
            water["river_count"] + water["river_polygon_count"]
            + water["lake_count"] + water["coastline_count"]
        )


class TestRingJoining:
    def test_joins_split_ring(self):
        rings = _join_rings([["1", "2", "3"], ["3", "4", "1"]])
        assert rings == [["1", "2", "3", "4", "1"]]

    def test_joins_reversed_segment(self):
        rings = _join_rings([["1", "2", "3"], ["1", "4", "3"]])
        assert len(rings) == 1
        assert rings[0][0] == rings[0][-1]

    def test_open_chain_kept_as_single_ring(self):
        rings = _join_rings([["1", "2"], ["4", "5"]])
        assert len(rings) == 2


# --- Query builder tests ----------------------------------------------------

class TestQueryBuilderWaterClauses:
    @pytest.mark.parametrize("detail", ["standard", "full"])
    def test_water_clauses_present(self, detail):
        q = _build_query(*BBOX, detail=detail)
        assert 'way["natural"="water"]' in q or '"natural"~"' in q
        assert '"waterway"~"' in q
        assert "riverbank" in q
        assert 'way["natural"="coastline"]' in q
        assert 'relation["natural"="water"]' in q
        assert 'relation["waterway"="riverbank"]' in q

    def test_minimal_tier_stays_roads_only(self):
        q = _build_query(*BBOX, detail="minimal")
        assert "waterway" not in q
        assert "coastline" not in q
        assert '"natural"' not in q

    def test_road_clauses_untouched(self):
        for detail in ("minimal", "standard", "full"):
            q = _build_query(*BBOX, detail=detail)
            assert '"highway"~"' in q


# --- Cleveland cache integration --------------------------------------------

class TestClevelandCache:
    def test_extraction_runs_on_cache(self):
        if not CLEVELAND_CACHE.exists():
            pytest.skip("Cleveland OSM cache not available")
        water = extract_water(CLEVELAND_CACHE, 41.4975, -81.7059)
        # Structural validity regardless of whether the cache predates the
        # water fetch fix (old 'standard' caches contain no water tags).
        for key in ("rivers", "river_polygons", "lakes", "coastlines"):
            assert isinstance(water[key], list)
        assert water["water_count"] == (
            water["river_count"] + water["river_polygon_count"]
            + water["lake_count"] + water["coastline_count"]
        )
        print(f"\nCleveland cache water counts: rivers={water['river_count']} "
              f"river_polygons={water['river_polygon_count']} lakes={water['lake_count']} "
              f"coastlines={water['coastline_count']}")
