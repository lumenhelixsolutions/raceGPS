#!/usr/bin/env python3
"""Generic route generator with multiple race modes and smart waypoint selection."""

import heapq
import random
import math
from typing import Any

# Matches validate-citypack.py defaults (--route-min-km / --route-max-km).
MAX_ROUTE_LENGTH_M = 15000.0
# Two road points closer than this are considered the same junction (mirrors
# the 60 m connect tolerance used during route assembly).
LOOP_SNAP_M = 60.0
# Hard cap on Dijkstra expansions so a pathological graph can't hang a compile.
_MAX_CLOSURE_POPS = 60000


def _haversine(a: dict, b: dict) -> float:
    """Approximate distance in meters between two lat/lon points."""
    R = 6371000
    dlat = math.radians(b["lat"] - a["lat"])
    dlon = math.radians(b["lon"] - a["lon"])
    lat1 = math.radians(a["lat"])
    lat2 = math.radians(b["lat"])
    x = math.sin(dlon) * math.cos(lat2)
    y = math.cos(lat1) * math.sin(lat2) - math.sin(lat1) * math.cos(lat2) * math.cos(dlon)
    return math.atan2(math.sqrt(x*x + y*y), math.sin(lat1)*math.sin(lat2) + math.cos(lat1)*math.cos(lat2)*math.cos(dlon)) * R


def _route_length(points: list[dict]) -> float:
    return sum(_haversine(points[i], points[i+1]) for i in range(len(points)-1))


def _centroid(roads: list[dict]) -> dict:
    """Compute approximate city center from road points."""
    all_pts = [p for r in roads for p in r["points"]]
    if not all_pts:
        return {"lat": 0, "lon": 0}
    return {"lat": sum(p["lat"] for p in all_pts) / len(all_pts),
            "lon": sum(p["lon"] for p in all_pts) / len(all_pts)}


def generate_routes(road_graph: dict[str, Any], city_id: str, mode: str = "all", count: int = 3, seed: int = 42) -> list[dict]:
    """Generate routes for a given mode.

    Modes:
        - cruise_sprint: checkpoint-to-checkpoint races
        - time_trial: single-lap fastest time
        - circuit: looped route returning to start
        - drift_run: short technical sections with many turns
        - all: generate a mix of all modes
    """
    roads = road_graph.get("roads", [])
    if len(roads) < 3:
        return []

    rng = random.Random(seed)
    center = _centroid(roads)
    modes_to_generate = ["cruise_sprint", "time_trial", "circuit", "drift_run"] if mode == "all" else [mode]
    routes = []

    for target_mode in modes_to_generate:
        gen_count = count if mode != "all" else max(1, count // len(modes_to_generate))
        for i in range(gen_count):
            route = _generate_single_route(roads, center, target_mode, rng, city_id, i)
            if route:
                routes.append(route)

    return routes


def _generate_single_route(roads: list[dict], center: dict, mode: str, rng: random.Random, city_id: str, idx: int) -> dict | None:
    """Generate one route of the specified mode."""
    # Pick start road near center
    center_roads = sorted(roads, key=lambda r: _haversine(r["points"][0], center))[:max(20, len(roads)//10)]
    if not center_roads:
        return None

    start_road = rng.choice(center_roads)
    start_pt = start_road["points"][0]
    route_points = [start_pt]
    current_road = start_road
    used_ids = {start_road["id"]}
    # (road, points_appended) per hop so the circuit closer can trim the tail.
    segments = [(start_road, 1)]

    max_segments = {"cruise_sprint": 40, "time_trial": 60, "circuit": 50, "drift_run": 15}[mode]
    min_distance = {"cruise_sprint": 800, "time_trial": 1500, "circuit": 1000, "drift_run": 400}[mode]

    for _ in range(max_segments):
        end_pt = current_road["points"][-1]
        current_layer = current_road.get("layer", 0)
        candidates = []
        for r in roads:
            if r["id"] in used_ids:
                continue
            # Roads on different layers (bridge over street, tunnel under)
            # may pass within meters of each other but do not connect.
            if r.get("layer", 0) != current_layer:
                continue
            for pt in r["points"]:
                if _haversine(end_pt, pt) < 120:
                    candidates.append(r)
                    break
        if not candidates:
            break

        # For drift_run, prefer roads with many points (winding)
        if mode == "drift_run":
            candidates.sort(key=lambda r: len(r["points"]), reverse=True)
            next_road = candidates[0]
        else:
            next_road = rng.choice(candidates)

        used_ids.add(next_road["id"])
        connect_idx = 0
        for i, pt in enumerate(next_road["points"]):
            if _haversine(end_pt, pt) < 60:
                connect_idx = i
                break
        appended = next_road["points"][connect_idx+1:]
        route_points.extend(appended)
        segments.append((next_road, len(appended)))
        current_road = next_road

    dist = _route_length(route_points)
    if dist < min_distance:
        return None

    # For circuit, close the loop back to the start through the road graph.
    closure = None
    if mode == "circuit" and len(route_points) >= 2:
        route_points, closure = _close_circuit_loop(
            roads, route_points, segments, start_road,
            start_road.get("layer", 0), min_distance)
        dist = _route_length(route_points)

    route_id = f"{city_id}_{mode}_{idx+1:03d}"
    difficulties = ["easy", "medium", "hard", "extreme"]
    difficulty = difficulties[min(idx, len(difficulties)-1)]

    return {
        "route_id": route_id,
        "mode": mode,
        "name": f"{city_id.replace('_',' ').title()} {mode.replace('_',' ').title()} {idx+1}",
        "difficulty": difficulty,
        "distance_meters": round(dist),
        "start": route_points[0],
        "finish": route_points[-1],
        "points": route_points,
        **({"loop_closure": closure} if closure is not None else {}),
    }


# ---------------------------------------------------------------------------
# Circuit loop closure
# ---------------------------------------------------------------------------

def _build_point_index(roads: list[dict], layer: int, cell_m: float = 150.0):
    """Grid index of road points (same layer only) for fast neighbor lookups."""
    cell_deg = cell_m / 111320.0  # indexing only; exact checks use haversine
    index: dict[tuple[int, int], list[tuple[int, int]]] = {}
    for ri, r in enumerate(roads):
        if r.get("layer", 0) != layer:
            continue
        for pi, p in enumerate(r["points"]):
            key = (math.floor(p["lat"] / cell_deg), math.floor(p["lon"] / cell_deg))
            index.setdefault(key, []).append((ri, pi))
    return index, cell_deg


def _min_dist_road_to_point(road: dict, pt: dict) -> float:
    return min(_haversine(p, pt) for p in road["points"])


def _neighbor_roads(roads: list[dict], ri: int, index: dict, cell_deg: float, snap_m: float) -> list[int]:
    """Same-layer roads sharing a point within snap_m of any point of roads[ri]."""
    out: set[int] = set()
    for p in roads[ri]["points"]:
        klat = math.floor(p["lat"] / cell_deg)
        klon = math.floor(p["lon"] / cell_deg)
        for dlat in (-1, 0, 1):
            for dlon in (-1, 0, 1):
                for rj, pj in index.get((klat + dlat, klon + dlon), ()):
                    if rj == ri or rj in out:
                        continue
                    if _haversine(p, roads[rj]["points"][pj]) <= snap_m:
                        out.add(rj)
    return list(out)


def _shortest_return_roads(roads: list[dict], from_ri: int, to_ri: int, layer: int,
                           start_pt: dict, snap_m: float = LOOP_SNAP_M,
                           index: dict | None = None, cell_deg: float | None = None) -> dict:
    """Dijkstra over the same-layer road graph from one road to another.

    Returns a dict with:
        reached:  True if to_ri is reachable without leaving `layer`
        path:     road indices from from_ri (exclusive) to the target (inclusive);
                  on failure, the path to the reachable road nearest start_pt
        length_m: summed length of `path`
        gap_m:    0.0 when reached, else distance from the best reachable road
                  to start_pt (the residual loop gap)
    """
    if index is None or cell_deg is None:
        index, cell_deg = _build_point_index(roads, layer)

    if from_ri == to_ri:
        return {"reached": True, "path": [], "length_m": 0.0, "gap_m": 0.0}

    dist = {from_ri: 0.0}
    prev: dict[int, int] = {}
    pq = [(0.0, from_ri)]
    best_ri = from_ri
    best_gap = _min_dist_road_to_point(roads[from_ri], start_pt)
    reached = False
    pops = 0

    while pq and pops < _MAX_CLOSURE_POPS:
        d, ri = heapq.heappop(pq)
        if d > dist.get(ri, math.inf):
            continue
        pops += 1
        if ri == to_ri:
            reached = True
            break
        gap = _min_dist_road_to_point(roads[ri], start_pt)
        if gap < best_gap:
            best_gap, best_ri = gap, ri
        for ni in _neighbor_roads(roads, ri, index, cell_deg, snap_m):
            nd = d + _route_length(roads[ni]["points"])
            if nd < dist.get(ni, math.inf):
                dist[ni] = nd
                prev[ni] = ri
                heapq.heappush(pq, (nd, ni))

    target = to_ri if reached else best_ri
    path = []
    cur = target
    while cur != from_ri:
        path.append(cur)
        cur = prev.get(cur)
        if cur is None:
            break
    path.reverse()

    return {
        "reached": reached,
        "path": path,
        "length_m": dist.get(target, 0.0),
        "gap_m": 0.0 if reached else best_gap,
    }


def _stitch_return_points(path_roads: list[dict], end_pt: dict, target_pt: dict,
                          exact_final: bool) -> list[dict]:
    """Walk the return roads from end_pt toward target_pt, appending points."""
    pts: list[dict] = []
    cur = end_pt
    n = len(path_roads)
    for j, road in enumerate(path_roads):
        rp = road["points"]
        idx = min(range(len(rp)), key=lambda i: _haversine(cur, rp[i]))
        if j < n - 1:
            nxt = path_roads[j + 1]["points"]
            aim = min(nxt, key=lambda p: _haversine(cur, p))
        else:
            aim = target_pt
        forward = _haversine(rp[-1], aim) <= _haversine(rp[0], aim)
        seg = rp[idx+1:] if forward else rp[idx-1::-1]
        if not seg:
            seg = [rp[-1] if forward else rp[0]]
        elif j == n - 1:
            # Last road: stop at the point nearest the target so the final
            # jump stays short (keeps the total within the length budget).
            best = min(range(len(seg)), key=lambda k: _haversine(seg[k], aim))
            seg = seg[:best + 1]
        pts.extend(seg)
        cur = pts[-1]
    if exact_final:
        pts.append({"lat": target_pt["lat"], "lon": target_pt["lon"]})
    return pts


def _close_circuit_loop(roads: list[dict], route_points: list[dict],
                        segments: list[tuple[dict, int]], start_road: dict,
                        layer: int, min_distance: float,
                        max_route_m: float = MAX_ROUTE_LENGTH_M,
                        snap_m: float = LOOP_SNAP_M) -> tuple[list[dict], dict]:
    """Close a circuit route back to its start through the same-layer road graph.

    Trims the outward tail if closure would exceed max_route_m; degrades to a
    best-effort approach (nearest reachable road) when no same-layer path
    exists. Never raises; always returns (points, closure_metadata).
    """
    start_pt = route_points[0]
    road_idx = {r["id"]: i for i, r in enumerate(roads)}
    index, cell_deg = _build_point_index(roads, layer)
    trimmed = 0

    def _meta(closed: bool, reason: str, residual: float, ret_ids: list, ret_len: float) -> dict:
        return {
            "closed": closed,
            "reason": reason,  # "ok" | "no_same_layer_path" | "length_cap"
            "residual_gap_m": round(residual, 1),
            "return_path_road_ids": ret_ids,
            "return_path_length_m": round(ret_len, 1),
            "trimmed_outward_segments": trimmed,
            "layer": layer,
            "max_route_m": max_route_m,
        }

    while True:
        end_pt = route_points[-1]
        dist = _route_length(route_points)
        if _haversine(end_pt, start_pt) <= snap_m and dist <= max_route_m:
            route_points.append({"lat": start_pt["lat"], "lon": start_pt["lon"]})
            return route_points, _meta(True, "ok", 0.0, [], 0.0)

        end_ri = road_idx[segments[-1][0]["id"]]
        res = _shortest_return_roads(roads, end_ri, road_idx[start_road["id"]],
                                     layer, start_pt, snap_m, index, cell_deg)

        if not res["reached"]:
            # No same-layer path: approach the start as closely as the graph
            # allows. (Trimming cannot help — the outward path is connected,
            # so reachability of the start never changes by shortening it.)
            ret_roads = [roads[i] for i in res["path"]]
            if ret_roads:
                route_points.extend(_stitch_return_points(ret_roads, end_pt, start_pt,
                                                          exact_final=False))
            return route_points, _meta(False, "no_same_layer_path", res["gap_m"],
                                       [roads[i]["id"] for i in res["path"]],
                                       res["length_m"])

        if dist + res["length_m"] <= max_route_m:
            ret_roads = [roads[i] for i in res["path"]]
            tail = _stitch_return_points(ret_roads, end_pt, start_pt, exact_final=True)
            if _route_length(route_points + tail) <= max_route_m:
                residual = _haversine(tail[-2], start_pt) if len(tail) >= 2 else 0.0
                route_points.extend(tail)
                return route_points, _meta(True, "ok", residual,
                                           [roads[i]["id"] for i in res["path"]],
                                           res["length_m"])

        # Over budget: trim the outward tail and retry from an earlier end.
        if len(segments) > 1:
            _, n = segments.pop()
            if n:
                del route_points[len(route_points) - n:]
            trimmed += 1
            if _route_length(route_points) >= min_distance and len(route_points) > 2:
                continue
        return route_points, _meta(False, "length_cap",
                                   _haversine(route_points[-1], start_pt), [], 0.0)


def place_checkpoints(route: dict, spacing_meters: float = 300.0, gate_radius: float = 18.0) -> list[dict]:
    """Place checkpoint gates along a route at regular intervals."""
    points = route.get("points", [])
    if len(points) < 2:
        return []

    checkpoints = []
    accumulated = 0.0
    cp_idx = 1

    for i in range(1, len(points)):
        a, b = points[i-1], points[i]
        seg_len = _haversine(a, b)
        accumulated += seg_len

        if accumulated >= spacing_meters:
            # Place checkpoint at point b
            heading = _heading(a, b)
            checkpoints.append({
                "id": f"{route['route_id']}_cp_{cp_idx:03d}",
                "lat": b["lat"],
                "lon": b["lon"],
                "heading": round(heading, 2),
                "radius_meters": gate_radius,
                "type": "gate",
                "distance_from_start_m": round(sum(_haversine(points[j], points[j+1]) for j in range(i)), 1),
            })
            accumulated = 0.0
            cp_idx += 1

    return checkpoints


def _heading(a: dict, b: dict) -> float:
    """Compute compass heading from a to b in degrees."""
    dlon = math.radians(b["lon"] - a["lon"])
    lat1 = math.radians(a["lat"])
    lat2 = math.radians(b["lat"])
    x = math.sin(dlon) * math.cos(lat2)
    y = math.cos(lat1) * math.sin(lat2) - math.sin(lat1) * math.cos(lat2) * math.cos(dlon)
    heading = math.degrees(math.atan2(x, y))
    return (heading + 360) % 360
