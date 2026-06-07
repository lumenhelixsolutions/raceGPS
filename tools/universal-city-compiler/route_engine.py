#!/usr/bin/env python3
"""Generic route generator with multiple race modes and smart waypoint selection."""

import random
import math
from typing import Any


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

    max_segments = {"cruise_sprint": 40, "time_trial": 60, "circuit": 50, "drift_run": 15}[mode]
    min_distance = {"cruise_sprint": 800, "time_trial": 1500, "circuit": 1000, "drift_run": 400}[mode]

    for _ in range(max_segments):
        end_pt = current_road["points"][-1]
        candidates = []
        for r in roads:
            if r["id"] in used_ids:
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
        route_points.extend(next_road["points"][connect_idx+1:])
        current_road = next_road

    dist = _route_length(route_points)
    if dist < min_distance:
        return None

    # For circuit, try to loop back
    if mode == "circuit" and len(route_points) > 2:
        if _haversine(route_points[-1], route_points[0]) > 500:
            # Not a good loop; append path back
            pass  # keep as-is for now

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
    }


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
