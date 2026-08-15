#!/usr/bin/env python3
"""Tests for circuit route loop closure in route_engine."""

import sys
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools" / "universal-city-compiler"))

from route_engine import (
    MAX_ROUTE_LENGTH_M,
    _close_circuit_loop,
    _haversine,
    _route_length,
    _shortest_return_roads,
    generate_routes,
)

# ~1 km of latitude at these test coordinates
KM_LAT = 0.009


def _road(rid, pts, layer=0):
    return {
        "id": rid,
        "name": rid,
        "highway": "residential",
        "points": [{"lat": a, "lon": b} for a, b in pts],
        "width": 7,
        "lane_count": 1,
        "one_way": False,
        "max_speed": 50,
        "surface": "asphalt",
        "layer": layer,
        "elevation_m": layer * 5.0,
        "is_bridge": layer > 0,
        "is_tunnel": layer < 0,
    }


def _graph(roads):
    return {"roads": roads, "intersections": [], "road_count": len(roads)}


def _ring_graph():
    """Four roads forming a ~2.3 km square loop (shared endpoints)."""
    return [
        _road("A", [(41.500, -81.690), (41.505, -81.690)]),
        _road("B", [(41.505, -81.690), (41.505, -81.683)]),
        _road("C", [(41.505, -81.683), (41.500, -81.683)]),
        _road("D", [(41.500, -81.683), (41.500, -81.690)]),
    ]


class TestCircuitLoopClosure:
    def test_simple_loop_closes_exactly(self):
        graph = _graph(_ring_graph())
        routes = generate_routes(graph, "t", mode="circuit", count=1, seed=42)
        assert len(routes) == 1
        route = routes[0]
        closure = route["loop_closure"]
        assert closure["closed"] is True
        assert closure["reason"] == "ok"
        assert closure["residual_gap_m"] == 0.0
        assert _haversine(route["start"], route["finish"]) == 0.0
        assert route["distance_meters"] <= MAX_ROUTE_LENGTH_M

    def test_gap_route_bridged_via_graph_path(self):
        # Line of 3 roads (~1.7 km). Outward runs to the far end; closure must
        # walk back through the graph (road reuse is legal on the return leg).
        roads = [
            _road("A", [(41.500, -81.690), (41.505, -81.690)]),
            _road("B", [(41.505, -81.690), (41.510, -81.690)]),
            _road("C", [(41.510, -81.690), (41.515, -81.690)]),
        ]
        routes = generate_routes(_graph(roads), "t", mode="circuit", count=1, seed=5)
        assert len(routes) == 1
        route = routes[0]
        closure = route["loop_closure"]
        assert closure["closed"] is True
        assert _haversine(route["start"], route["finish"]) == 0.0
        assert route["distance_meters"] <= MAX_ROUTE_LENGTH_M
        # Return leg walks back through intermediate roads (reuse is legal).
        assert "B" in closure["return_path_road_ids"]
        # Route must be longer than the one-way outward path (it came back).
        assert route["distance_meters"] > 1700

    def test_no_path_degrades_with_residual(self):
        start_road = _road("S", [(41.500, -81.690), (41.501, -81.690)])
        x1 = _road("X1", [(41.520, -81.690), (41.521, -81.690)])
        x2 = _road("X2", [(41.521, -81.690), (41.522, -81.690)])
        roads = [start_road, x1, x2]
        # Simulate an outward path that ended in a disconnected cluster.
        route_points = [start_road["points"][0], x1["points"][1], x2["points"][1]]
        segments = [(start_road, 1), (x1, 1), (x2, 1)]

        points, closure = _close_circuit_loop(
            roads, list(route_points), list(segments), start_road, layer=0,
            min_distance=1000.0)

        assert closure["closed"] is False
        assert closure["reason"] == "no_same_layer_path"
        assert closure["residual_gap_m"] > 1000.0  # ~2 km back to start
        # Best-effort: the return leg moved toward the start (onto X1).
        assert "X1" in closure["return_path_road_ids"]
        assert _haversine(points[-1], start_road["points"][0]) > 1000.0

    def test_cross_layer_closure_forbidden(self):
        # S and E (layer 0) are only physically linked by a layer-1 bridge.
        s = _road("S", [(41.500, -81.690), (41.501, -81.690)], layer=0)
        br = _road("BR", [(41.501, -81.690), (41.510, -81.690)], layer=1)
        e = _road("E", [(41.510, -81.690), (41.511, -81.690)], layer=0)
        roads = [s, br, e]
        idx = {r["id"]: i for i, r in enumerate(roads)}

        res = _shortest_return_roads(roads, idx["E"], idx["S"], layer=0,
                                     start_pt=s["points"][0])
        assert res["reached"] is False
        assert "BR" not in [roads[i]["id"] for i in res["path"]]
        assert res["gap_m"] > 500.0

    def test_length_cap_trims_outward_path(self):
        # 14 x ~1 km collinear segments: outward ~14 km, return along the same
        # line would double it past the 15 km cap, so the tail must be trimmed.
        roads = []
        for i in range(14):
            roads.append(_road(f"L{i}", [(41.500 + i * KM_LAT, -81.690),
                                         (41.500 + (i + 1) * KM_LAT, -81.690)]))
        route_points = [roads[0]["points"][0]] + [r["points"][1] for r in roads]
        segments = [(roads[0], 1)] + [(r, 1) for r in roads[1:]]
        outward = _route_length(route_points)
        assert outward > 13000  # sanity: cap is actually at risk

        points, closure = _close_circuit_loop(
            roads, list(route_points), list(segments), roads[0], layer=0,
            min_distance=1000.0)

        assert closure["closed"] is True
        assert closure["trimmed_outward_segments"] >= 1
        total = _route_length(points)
        assert total <= MAX_ROUTE_LENGTH_M
        assert total >= 1000.0
        assert _haversine(points[0], points[-1]) == 0.0

    def test_non_circuit_modes_have_no_closure_metadata(self):
        routes = generate_routes(_graph(_ring_graph()), "t", mode="cruise_sprint",
                                 count=1, seed=42)
        for route in routes:
            assert "loop_closure" not in route
