#!/usr/bin/env python3
"""Generate Cruise Sprint routes from the road graph."""

import random
from typing import Any


def _haversine(a: dict, b: dict) -> float:
    """Approximate distance in meters between two lat/lon points."""
    import math
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


def generate_cruise_sprint(road_graph: dict[str, Any]) -> list[dict]:
    """Generate 3 short Cruise Sprint routes through Akron."""
    roads = road_graph.get("roads", [])
    if len(roads) < 3:
        return []

    routes = []
    rng = random.Random(42)  # reproducible

    for route_idx in range(2):
        # Pick a start road near the center (roughly 41.08, -81.52)
        center_roads = sorted(roads, key=lambda r: abs(r["points"][0]["lat"] - 41.08) + abs(r["points"][0]["lon"] + 81.52))[:20]
        start_road = rng.choice(center_roads)
        start_pt = start_road["points"][0]

        # Build a route by following connected roads
        route_points = [start_pt]
        current_road = start_road
        used_ids = {start_road["id"]}

        for _ in range(30):  # up to 30 road segments
            # Find roads that connect to the end of current road
            end_pt = current_road["points"][-1]
            candidates = []
            for r in roads:
                if r["id"] in used_ids:
                    continue
                # Check if any point of r is close to end_pt
                for pt in r["points"]:
                    if _haversine(end_pt, pt) < 100:  # within 100m
                        candidates.append(r)
                        break
            if not candidates:
                break
            next_road = rng.choice(candidates)
            used_ids.add(next_road["id"])
            # Append points, avoiding duplication of the connection point
            connect_idx = 0
            for i, pt in enumerate(next_road["points"]):
                if _haversine(end_pt, pt) < 50:
                    connect_idx = i
                    break
            route_points.extend(next_road["points"][connect_idx+1:])
            current_road = next_road

        dist = _route_length(route_points)
        if dist < 800:
            continue

        routes.append({
            "route_id": f"akron_cruise_sprint_{route_idx+1:03d}",
            "mode": "cruise_sprint",
            "name": f"Akron Sprint {route_idx + 1}",
            "difficulty": "beta",
            "distance_meters": round(dist),
            "start": route_points[0],
            "finish": route_points[-1],
            "points": route_points,
            "checkpoints": [],
            "scoring": {
                "time_bonus": True,
                "clean_driving_bonus": True,
                "missed_checkpoint_penalty": 10,
            },
        })

    return routes
