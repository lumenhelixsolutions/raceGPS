"""Tests for tools/validate-citypack.py extended checks (S14).

Covers: near-miss junction detection, snapped connectivity, route validation
(length bounds / on-network / loop closure), road geometry sanity, and
backward compatibility of the CLI (exit-code semantics, existing checks).
"""

import importlib.util
import json
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
VALIDATOR = ROOT / 'tools' / 'validate-citypack.py'
AKRON = ROOT / 'citypacks' / 'akron-oh-beta-001'

spec = importlib.util.spec_from_file_location('validate_citypack', VALIDATOR)
vc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vc)


# ---------------------------------------------------------------------------
# Helpers / fixtures
# ---------------------------------------------------------------------------

def make_road(rid, pts):
    """Road-graph style road: pts is a list of (lat, lon) tuples."""
    return {'id': rid, 'name': rid,
            'points': [{'lat': a, 'lon': b} for a, b in pts]}


def run_cli(citypack_dir, extra_args=()):
    return subprocess.run(
        [sys.executable, str(VALIDATOR), str(citypack_dir), *extra_args],
        capture_output=True, text=True, timeout=300)


def write_minimal_pack(d, roads_latlon, routes=None, xodr_ok=True):
    """Write a minimal citypack.json + city.xodr pack into dir `d`.

    roads_latlon: list of (road_id, [(lat, lon), ...]).
    """
    roads = [{'id': rid, 'coordinates': [[a, b] for a, b in pts]}
             for rid, pts in roads_latlon]
    cp = {
        'name': 'test-pack', 'version': '1.0', 'generated_at': '2025-01-01',
        'format': 'racegps-citypack',
        'statistics': {'roads': len(roads), 'buildings': 0, 'pois': 0,
                       'junctions': 0},
        'roads': roads, 'buildings': [], 'pois': [], 'routes': routes or [],
    }
    (d / 'citypack.json').write_text(json.dumps(cp))
    if xodr_ok:
        (d / 'city.xodr').write_text(
            '<OpenDRIVE><road id="1" length="10.0"/></OpenDRIVE>')
    return d


# ---------------------------------------------------------------------------
# Near-miss junction detection + snapped connectivity
# ---------------------------------------------------------------------------

class TestNearMissJunctions:
    def test_near_miss_pair_flagged_and_snapped(self):
        # End of A and start of B are ~1.1 m apart (1e-5 deg lat).
        roads = [
            make_road('A', [(41.0, -81.0), (41.001, -81.0)]),
            make_road('B', [(41.00101, -81.0), (41.002, -81.0)]),
        ]
        res = vc.check_connectivity_snapped(roads, tolerance_m=2.0)
        assert res['near_miss_count'] == 1
        assert res['near_miss_samples'][0][0:2] == ('A', 'B')
        assert res['snapped_components'] == 1      # snap connects them
        assert not res['exact_connected']          # exact matching does not

    def test_far_gap_is_not_near_miss(self):
        roads = [
            make_road('A', [(41.0, -81.0), (41.001, -81.0)]),
            make_road('B', [(41.005, -81.0), (41.006, -81.0)]),
        ]
        res = vc.check_connectivity_snapped(roads, tolerance_m=2.0)
        assert res['near_miss_count'] == 0
        assert res['snapped_components'] == 2

    def test_exact_shared_endpoint_is_not_near_miss(self):
        shared = (41.001, -81.0)
        roads = [
            make_road('A', [(41.0, -81.0), shared]),
            make_road('B', [shared, (41.002, -81.0)]),
        ]
        res = vc.check_connectivity_snapped(roads, tolerance_m=2.0)
        assert res['near_miss_count'] == 0
        assert res['snapped_components'] == 1
        assert res['exact_connected']

    def test_empty_roads(self):
        res = vc.check_connectivity_snapped([], tolerance_m=2.0)
        assert res['near_miss_count'] == 0
        assert res['snapped_nodes'] == 0


class TestIntersectionConnectivity:
    INTERSECTIONS = [
        {'node_id': 'n1', 'lat': 41.0, 'lon': -81.0, 'road_ids': ['A', 'B']},
        {'node_id': 'n2', 'lat': 41.001, 'lon': -81.0, 'road_ids': ['B', 'C']},
    ]

    def test_connected_via_intersections(self):
        roads = [make_road(r, [(41.0, -81.0), (41.001, -81.0)])
                 for r in ('A', 'B', 'C')]
        res = vc.check_intersection_connectivity(roads, self.INTERSECTIONS)
        assert res['components'] == 1
        assert res['roads'] == 3
        assert res['roads_not_in_any_intersection'] == 0

    def test_fragmented_graph(self):
        roads = [make_road(r, [(41.0, -81.0), (41.001, -81.0)])
                 for r in ('A', 'B', 'C', 'D')]
        res = vc.check_intersection_connectivity(roads, self.INTERSECTIONS)
        assert res['components'] == 2          # {A,B,C} and orphan D
        assert res['roads_not_in_any_intersection'] == 1
        assert res['isolated_samples'] == ['D']


# ---------------------------------------------------------------------------
# Route validation
# ---------------------------------------------------------------------------

class TestRoutes:
    def _route(self, rid='r1', **kw):
        base = {'route_id': rid, 'name': rid, 'mode': 'cruise_sprint',
                'distance_meters': 5000,
                'points': [{'lat': 41.0, 'lon': -81.0},
                           {'lat': 41.01, 'lon': -81.0}]}
        base.update(kw)
        return base

    def test_valid_route_passes(self):
        errors, warnings, _ = vc.check_routes([self._route()], None)
        assert errors == [] and warnings == []

    def test_too_short_route_is_error(self):
        errors, _, _ = vc.check_routes(
            [self._route(rid='short', distance_meters=500)], None)
        assert any('below minimum' in e and 'short' in e for e in errors)

    def test_too_long_route_is_error(self):
        errors, _, _ = vc.check_routes(
            [self._route(rid='long', distance_meters=20000)], None)
        assert any('above maximum' in e and 'long' in e for e in errors)

    def test_boundary_lengths_pass(self):
        errors, _, _ = vc.check_routes(
            [self._route(rid='lo', distance_meters=1000),
             self._route(rid='hi', distance_meters=15000)], None)
        assert errors == []

    def test_length_computed_from_points_when_missing(self):
        # ~1.11 km straight line -> within bounds, no error.
        route = self._route(rid='nodist')
        del route['distance_meters']
        errors, _, _ = vc.check_routes([route], None)
        assert errors == []

    def test_route_off_network_warns(self):
        road_pts = [(41.0 + i * 0.0005, -81.0) for i in range(20)]
        index = vc.SpatialIndex(road_pts, 30.0)
        # Route offset ~84 m east (0.001 deg lon at lat 41) -> off network.
        route = self._route(rid='off', points=[
            {'lat': 41.0 + i * 0.0005, 'lon': -80.999} for i in range(10)])
        errors, warnings, _ = vc.check_routes([route], index)
        assert errors == []
        assert any('from road network' in w and 'off' in w for w in warnings)

    def test_route_on_network_no_warning(self):
        road_pts = [(41.0 + i * 0.0005, -81.0) for i in range(20)]
        index = vc.SpatialIndex(road_pts, 30.0)
        route = self._route(rid='on', points=[
            {'lat': 41.0 + i * 0.0005, 'lon': -81.0} for i in range(10)])
        _, warnings, _ = vc.check_routes([route], index)
        assert warnings == []

    def test_loop_route_with_gap_warns(self):
        route = self._route(rid='lp', mode='loop', points=[
            {'lat': 41.0, 'lon': -81.0}, {'lat': 41.05, 'lon': -81.0}])
        _, warnings, _ = vc.check_routes([route], None)
        assert any('loop' in w for w in warnings)

    def test_closed_loop_route_passes(self):
        route = self._route(rid='lp', mode='circuit', points=[
            {'lat': 41.0, 'lon': -81.0}, {'lat': 41.01, 'lon': -81.0},
            {'lat': 41.0, 'lon': -81.0}])
        errors, warnings, _ = vc.check_routes([route], None)
        assert errors == [] and warnings == []

    def test_sprint_route_gap_is_fine(self):
        # Non-loop modes are allowed distant start/finish.
        route = self._route(rid='sp', mode='cruise_sprint', points=[
            {'lat': 41.0, 'lon': -81.0}, {'lat': 41.05, 'lon': -81.0}])
        _, warnings, _ = vc.check_routes([route], None)
        assert warnings == []


# ---------------------------------------------------------------------------
# Road geometry sanity
# ---------------------------------------------------------------------------

class TestRoadGeometry:
    def test_duplicate_consecutive_points_warn(self):
        road = make_road('dup', [(41.0, -81.0), (41.0005, -81.0),
                                 (41.0005, -81.0), (41.001, -81.0)])
        errors, warnings, stats = vc.check_road_geometry([road])
        assert errors == []
        assert stats['duplicate_points'] == 1
        assert stats['duplicate_point_roads'] == 1
        assert any('duplicate' in w for w in warnings)

    def test_zero_length_segment_warn(self):
        # Distinct but ~0.0001 m apart (< 0.01 m epsilon).
        road = make_road('zero', [(41.0, -81.0), (41.000000001, -81.0),
                                  (41.001, -81.0)])
        errors, warnings, stats = vc.check_road_geometry([road])
        assert errors == []
        assert stats['zero_length_segments'] == 1
        assert any('zero-length' in w for w in warnings)

    def test_out_of_range_coordinate_is_error(self):
        road = make_road('bad', [(95.0, -81.0), (41.0, -81.0)])
        errors, _, stats = vc.check_road_geometry([road])
        assert stats['out_of_range_coords'] == 1
        assert any('bad' in e and 'lat/lon' in e for e in errors)

    def test_out_of_bounds_warns(self):
        bounds = (-81.01, 40.99, -80.99, 41.01)  # west, south, east, north
        road = make_road('oob', [(41.5, -81.0), (41.6, -81.0)])
        errors, warnings, stats = vc.check_road_geometry([road], bounds=bounds)
        assert errors == []
        assert stats['out_of_bounds_roads'] == 1
        assert any('bounds' in w for w in warnings)

    def test_degenerate_road_warns(self):
        # Three distinct points, total length < 0.5 m, each segment > epsilon.
        road = make_road('deg', [(41.0, -81.0), (41.000001, -81.0),
                                 (41.000002, -81.0)])
        _, warnings, stats = vc.check_road_geometry([road])
        assert stats['degenerate_roads'] == 1
        assert stats['duplicate_points'] == 0
        assert stats['zero_length_segments'] == 0
        assert any('degenerate' in w for w in warnings)

    def test_sparse_polyline_warns(self):
        road = make_road('sparse', [(41.0, -81.0), (41.0, -80.0)])  # ~84 km gap
        _, warnings, stats = vc.check_road_geometry([road])
        assert stats['sparse_roads'] == 1
        assert any('sparse' in w.lower() or 'gap' in w for w in warnings)

    def test_clean_roads_no_issues(self):
        roads = [make_road('ok1', [(41.0, -81.0), (41.0005, -81.0),
                                   (41.001, -81.0)]),
                 make_road('ok2', [(41.001, -81.0), (41.0015, -81.0)])]
        errors, warnings, stats = vc.check_road_geometry(roads)
        assert errors == [] and warnings == []
        assert stats['roads_checked'] == 2


# ---------------------------------------------------------------------------
# Extract points / haversine unit sanity
# ---------------------------------------------------------------------------

class TestHelpers:
    def test_extract_points_both_shapes(self):
        r1 = {'points': [{'lat': 41.0, 'lon': -81.0}, {'lat': 41.1, 'lon': -81.0}]}
        r2 = {'coordinates': [[41.0, -81.0], [41.1, -81.0]]}
        assert vc.extract_points(r1) == vc.extract_points(r2) == \
            [(41.0, -81.0), (41.1, -81.0)]

    def test_haversine_known_distance(self):
        # 0.01 deg latitude ~ 1111.95 m.
        d = vc.haversine_m((41.0, -81.0), (41.01, -81.0))
        assert abs(d - 1111.95) < 2.0


# ---------------------------------------------------------------------------
# CLI backward compatibility
# ---------------------------------------------------------------------------

class TestCliBackwardCompat:
    CONNECTED = [('A', [(41.0, -81.0), (41.001, -81.0)]),
                 ('B', [(41.001, -81.0), (41.002, -81.0)])]

    def test_valid_pack_passes_exit_0(self, tmp_path):
        write_minimal_pack(tmp_path, self.CONNECTED)
        r = run_cli(tmp_path)
        assert r.returncode == 0, r.stdout + r.stderr
        assert 'Validation passed.' in r.stdout
        assert 'Check summary' in r.stdout

    def test_existing_schema_error_still_fails(self, tmp_path):
        # Road with a single coordinate violates the pre-existing min-2 check.
        write_minimal_pack(tmp_path, [('A', [(41.0, -81.0)])])
        r = run_cli(tmp_path)
        assert r.returncode == 1
        assert 'fewer than 2 coordinates' in r.stdout
        assert 'ERRORS' in r.stdout

    def test_missing_files_still_fail(self, tmp_path):
        r = run_cli(tmp_path)
        assert r.returncode == 1
        assert 'citypack.json not found' in r.stdout
        assert 'city.xodr not found' in r.stdout

    def test_missing_directory_fails(self, tmp_path):
        r = run_cli(tmp_path / 'nope')
        assert r.returncode == 1
        assert 'Not a directory' in r.stderr

    def test_disconnected_exact_graph_still_fails(self, tmp_path):
        write_minimal_pack(tmp_path,
                           [('A', [(41.0, -81.0), (41.001, -81.0)]),
                            ('B', [(41.005, -81.0), (41.006, -81.0)])])
        r = run_cli(tmp_path)
        assert r.returncode == 1
        assert 'not fully connected' in r.stdout

    def test_warnings_alone_do_not_fail(self, tmp_path):
        # Connected graph, but road A carries duplicate consecutive points.
        write_minimal_pack(tmp_path,
                           [('A', [(41.0, -81.0), (41.0005, -81.0),
                                   (41.0005, -81.0), (41.001, -81.0)]),
                            ('B', [(41.001, -81.0), (41.002, -81.0)])])
        r = run_cli(tmp_path)
        assert r.returncode == 0, r.stdout + r.stderr
        assert 'WARNINGS' in r.stdout
        assert 'Validation passed.' in r.stdout

    def test_near_miss_pack_warns_and_fails_exact(self, tmp_path):
        # ~1.1 m gap: exact connectivity fails (pre-existing error semantics)
        # and the new near-miss warning explains why.
        write_minimal_pack(tmp_path,
                           [('A', [(41.0, -81.0), (41.001, -81.0)]),
                            ('B', [(41.00101, -81.0), (41.002, -81.0)])])
        r = run_cli(tmp_path)
        assert r.returncode == 1
        assert 'Near-miss junctions: 1' in r.stdout
        assert 'near-miss' in r.stdout

    def test_summary_counts_present(self, tmp_path):
        write_minimal_pack(tmp_path, self.CONNECTED)
        r = run_cli(tmp_path)
        assert 'Check summary' in r.stdout
        assert 'TOTAL' in r.stdout
        for cat in ('schema', 'xodr', 'connectivity', 'geometry'):
            assert cat in r.stdout


# ---------------------------------------------------------------------------
# Real Akron citypack (integration; skip gracefully if absent)
# ---------------------------------------------------------------------------

@pytest.mark.skipif(not AKRON.is_dir(), reason='Akron citypack not present on disk')
def test_akron_citypack_runs_and_reports():
    r = run_cli(AKRON)
    assert r.returncode in (0, 1)
    assert 'Check summary' in r.stdout
    assert 'Near-miss junctions:' in r.stdout
    assert 'Road geometry' in r.stdout
    assert 'Routes: 2 checked' in r.stdout
    assert 'ERRORS' in r.stdout or 'Validation passed' in r.stdout


# ---------------------------------------------------------------------------
# Manifest-named packs (docs/CITYPACK_CONTRACT.md dialects)
# ---------------------------------------------------------------------------

class TestManifestNamedPacks:
    """Packs that ship *_semantic_manifest.json + <city>.xodr / road-graph
    instead of citypack.json / city.xodr."""

    ROADS = [
        {'id': 'A', 'name': 'A',
         'points': [{'lat': 41.0, 'lon': -81.0}, {'lat': 41.001, 'lon': -81.0}]},
        {'id': 'B', 'name': 'B',
         'points': [{'lat': 41.001, 'lon': -81.0}, {'lat': 41.002, 'lon': -81.0}]},
    ]
    INTERSECTIONS = [
        {'node_id': 'n1', 'lat': 41.001, 'lon': -81.0, 'road_ids': ['A', 'B']},
    ]

    def _write_manifest(self, d, city='testville'):
        (d / f'{city}_semantic_manifest.json').write_text(json.dumps({
            'city_id': city, 'display_name': 'Testville', 'version': '1.0',
            'bounds': {'west': -81.01, 'south': 40.99,
                       'east': -80.99, 'north': 41.01},
        }))

    def _write_road_graph(self, d, city='testville'):
        (d / f'{city}_road_graph.json').write_text(json.dumps({
            'roads': self.ROADS, 'intersections': self.INTERSECTIONS,
            'node_count': 4, 'road_count': 2, 'intersection_count': 1,
        }))

    def test_manifest_and_named_xodr_satisfy_requirements(self, tmp_path):
        # Dialect A: manifest + <city>.xodr + road graph -> no file errors.
        self._write_manifest(tmp_path)
        self._write_road_graph(tmp_path)
        (tmp_path / 'testville.xodr').write_text(
            '<OpenDRIVE><road id="1" length="10.0"/></OpenDRIVE>')
        r = run_cli(tmp_path)
        assert r.returncode == 0, r.stdout + r.stderr
        assert 'citypack.json not found' not in r.stdout
        assert 'city.xodr not found' not in r.stdout
        assert 'Validation passed.' in r.stdout

    def test_road_graph_fallback_downgrades_missing_xodr(self, tmp_path):
        # Dialect B: manifest + road graph, no XODR at all -> warning only.
        self._write_manifest(tmp_path)
        self._write_road_graph(tmp_path)
        r = run_cli(tmp_path)
        assert r.returncode == 0, r.stdout + r.stderr
        assert 'city.xodr not found' not in r.stdout
        assert 'no XODR; runtime will use road-graph fallback' in r.stdout
        assert 'WARNINGS' in r.stdout
        assert 'Validation passed.' in r.stdout

    def test_no_xodr_and_no_road_graph_still_errors(self, tmp_path):
        # Manifest satisfies citypack.json, but with neither XODR nor road
        # graph the XODR requirement still fails.
        self._write_manifest(tmp_path)
        r = run_cli(tmp_path)
        assert r.returncode == 1
        assert 'city.xodr not found' in r.stdout
        assert 'citypack.json not found' not in r.stdout

    def test_citypack_json_still_checked_when_present(self, tmp_path):
        # citypack.json remains authoritative (and checked) when it exists,
        # even alongside a manifest.
        self._write_manifest(tmp_path)
        write_minimal_pack(tmp_path, [('A', [(41.0, -81.0)])])  # 1 coord: error
        r = run_cli(tmp_path)
        assert r.returncode == 1
        assert 'fewer than 2 coordinates' in r.stdout
