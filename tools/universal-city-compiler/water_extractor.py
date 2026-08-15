#!/usr/bin/env python3
"""Extract water bodies (rivers, lakes, riverbank polygons, coastlines) from OSM data.

Feature mapping:
    - waterway=river|stream|canal  -> rivers (polylines)
    - waterway=riverbank           -> river_polygons (closed polygons)
    - natural=water & water=river  -> river_polygons (closed polygons)
    - natural=water / water=lake|reservoir|pond|basin -> lakes (closed polygons)
    - natural=coastline            -> coastlines (polylines; close against the
      world bounds on the importer side to build the sea/lake polygon)

Multipolygon relations (the usual mapping for large rivers such as the
Cuyahoga and for big lakes) are assembled from their outer member ways;
the longest ring becomes ``points`` and any remaining closed rings are
emitted as ``holes``.
"""

import math
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any

RIVER_WATERWAYS = ("river", "stream", "canal")
RIVER_POLYGON_WATERWAYS = ("riverbank",)
LAKE_WATERS = ("lake", "reservoir", "pond", "basin")


def extract_water(osm_path: Path, origin_lat: float = 0.0, origin_lon: float = 0.0) -> dict[str, Any]:
    """Extract water features from OSM.

    Returns dict with:
        - rivers: list of {id, name, type, points[{lat,lon}]} (polylines)
        - river_polygons: list of {id, name, type, points, holes, area_approx_m2, source}
        - lakes: list of {id, name, type, points, holes, area_approx_m2, source}
        - coastlines: list of {id, points} (polylines)
        - river_count, river_polygon_count, lake_count, coastline_count, water_count
    """
    tree = ET.parse(osm_path)
    root = tree.getroot()

    nodes: dict[str, tuple[float, float]] = {}
    ways: dict[str, dict[str, Any]] = {}
    relations: list[dict[str, Any]] = []

    for elem in root:
        if elem.tag == "node":
            nodes[elem.get("id")] = (float(elem.get("lat", 0)), float(elem.get("lon", 0)))
        elif elem.tag == "way":
            tags = {t.get("k"): t.get("v") for t in elem if t.tag == "tag"}
            nds = [n.get("ref") for n in elem if n.tag == "nd"]
            ways[elem.get("id")] = {"tags": tags, "nds": nds}
        elif elem.tag == "relation":
            tags = {t.get("k"): t.get("v") for t in elem if t.tag == "tag"}
            members = [
                (m.get("type"), m.get("ref"), m.get("role", ""))
                for m in elem if m.tag == "member"
            ]
            relations.append({"id": elem.get("id"), "tags": tags, "members": members})

    rivers: list[dict[str, Any]] = []
    river_polygons: list[dict[str, Any]] = []
    lakes: list[dict[str, Any]] = []
    coastlines: list[dict[str, Any]] = []

    def way_coords(nds: list[str]) -> list[tuple[float, float]]:
        return [nodes[ref] for ref in nds if ref in nodes]

    for way_id, way in ways.items():
        tags = way["tags"]
        coords = way_coords(way["nds"])
        if len(coords) < 2:
            continue
        points = [{"lat": c[0], "lon": c[1]} for c in coords]
        closed = len(way["nds"]) > 2 and way["nds"][0] == way["nds"][-1]

        # Rivers / streams / canals (polylines)
        if tags.get("waterway") in RIVER_WATERWAYS:
            rivers.append({
                "id": way_id,
                "name": tags.get("name", ""),
                "type": tags.get("waterway", "river"),
                "points": points,
            })

        # Riverbank polygons (waterway=riverbank, or natural=water + water=river)
        if tags.get("waterway") in RIVER_POLYGON_WATERWAYS or (
            tags.get("natural") == "water" and tags.get("water") == "river"
        ):
            river_polygons.append(_polygon_entry(way_id, tags, coords, closed, source="way"))

        # Lakes / reservoirs / ponds (natural=water, excluding water=river handled above)
        if (tags.get("natural") == "water" and tags.get("water") != "river") or (
            tags.get("water") in LAKE_WATERS
        ):
            lakes.append(_polygon_entry(way_id, tags, coords, closed, source="way"))

        # Coastline
        if tags.get("natural") == "coastline":
            coastlines.append({
                "id": way_id,
                "points": points,
            })

    # Multipolygon relations (large rivers, lakes): assemble outer rings.
    for rel in relations:
        tags = rel["tags"]
        target: list[dict[str, Any]] | None = None
        if tags.get("waterway") in RIVER_POLYGON_WATERWAYS or (
            tags.get("natural") == "water" and tags.get("water") == "river"
        ):
            target = river_polygons
        elif (tags.get("natural") == "water" and tags.get("water") != "river") or (
            tags.get("water") in LAKE_WATERS
        ):
            target = lakes
        if target is None:
            continue

        member_nds = []
        for mtype, ref, role in rel["members"]:
            if mtype != "way" or role not in ("outer", ""):
                continue
            way = ways.get(ref)
            if way is not None:
                member_nds.append(way["nds"])
        if not member_nds:
            continue

        rings = _join_rings(member_nds)
        closed_rings = []
        for ring in rings:
            coords = way_coords(ring)
            if len(coords) >= 3:
                closed_rings.append(coords)
        if not closed_rings:
            continue

        # Longest ring is the outer shell; remaining rings are holes.
        closed_rings.sort(key=len, reverse=True)
        shell = closed_rings[0]
        entry = _polygon_entry(rel["id"], tags, shell, closed=True, source="relation")
        entry["holes"] = [
            [{"lat": c[0], "lon": c[1]} for c in ring] for ring in closed_rings[1:]
        ]
        target.append(entry)

    return {
        "rivers": rivers,
        "river_polygons": river_polygons,
        "lakes": lakes,
        "coastlines": coastlines,
        "river_count": len(rivers),
        "river_polygon_count": len(river_polygons),
        "lake_count": len(lakes),
        "coastline_count": len(coastlines),
        "water_count": len(rivers) + len(river_polygons) + len(lakes) + len(coastlines),
    }


def _polygon_entry(way_id: str, tags: dict, coords: list[tuple[float, float]],
                   closed: bool, source: str) -> dict[str, Any]:
    return {
        "id": way_id,
        "name": tags.get("name", ""),
        "type": tags.get("water") or tags.get("waterway") or "lake",
        "points": [{"lat": c[0], "lon": c[1]} for c in coords],
        "holes": [],
        "closed": closed,
        "area_approx_m2": round(_polygon_area(coords), 1),
        "source": source,
    }


def _join_rings(segments: list[list[str]]) -> list[list[str]]:
    """Join way node-ref segments into closed rings by matching endpoints."""
    rings = []
    segs = [list(s) for s in segments if len(s) >= 2]
    while segs:
        ring = segs.pop(0)
        while ring[0] != ring[-1]:
            joined = False
            for i, seg in enumerate(segs):
                if ring[-1] == seg[0]:
                    ring.extend(seg[1:])
                elif ring[-1] == seg[-1]:
                    ring.extend(seg[-2::-1])
                elif ring[0] == seg[-1]:
                    ring = seg[:-1] + ring
                elif ring[0] == seg[0]:
                    ring = seg[:0:-1] + ring
                else:
                    continue
                segs.pop(i)
                joined = True
                break
            if not joined:
                break  # open ring: keep what we have
        rings.append(ring)
    return rings


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
