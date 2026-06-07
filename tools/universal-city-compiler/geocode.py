#!/usr/bin/env python3
"""Nominatim geocoding — resolve city names to bounding boxes."""

import time
import urllib.request
import urllib.parse
import json
from typing import Optional

NOMINATIM_URL = "https://nominatim.openstreetmap.org/search"


def geocode_city(city_name: str, limit: int = 1) -> Optional[dict]:
    """Resolve a city name to lat/lon bounds and display name.

    Returns dict with:
        - name, display_name, lat, lon
        - boundingbox [south, north, west, east]
        - bounds dict {south, west, north, east}
        - osm_type, osm_id, place_id
    """
    params = urllib.parse.urlencode({
        "q": city_name,
        "format": "json",
        "limit": limit,
        "addressdetails": 1,
        "bounded": 0,
    })
    url = f"{NOMINATIM_URL}?{params}"
    req = urllib.request.Request(url, method="GET")
    req.add_header("User-Agent", "raceGPS-Universal-Compiler/1.0")

    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                data = json.loads(resp.read().decode("utf-8"))
            if not data:
                return None

            result = data[0]
            bb = result.get("boundingbox", [])
            if len(bb) >= 4:
                bounds = {
                    "south": float(bb[0]),
                    "north": float(bb[1]),
                    "west": float(bb[2]),
                    "east": float(bb[3]),
                }
            else:
                lat = float(result.get("lat", 0))
                lon = float(result.get("lon", 0))
                # Approximate 5km box around center
                bounds = {
                    "south": lat - 0.045,
                    "north": lat + 0.045,
                    "west": lon - 0.045,
                    "east": lon + 0.045,
                }

            return {
                "name": result.get("name", city_name),
                "display_name": result.get("display_name", city_name),
                "lat": float(result.get("lat", 0)),
                "lon": float(result.get("lon", 0)),
                "bounds": bounds,
                "osm_type": result.get("osm_type"),
                "osm_id": result.get("osm_id"),
                "place_id": result.get("place_id"),
            }
        except urllib.error.HTTPError as e:
            if e.code == 429:
                wait = 2 * (attempt + 1)
                print(f"      Nominatim rate limited. Waiting {wait}s...")
                time.sleep(wait)
            else:
                raise
    raise RuntimeError("Nominatim failed after 3 retries")


def expand_bounds(bounds: dict, padding_km: float = 2.0) -> dict:
    """Expand bounds by padding_km in all directions."""
    km_per_deg_lat = 111.0
    km_per_deg_lon = 111.32  # approximate at mid-latitudes
    pad_lat = padding_km / km_per_deg_lat
    pad_lon = padding_km / km_per_deg_lon
    return {
        "south": bounds["south"] - pad_lat,
        "north": bounds["north"] + pad_lat,
        "west": bounds["west"] - pad_lon,
        "east": bounds["east"] + pad_lon,
    }


def bounds_from_center(lat: float, lon: float, radius_km: float = 5.0) -> dict:
    """Create a square bounding box from center + radius."""
    km_per_deg_lat = 111.0
    km_per_deg_lon = 111.32
    dlat = radius_km / km_per_deg_lat
    dlon = radius_km / km_per_deg_lon
    return {
        "south": lat - dlat,
        "north": lat + dlat,
        "west": lon - dlon,
        "east": lon + dlon,
    }
