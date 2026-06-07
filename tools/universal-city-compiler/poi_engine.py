#!/usr/bin/env python3
"""Generic POI classifier from OSM data."""

import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


def classify_pois(osm_path: Path) -> list[dict[str, Any]]:
    """Extract and classify POIs from OSM."""
    tree = ET.parse(osm_path)
    root = tree.getroot()

    nodes: dict[str, tuple[float, float]] = {}
    pois = []

    for elem in root:
        if elem.tag == "node":
            nid = elem.get("id")
            lat = float(elem.get("lat", 0))
            lon = float(elem.get("lon", 0))
            nodes[nid] = (lat, lon)

            tags = {t.get("k"): t.get("v") for t in elem if t.tag == "tag"}
            poi_type = _classify_tags(tags)
            if poi_type:
                pois.append({
                    "id": nid,
                    "lat": lat,
                    "lon": lon,
                    "type": poi_type,
                    "subtype": _subtype_tags(tags),
                    "name": tags.get("name", ""),
                    "tags": tags,
                })
        elif elem.tag == "way":
            tags = {t.get("k"): t.get("v") for t in elem if t.tag == "tag"}
            poi_type = _classify_tags(tags)
            if poi_type:
                nds = [n.get("ref") for n in elem if n.tag == "nd"]
                way_nodes = [nodes[nid] for nid in nds if nid in nodes]
                if way_nodes:
                    lat = sum(n[0] for n in way_nodes) / len(way_nodes)
                    lon = sum(n[1] for n in way_nodes) / len(way_nodes)
                    pois.append({
                        "id": elem.get("id"),
                        "lat": lat,
                        "lon": lon,
                        "type": poi_type,
                        "subtype": _subtype_tags(tags),
                        "name": tags.get("name", ""),
                        "tags": tags,
                    })

    return pois


def _classify_tags(tags: dict[str, str]) -> str | None:
    """Map OSM tags to raceGPS POI types."""
    if tags.get("tourism") in ("attraction", "museum", "artwork", "viewpoint", "zoo", "aquarium"):
        return "landmark"
    if tags.get("historic"):
        return "landmark"
    if tags.get("amenity") in ("restaurant", "cafe", "bar", "fast_food", "pub", "food_court"):
        return "food"
    if tags.get("amenity") in ("fuel", "parking", "car_wash"):
        return "service"
    if tags.get("shop"):
        return "shop"
    if tags.get("amenity") in ("school", "university", "college", "library", "kindergarten"):
        return "education"
    if tags.get("amenity") in ("hospital", "clinic", "pharmacy", "doctors", "dentist"):
        return "health"
    if tags.get("leisure") in ("park", "garden", "sports_centre", "stadium", "pitch", "swimming_pool", "golf_course"):
        return "recreation"
    if tags.get("building") in ("church", "cathedral", "mosque", "temple", "synagogue", "shrine"):
        return "landmark"
    if tags.get("building") == "stadium":
        return "recreation"
    if tags.get("building") == "theatre":
        return "landmark"
    if tags.get("amenity") == "cinema":
        return "entertainment"
    if tags.get("amenity") == "theatre":
        return "entertainment"
    if tags.get("amenity") in ("bank", "atm"):
        return "finance"
    if tags.get("amenity") in ("police", "fire_station", "courthouse", "townhall"):
        return "government"
    if tags.get("amenity") == "post_office":
        return "service"
    if tags.get("tourism") == "hotel":
        return "accommodation"
    if tags.get("tourism") in ("hostel", "motel", "guest_house"):
        return "accommodation"
    return None


def _subtype_tags(tags: dict[str, str]) -> str:
    """Extract a more specific subtype from OSM tags."""
    for key in ("amenity", "tourism", "historic", "leisure", "shop", "building"):
        if tags.get(key):
            return tags[key]
    return "unknown"
