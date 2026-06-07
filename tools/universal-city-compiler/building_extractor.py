#!/usr/bin/env python3
"""Extract building footprints and metadata from OSM data."""

import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


def extract_buildings(osm_path: Path, origin_lat: float = 0.0, origin_lon: float = 0.0) -> list[dict[str, Any]]:
    """Extract building footprints as lists of lat/lon points."""
    tree = ET.parse(osm_path)
    root = tree.getroot()

    nodes: dict[str, tuple[float, float]] = {}
    for elem in root:
        if elem.tag == "node":
            nid = elem.get("id")
            lat = float(elem.get("lat", 0))
            lon = float(elem.get("lon", 0))
            nodes[nid] = (lat, lon)

    buildings = []
    for elem in root:
        if elem.tag != "way":
            continue
        tags = {t.get("k"): t.get("v") for t in elem if t.tag == "tag"}
        if "building" not in tags:
            continue

        nds = [n.get("ref") for n in elem if n.tag == "nd"]
        way_nodes = [nodes[nid] for nid in nds if nid in nodes]
        if len(way_nodes) < 3:
            continue

        # Compute centroid
        lat = sum(n[0] for n in way_nodes) / len(way_nodes)
        lon = sum(n[1] for n in way_nodes) / len(way_nodes)

        # Estimate height from levels or building type
        levels = tags.get("building:levels", "")
        height = tags.get("height", "")
        estimated_height = _estimate_height(tags.get("building", ""), levels, height)

        buildings.append({
            "id": elem.get("id"),
            "lat": lat,
            "lon": lon,
            "footprint": [{"lat": n[0], "lon": n[1]} for n in way_nodes],
            "building_type": tags.get("building", "yes"),
            "name": tags.get("name", ""),
            "levels": int(levels) if levels.isdigit() else None,
            "height_meters": estimated_height,
            "material": tags.get("building:material", "concrete"),
        })

    return buildings


def _estimate_height(building_type: str, levels: str, height: str) -> float:
    """Estimate building height in meters."""
    if height:
        try:
            return float(height.replace("m", "").strip())
        except ValueError:
            pass
    if levels:
        try:
            return int(levels) * 3.5
        except ValueError:
            pass
    defaults = {
        "yes": 8.0, "house": 8.0, "residential": 8.0, "apartments": 12.0,
        "commercial": 12.0, "retail": 8.0, "office": 15.0, "industrial": 10.0,
        "warehouse": 12.0, "school": 10.0, "university": 15.0, "hospital": 18.0,
        "church": 15.0, "cathedral": 30.0, "mosque": 20.0, "temple": 18.0,
        "stadium": 35.0, "theatre": 18.0, "hotel": 20.0, "parking": 6.0,
        "garage": 6.0, "shed": 3.5, "roof": 4.0, "terrace": 8.0,
        "detached": 8.0, "semidetached_house": 8.0, "bungalow": 5.0,
        "skyscraper": 120.0, "tower": 50.0, "bridge": 15.0,
    }
    return defaults.get(building_type, 8.0)


def export_buildings(buildings: list[dict], path: Path) -> None:
    """Save buildings to JSON."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        import json
        json.dump({"buildings": buildings, "count": len(buildings)}, f, indent=2)
