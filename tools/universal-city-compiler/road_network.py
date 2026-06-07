#!/usr/bin/env python3
"""Generic semantic road graph builder from OSM data."""

import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


def build_road_graph(osm_path: Path, origin_lat: float = 0.0, origin_lon: float = 0.0) -> dict[str, Any]:
    """Parse OSM and build a semantic road graph with world-space coordinates."""
    tree = ET.parse(osm_path)
    root = tree.getroot()

    nodes: dict[str, tuple[float, float]] = {}
    ways: list[dict] = []
    relations: list[dict] = []

    for elem in root:
        if elem.tag == "node":
            nid = elem.get("id")
            lat = float(elem.get("lat", 0))
            lon = float(elem.get("lon", 0))
            nodes[nid] = (lat, lon)
        elif elem.tag == "way":
            tags = {t.get("k"): t.get("v") for t in elem if t.tag == "tag"}
            nds = [n.get("ref") for n in elem if n.tag == "nd"]
            ways.append({"id": elem.get("id"), "tags": tags, "nodes": nds})
        elif elem.tag == "relation":
            tags = {t.get("k"): t.get("v") for t in elem if t.tag == "tag"}
            members = [{"type": m.get("type"), "ref": m.get("ref"), "role": m.get("role")}
                       for m in elem if m.tag == "member"]
            relations.append({"id": elem.get("id"), "tags": tags, "members": members})

    # Build roads from highway ways
    roads = []
    node_to_ways: dict[str, list[str]] = {}
    for w in ways:
        if "highway" not in w["tags"]:
            continue

        highway = w["tags"]["highway"]
        if highway in ("footway", "cycleway", "path", "steps", "corridor", "track"):
            continue

        points = []
        for nid in w["nodes"]:
            if nid in nodes:
                points.append({"lat": nodes[nid][0], "lon": nodes[nid][1]})
            node_to_ways.setdefault(nid, []).append(w["id"])

        if len(points) < 2:
            continue

        width_map = {
            "motorway": 14, "motorway_link": 10, "trunk": 12, "trunk_link": 9,
            "primary": 10, "primary_link": 9, "secondary": 9, "secondary_link": 8,
            "tertiary": 8, "tertiary_link": 7, "residential": 7,
            "unclassified": 7, "service": 5, "living_street": 6, "pedestrian": 6,
        }

        lanes = w["tags"].get("lanes", "")
        try:
            lane_count = int(lanes.split(";")[0])
        except ValueError:
            lane_count = 2 if highway in ("motorway", "trunk", "primary") else 1

        roads.append({
            "id": w["id"],
            "name": w["tags"].get("name", ""),
            "highway": highway,
            "points": points,
            "width": width_map.get(highway, 7),
            "lane_count": lane_count,
            "one_way": w["tags"].get("oneway", "no") == "yes",
            "max_speed": _parse_maxspeed(w["tags"].get("maxspeed", "")),
            "surface": w["tags"].get("surface", "asphalt"),
        })

    # Find intersections (nodes shared by 2+ roads)
    intersections = []
    for nid, way_ids in node_to_ways.items():
        if len(way_ids) >= 2 and nid in nodes:
            intersections.append({
                "node_id": nid,
                "lat": nodes[nid][0],
                "lon": nodes[nid][1],
                "road_ids": way_ids,
            })

    # Compute world bounds from all road points
    all_lats = [p["lat"] for r in roads for p in r["points"]]
    all_lons = [p["lon"] for r in roads for p in r["points"]]
    bounds = {
        "south": min(all_lats) if all_lats else -1,
        "north": max(all_lats) if all_lats else 1,
        "west": min(all_lons) if all_lons else -1,
        "east": max(all_lons) if all_lons else 1,
    }

    return {
        "roads": roads,
        "intersections": intersections,
        "bounds": bounds,
        "road_count": len(roads),
        "intersection_count": len(intersections),
        "origin": {"lat": origin_lat, "lon": origin_lon},
    }


def _parse_maxspeed(val: str) -> int:
    """Parse OSM maxspeed tag to km/h integer."""
    if not val:
        return 50
    val = val.strip().lower()
    if val == "none":
        return 250
    if "mph" in val:
        try:
            return int(float(val.replace("mph", "").strip()) * 1.60934)
        except ValueError:
            return 50
    try:
        return int(float(val))
    except ValueError:
        return 50
