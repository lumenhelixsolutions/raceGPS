#!/usr/bin/env python3
"""Generic semantic road graph builder from OSM data."""

import math
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any

# Meters of vertical offset applied per OSM layer level so the UE5 importer
# can render bridges above / tunnels below crossing roads.
LAYER_HEIGHT_M = 5.0

# Default endpoint snap tolerance. The citypack validator's near-miss report
# shows the overwhelming bulk of broken-junction pairs are sub-meter
# (top pairs 0.2-0.3 m), while the narrowest road we model is 5 m wide —
# 1.5 m repairs tessellation/rounding gaps without risking merges of
# parallel carriageways.
SNAP_TOLERANCE_M = 1.5

_FALSY_TAG_VALUES = ("", "no", "false", "0")


def _is_truthy_tag(val: str | None) -> bool:
    """OSM tag presence check: bridge=yes/viaduct/... vs bridge=no/absent."""
    return (val or "").strip().lower() not in _FALSY_TAG_VALUES


def _dist_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    """Haversine distance in meters between two lat/lon points."""
    R = 6371000
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = (math.sin(dlat / 2) ** 2
         + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2))
         * math.sin(dlon / 2) ** 2)
    return 2 * R * math.asin(math.sqrt(a))


def _snap_road_endpoints(roads: list[dict], tolerance_m: float = SNAP_TOLERANCE_M) -> dict:
    """Snap near-miss road endpoints together, per layer, in O(n).

    Endpoints (first/last point of each road) within `tolerance_m` are
    clustered with a spatial grid + union-find. Cluster target selection:

    - If the cluster contains endpoints that were ALREADY exactly equal
      (a real OSM shared-node junction), those never move; lone outliers
      snap onto the nearest such exact group.
    - Otherwise (all endpoints distinct) the cluster snaps to its centroid.
      Centroid is used rather than highest-degree endpoint because clusters
      here are pure singletons — no endpoint has more "degree" than another.

    Endpoints on different OSM layers are NEVER snapped together: a bridge
    endpoint 1 m above a surface street is not a junction. Snapping mutates
    road point coordinates in place; returns an additive stats report.
    """
    endpoints = []  # (road_index, end_key, lat, lon, layer)
    for i, r in enumerate(roads):
        for end in (0, -1):
            p = r["points"][end]
            endpoints.append((i, end, p["lat"], p["lon"], r.get("layer", 0)))

    report = {
        "enabled": tolerance_m > 0,
        "tolerance_m": tolerance_m,
        "endpoints_total": len(endpoints),
        "pairs_snapped": 0,
        "cross_layer_pairs_skipped": 0,
        "clusters_merged": 0,
        "endpoints_moved": 0,
        "max_distance_moved_m": 0.0,
        "intersections_added": 0,
        "intersections_augmented": 0,
    }
    if tolerance_m <= 0 or len(endpoints) < 2:
        return report

    # Spatial grid with ground-sized cells (>= tolerance) so any pair within
    # tolerance lands in the same or an adjacent cell (3x3 scan suffices).
    ref_lat = math.radians(sum(e[2] for e in endpoints) / len(endpoints))
    m_per_deg_lon = max(111320.0 * math.cos(ref_lat), 11132.0)  # clamp near poles
    cell_lat = tolerance_m / 110540.0   # 1 deg lat >= 110.54 km everywhere
    cell_lon = tolerance_m / m_per_deg_lon

    grid: dict[tuple[int, int], list[int]] = {}
    for idx, (_, _, lat, lon, _) in enumerate(endpoints):
        key = (math.floor(lat / cell_lat), math.floor(lon / cell_lon))
        grid.setdefault(key, []).append(idx)

    parent = list(range(len(endpoints)))

    def find(x: int) -> int:
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a: int, b: int) -> None:
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[rb] = ra

    for idx, (_, _, lat, lon, layer) in enumerate(endpoints):
        klat = math.floor(lat / cell_lat)
        klon = math.floor(lon / cell_lon)
        for dlat in (-1, 0, 1):
            for dlon in (-1, 0, 1):
                for j in grid.get((klat + dlat, klon + dlon), ()):
                    if j <= idx:
                        continue
                    _, _, lat2, lon2, layer2 = endpoints[j]
                    if _dist_m(lat, lon, lat2, lon2) <= tolerance_m:
                        if layer == layer2:
                            if (lat, lon) != (lat2, lon2):
                                report["pairs_snapped"] += 1
                            union(idx, j)
                        else:
                            report["cross_layer_pairs_skipped"] += 1

    clusters: dict[int, list[int]] = {}
    for idx in range(len(endpoints)):
        clusters.setdefault(find(idx), []).append(idx)

    def _move(idx: int, tgt_lat: float, tgt_lon: float) -> None:
        ri, end, lat, lon, _ = endpoints[idx]
        if (lat, lon) == (tgt_lat, tgt_lon):
            return
        moved = _dist_m(lat, lon, tgt_lat, tgt_lon)
        report["max_distance_moved_m"] = max(report["max_distance_moved_m"], moved)
        report["endpoints_moved"] += 1
        roads[ri]["points"][end]["lat"] = tgt_lat
        roads[ri]["points"][end]["lon"] = tgt_lon

    for members in clusters.values():
        if len(members) < 2:
            continue
        by_coord: dict[tuple[float, float], list[int]] = {}
        for m in members:
            by_coord.setdefault((endpoints[m][2], endpoints[m][3]), []).append(m)
        exact_groups = [c for c, mem in by_coord.items() if len(mem) >= 2]

        if exact_groups:
            # Already-connected junctions stay put; lone outliers join the
            # nearest exact group (guaranteed within tolerance by clustering,
            # but verify anyway for transitive-chain edge cases).
            for coord, mem in by_coord.items():
                if len(mem) >= 2:
                    continue
                tgt = min(exact_groups,
                          key=lambda c: _dist_m(coord[0], coord[1], c[0], c[1]))
                if _dist_m(coord[0], coord[1], tgt[0], tgt[1]) <= tolerance_m:
                    for m in mem:
                        _move(m, tgt[0], tgt[1])
        else:
            tgt_lat = sum(endpoints[m][2] for m in members) / len(members)
            tgt_lon = sum(endpoints[m][3] for m in members) / len(members)
            for m in members:
                _move(m, tgt_lat, tgt_lon)

        road_ids = {endpoints[m][0] for m in members}
        if len(road_ids) >= 2:
            report["clusters_merged"] += 1

    report["max_distance_moved_m"] = round(report["max_distance_moved_m"], 3)
    return report


def _parse_layer(tags: dict) -> int:
    """Resolve the effective OSM layer for a way.

    An explicit numeric `layer` tag wins. Otherwise `bridge=*` implies +1 and
    `tunnel=*` implies -1. Untagged ways default to ground level (0), which
    keeps previously compiled citypacks (e.g. Akron) byte-compatible.
    """
    raw = (tags.get("layer") or "").strip()
    if raw:
        try:
            return int(raw)
        except ValueError:
            pass  # fall through to bridge/tunnel inference
    if _is_truthy_tag(tags.get("bridge")):
        return 1
    if _is_truthy_tag(tags.get("tunnel")):
        return -1
    return 0


def build_road_graph(osm_path: Path, origin_lat: float = 0.0, origin_lon: float = 0.0,
                     snap_tolerance_m: float = SNAP_TOLERANCE_M) -> dict[str, Any]:
    """Parse OSM and build a semantic road graph with world-space coordinates."""
    tree = ET.parse(osm_path)
    root = tree.getroot()

    nodes: dict[str, tuple[float, float]] = {}
    ways: list[dict] = []
    relations: list[dict] = []

    for elem in root:
        if elem.tag == "node":
            nid = elem.get("id")
            lat = float(elem.get("lat", 0))
            lon = float(elem.get("lon", 0))
            nodes[nid] = (lat, lon)
        elif elem.tag == "way":
            tags = {t.get("k"): t.get("v") for t in elem if t.tag == "tag"}
            nds = [n.get("ref") for n in elem if n.tag == "nd"]
            ways.append({"id": elem.get("id"), "tags": tags, "nodes": nds})
        elif elem.tag == "relation":
            tags = {t.get("k"): t.get("v") for t in elem if t.tag == "tag"}
            members = [{"type": m.get("type"), "ref": m.get("ref"), "role": m.get("role")}
                       for m in elem if m.tag == "member"]
            relations.append({"id": elem.get("id"), "tags": tags, "members": members})

    # Build roads from highway ways
    roads = []
    node_to_ways: dict[str, list[str]] = {}
    road_layers: dict[str, int] = {}
    for w in ways:
        if "highway" not in w["tags"]:
            continue

        highway = w["tags"]["highway"]
        if highway in ("footway", "cycleway", "path", "steps", "corridor", "track"):
            continue

        points = []
        for nid in w["nodes"]:
            if nid in nodes:
                points.append({"lat": nodes[nid][0], "lon": nodes[nid][1]})
            node_to_ways.setdefault(nid, []).append(w["id"])

        if len(points) < 2:
            continue

        width_map = {
            "motorway": 14, "motorway_link": 10, "trunk": 12, "trunk_link": 9,
            "primary": 10, "primary_link": 9, "secondary": 9, "secondary_link": 8,
            "tertiary": 8, "tertiary_link": 7, "residential": 7,
            "unclassified": 7, "service": 5, "living_street": 6, "pedestrian": 6,
        }

        lanes = w["tags"].get("lanes", "")
        try:
            lane_count = int(lanes.split(";")[0])
        except ValueError:
            lane_count = 2 if highway in ("motorway", "trunk", "primary") else 1

        layer = _parse_layer(w["tags"])
        road_layers[w["id"]] = layer

        roads.append({
            "id": w["id"],
            "name": w["tags"].get("name", ""),
            "highway": highway,
            "points": points,
            "width": width_map.get(highway, 7),
            "lane_count": lane_count,
            "one_way": w["tags"].get("oneway", "no") == "yes",
            "max_speed": _parse_maxspeed(w["tags"].get("maxspeed", "")),
            "surface": w["tags"].get("surface", "asphalt"),
            "layer": layer,
            "elevation_m": layer * LAYER_HEIGHT_M,
            "is_bridge": _is_truthy_tag(w["tags"].get("bridge")),
            "is_tunnel": _is_truthy_tag(w["tags"].get("tunnel")),
        })

    # Snap near-miss endpoints (per layer) so visually touching roads actually
    # connect. Runs BEFORE intersection detection and bounds so downstream
    # consumers (intersections, route generation, export) see snapped coords.
    snap_report = _snap_road_endpoints(roads, snap_tolerance_m)

    # Find intersections (nodes shared by 2+ roads on the SAME layer).
    # Roads at different layers (e.g. a bridge over a surface street) share a
    # node in OSM but do not physically connect, so they must not junction.
    intersections = []
    for nid, way_ids in node_to_ways.items():
        if len(way_ids) >= 2 and nid in nodes:
            by_layer: dict[int, list[str]] = {}
            for wid in way_ids:
                by_layer.setdefault(road_layers.get(wid, 0), []).append(wid)
            for lyr, ids in by_layer.items():
                if len(ids) >= 2:
                    intersections.append({
                        "node_id": nid,
                        "lat": nodes[nid][0],
                        "lon": nodes[nid][1],
                        "road_ids": ids,
                        "layer": lyr,
                    })

    # Fold snapped endpoints into the intersection list: an endpoint shared by
    # 2+ roads after snapping is a junction even when OSM used distinct node
    # ids. Existing OSM-node intersections get the newly connected road ids;
    # brand-new junction points get additive "from_snap" entries.
    if snap_report["enabled"]:
        existing = {(i["layer"], i["lat"], i["lon"]): i for i in intersections}
        buckets: dict[tuple[int, float, float], set[str]] = {}
        for r in roads:
            for end in (0, -1):
                p = r["points"][end]
                key = (r.get("layer", 0), p["lat"], p["lon"])
                buckets.setdefault(key, set()).add(r["id"])
        snap_seq = 0
        for (lyr, lat, lon), rids in buckets.items():
            if len(rids) < 2:
                continue
            hit = existing.get((lyr, lat, lon))
            if hit is not None:
                before = len(hit["road_ids"])
                for rid in sorted(rids):
                    if rid not in hit["road_ids"]:
                        hit["road_ids"].append(rid)
                if len(hit["road_ids"]) > before:
                    snap_report["intersections_augmented"] += 1
            else:
                snap_seq += 1
                intersections.append({
                    "node_id": f"snap_{snap_seq}",
                    "lat": lat,
                    "lon": lon,
                    "road_ids": sorted(rids),
                    "layer": lyr,
                    "from_snap": True,
                })
                existing[(lyr, lat, lon)] = intersections[-1]
                snap_report["intersections_added"] += 1

    # Compute world bounds from all road points
    all_lats = [p["lat"] for r in roads for p in r["points"]]
    all_lons = [p["lon"] for r in roads for p in r["points"]]
    bounds = {
        "south": min(all_lats) if all_lats else -1,
        "north": max(all_lats) if all_lats else 1,
        "west": min(all_lons) if all_lons else -1,
        "east": max(all_lons) if all_lons else 1,
    }

    return {
        "roads": roads,
        "intersections": intersections,
        "bounds": bounds,
        "road_count": len(roads),
        "intersection_count": len(intersections),
        "origin": {"lat": origin_lat, "lon": origin_lon},
        "endpoint_snap": snap_report,
    }


def _parse_maxspeed(val: str) -> int:
    """Parse OSM maxspeed tag to km/h integer."""
    if not val:
        return 50
    val = val.strip().lower()
    if val == "none":
        return 250
    if "mph" in val:
        try:
            return int(float(val.replace("mph", "").strip()) * 1.60934)
        except ValueError:
            return 50
    try:
        return int(float(val))
    except ValueError:
        return 50
