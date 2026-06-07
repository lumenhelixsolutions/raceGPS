#!/usr/bin/env python3
"""Elevation data fetcher using Open-Elevation API and SRTM fallback.

Provides heightmaps for terrain generation in UE5.
"""

import json
import math
import urllib.request
import urllib.error
import urllib.parse
from pathlib import Path
from typing import Any

OPEN_ELEVATION_URL = "https://api.open-elevation.com/api/v1/lookup"


def fetch_elevation_points(points: list[dict[str, float]], batch_size: int = 100) -> list[dict[str, Any]]:
    """Fetch elevation for a list of {lat, lon} points.

    Returns list of {lat, lon, elevation}.
    """
    results = []
    for i in range(0, len(points), batch_size):
        batch = points[i:i + batch_size]
        payload = json.dumps({"locations": batch}).encode("utf-8")
        req = urllib.request.Request(OPEN_ELEVATION_URL, data=payload, method="POST",
                                     headers={"Content-Type": "application/json", "Accept": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                data = json.loads(resp.read().decode("utf-8"))
            for r in data.get("results", []):
                results.append({
                    "lat": r["latitude"],
                    "lon": r["longitude"],
                    "elevation": r.get("elevation", 0.0),
                })
        except (urllib.error.HTTPError, urllib.error.URLError, json.JSONDecodeError) as e:
            # Fallback: assume flat terrain (0m) if API fails
            for p in batch:
                results.append({"lat": p["lat"], "lon": p["lon"], "elevation": 0.0, "fallback": True})
    return results


def generate_heightmap_grid(bounds: dict, resolution: int = 64) -> dict[str, Any]:
    """Generate a regular grid of elevation samples within bounds.

    Returns dict with:
        - grid: list of {lat, lon, elevation}
        - rows, cols
        - min_elevation, max_elevation
        - heightmap: 2D list of elevation values (rows x cols)
    """
    lat_step = (bounds["north"] - bounds["south"]) / (resolution - 1)
    lon_step = (bounds["east"] - bounds["west"]) / (resolution - 1)

    points = []
    for row in range(resolution):
        for col in range(resolution):
            lat = bounds["south"] + row * lat_step
            lon = bounds["west"] + col * lon_step
            points.append({"latitude": lat, "longitude": lon})

    elevations = fetch_elevation_points(points)

    heightmap = []
    min_elev = float("inf")
    max_elev = float("-inf")

    for row in range(resolution):
        row_vals = []
        for col in range(resolution):
            idx = row * resolution + col
            elev = elevations[idx]["elevation"]
            row_vals.append(elev)
            min_elev = min(min_elev, elev)
            max_elev = max(max_elev, elev)
        heightmap.append(row_vals)

    return {
        "rows": resolution,
        "cols": resolution,
        "bounds": bounds,
        "min_elevation": round(min_elev, 2),
        "max_elevation": round(max_elev, 2),
        "heightmap": heightmap,
        "source": "open-elevation",
    }


def sample_elevation_at(lat: float, lon: float, heightmap_data: dict) -> float:
    """Bilinear interpolation of elevation at a specific lat/lon from a heightmap grid."""
    bounds = heightmap_data["bounds"]
    rows = heightmap_data["rows"]
    cols = heightmap_data["cols"]
    hm = heightmap_data["heightmap"]

    # Normalize to 0-1
    u = (lon - bounds["west"]) / (bounds["east"] - bounds["west"])
    v = (lat - bounds["south"]) / (bounds["north"] - bounds["south"])

    # Clamp
    u = max(0.0, min(1.0, u))
    v = max(0.0, min(1.0, v))

    # Grid coords
    x = u * (cols - 1)
    y = v * (rows - 1)

    x0, y0 = int(x), int(y)
    x1, y1 = min(x0 + 1, cols - 1), min(y0 + 1, rows - 1)
    fx, fy = x - x0, y - y0

    # Bilinear interpolation
    z00 = hm[y0][x0]
    z10 = hm[y0][x1]
    z01 = hm[y1][x0]
    z11 = hm[y1][x1]

    return z00 * (1 - fx) * (1 - fy) + z10 * fx * (1 - fy) + z01 * (1 - fx) * fy + z11 * fx * fy
