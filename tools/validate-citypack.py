#!/usr/bin/env python3
"""
raceGPS — Citypack Validation Suite
Validates citypack directory structure, OpenDRIVE XML, and route connectivity.
"""

import argparse
import json
import math
import os
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path


def validate_citypack_json(filepath: str) -> list:
    errors = []
    try:
        with open(filepath) as f:
            data = json.load(f)
    except Exception as e:
        errors.append(f'Invalid JSON: {e}')
        return errors

    required = ['name', 'version', 'generated_at', 'format', 'statistics', 'roads', 'buildings', 'pois', 'routes']
    for key in required:
        if key not in data:
            errors.append(f'Missing required key: {key}')

    stats = data.get('statistics', {})
    roads = data.get('roads', [])
    buildings = data.get('buildings', [])
    pois = data.get('pois', [])
    routes = data.get('routes', [])

    if len(roads) != stats.get('roads', 0):
        errors.append(f'Road count mismatch: stats says {stats.get("roads", 0)}, found {len(roads)}')
    if len(buildings) != stats.get('buildings', 0):
        errors.append(f'Building count mismatch: stats says {stats.get("buildings", 0)}, found {len(buildings)}')
    if len(pois) != stats.get('pois', 0):
        errors.append(f'POI count mismatch: stats says {stats.get("pois", 0)}, found {len(pois)}')

    for i, road in enumerate(roads):
        if len(road.get('coordinates', [])) < 2:
            errors.append(f'Road {i} ({road.get("id")}) has fewer than 2 coordinates')

    for i, route in enumerate(routes):
        if not route.get('id') or not route.get('name'):
            errors.append(f'Route {i} missing id or name')

    return errors


def validate_xodr(filepath: str) -> list:
    errors = []
    try:
        tree = ET.parse(filepath)
        root = tree.getroot()
    except ET.ParseError as e:
        errors.append(f'Invalid XML: {e}')
        return errors
    except Exception as e:
        errors.append(f'Cannot read XODR: {e}')
        return errors

    if root.tag != 'OpenDRIVE':
        errors.append(f'Root tag is "{root.tag}", expected "OpenDRIVE"')

    roads = root.findall('road')
    if len(roads) == 0:
        errors.append('No roads found in XODR')

    for road in roads:
        if 'id' not in road.attrib:
            errors.append('Road missing id attribute')
        if 'length' not in road.attrib:
            errors.append(f'Road {road.attrib.get("id", "?")} missing length attribute')

    return errors


def check_connectivity(roads: list) -> dict:
    """Simple graph connectivity check on road endpoints."""
    from collections import defaultdict
    graph = defaultdict(list)

    for road in roads:
        coords = road.get('coordinates', [])
        if len(coords) < 2:
            continue
        start = tuple(coords[0])
        end = tuple(coords[-1])
        graph[start].append(end)
        graph[end].append(start)

    if not graph:
        return {'connected': False, 'components': 0, 'error': 'No valid road endpoints'}

    # BFS from first node
    nodes = list(graph.keys())
    visited = set()
    stack = [nodes[0]]
    while stack:
        node = stack.pop()
        if node in visited:
            continue
        visited.add(node)
        for neighbor in graph[node]:
            if neighbor not in visited:
                stack.append(neighbor)

    components = 1 if len(visited) == len(nodes) else 2  # Simplified
    return {
        'connected': len(visited) == len(nodes),
        'components': components,
        'nodes': len(nodes),
        'visited': len(visited)
    }


# ---------------------------------------------------------------------------
# Extended checks (ADDITIVE — existing checks above are unchanged):
#   * near-miss junction detection (snap/tolerance connectivity + warnings)
#   * route validation (length bounds, on-network, loop closure)
#   * road geometry sanity (duplicates, zero-length segments, bounds, density)
# ---------------------------------------------------------------------------

MAX_REPORTED_ISSUES = 20  # cap on printed detail lines per check category


def haversine_m(a, b):
    """Great-circle distance in meters between (lat, lon) pairs."""
    lat1, lon1 = a
    lat2, lon2 = b
    r = 6371000.0
    p1 = math.radians(lat1)
    p2 = math.radians(lat2)
    dp = math.radians(lat2 - lat1)
    dl = math.radians(lon2 - lon1)
    h = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * r * math.asin(min(1.0, math.sqrt(h)))


def extract_points(road):
    """Normalize a road polyline to a list of (lat, lon) float tuples.

    Supports road-graph style {'points': [{'lat':..,'lon':..}, ...]} and
    citypack.json style {'coordinates': [[lat, lon], ...]}.
    """
    pts = []
    for p in road.get('points', []) or []:
        if isinstance(p, dict) and 'lat' in p and 'lon' in p:
            try:
                pts.append((float(p['lat']), float(p['lon'])))
            except (TypeError, ValueError):
                continue
    if pts:
        return pts
    for c in road.get('coordinates', []) or []:
        if isinstance(c, (list, tuple)) and len(c) >= 2:
            try:
                pts.append((float(c[0]), float(c[1])))
            except (TypeError, ValueError):
                continue
    return pts


class UnionFind:
    def __init__(self):
        self.parent = {}

    def find(self, x):
        self.parent.setdefault(x, x)
        root = x
        while self.parent[root] != root:
            root = self.parent[root]
        while self.parent[x] != root:
            self.parent[x], x = root, self.parent[x]
        return root

    def union(self, a, b):
        ra, rb = self.find(a), self.find(b)
        if ra != rb:
            self.parent[rb] = ra


class SpatialIndex:
    """Uniform-grid spatial index over (lat, lon) points for radius queries."""

    def __init__(self, points, cell_m):
        self.cell_deg = max(cell_m, 0.1) / 111320.0
        self.buckets = defaultdict(list)
        for p in points:
            self.buckets[self._key(p)].append(p)

    def _key(self, p):
        return (int(math.floor(p[0] / self.cell_deg)),
                int(math.floor(p[1] / self.cell_deg)))

    def min_distance_m(self, p):
        """Distance to nearest indexed point, or None if no point within ~1 cell."""
        kx, ky = self._key(p)
        best = None
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for q in self.buckets.get((kx + dx, ky + dy), []):
                    d = haversine_m(p, q)
                    if best is None or d < best:
                        best = d
        return best


def check_intersection_connectivity(roads, intersections):
    """Connectivity via an explicit intersections list (road-graph shape:
    intersections carry road_ids). This is the authoritative topology when
    present, since road polyline endpoints often do not coincide exactly
    with intersection coordinates.

    Returns dict with component count, road count, and sample isolated ids.
    """
    road_ids = {r.get('id') for r in roads if r.get('id') is not None}
    uf = UnionFind()
    for rid in road_ids:
        uf.find(rid)
    referenced = set()
    for inter in intersections:
        ids = [i for i in (inter.get('road_ids') or []) if i in road_ids]
        referenced.update(ids)
        for other in ids[1:]:
            uf.union(ids[0], other)
    components = len({uf.find(r) for r in road_ids})
    isolated = sorted(road_ids - referenced)
    return {
        'components': components,
        'roads': len(road_ids),
        'intersections': len(intersections),
        'roads_not_in_any_intersection': len(isolated),
        'isolated_samples': [str(i) for i in isolated[:MAX_REPORTED_ISSUES]],
    }


def check_connectivity_snapped(roads, tolerance_m=2.0):
    """Tolerance-based endpoint connectivity + near-miss junction detection.

    Endpoints within `tolerance_m` count as connected (snapping). Endpoint
    pairs that are within tolerance but NOT exactly equal are reported as
    near-miss pairs (likely broken junctions).

    Returns a dict with exact-graph results, snapped component count, and
    near-miss count/samples.
    """
    endpoints = []  # (lat, lon, road_id, end_label)
    road_edges = []  # (endpoint_index_start, endpoint_index_end) per road
    for idx, road in enumerate(roads):
        pts = extract_points(road)
        if len(pts) < 2:
            continue
        rid = road.get('id', f'#{idx}')
        i_start = len(endpoints)
        endpoints.append((pts[0][0], pts[0][1], rid, 'start'))
        endpoints.append((pts[-1][0], pts[-1][1], rid, 'end'))
        road_edges.append((i_start, i_start + 1))

    # Exact-tuple connectivity, computed generically over both road shapes
    # ('points' and 'coordinates'); distinct from the legacy check_connectivity
    # used in main(), which stays unchanged for citypack.json behavior.
    exact_uf = UnionFind()
    for i_start, i_end in road_edges:
        exact_uf.union(i_start, i_end)
    coord_owner = {}
    for i, (lat, lon, _rid, _end) in enumerate(endpoints):
        key = (lat, lon)
        if key in coord_owner:
            exact_uf.union(i, coord_owner[key])
        else:
            coord_owner[key] = i
    exact_components = len({exact_uf.find(i) for i in range(len(endpoints))})

    result = {
        'exact_connected': bool(endpoints) and exact_components == 1,
        'exact_components': exact_components,
        'exact_nodes': len(coord_owner),
        'snapped_components': 0,
        'snapped_nodes': len(endpoints),
        'near_miss_count': 0,
        'near_miss_samples': [],
        'tolerance_m': tolerance_m,
    }
    if not endpoints:
        return result

    cell_deg = max(tolerance_m, 0.1) / 111320.0
    buckets = defaultdict(list)
    for i, (lat, lon, _rid, _end) in enumerate(endpoints):
        key = (int(math.floor(lat / cell_deg)), int(math.floor(lon / cell_deg)))
        buckets[key].append(i)

    uf = UnionFind()
    # Each road is itself an edge between its two endpoints.
    for i_start, i_end in road_edges:
        uf.union(i_start, i_end)

    seen_pairs = set()
    near_miss = []
    for (kx, ky), idxs in buckets.items():
        for i in idxs:
            lat1, lon1, rid1, _e1 = endpoints[i]
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for j in buckets.get((kx + dx, ky + dy), []):
                        if j <= i or (i, j) in seen_pairs:
                            continue
                        seen_pairs.add((i, j))
                        lat2, lon2, rid2, _e2 = endpoints[j]
                        d = haversine_m((lat1, lon1), (lat2, lon2))
                        if d <= tolerance_m:
                            uf.union(i, j)
                            if rid1 != rid2 and (lat1, lon1) != (lat2, lon2):
                                near_miss.append((rid1, rid2, d))

    result['snapped_components'] = len({uf.find(i) for i in range(len(endpoints))})
    result['near_miss_count'] = len(near_miss)
    near_miss.sort(key=lambda t: t[2])
    result['near_miss_samples'] = near_miss[:MAX_REPORTED_ISSUES]
    return result


def path_length_m(pts):
    return sum(haversine_m(pts[i], pts[i + 1]) for i in range(len(pts) - 1))


def _route_points(route):
    pts = []
    raw = route.get('points') or route.get('coordinates') or []
    for p in raw:
        try:
            if isinstance(p, dict) and 'lat' in p and 'lon' in p:
                pts.append((float(p['lat']), float(p['lon'])))
            elif isinstance(p, (list, tuple)) and len(p) >= 2:
                pts.append((float(p[0]), float(p[1])))
        except (TypeError, ValueError):
            continue
    return pts


def check_routes(routes, road_index=None, min_km=1.0, max_km=15.0,
                 on_road_tolerance_m=30.0, loop_tolerance_m=100.0):
    """Validate routes: sane length, on/near road network, loop closure.

    Returns (errors, warnings, info_lines). Length-bound violations are
    errors; on-network deviation and loop-closure gaps are warnings.
    """
    errors, warnings, info = [], [], []
    for i, route in enumerate(routes):
        if not isinstance(route, dict):
            warnings.append(f'Route #{i}: not an object, skipped')
            continue
        rid = route.get('route_id') or route.get('id') or f'#{i}'
        pts = _route_points(route)

        # --- length bounds ---
        length_m = route.get('distance_meters')
        try:
            length_m = float(length_m) if length_m is not None else None
        except (TypeError, ValueError):
            length_m = None
        if length_m is None and len(pts) >= 2:
            length_m = path_length_m(pts)
        if length_m is None:
            warnings.append(f'Route {rid}: no distance_meters and <2 usable points; length unverifiable')
        else:
            info.append(f'  Route {rid}: {length_m / 1000.0:.2f} km, {len(pts)} points')
            if length_m < min_km * 1000:
                errors.append(f'Route {rid}: length {length_m / 1000.0:.2f} km below minimum {min_km} km')
            elif length_m > max_km * 1000:
                errors.append(f'Route {rid}: length {length_m / 1000.0:.2f} km above maximum {max_km} km')

        # --- on/near road network ---
        if road_index is not None and pts:
            off = []
            for p in pts:
                d = road_index.min_distance_m(p)
                if d is None or d > on_road_tolerance_m:
                    off.append((p, d))
            if off:
                worst = max((d for _p, d in off if d is not None), default=None)
                msg = (f'Route {rid}: {len(off)}/{len(pts)} points more than '
                       f'{on_road_tolerance_m:.0f} m from road network')
                if worst is not None:
                    msg += f'; worst deviation {worst:.1f} m'
                warnings.append(msg)

        # --- loop closure for loop-typed routes ---
        mode = str(route.get('mode') or route.get('type') or '').lower()
        if ('loop' in mode or 'circuit' in mode) and pts:
            start, finish = pts[0], pts[-1]
            f = route.get('finish')
            if isinstance(f, dict) and 'lat' in f and 'lon' in f:
                try:
                    finish = (float(f['lat']), float(f['lon']))
                except (TypeError, ValueError):
                    pass
            gap = haversine_m(start, finish)
            if gap > loop_tolerance_m:
                warnings.append(
                    f'Route {rid}: loop-typed route (mode={mode or "?"}) but '
                    f'start/finish are {gap:.0f} m apart')
    return errors, warnings, info


def check_road_geometry(roads, bounds=None, zero_length_epsilon_m=0.01,
                        degenerate_length_m=0.5, sparse_segment_m=500.0,
                        bounds_margin_deg=0.005):
    """Road geometry sanity checks.

    Errors: coordinates outside valid lat/lon ranges.
    Warnings: duplicate consecutive points, zero-length segments, points
    outside declared bounds, degenerate (near-zero length) roads, sparse
    polylines (segment gap > sparse_segment_m).

    Returns (errors, warnings, stats).
    """
    stats = {
        'roads_checked': 0,
        'duplicate_point_roads': 0, 'duplicate_points': 0,
        'zero_length_segment_roads': 0, 'zero_length_segments': 0,
        'out_of_range_coords': 0,
        'out_of_bounds_roads': 0,
        'degenerate_roads': 0,
        'sparse_roads': 0,
    }
    range_err_msgs = []
    dup_samples, zero_samples, oob_samples = [], [], []
    deg_samples, sparse_samples = [], []

    for idx, road in enumerate(roads):
        stats['roads_checked'] += 1
        pts = extract_points(road)
        if not pts:
            continue
        rid = road.get('id', f'#{idx}')

        bad = [p for p in pts if not (-90.0 <= p[0] <= 90.0 and -180.0 <= p[1] <= 180.0)]
        if bad:
            stats['out_of_range_coords'] += len(bad)
            range_err_msgs.append(
                f'Road {rid}: {len(bad)} coordinate(s) outside valid lat/lon range '
                f'(first: {bad[0]})')

        if bounds is not None:
            west, south, east, north = bounds
            outside = [p for p in pts
                       if not (south - bounds_margin_deg <= p[0] <= north + bounds_margin_deg
                               and west - bounds_margin_deg <= p[1] <= east + bounds_margin_deg)]
            if outside:
                stats['out_of_bounds_roads'] += 1
                if len(oob_samples) < 5:
                    oob_samples.append(f'Road {rid}: {len(outside)} point(s) outside declared bounds')

        dups = zeros = 0
        total = 0.0
        maxseg = 0.0
        for a, b in zip(pts, pts[1:]):
            d = haversine_m(a, b)
            if a == b:
                dups += 1
            elif d < zero_length_epsilon_m:
                zeros += 1
            total += d
            if d > maxseg:
                maxseg = d
        if dups:
            stats['duplicate_point_roads'] += 1
            stats['duplicate_points'] += dups
            if len(dup_samples) < 5:
                dup_samples.append(f'Road {rid}: {dups} duplicate consecutive point(s)')
        if zeros:
            stats['zero_length_segment_roads'] += 1
            stats['zero_length_segments'] += zeros
            if len(zero_samples) < 5:
                zero_samples.append(f'Road {rid}: {zeros} zero-length segment(s)')
        if len(pts) >= 2:
            if total < degenerate_length_m:
                stats['degenerate_roads'] += 1
                if len(deg_samples) < 5:
                    deg_samples.append(
                        f'Road {rid}: near-zero total length ({total:.3f} m across {len(pts)} points)')
            elif maxseg > sparse_segment_m:
                stats['sparse_roads'] += 1
                if len(sparse_samples) < 5:
                    sparse_samples.append(
                        f'Road {rid}: max segment gap {maxseg:.0f} m exceeds '
                        f'{sparse_segment_m:.0f} m (sparse coordinates?)')

    errors = range_err_msgs[:MAX_REPORTED_ISSUES]
    if len(range_err_msgs) > MAX_REPORTED_ISSUES:
        errors.append(f'... and {len(range_err_msgs) - MAX_REPORTED_ISSUES} more road(s) '
                      f'with out-of-range coordinates')

    warnings = []
    if stats['duplicate_points']:
        warnings.append(f"{stats['duplicate_points']} duplicate consecutive point(s) "
                        f"in {stats['duplicate_point_roads']} road(s)")
        warnings.extend(dup_samples)
    if stats['zero_length_segments']:
        warnings.append(f"{stats['zero_length_segments']} zero-length segment(s) "
                        f"in {stats['zero_length_segment_roads']} road(s)")
        warnings.extend(zero_samples)
    if stats['out_of_bounds_roads']:
        warnings.append(f"{stats['out_of_bounds_roads']} road(s) extend outside declared bounds")
        warnings.extend(oob_samples)
    if stats['degenerate_roads']:
        warnings.append(f"{stats['degenerate_roads']} degenerate road(s) with near-zero total length")
        warnings.extend(deg_samples)
    if stats['sparse_roads']:
        warnings.append(f"{stats['sparse_roads']} road(s) with segment gaps over {sparse_segment_m:.0f} m")
        warnings.extend(sparse_samples)

    return errors, warnings, stats


def find_aux_files(citypack_dir):
    """Discover additive data sources: semantic manifest, road graph, routes,
    and extra XODR files (existing citypack.json / city.xodr handling is
    unchanged and lives in main())."""
    aux = {'manifest': None, 'road_graph': None, 'routes': [], 'xodr': []}
    for name in sorted(os.listdir(citypack_dir)):
        path = os.path.join(citypack_dir, name)
        if not os.path.isfile(path):
            continue
        if name.endswith('_semantic_manifest.json'):
            aux['manifest'] = path
        elif name.endswith('_road_graph.json'):
            aux['road_graph'] = path
        elif name.endswith('_routes.json'):
            aux['routes'].append(path)
        elif name.endswith('.xodr') and name != 'city.xodr':
            aux['xodr'].append(path)
    routes_dir = os.path.join(citypack_dir, 'routes')
    if os.path.isdir(routes_dir):
        for name in sorted(os.listdir(routes_dir)):
            if name.endswith('.json'):
                aux['routes'].append(os.path.join(routes_dir, name))
    return aux


def main():
    parser = argparse.ArgumentParser(description='raceGPS Citypack Validator')
    parser.add_argument('citypack_dir', help='Path to citypack directory')
    parser.add_argument('--junction-tolerance-m', type=float, default=2.0,
                        help='Endpoint snap tolerance for near-miss junction detection (meters)')
    parser.add_argument('--route-min-km', type=float, default=1.0,
                        help='Minimum sane route length (km)')
    parser.add_argument('--route-max-km', type=float, default=15.0,
                        help='Maximum sane route length (km)')
    parser.add_argument('--route-on-road-tolerance-m', type=float, default=30.0,
                        help='Max allowed distance of route points from road network (meters)')
    args = parser.parse_args()

    if not os.path.isdir(args.citypack_dir):
        print(f'ERROR: Not a directory: {args.citypack_dir}', file=sys.stderr)
        sys.exit(1)

    cp_path = os.path.join(args.citypack_dir, 'citypack.json')
    xodr_path = os.path.join(args.citypack_dir, 'city.xodr')

    all_errors = []
    all_warnings = []
    counts = {}

    def record(category, errors=(), warnings=()):
        all_errors.extend(errors)
        all_warnings.extend(warnings)
        c = counts.setdefault(category, {'errors': 0, 'warnings': 0})
        c['errors'] += len(errors)
        c['warnings'] += len(warnings)

    aux = find_aux_files(args.citypack_dir)

    cp_data = None
    if os.path.exists(cp_path):
        record('schema', validate_citypack_json(cp_path))
    elif aux['manifest']:
        # Manifest-named pack (docs/CITYPACK_CONTRACT.md): *_semantic_manifest.json
        # satisfies the manifest requirement.
        pass
    else:
        record('files', ['citypack.json not found'])

    if os.path.exists(xodr_path):
        record('xodr', validate_xodr(xodr_path))
    elif aux['xodr']:
        # Any other *.xodr (e.g. akron.xodr) satisfies the XODR requirement;
        # it is validated in the extended section below.
        pass
    elif aux['road_graph']:
        # Dialect B pack: no XODR, runtime uses the road-graph fallback.
        record('files', warnings=['no XODR; runtime will use road-graph fallback'])
    else:
        record('files', ['city.xodr not found'])

    # Load citypack for connectivity check
    if os.path.exists(cp_path):
        with open(cp_path) as f:
            cp_data = json.load(f)
        conn = check_connectivity(cp_data.get('roads', []))
        print(f'Connectivity: {conn}')
        conn_errors = []
        if not conn['connected']:
            conn_errors.append(f'Road graph not fully connected ({conn.get("visited", 0)}/{conn.get("nodes", 0)} nodes reachable)')
        record('connectivity', conn_errors)

        stats = cp_data.get('statistics', {})
        print(f'Statistics:')
        print(f'  Roads:      {stats.get("roads", 0)}')
        print(f'  Junctions:  {stats.get("junctions", 0)}')
        print(f'  Buildings:  {stats.get("buildings", 0)}')
        print(f'  POIs:       {stats.get("pois", 0)}')
        print(f'  Routes:     {len(cp_data.get("routes", []))}')

    # ------------------------------------------------------------------
    # Extended checks (additive)
    # ------------------------------------------------------------------
    # Extra XODR files (e.g. akron.xodr) get the same XODR checks.
    for xp in aux['xodr']:
        name = os.path.basename(xp)
        record('xodr', [f'{name}: {e}' for e in validate_xodr(xp)])

    # Manifest -> declared bounds for geometry checks.
    bounds = None
    if aux['manifest']:
        try:
            with open(aux['manifest']) as f:
                manifest = json.load(f)
            b = manifest.get('bounds') or {}
            if all(k in b for k in ('west', 'south', 'east', 'north')):
                bounds = (float(b['west']), float(b['south']),
                          float(b['east']), float(b['north']))
        except Exception as e:
            record('schema', warnings=[f'{os.path.basename(aux["manifest"])}: unreadable manifest ({e})'])

    # Pick the richest road source: prefer *_road_graph.json, fall back to
    # citypack.json roads.
    roads_src = []
    roads_label = None
    intersections_src = []
    if aux['road_graph']:
        try:
            with open(aux['road_graph']) as f:
                rg = json.load(f)
            roads_src = rg.get('roads', []) or []
            intersections_src = rg.get('intersections', []) or []
            roads_label = os.path.basename(aux['road_graph'])
        except Exception as e:
            record('schema', [f'{os.path.basename(aux["road_graph"])}: invalid JSON ({e})'])
    if not roads_src and cp_data is not None:
        roads_src = cp_data.get('roads', []) or []
        roads_label = 'citypack.json'

    # Near-miss junction detection + snapped connectivity.
    if roads_src:
        tol = args.junction_tolerance_m
        snap = check_connectivity_snapped(roads_src, tolerance_m=tol)
        print(f'\nSnapped connectivity ({roads_label}, tol {tol} m): '
              f'{snap["snapped_components"]} component(s), {snap["snapped_nodes"]} endpoints')
        print(f'Near-miss junctions: {snap["near_miss_count"]} endpoint pair(s) within '
              f'{tol} m but not exactly connected')
        conn_err, conn_warn = [], []
        if intersections_src:
            # Explicit intersection topology is authoritative when present:
            # polyline endpoints frequently do not coincide exactly with
            # intersection coordinates, which would inflate component counts.
            iconn = check_intersection_connectivity(roads_src, intersections_src)
            print(f'Intersection connectivity: {iconn["components"]} component(s) '
                  f'across {iconn["roads"]} roads / {iconn["intersections"]} intersections; '
                  f'{iconn["roads_not_in_any_intersection"]} road(s) in no intersection')
            if iconn['components'] > 1:
                conn_err.append(f'Road graph has {iconn["components"]} disconnected components '
                                f'(via intersections topology)')
            if iconn['roads_not_in_any_intersection']:
                conn_warn.append(f'{iconn["roads_not_in_any_intersection"]} road(s) not referenced '
                                 f'by any intersection')
        elif snap['snapped_nodes'] and snap['snapped_components'] > 1:
            conn_err.append(f'Road graph has {snap["snapped_components"]} disconnected components '
                            f'even at {tol} m snap tolerance')
        if snap['near_miss_count']:
            conn_warn.append(f'{snap["near_miss_count"]} near-miss endpoint pair(s) within {tol} m '
                             f'but not exactly connected (possible broken junctions)')
            for a, b, d in snap['near_miss_samples']:
                conn_warn.append(f'  near-miss: road {a} <-> road {b}: {d:.2f} m apart')
        record('connectivity', conn_err, conn_warn)

    # Road geometry sanity.
    if roads_src:
        geo_err, geo_warn, gstats = check_road_geometry(roads_src, bounds=bounds)
        print(f'\nRoad geometry ({roads_label}): {gstats["roads_checked"]} roads checked')
        print(f'  duplicate consecutive points:  {gstats["duplicate_points"]} '
              f'(in {gstats["duplicate_point_roads"]} roads)')
        print(f'  zero-length segments:          {gstats["zero_length_segments"]} '
              f'(in {gstats["zero_length_segment_roads"]} roads)')
        print(f'  out-of-range coordinates:      {gstats["out_of_range_coords"]}')
        print(f'  out-of-bounds roads:           {gstats["out_of_bounds_roads"]}')
        print(f'  degenerate (near-zero length): {gstats["degenerate_roads"]}')
        print(f'  sparse (segment gap > 500 m):  {gstats["sparse_roads"]}')
        record('geometry', geo_err, geo_warn)

    # Route validation: citypack.json routes + *_routes.json + routes/*.json.
    routes_all = []
    if cp_data is not None:
        routes_all.extend(r for r in (cp_data.get('routes') or []) if isinstance(r, dict))
    for rf in aux['routes']:
        try:
            with open(rf) as f:
                rdata = json.load(f)
            if isinstance(rdata, dict):
                rdata = rdata.get('routes', [])
            if isinstance(rdata, list):
                routes_all.extend(r for r in rdata if isinstance(r, dict))
        except Exception as e:
            record('routes', [f'{os.path.basename(rf)}: invalid JSON ({e})'])

    if routes_all:
        road_index = None
        if roads_src:
            all_pts = []
            for road in roads_src:
                all_pts.extend(extract_points(road))
            if all_pts:
                road_index = SpatialIndex(all_pts, args.route_on_road_tolerance_m)
        r_err, r_warn, r_info = check_routes(
            routes_all, road_index,
            min_km=args.route_min_km, max_km=args.route_max_km,
            on_road_tolerance_m=args.route_on_road_tolerance_m)
        print(f'\nRoutes: {len(routes_all)} checked')
        for line in r_info[:MAX_REPORTED_ISSUES]:
            print(line)
        record('routes', r_err, r_warn)

    # ------------------------------------------------------------------
    # Summary (counts per check category, warnings vs errors)
    # ------------------------------------------------------------------
    print('\nCheck summary:')
    total_e = 0
    total_w = 0
    for cat, c in counts.items():
        print(f'  {cat:<14} errors={c["errors"]}  warnings={c["warnings"]}')
        total_e += c['errors']
        total_w += c['warnings']
    print(f'  {"TOTAL":<14} errors={total_e}  warnings={total_w}')

    if all_warnings:
        print(f'\nWARNINGS ({len(all_warnings)}):')
        for w in all_warnings[:MAX_REPORTED_ISSUES * 2]:
            print(f'  - {w}')
        if len(all_warnings) > MAX_REPORTED_ISSUES * 2:
            print(f'  ... and {len(all_warnings) - MAX_REPORTED_ISSUES * 2} more')

    if all_errors:
        print(f'\nERRORS ({len(all_errors)}):')
        for e in all_errors:
            print(f'  - {e}')
        sys.exit(1)
    else:
        print('\nValidation passed.')
        sys.exit(0)


if __name__ == '__main__':
    main()
