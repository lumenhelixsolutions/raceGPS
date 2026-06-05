#!/usr/bin/env python3
"""
Convert OSM road graph → OpenDRIVE (.xodr) XML.
Pure-Python implementation — no CARLA dependency.
Generates valid OpenDRIVE 1.4 format from semantic road graph JSON.
"""

import json
import math
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


def _geo_to_local(lat: float, lon: float, origin_lat: float, origin_lon: float) -> tuple[float, float]:
    """Equirectangular projection: (lat, lon) → local (x, y) in meters."""
    meters_per_lon = 111320.0 * math.cos(math.radians(origin_lat))
    meters_per_lat = 111320.0
    x = (lon - origin_lon) * meters_per_lon
    y = (lat - origin_lat) * meters_per_lat
    return x, y


def _heading(dx: float, dy: float) -> float:
    """Heading in radians, OpenDRIVE convention: 0 = +x, CCW positive."""
    return math.atan2(dy, dx)


def _format_float(v: float) -> str:
    return f"{v:.6f}"


def _make_header(root: ET.Element, origin_lat: float, origin_lon: float) -> None:
    hdr = ET.SubElement(root, "header")
    hdr.set("revMajor", "1")
    hdr.set("revMinor", "4")
    hdr.set("name", "akron_oh_beta")
    hdr.set("version", "1")
    hdr.set("date", "2026-06-04")
    hdr.set("north", _format_float(origin_lat + 0.1))
    hdr.set("south", _format_float(origin_lat - 0.1))
    hdr.set("east", _format_float(origin_lon + 0.1))
    hdr.set("west", _format_float(origin_lon - 0.1))

    geo = ET.SubElement(hdr, "geoReference")
    geo.text = (
        f'+proj=tmerc +lat_0={origin_lat} +lon_0={origin_lon} '
        f'+k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs'
    )


def _build_plan_view(road_elem: ET.Element, points: list[dict], origin_lat: float, origin_lon: float) -> float:
    """Build <planView> with <geometry><line> segments. Returns total road length."""
    plan_view = ET.SubElement(road_elem, "planView")

    total_length = 0.0
    local_pts = [_geo_to_local(p["lat"], p["lon"], origin_lat, origin_lon) for p in points]

    for i in range(len(local_pts) - 1):
        x1, y1 = local_pts[i]
        x2, y2 = local_pts[i + 1]
        dx = x2 - x1
        dy = y2 - y1
        seg_len = math.hypot(dx, dy)
        hdg = _heading(dx, dy)

        geom = ET.SubElement(plan_view, "geometry")
        geom.set("s", _format_float(total_length))
        geom.set("x", _format_float(x1))
        geom.set("y", _format_float(y1))
        geom.set("hdg", _format_float(hdg))
        geom.set("length", _format_float(seg_len))

        ET.SubElement(geom, "line")

        total_length += seg_len

    return total_length


def _build_lanes(road_elem: ET.Element, width: float, one_way: bool, total_length: float) -> None:
    """Build <lanes> with a single laneSection covering the whole road."""
    lanes = ET.SubElement(road_elem, "lanes")

    lane_sec = ET.SubElement(lanes, "laneSection")
    lane_sec.set("s", "0.0")

    # Center lane (reference line)
    center = ET.SubElement(lane_sec, "center")
    center_lane = ET.SubElement(center, "lane")
    center_lane.set("id", "0")
    center_lane.set("type", "none")
    center_lane.set("level", "false")

    cl_width = ET.SubElement(center_lane, "width")
    cl_width.set("sOffset", "0.0")
    cl_width.set("a", "0.0")
    cl_width.set("b", "0.0")
    cl_width.set("c", "0.0")
    cl_width.set("d", "0.0")

    if not one_way:
        # Left lanes (positive ids) — oncoming traffic
        left = ET.SubElement(lane_sec, "left")
        left_lane = ET.SubElement(left, "lane")
        left_lane.set("id", "1")
        left_lane.set("type", "driving")
        left_lane.set("level", "false")

        lw = ET.SubElement(left_lane, "width")
        lw.set("sOffset", "0.0")
        lw.set("a", _format_float(width * 0.5))
        lw.set("b", "0.0")
        lw.set("c", "0.0")
        lw.set("d", "0.0")

        lb = ET.SubElement(left_lane, "border")
        lb.set("sOffset", "0.0")
        lb.set("a", _format_float(width * 0.5))
        lb.set("b", "0.0")
        lb.set("c", "0.0")
        lb.set("d", "0.0")

    # Right lanes (negative ids) — forward traffic
    right = ET.SubElement(lane_sec, "right")
    right_lane = ET.SubElement(right, "lane")
    right_lane.set("id", "-1")
    right_lane.set("type", "driving")
    right_lane.set("level", "false")

    rw = ET.SubElement(right_lane, "width")
    rw.set("sOffset", "0.0")
    rw.set("a", _format_float(width * 0.5))
    rw.set("b", "0.0")
    rw.set("c", "0.0")
    rw.set("d", "0.0")

    rb = ET.SubElement(right_lane, "border")
    rb.set("sOffset", "0.0")
    rb.set("a", _format_float(width * 0.5))
    rb.set("b", "0.0")
    rb.set("c", "0.0")
    rb.set("d", "0.0")

    # Road markings
    for lane_id, side in [("0", "center"), ("1" if not one_way else None, "left"), ("-1", "right")]:
        if lane_id is None:
            continue
        side_elem = lane_sec.find(side) if side != "center" else lane_sec.find("center")
        if side_elem is None:
            continue
        lane_elem = side_elem.find(f"lane[@id='{lane_id}']")
        if lane_elem is None:
            continue
        rm = ET.SubElement(lane_elem, "roadMark")
        rm.set("sOffset", "0.0")
        rm.set("type", "solid" if lane_id == "0" else "broken")
        rm.set("weight", "standard")
        rm.set("color", "white" if lane_id == "0" else "yellow")
        rm.set("width", "0.15")


def _build_road_link(road_elem: ET.Element, road_id: str, intersections: list[dict]) -> None:
    """Build <link> with predecessor / successor junction references."""
    # Find intersections that include this road
    connected_junctions = []
    for inter in intersections:
        if road_id in inter.get("road_ids", []):
            connected_junctions.append(inter["node_id"])

    if not connected_junctions:
        return

    link = ET.SubElement(road_elem, "link")

    # First intersection → predecessor
    pred = ET.SubElement(link, "predecessor")
    pred.set("elementType", "junction")
    pred.set("elementId", connected_junctions[0])

    # Last intersection → successor (if different)
    if len(connected_junctions) > 1 and connected_junctions[-1] != connected_junctions[0]:
        succ = ET.SubElement(link, "successor")
        succ.set("elementType", "junction")
        succ.set("elementId", connected_junctions[-1])


def _build_junctions(root: ET.Element, intersections: list[dict]) -> None:
    """Build <junction> elements for each intersection."""
    for inter in intersections:
        junction = ET.SubElement(root, "junction")
        junction.set("id", inter["node_id"])
        junction.set("name", f"junction_{inter['node_id']}")

        road_ids = inter.get("road_ids", [])
        for i, rid in enumerate(road_ids):
            for j, other_rid in enumerate(road_ids):
                if i == j:
                    continue
                conn = ET.SubElement(junction, "connection")
                conn.set("id", f"{i}_{j}")
                conn.set("incomingRoad", rid)
                conn.set("connectingRoad", other_rid)
                # Contact point: start if this road begins at junction, else end
                # We don't have that info, so default to start for simplicity
                conn.set("contactPoint", "start")


def generate_xodr(road_graph: dict[str, Any], output_path: Path | None = None) -> str:
    """
    Generate OpenDRIVE XML from semantic road graph.

    Args:
        road_graph: Dict with 'roads' and 'intersections' lists.
        output_path: Optional path to write the .xodr file.

    Returns:
        The XML string.
    """
    roads = road_graph.get("roads", [])
    intersections = road_graph.get("intersections", [])

    if not roads:
        raise ValueError("Road graph contains no roads")

    # Compute origin as city center
    all_lats = [p["lat"] for r in roads for p in r["points"]]
    all_lons = [p["lon"] for r in roads for p in r["points"]]
    origin_lat = sum(all_lats) / len(all_lats)
    origin_lon = sum(all_lons) / len(all_lons)

    root = ET.Element("OpenDRIVE")
    _make_header(root, origin_lat, origin_lon)

    for road in roads:
        road_elem = ET.SubElement(root, "road")
        road_id = str(road["id"])
        road_elem.set("id", road_id)
        road_elem.set("name", road.get("name", "Unnamed"))
        road_elem.set("length", "0.0")  # updated below
        road_elem.set("junction", "-1")
        road_elem.set("rule", "RHT")  # Right-hand traffic

        # Junction links
        _build_road_link(road_elem, road_id, intersections)

        # Plan view
        total_length = _build_plan_view(
            road_elem, road["points"], origin_lat, origin_lon
        )
        road_elem.set("length", _format_float(total_length))

        # Elevation profile (flat for now)
        elevation = ET.SubElement(road_elem, "elevationProfile")
        elev_prof = ET.SubElement(elevation, "elevation")
        elev_prof.set("s", "0.0")
        elev_prof.set("a", "0.0")
        elev_prof.set("b", "0.0")
        elev_prof.set("c", "0.0")
        elev_prof.set("d", "0.0")

        # Lateral profile (flat)
        lateral = ET.SubElement(road_elem, "lateralProfile")
        sup = ET.SubElement(lateral, "superelevation")
        sup.set("s", "0.0")
        sup.set("a", "0.0")
        sup.set("b", "0.0")
        sup.set("c", "0.0")
        sup.set("d", "0.0")

        # Lanes
        _build_lanes(
            road_elem,
            width=road.get("width", 7.0),
            one_way=road.get("oneway", False),
            total_length=total_length,
        )

        # Objects (empty for now)
        ET.SubElement(road_elem, "objects")
        # Signals (empty for now)
        ET.SubElement(road_elem, "signals")
        # Surface (empty for now)
        ET.SubElement(road_elem, "surface")

    # Junctions
    _build_junctions(root, intersections)

    # Pretty-print XML
    ET.indent(root, space="  ")
    xml_str = '<?xml version="1.0" encoding="UTF-8"?>\n' + ET.tostring(root, encoding="unicode")

    if output_path:
        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(xml_str)
        print(f"[xodr] Wrote {len(roads)} roads, {len(intersections)} junctions -> {output_path}")

    return xml_str


def convert_osm_to_xodr(osm_path: Path) -> str:
    """Legacy CARLA-compatible wrapper — now pure Python."""
    from road_graph import build_road_graph

    road_graph = build_road_graph(osm_path)
    return generate_xodr(road_graph)


if __name__ == "__main__":
    import sys

    if len(sys.argv) >= 2:
        graph_path = Path(sys.argv[1])
    else:
        graph_path = Path(__file__).resolve().parents[2] / "citypacks" / "akron-oh-beta-001" / "akron_road_graph.json"

    out_path = graph_path.with_suffix(".xodr")
    if graph_path.suffix == ".json":
        with open(graph_path, "r", encoding="utf-8") as f:
            road_graph = json.load(f)
        generate_xodr(road_graph, out_path)
    else:
        # Assume OSM XML
        generate_xodr(convert_osm_to_xodr(graph_path), out_path)
