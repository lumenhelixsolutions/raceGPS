#!/usr/bin/env python3
"""Extract water bodies (rivers, lakes, coastlines) from OSM data."""

import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


def extract_water(osm_path: Path, origin_lat: float = 0.0, origin_lon: float = 0.0) -> dict[str, Any]:
    """Extract water features from OSM.

    Returns dict with:
        - rivers: list of {id, name, points[{lat,lon}]}
        - lakes: list of {id, name, points[{lat,lon}], area_approx_m2}
        - coastlines: list of {id, points[{lat,lon}]}
    """
    tree = ET.parse(osm_path)
    root = tree.getroot()

    nodes: dict[str, tuple[float, float]] = {}
    for elem in root:
        if elem.tag == "node":
            nid = elem.get("id")
            lat = float(elem.get("lat", 0))
            lon = float(elem.get("lon", 0))
            nodes[nid] = (lat, lon)

    rivers = []
    lakes = []
    coastlines = []

    for elem in root:
        if elem.tag != "way":
            continue
        tags = {t.get("k"): t.get("v") for t in elem if t.tag == "tag"}

        nds = [n.get("ref") for n in elem if n.tag == "nd"]
        way_nodes = [nodes[nid] for nid in nds if nid in nodes]
        if len(way_nodes) < 2:
            continue

        points = [{"lat": n[0], "lon": n[1]} for n in way_nodes]

        # Rivers / streams / canals
        if tags.get("waterway") in ("river", "stream", "canal"):
            rivers.append({
                "id": elem.get("id"),
                "name": tags.get("name", ""),
                "type": tags.get("waterway", "river"),
                "points": points,
            })

        # Lakes / reservoirs / ponds
        if tags.get("natural") == "water" or tags.get("water") in ("lake", "reservoir", "pond"):
            # Approximate area using shoelace formula
            area = _polygon_area(way_nodes)
            lakes.append({
                "id": elem.get("id"),
                "name": tags.get("name", ""),
                "type": tags.get("water", "lake"),
                "points": points,
                "area_approx_m2": round(area, 1),
            })

        # Coastline
        if tags.get("natural") == "coastline":
            coastlines.append({
                "id": elem.get("id"),
                "points": points,
            })

    return {
        "rivers": rivers,
        "lakes": lakes,
        "coastlines": coastlines,
        "river_count": len(rivers),
        "lake_count": len(lakes),
        "coastline_count": len(coastlines),
    }


def _polygon_area(nodes: list[tuple[float, float]]) -> float:
    """Approximate polygon area in m² using shoelace on lat/lon (rough)."""
    if len(nodes) < 3:
        return 0.0
    # Convert to approximate meters
    avg_lat = sum(n[0] for n in nodes) / len(nodes)
    m_per_deg_lat = 111320.0
    m_per_deg_lon = 111320.0 * math.cos(math.radians(avg_lat))

    area = 0.0
    n = len(nodes)
    for i in range(n):
        x1 = nodes[i][1] * m_per_deg_lon
        y1 = nodes[i][0] * m_per_deg_lat
        x2 = nodes[(i + 1) % n][1] * m_per_deg_lon
        y2 = nodes[(i + 1) % n][0] * m_per_deg_lat
        area += x1 * y2 - x2 * y1
    return abs(area) / 2.0


import math
