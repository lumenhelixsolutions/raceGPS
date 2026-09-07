#!/usr/bin/env python3
"""Generate raceGPS Cleveland M5 environment JSON from racing_line.json.

Offline camera-needed dressing only: lake, skyline silhouette, barriers,
cones, grid, S/F, infield grass, runway vs taxiway tags, hangar boxes.
Does not fetch OSM/Cesium and does not dump a 5 km pond pack.
"""
from __future__ import annotations

import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / "citypacks/cleveland/burke_gp_1997"

LAT0 = 41.51722
LON0 = -81.68306
R_LAT = 111320.0
R_LON = 111320.0 * math.cos(math.radians(LAT0))
EARTH_R = 6_371_000.0

# Barrier lateral offset from centerline (meters).
BARRIER_OFFSET_M = 9.0
# Skip T1 vortex runoff (wide open right-hairpin / old T3).
T1_SKIP = (700.0, 860.0)
# Skip a short pit-wall gap near S/F (old 1982 T1/T2 pit exit visual only).
PIT_SKIP = (3230.0, 80.0)  # wraps: s>=3230 or s<=80


def ll_to_xy(lat: float, lon: float) -> tuple[float, float]:
    return ((lon - LON0) * R_LON, (lat - LAT0) * R_LAT)


def xy_to_ll(x: float, y: float) -> tuple[float, float]:
    return (LAT0 + y / R_LAT, LON0 + x / R_LON)


def hav_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    p1, p2 = map(math.radians, (lat1, lat2))
    dlat = p2 - p1
    dlon = math.radians(lon2 - lon1)
    h = math.sin(dlat / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dlon / 2) ** 2
    return 2 * EARTH_R * math.asin(math.sqrt(min(1.0, h)))


def offset_ll(lat: float, lon: float, heading_deg: float, left_m: float) -> tuple[float, float]:
    """Offset left of compass heading (0=N, clockwise). Left = CCW 90 deg."""
    h = math.radians(heading_deg)
    # travel: east=sin, north=cos; left: east=-north_travel, north=east_travel
    east = -math.cos(h) * left_m
    north = math.sin(h) * left_m
    return xy_to_ll(* (east + ll_to_xy(lat, lon)[0], north + ll_to_xy(lat, lon)[1]))


def along_ll(lat: float, lon: float, heading_deg: float, along_m: float) -> tuple[float, float]:
    h = math.radians(heading_deg)
    east = math.sin(h) * along_m
    north = math.cos(h) * along_m
    x, y = ll_to_xy(lat, lon)
    return xy_to_ll(x + east, y + north)


def in_wrap_skip(s: float, lo: float, hi: float) -> bool:
    if lo <= hi:
        return lo <= s <= hi
    return s >= lo or s <= hi


def load_line() -> dict:
    return json.loads((PACK / "racing_line.json").read_text())


def build_water(samples: list[dict]) -> dict:
    lats = [s["lat"] for s in samples]
    lons = [s["lon"] for s in samples]
    max_lat = max(lats)
    min_lon, max_lon = min(lons), max(lons)
    # Shore ~220 m north of the north-runway racing line, along lake-side 06L/24R.
    # Lake Erie is north of Burke Lakefront; downtown is south.
    shore_lat = max_lat + 220.0 / R_LAT
    outer_lat = shore_lat + 1800.0 / R_LAT  # ~1.8 km of water for horizon
    pad_lon = 900.0 / R_LON
    west = min_lon - pad_lon
    east = max_lon + pad_lon
    # Align the south edge with runway heading ~058 true (lake-side 06L/24R).
    # Trapezoid: SW/SE shoreline, NW/NE outer lake.
    yaw = math.radians(58.0)
    # Shift east/west corners along runway to follow the field.
    def shift(lat, lon, along_m, left_m):
        lat2, lon2 = along_ll(lat, lon, 58.0, along_m)
        return offset_ll(lat2, lon2, 58.0, left_m)

    # Base shoreline points from circuit max-lat line.
    sw = (shore_lat - 80.0 / R_LAT, west)
    se = (shore_lat + 40.0 / R_LAT, east)
    ne = (outer_lat + 40.0 / R_LAT, east + 200.0 / R_LON)
    nw = (outer_lat - 80.0 / R_LAT, west - 200.0 / R_LON)
    # Inner shoreline polyline (south edge only) for optional foam/edge mesh.
    n_shore = 8
    shoreline = []
    for i in range(n_shore):
        t = i / (n_shore - 1)
        lat = sw[0] + (se[0] - sw[0]) * t
        lon = sw[1] + (se[1] - sw[1]) * t
        shoreline.append({"lat": round(lat, 7), "lon": round(lon, 7)})

    poly = [
        {"lat": round(sw[0], 7), "lon": round(sw[1], 7)},
        {"lat": round(se[0], 7), "lon": round(se[1], 7)},
        {"lat": round(ne[0], 7), "lon": round(ne[1], 7)},
        {"lat": round(nw[0], 7), "lon": round(nw[1], 7)},
    ]
    return {
        "id": "lake_erie_burke_nearfield",
        "material": "Water_Surface",
        "notes": (
            "Single Lake Erie sheet NORTH of Burke Lakefront. South edge is "
            "racing_line max lat + ~220 m toward the lake (not OSM hydro dump). "
            "Do not treat this as a 5 km citypack pond inventory."
        ),
        "assumptions": [
            "Circuit max sample lat is the north-runway 06L/24R racing line, not the FAA shoreline.",
            "Real BKL shoreline sits a few hundred meters north of 06L; 220 m offset is a camera-scale approximation.",
            "Outer (north) edge is a far-field clip ~1.8 km offshore for horizon fill, not a mapped isobath.",
        ],
        "points": poly,
        "inner_shoreline": {
            "name": "Burke lakefront shoreline (approx)",
            "points": shoreline,
        },
    }


def build_skyline() -> dict:
    """Tier 0 downtown silhouette SOUTH of the airport.

    Coordinates are well-known published approximations (Wikipedia / OSM
    centroids, rounded). Heights are architectural roof heights, not spire
    when the difference is large, except Key Tower (289 m to roof/antenna
    cluster per common skyline tables). This is NOT photoreal downtown.
    """
    # All latitudes are south of circuit min lat (~41.51376).
    buildings = [
        {"name": "Key Tower", "lat": 41.50139, "lon": -81.69361, "height_m": 289.0, "width_m": 52.0, "depth_m": 52.0, "yaw_deg": 0.0, "material": "Building_Glass"},
        {"name": "Terminal Tower", "lat": 41.49847, "lon": -81.69395, "height_m": 235.0, "width_m": 48.0, "depth_m": 48.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "200 Public Square", "lat": 41.49972, "lon": -81.69028, "height_m": 201.0, "width_m": 55.0, "depth_m": 45.0, "yaw_deg": 5.0, "material": "Building_Glass"},
        {"name": "Tower at Erieview", "lat": 41.50497, "lon": -81.69056, "height_m": 129.0, "width_m": 40.0, "depth_m": 40.0, "yaw_deg": 0.0, "material": "Building_Glass"},
        {"name": "One Cleveland Center", "lat": 41.50417, "lon": -81.68889, "height_m": 137.0, "width_m": 42.0, "depth_m": 36.0, "yaw_deg": 10.0, "material": "Building_Glass"},
        {"name": "Fifth Third Center", "lat": 41.50111, "lon": -81.69139, "height_m": 136.0, "width_m": 38.0, "depth_m": 38.0, "yaw_deg": 0.0, "material": "Building_Glass"},
        {"name": "Carl B. Stokes U.S. Courthouse", "lat": 41.50556, "lon": -81.69139, "height_m": 131.0, "width_m": 50.0, "depth_m": 40.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Justice Center", "lat": 41.50333, "lon": -81.69639, "height_m": 128.0, "width_m": 55.0, "depth_m": 45.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "AECOM Building", "lat": 41.50472, "lon": -81.68778, "height_m": 116.0, "width_m": 40.0, "depth_m": 32.0, "yaw_deg": 5.0, "material": "Building_Glass"},
        {"name": "AT&T Huron Road Building", "lat": 41.49917, "lon": -81.68556, "height_m": 111.0, "width_m": 36.0, "depth_m": 36.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Ameritrust Tower", "lat": 41.49944, "lon": -81.68583, "height_m": 118.0, "width_m": 34.0, "depth_m": 34.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "PNC Center", "lat": 41.50194, "lon": -81.68722, "height_m": 101.0, "width_m": 36.0, "depth_m": 36.0, "yaw_deg": 0.0, "material": "Building_Glass"},
        {"name": "Marriott at Key Center", "lat": 41.50194, "lon": -81.69389, "height_m": 97.0, "width_m": 40.0, "depth_m": 32.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Hilton Cleveland Downtown", "lat": 41.50583, "lon": -81.69306, "height_m": 72.0, "width_m": 70.0, "depth_m": 36.0, "yaw_deg": 80.0, "material": "Building_Glass"},
        {"name": "North Point Tower", "lat": 41.50667, "lon": -81.69000, "height_m": 82.0, "width_m": 40.0, "depth_m": 32.0, "yaw_deg": 10.0, "material": "Building_Glass"},
        {"name": "Medical Mutual Building", "lat": 41.50333, "lon": -81.68583, "height_m": 92.0, "width_m": 38.0, "depth_m": 32.0, "yaw_deg": 0.0, "material": "Building_Glass"},
        {"name": "55 Public Square", "lat": 41.50000, "lon": -81.69444, "height_m": 91.0, "width_m": 36.0, "depth_m": 32.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "75 Public Square", "lat": 41.49972, "lon": -81.69500, "height_m": 83.0, "width_m": 32.0, "depth_m": 28.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Leader Building", "lat": 41.49972, "lon": -81.69194, "height_m": 81.0, "width_m": 34.0, "depth_m": 30.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "The 9 Cleveland", "lat": 41.49944, "lon": -81.68833, "height_m": 80.0, "width_m": 40.0, "depth_m": 36.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Federal Reserve Bank of Cleveland", "lat": 41.50139, "lon": -81.69194, "height_m": 82.0, "width_m": 50.0, "depth_m": 40.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Ernst & Young Tower", "lat": 41.49667, "lon": -81.69944, "height_m": 101.0, "width_m": 42.0, "depth_m": 36.0, "yaw_deg": 15.0, "material": "Building_Glass"},
        {"name": "Standard Building", "lat": 41.50083, "lon": -81.69639, "height_m": 68.0, "width_m": 36.0, "depth_m": 32.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Rockefeller Building", "lat": 41.49861, "lon": -81.69722, "height_m": 64.0, "width_m": 32.0, "depth_m": 32.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Cleveland City Hall", "lat": 41.50500, "lon": -81.69361, "height_m": 40.0, "width_m": 80.0, "depth_m": 50.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Cleveland Public Auditorium", "lat": 41.50556, "lon": -81.69472, "height_m": 35.0, "width_m": 90.0, "depth_m": 60.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Cuyahoga County Courthouse", "lat": 41.50417, "lon": -81.69722, "height_m": 45.0, "width_m": 70.0, "depth_m": 50.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Ohio Savings Plaza", "lat": 41.50139, "lon": -81.68917, "height_m": 88.0, "width_m": 36.0, "depth_m": 32.0, "yaw_deg": 0.0, "material": "Building_Glass"},
        {"name": "Residences at 1717", "lat": 41.50278, "lon": -81.68694, "height_m": 89.0, "width_m": 32.0, "depth_m": 28.0, "yaw_deg": 5.0, "material": "Building_Glass"},
        {"name": "Eaton Center", "lat": 41.49944, "lon": -81.68778, "height_m": 87.0, "width_m": 38.0, "depth_m": 32.0, "yaw_deg": 0.0, "material": "Building_Glass"},
        {"name": "North Point B", "lat": 41.50639, "lon": -81.68917, "height_m": 70.0, "width_m": 36.0, "depth_m": 30.0, "yaw_deg": 10.0, "material": "Building_Glass"},
        {"name": "Reserve Square A", "lat": 41.50194, "lon": -81.68444, "height_m": 83.0, "width_m": 28.0, "depth_m": 28.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Reserve Square B", "lat": 41.50167, "lon": -81.68389, "height_m": 83.0, "width_m": 28.0, "depth_m": 28.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Westin Cleveland Downtown", "lat": 41.49917, "lon": -81.68889, "height_m": 67.0, "width_m": 40.0, "depth_m": 30.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Hanna Building", "lat": 41.50056, "lon": -81.68194, "height_m": 58.0, "width_m": 50.0, "depth_m": 36.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Playhouse Square stretch A", "lat": 41.50083, "lon": -81.68083, "height_m": 45.0, "width_m": 40.0, "depth_m": 28.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Playhouse Square stretch B", "lat": 41.50056, "lon": -81.67972, "height_m": 42.0, "width_m": 36.0, "depth_m": 26.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Flats East Bank A", "lat": 41.49694, "lon": -81.70056, "height_m": 55.0, "width_m": 40.0, "depth_m": 30.0, "yaw_deg": 20.0, "material": "Building_Glass"},
        {"name": "Flats East Bank B", "lat": 41.49750, "lon": -81.70111, "height_m": 48.0, "width_m": 36.0, "depth_m": 28.0, "yaw_deg": 20.0, "material": "Building_Glass"},
        {"name": "Warehouse District A", "lat": 41.49889, "lon": -81.70000, "height_m": 38.0, "width_m": 32.0, "depth_m": 24.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "Warehouse District B", "lat": 41.49944, "lon": -81.70083, "height_m": 34.0, "width_m": 30.0, "depth_m": 22.0, "yaw_deg": 0.0, "material": "Building_Concrete"},
        {"name": "East 9th midrise A", "lat": 41.50389, "lon": -81.68667, "height_m": 62.0, "width_m": 28.0, "depth_m": 24.0, "yaw_deg": 0.0, "material": "Building_Glass"},
        {"name": "East 9th midrise B", "lat": 41.50222, "lon": -81.68611, "height_m": 58.0, "width_m": 28.0, "depth_m": 24.0, "yaw_deg": 0.0, "material": "Building_Glass"},
        {"name": "Lakeside Avenue office", "lat": 41.50611, "lon": -81.69250, "height_m": 50.0, "width_m": 44.0, "depth_m": 28.0, "yaw_deg": 80.0, "material": "Building_Concrete"},
        {"name": "Galleria at Erieview massing", "lat": 41.50444, "lon": -81.68944, "height_m": 28.0, "width_m": 80.0, "depth_m": 40.0, "yaw_deg": 10.0, "material": "Building_Glass"},
    ]
    for b in buildings:
        b["tier"] = 0
        b["kind"] = "silhouette_volume"
    return {
        "id": "cleveland_downtown_silhouette",
        "tier": 0,
        "material_rule": "Building_Glass if height_m >= 90 else Building_Concrete (overridden per-building)",
        "notes": (
            "Low-poly extruded boxes SOUTH of Burke Lakefront for a camera "
            "skyline. Not photoreal storefronts, not Cesium photogrammetry, "
            "not interior downtown."
        ),
        "assumptions": [
            "Lat/lon are Wikipedia/OSM centroid approximations rounded to 1e-5 deg (~1 m).",
            "Heights are commonly published roof/architectural heights (meters).",
            "Footprints are schematic rectangles (width_m/depth_m/yaw_deg), not cadastral lots.",
            "Playhouse Square / Flats / Warehouse District extras are massing fillers, not named-accurate towers.",
            "All volumes sit south of the circuit min latitude so they read as downtown behind the airport.",
        ],
        "buildings": buildings,
    }


def build_dressing(line: dict) -> dict:
    samples = line["samples"]
    length = float(line.get("measured_length_m") or samples[-1]["s"])
    sf = samples[0]
    heading0 = float(sf["heading_deg"])

    barriers = []
    cones = []
    # Barriers every sample (~8 m) both sides except skip zones.
    for s in samples:
        ss = float(s["s"])
        skip = in_wrap_skip(ss, *T1_SKIP) or in_wrap_skip(ss, *PIT_SKIP)
        if skip:
            continue
        is_turn = s.get("turn_index") is not None
        btype = "tire" if is_turn or abs(float(s.get("curvature") or 0.0)) > 0.012 else "concrete"
        h = float(s["heading_deg"])
        left = offset_ll(s["lat"], s["lon"], h, BARRIER_OFFSET_M)
        right = offset_ll(s["lat"], s["lon"], h, -BARRIER_OFFSET_M)
        yaw = h  # boxes face along track
        for side, (lat, lon) in (("left", left), ("right", right)):
            barriers.append({
                "lat": round(lat, 7),
                "lon": round(lon, 7),
                "s": round(ss, 3),
                "side": side,
                "type": btype,
                "yaw_deg": round(yaw, 3),
                "length_m": 7.5,
                "width_m": 0.55 if btype == "concrete" else 0.9,
                "height_m": 1.05 if btype == "concrete" else 0.85,
            })

    # Second-row tire stacks on T4/T8/T9/T10 only (cinematic corners, T1 stays open).
    CRITICAL_TURNS = {4, 8, 9, 10}
    extra_barriers = []
    for s in samples:
        if s.get("turn_index") not in CRITICAL_TURNS:
            continue
        ss = float(s["s"])
        if in_wrap_skip(ss, *T1_SKIP) or in_wrap_skip(ss, *PIT_SKIP):
            continue
        h = float(s["heading_deg"])
        yaw = h
        for side, sign in (("left", 1.0), ("right", -1.0)):
            lat, lon = offset_ll(s["lat"], s["lon"], h, sign * 7.0)
            extra_barriers.append({
                "lat": round(lat, 7),
                "lon": round(lon, 7),
                "s": round(ss, 3),
                "side": side,
                "type": "tire",
                "yaw_deg": round(yaw, 3),
                "length_m": 5.5,
                "width_m": 0.9,
                "height_m": 0.85,
                "role": "corner_stack",
            })
    barriers.extend(extra_barriers)
    if len(barriers) > 1100:
        barriers = barriers[:1100]

    def add_cone(lat, lon, s, role):
        cones.append({
            "lat": round(lat, 7),
            "lon": round(lon, 7),
            "s": round(s, 3),
            "role": role,
            "height_m": 0.5,
        })

    # Denser cones: T1 vortex, grid, T4/T8/T9-T10, plus runway-edge dashes.
    # Cap ~360 so one procedural cone mesh stays cheap.
    CONE_CAP = 360
    for s in samples:
        if len(cones) >= CONE_CAP:
            break
        ss = float(s["s"])
        h = float(s["heading_deg"])
        ti = s.get("turn_index")
        near_t1 = 650.0 <= ss <= 930.0
        near_grid = ss <= 80.0 or ss >= length - 100.0
        near_t4 = 990.0 <= ss <= 1100.0
        near_t8 = 2600.0 <= ss <= 2730.0
        near_t910 = 3070.0 <= ss <= 3240.0
        runway_straight = 1120.0 <= ss <= 2260.0
        dense = near_t1 or near_grid or near_t4 or near_t8 or near_t910
        if dense:
            role = "t1" if near_t1 else ("grid" if near_grid else "corner")
            for sign in (1.0, -1.0):
                lat, lon = offset_ll(s["lat"], s["lon"], h, sign * 6.0)
                add_cone(lat, lon, ss, role)
                if len(cones) >= CONE_CAP:
                    break
        elif runway_straight and int(round(ss / 8.0)) % 3 == 0:
            # Cinematic runway edge — every ~24 m both sides.
            for sign in (1.0, -1.0):
                lat, lon = offset_ll(s["lat"], s["lon"], h, sign * 7.5)
                add_cone(lat, lon, ss, "runway")
        elif int(round(ss / 8.0)) % 6 == 0:
            lat, lon = offset_ll(s["lat"], s["lon"], h, 6.0)
            add_cone(lat, lon, ss, "sparse")

    # Grid slots: 3 poses at s=0 minus spacing along heading.
    grid_spacing = 8.0
    laterals = [1.8, -1.8, 1.8]
    roles = ["PLAYER", "AI", "AI"]
    grid_slots = []
    for i, (role, lat_off) in enumerate(zip(roles, laterals)):
        along = -grid_spacing * (i + 1)
        lat, lon = along_ll(sf["lat"], sf["lon"], heading0, along)
        lat, lon = offset_ll(lat, lon, heading0, lat_off)
        s_val = along if along >= 0 else along + length
        grid_slots.append({
            "role": role,
            "index": i,
            "lat": round(lat, 7),
            "lon": round(lon, 7),
            "s": round(s_val, 3),
            "heading_deg": heading0,
            "lateral_m": lat_off,
            "spacing_m": grid_spacing,
        })

    start_finish = {
        "lat": sf["lat"],
        "lon": sf["lon"],
        "s": 0.0,
        "heading_deg": heading0,
        "width_m": 12.0,
        "depth_m": 1.2,
        "material": "Road_Marking",
    }

    # Infield grass: clockwise → infield is to the RIGHT of heading.
    inner = []
    step = 6
    for i, s in enumerate(samples):
        if i % step:
            continue
        lat, lon = offset_ll(s["lat"], s["lon"], float(s["heading_deg"]), -28.0)
        inner.append((lat, lon, float(s["s"])))
    # Split into two polygons: SW infield (s 0–1800) and NE infield (s 1800–end).
    def ring(pts):
        return [{"lat": round(p[0], 7), "lon": round(p[1], 7)} for p in pts]

    poly_a = [p for p in inner if p[2] < 1800]
    poly_b = [p for p in inner if p[2] >= 1800]
    infield = []
    if len(poly_a) >= 4:
        infield.append({"name": "infield_sw", "material": "Vegetation_Grass", "points": ring(poly_a)})
    if len(poly_b) >= 4:
        infield.append({"name": "infield_ne", "material": "Vegetation_Grass", "points": ring(poly_b)})

    # Coarse runway vs taxiway boxes (camera material tags, not FAA surveys).
    # 06L/24R lake-side north runway from OSM endpoints used by the circuit builder.
    runway_regions = [
        {
            "name": "06L/24R lake-side",
            "material": "Road_Asphalt",
            "tag": "runway",
            "min_lat": 41.51390,
            "max_lat": 41.52300,
            "min_lon": -81.69220,
            "max_lon": -81.67370,
            "notes": "North runway (lake side). Coarse AABB covering 06L/24R used by the 1997 layout.",
        }
    ]
    taxiway_regions = [
        {
            "name": "Taxiway G S/F",
            "material": "Road_Asphalt",
            "tag": "taxiway",
            "min_lat": 41.51220,
            "max_lat": 41.52120,
            "min_lon": -81.69080,
            "max_lon": -81.67240,
            "notes": "South taxiway G carries start/finish heading SW toward the 06 ends.",
        }
    ]

    # Old 1982 T1/T2 became the extended pit exit (metadata only).
    pit_visual = {
        "name": "1982 T1/T2 pit exit (visual)",
        "modeled_in_xodr": False,
        "material": "Road_Asphalt",
        "polyline": [
            {"lat": 41.51620, "lon": -81.68280, "note": "near S/F on G"},
            {"lat": 41.51480, "lon": -81.68640},
            {"lat": 41.51340, "lon": -81.68940},
            {"lat": 41.51276, "lon": -81.69159, "note": "06R threshold area (not driven)"},
        ],
        "notes": "Visual/metadata only. Matches citypack pit_lane notes; not a second OpenDRIVE road.",
    }

    # Real-ish Burke Lakefront hangar line SOUTH of taxiway G / airside of the
    # OSM HISM city. Complements the baked 5 km city, does not replace it.
    # yaw 58 matches 06/24. mesh_path is a CARLA offline static-mesh hook;
    # Content/Carla/Static currently has Car/GenericMaterials/Truck only, so
    # C++ falls back to procedural boxes when the uasset is absent.
    _carla_hangar = "/Game/Carla/Static/Buildings/SM_Hangar_01"
    hangar_mesh_candidates = [
        "/Game/Carla/Static/Buildings/SM_Hangar_01",
        "/Game/Carla/Static/Buildings/SM_HangarLarge",
        "/Game/Carla/Static/Props/SM_AirportHangar",
        "/Game/Carla/Static/StaticMesh/SM_Hangar",
        "/Game/Carla/Static/Architecture/SM_Hangar",
    ]
    airport_boxes = [
        {"name": "hangar_t_row_w", "lat": 41.51255, "lon": -81.68890, "height_m": 7.0, "width_m": 72.0, "depth_m": 18.0, "yaw_deg": 58.0, "material": "Building_Concrete", "kind": "t_hangar", "mesh_path": _carla_hangar, "complement_hism": True},
        {"name": "hangar_west", "lat": 41.51290, "lon": -81.68680, "height_m": 12.0, "width_m": 48.0, "depth_m": 28.0, "yaw_deg": 58.0, "material": "Building_Concrete", "kind": "hangar", "mesh_path": _carla_hangar, "complement_hism": True},
        {"name": "hangar_corp_sw", "lat": 41.51270, "lon": -81.68520, "height_m": 10.0, "width_m": 36.0, "depth_m": 24.0, "yaw_deg": 58.0, "material": "Building_Concrete", "kind": "hangar", "mesh_path": _carla_hangar, "complement_hism": True},
        {"name": "hangar_fbo", "lat": 41.51310, "lon": -81.68390, "height_m": 11.0, "width_m": 55.0, "depth_m": 32.0, "yaw_deg": 58.0, "material": "Building_Concrete", "kind": "hangar", "mesh_path": _carla_hangar, "complement_hism": True},
        {"name": "hangar_ops", "lat": 41.51355, "lon": -81.68480, "height_m": 8.0, "width_m": 22.0, "depth_m": 16.0, "yaw_deg": 58.0, "material": "Building_Concrete", "kind": "ops", "mesh_path": _carla_hangar, "complement_hism": True},
        {"name": "hangar_mid", "lat": 41.51340, "lon": -81.68240, "height_m": 11.0, "width_m": 42.0, "depth_m": 26.0, "yaw_deg": 58.0, "material": "Building_Concrete", "kind": "hangar", "mesh_path": _carla_hangar, "complement_hism": True},
        {"name": "hangar_maint", "lat": 41.51320, "lon": -81.68110, "height_m": 9.0, "width_m": 38.0, "depth_m": 22.0, "yaw_deg": 58.0, "material": "Building_Concrete", "kind": "hangar", "mesh_path": _carla_hangar, "complement_hism": True},
        {"name": "hangar_fsdo", "lat": 41.51370, "lon": -81.68020, "height_m": 9.0, "width_m": 32.0, "depth_m": 22.0, "yaw_deg": 58.0, "material": "Building_Concrete", "kind": "hangar", "mesh_path": _carla_hangar, "complement_hism": True},
        {"name": "hangar_east", "lat": 41.51410, "lon": -81.67790, "height_m": 10.0, "width_m": 40.0, "depth_m": 24.0, "yaw_deg": 58.0, "material": "Building_Concrete", "kind": "hangar", "mesh_path": _carla_hangar, "complement_hism": True},
        {"name": "hangar_t_row_e", "lat": 41.51435, "lon": -81.67620, "height_m": 7.0, "width_m": 60.0, "depth_m": 16.0, "yaw_deg": 58.0, "material": "Building_Concrete", "kind": "t_hangar", "mesh_path": _carla_hangar, "complement_hism": True},
    ]

    # Taxiway G centerline dashes follow the racing line (the 1997 layout
    # *is* G + 06L/24R). Hold-shorts at authored 06/24 ends. Cap 240.
    taxiway_markings = []
    MARK_CAP = 240
    for i, s in enumerate(samples):
        if len(taxiway_markings) >= MARK_CAP - 8:
            break
        # dash on even samples (~8 m on, 8 m off)
        if i % 2 != 0:
            continue
        taxiway_markings.append({
            "name": f"cl_{i}",
            "type": "centerline",
            "lat": round(float(s["lat"]), 7),
            "lon": round(float(s["lon"]), 7),
            "yaw_deg": round(float(s["heading_deg"]), 3),
            "length_m": 4.0,
            "width_m": 0.18,
            "height_m": 0.02,
            "material": "Road_Marking",
        })
    taxiway_markings.extend([
        {"name": "hold_06L", "type": "hold_short", "lat": 41.51420, "lon": -81.68850, "yaw_deg": 58.0, "length_m": 0.9, "width_m": 14.0, "height_m": 0.03, "material": "Road_Marking"},
        {"name": "hold_06L_b", "type": "hold_short", "lat": 41.51428, "lon": -81.68835, "yaw_deg": 58.0, "length_m": 0.9, "width_m": 14.0, "height_m": 0.03, "material": "Road_Marking"},
        {"name": "hold_24R", "type": "hold_short", "lat": 41.52100, "lon": -81.67550, "yaw_deg": 58.0, "length_m": 0.9, "width_m": 14.0, "height_m": 0.03, "material": "Road_Marking"},
        {"name": "hold_24R_b", "type": "hold_short", "lat": 41.52092, "lon": -81.67565, "yaw_deg": 58.0, "length_m": 0.9, "width_m": 14.0, "height_m": 0.03, "material": "Road_Marking"},
        {"name": "thr_06L", "type": "threshold", "lat": 41.51355, "lon": -81.69040, "yaw_deg": 58.0, "length_m": 1.2, "width_m": 18.0, "height_m": 0.03, "material": "Road_Marking"},
        {"name": "thr_24R", "type": "threshold", "lat": 41.52210, "lon": -81.67390, "yaw_deg": 58.0, "length_m": 1.2, "width_m": 18.0, "height_m": 0.03, "material": "Road_Marking"},
    ])

    # Low grass south of the hangar line (landside). Does not cover the circuit.
    infield.append({
        "name": "hangar_apron_green",
        "material": "Vegetation_Grass",
        "points": [
            {"lat": 41.51190, "lon": -81.69020},
            {"lat": 41.51190, "lon": -81.67580},
            {"lat": 41.51285, "lon": -81.67580},
            {"lat": 41.51285, "lon": -81.69020},
        ],
        "notes": "Apron grass south of G hangars; complements HISM city, does not replace it.",
    })

    return {
        "id": "cleveland_burke_gp_1997_dressing",
        "generated_from": "racing_line.json",
        "barrier_offset_m": BARRIER_OFFSET_M,
        "skip": {
            "t1_vortex_s": list(T1_SKIP),
            "pit_gap_s_wrap": list(PIT_SKIP),
        },
        "barriers": barriers,
        "cones": cones,
        "grid_slots": grid_slots,
        "start_finish": start_finish,
        "infield_grass": infield,
        "runway_regions": runway_regions,
        "taxiway_regions": taxiway_regions,
        "taxiway_markings": taxiway_markings,
        "pit_visual": pit_visual,
        "airport_boxes": airport_boxes,
        "hangar_mesh_candidates": hangar_mesh_candidates,
        "prefer_carla_hangar_mesh": True,
        "assumptions": [
            "Barriers are a constant 9 m lateral offset of the reconstructed centerline, not catch-fence surveys.",
            "T1 vortex (s 700–860) is left open (no walls) for the wide right-hairpin runoff.",
            "A wrap gap near S/F (s>=3230 or s<=80) stands in for the old pit exit opening.",
            "Tire vs concrete is curvature/turn_index, not a historical photo inventory.",
            "Grid is three poses behind s=0 at 8 m spacing with 1.8 m stagger; PLAYER then two AI.",
            "Infield polygons are a 28 m inward offset of a decimated centerline (clockwise → right = infield).",
            "Runway/taxiway regions are coarse lat/lon AABBs for material tags (06L/24R vs G), not FAA rectangles.",
            "Hangars are ~10 real-ish Burke footprints south of G (T-rows, FBO, FSDO, maint) — no terminals. Complement HISM city, do not replace it.",
            "hangar_mesh_candidates are offline CARLA static-mesh paths; C++ LoadObject prefers them over boxes when the uasset exists. No CARLA server.",
            "taxiway_markings: dashed G centerline along the racing line plus authored 06L/24R hold-shorts (Road_Marking).",
            "Extra tire stacks on T4/T8/T9/T10 only; T1 vortex stays open. Cones capped at 360, barriers at 1100.",
        ],
    }


def build_environment() -> dict:
    return {
        "id": "cleveland_burke_gp_1997_environment",
        "offline": True,
        "cesium_required": False,
        "carla_required": False,
        "geo": {
            "frame": "wgs84",
            "origin": {"lat": LAT0, "lon": LON0},
            "world": "Z-up, 1uu=1cm, X=east, Y=north (URacingLineComponent::GeoToWorld)",
        },
        "lighting": {
            "actor": "ADayNightCycle",
            "spawn_if_missing": True,
            "time_local": "14:00",
            "notes": "Day demo / noon-afternoon. Actor already exists in the module; environment only spawns if none is in the world.",
        },
        "materials": {
            "provider": "URaceGPSMaterialProvider",
            "slots": [
                "Water_Surface",
                "Vegetation_Grass",
                "Road_Asphalt",
                "Road_Marking",
                "Building_Concrete",
                "Building_Glass",
            ],
        },
        "layers": [
            {"id": "water", "file": "water.json", "material": "Water_Surface", "draw": "near_and_far"},
            {"id": "infield_grass", "file": "track_dressing.json#infield_grass", "material": "Vegetation_Grass", "draw": "near"},
            {"id": "runway", "file": "track_dressing.json#runway_regions", "material": "Road_Asphalt", "draw": "near"},
            {"id": "taxiway", "file": "track_dressing.json#taxiway_regions", "material": "Road_Asphalt", "draw": "near"},
            {"id": "taxiway_markings", "file": "track_dressing.json#taxiway_markings", "material": "Road_Marking", "draw": "near"},
            {"id": "barriers", "file": "track_dressing.json#barriers", "draw": "near"},
            {"id": "cones", "file": "track_dressing.json#cones", "draw": "near"},
            {"id": "start_finish", "file": "track_dressing.json#start_finish", "material": "Road_Marking", "draw": "near"},
            {"id": "grid_slots", "file": "track_dressing.json#grid_slots", "draw": "near"},
            {"id": "pit_visual", "file": "track_dressing.json#pit_visual", "draw": "near"},
            {"id": "airport_boxes", "file": "track_dressing.json#airport_boxes", "material": "Building_Concrete", "draw": "near"},
            {"id": "skyline", "file": "skyline.json", "draw": "far"},
            {"id": "horizon", "procedural": "atmospheric_fog_on_ADayNightCycle", "draw": "far"},
        ],
        "non_goals": [
            "No downtown interiors or storefronts.",
            "No Cesium / 3D Tiles.",
            "No CARLA.",
            "No AStreetFurnitureSpawner city-intersection barriers.",
            "No 811-pond hydro dump from a 5 km pack.",
        ],
    }


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--dressing-only", action="store_true", help="do not rewrite skyline.json / water.json")
    args = ap.parse_args()
    PACK.mkdir(parents=True, exist_ok=True)
    line = load_line()
    water = build_water(line["samples"])
    skyline = build_skyline()
    dressing = build_dressing(line)
    env = build_environment()

        if not args.dressing_only:
        (PACK / "water.json").write_text(json.dumps(water, indent=2) + "\n")
        (PACK / "skyline.json").write_text(json.dumps(skyline, indent=2) + "\n")
    (PACK / "track_dressing.json").write_text(json.dumps(dressing, indent=2) + "\n")
    (PACK / "environment.json").write_text(json.dumps(env, indent=2) + "\n")

    ue_pack = ROOT / "apps/unreal-akron-beta/citypacks/cleveland/burke_gp_1997"
    if ue_pack.is_dir():
        import shutil
        for name in ("track_dressing.json", "environment.json"):
            shutil.copy2(PACK / name, ue_pack / name)
        if not args.dressing_only:
            for name in ("water.json", "skyline.json"):
                shutil.copy2(PACK / name, ue_pack / name)

    man_path = PACK / "manifest.json"
    man = json.loads(man_path.read_text())
    man["environment"] = "environment.json"
    man["water"] = "water.json"
    man["skyline"] = "skyline.json"
    man["track_dressing"] = "track_dressing.json"
    man["offline"] = True
    man["carla_required"] = False
    man["cesium_required"] = False
    man_path.write_text(json.dumps(man, indent=2) + "\n")

    lats = [p["lat"] for p in water["points"]]
    lons = [p["lon"] for p in water["points"]]
    print("barriers", len(dressing["barriers"]))
    print("cones", len(dressing["cones"]))
    print("skyline", len(skyline["buildings"]))
    print("grid", len(dressing["grid_slots"]))
    print("infield", len(dressing["infield_grass"]))
    print("hangars", len(dressing["airport_boxes"]))
    print("lake_bbox", min(lats), max(lats), min(lons), max(lons))


if __name__ == "__main__":
    main()
