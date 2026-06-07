#!/usr/bin/env python3
"""Generate vegetation scatter zones from OSM landuse and natural features."""

import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


def extract_vegetation_zones(osm_path: Path) -> list[dict[str, Any]]:
    """Extract areas suitable for vegetation scatter.

    Returns list of scatter zones:
        - id, type (forest, grass, scrub), points[{lat,lon}], density
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

    zones = []
    for elem in root:
        if elem.tag != "way":
            continue
        tags = {t.get("k"): t.get("v") for t in elem if t.tag == "tag"}

        veg_type = None
        density = 0.5

        if tags.get("landuse") == "forest":
            veg_type = "forest"
            density = 0.9
        elif tags.get("natural") == "wood":
            veg_type = "forest"
            density = 0.85
        elif tags.get("landuse") == "grass":
            veg_type = "grass"
            density = 0.4
        elif tags.get("natural") == "grassland":
            veg_type = "grass"
            density = 0.3
        elif tags.get("natural") == "scrub":
            veg_type = "scrub"
            density = 0.6
        elif tags.get("landuse") == "meadow":
            veg_type = "grass"
            density = 0.35
        elif tags.get("leisure") == "park":
            veg_type = "mixed"
            density = 0.5
        elif tags.get("natural") == "heath":
            veg_type = "scrub"
            density = 0.45

        if not veg_type:
            continue

        nds = [n.get("ref") for n in elem if n.tag == "nd"]
        way_nodes = [nodes[nid] for nid in nds if nid in nodes]
        if len(way_nodes) < 3:
            continue

        # Compute centroid
        lat = sum(n[0] for n in way_nodes) / len(way_nodes)
        lon = sum(n[1] for n in way_nodes) / len(way_nodes)

        zones.append({
            "id": elem.get("id"),
            "type": veg_type,
            "name": tags.get("name", ""),
            "lat": lat,
            "lon": lon,
            "points": [{"lat": n[0], "lon": n[1]} for n in way_nodes],
            "density": density,
        })

    return zones
