#!/usr/bin/env python3
"""Extract building footprints and heights from OSM data."""

import json
import math
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


def _geo_to_local(lat: float, lon: float, origin_lat: float, origin_lon: float) -> tuple[float, float]:
    """Equirectangular projection: (lat, lon) -> local (x, y) in meters."""
    meters_per_lon = 111320.0 * math.cos(math.radians(origin_lat))
    meters_per_lat = 111320.0
    x = (lon - origin_lon) * meters_per_lon
    y = (lat - origin_lat) * meters_per_lat
    return x, y


def _parse_height(tags: dict[str, str]) -> float:
    """Extract building height in meters from OSM tags."""
    # Direct height tag
    if "height" in tags:
        h = tags["height"]
        # Handle values like "12.5", "12.5 m", "41 ft"
        h = h.replace("m", "").replace(" ", "").strip()
        try:
            val = float(h)
            # If value seems like feet (>50), convert
            if val > 50:
                return val * 0.3048
            return val
        except ValueError:
            pass

    # Building levels * 3m per level
    if "building:levels" in tags:
        try:
            levels = int(tags["building:levels"])
            return levels * 3.0
        except ValueError:
            pass

    # Default heights by building type
    type_defaults = {
        "industrial": 12.0,
        "warehouse": 10.0,
        "commercial": 15.0,
        "retail": 12.0,
        "office": 18.0,
        "apartments": 12.0,
        "school": 10.0,
        "hospital": 15.0,
        "church": 12.0,
        "garage": 3.5,
        "shed": 3.0,
    }

    building_type = tags.get("building", "")
    return type_defaults.get(building_type, 8.0)


def _classify_building_type(tags: dict[str, str]) -> str:
    """Classify building into a game-relevant category."""
    building = tags.get("building", "")

    if building in ("house", "detached", "semidetached_house", "bungalow", "terraced"):
        return "residential_low"
    if building in ("apartments", "residential"):
        return "residential_mid"
    if building in ("commercial", "retail", "supermarket", "mall"):
        return "commercial"
    if building in ("office", "civic", "public"):
        return "office"
    if building in ("industrial", "warehouse", "factory"):
        return "industrial"
    if building in ("school", "university", "college"):
        return "education"
    if building in ("hospital", "clinic"):
        return "healthcare"
    if building in ("church", "mosque", "temple", "synagogue"):
        return "religious"
    if building in ("parking", "garage"):
        return "parking"
    if building in ("shed", "roof", "greenhouse"):
        return "minor"

    return "mixed"


def extract_buildings(osm_path: Path, origin_lat: float, origin_lon: float) -> list[dict[str, Any]]:
    """Parse OSM and extract building footprints with heights."""
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
        if len(nds) < 3:
            continue

        # Build footprint polygon in local meters
        footprint = []
        for nid in nds:
            if nid in nodes:
                lat, lon = nodes[nid]
                x, y = _geo_to_local(lat, lon, origin_lat, origin_lon)
                footprint.append({"x": round(x, 2), "y": round(y, 2)})

        if len(footprint) < 3:
            continue

        height = _parse_height(tags)
        btype = _classify_building_type(tags)
        name = tags.get("name", "")
        addr = tags.get("addr:street", "")

        # Calculate approximate footprint area (shoelace formula)
        area = 0.0
        n = len(footprint)
        for i in range(n):
            j = (i + 1) % n
            area += footprint[i]["x"] * footprint[j]["y"]
            area -= footprint[j]["x"] * footprint[i]["y"]
        area = abs(area) / 2.0

        buildings.append({
            "id": elem.get("id"),
            "type": btype,
            "name": name,
            "address": addr,
            "height": round(height, 2),
            "footprint": footprint,
            "area_m2": round(area, 2),
            "num_points": len(footprint),
        })

    return buildings


def export_buildings(buildings: list[dict[str, Any]], output_path: Path) -> None:
    """Export building data to JSON."""
    with open(output_path, "w") as f:
        json.dump({
            "building_count": len(buildings),
            "buildings": buildings,
        }, f, indent=2)
