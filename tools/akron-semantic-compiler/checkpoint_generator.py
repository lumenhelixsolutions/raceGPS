#!/usr/bin/env python3
"""Place checkpoint gates along generated routes."""

import math
from typing import Any


def _haversine(a: dict, b: dict) -> float:
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


def _heading_at_point(points: list[dict], idx: int) -> float:
    """Compute heading (degrees, 0=north, clockwise) at point idx."""
    if idx >= len(points) - 1:
        idx = len(points) - 2
    a, b = points[idx], points[idx + 1]
    dx = b["lon"] - a["lon"]
    dy = b["lat"] - a["lat"]
    heading = (90 - math.degrees(math.atan2(dy, dx))) % 360
    return heading


def place_checkpoints(route: dict[str, Any], _road_graph: dict[str, Any]) -> list[dict]:
    """Place 3-5 checkpoints evenly along the route."""
    points = route.get("points", [])
    if len(points) < 4:
        return []

    total_dist = _route_length(points)
    num_checkpoints = max(3, min(5, int(total_dist / 400)))
    spacing = total_dist / (num_checkpoints + 1)

    checkpoints = []
    dist_acc = 0
    next_cp_dist = spacing
    cp_idx = 1

    for i in range(len(points) - 1):
        seg_dist = _haversine(points[i], points[i + 1])
        while dist_acc + seg_dist >= next_cp_dist and cp_idx <= num_checkpoints:
            t = (next_cp_dist - dist_acc) / seg_dist
            lat = points[i]["lat"] + (points[i + 1]["lat"] - points[i]["lat"]) * t
            lon = points[i]["lon"] + (points[i + 1]["lon"] - points[i]["lon"]) * t
            heading = _heading_at_point(points, i)

            checkpoints.append({
                "id": f"cp_{cp_idx:03d}",
                "lat": round(lat, 6),
                "lon": round(lon, 6),
                "heading": round(heading, 1),
                "radius_meters": 18,
                "type": "gate",
            })
            cp_idx += 1
            next_cp_dist += spacing
        dist_acc += seg_dist

    return checkpoints
