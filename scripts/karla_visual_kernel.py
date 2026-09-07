#!/usr/bin/env python3
"""Karla — raceGPS offline visual kernel (first slice: skyline volumes).

Not CARLA. CARLA is an optional mesh source. Karla turns geographic semantics
(OSM / citypack buildings) into camera-needed silhouette volumes.

  python scripts/karla_visual_kernel.py \\
    --buildings citypacks/cleveland_5.0km/cleveland_5.0km_buildings.json \\
    --racing-line citypacks/cleveland/burke_gp_1997/racing_line.json \\
    --out citypacks/cleveland/burke_gp_1997/skyline.json

No network, no Cesium, no CARLA server. Heights: OSM height tag, else levels*3.5 m.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

DEFAULT_LEVEL_M = 3.5
MIN_HEIGHT_M = 24.0
MAX_VOLUMES = 80


def _latlon(obj: dict) -> tuple[float, float] | None:
    if "lat" in obj and "lon" in obj:
        return float(obj["lat"]), float(obj["lon"])
    fp = obj.get("footprint") or obj.get("points") or []
    if not fp:
        return None
    lats = [float(p["lat"]) for p in fp if "lat" in p]
    lons = [float(p["lon"]) for p in fp if "lon" in p]
    if not lats:
        return None
    return sum(lats) / len(lats), sum(lons) / len(lons)


def _height_m(obj: dict) -> float:
    for k in ("height", "height_m", "Height"):
        if obj.get(k) not in (None, "", 0, 0.0):
            try:
                return float(obj[k])
            except (TypeError, ValueError):
                pass
    levels = obj.get("levels") or obj.get("building:levels") or obj.get("Levels")
    if levels:
        try:
            return float(levels) * DEFAULT_LEVEL_M
        except (TypeError, ValueError):
            pass
    return 0.0


def _footprint_size(obj: dict) -> tuple[float, float, float]:
    """Return width_m, depth_m, yaw_deg from footprint or authored sizes."""
    if obj.get("width_m") and obj.get("depth_m"):
        return float(obj["width_m"]), float(obj["depth_m"]), float(obj.get("yaw_deg") or 0.0)
    fp = obj.get("footprint") or []
    if len(fp) < 2:
        return 30.0, 30.0, 0.0
    lats = [float(p["lat"]) for p in fp]
    lons = [float(p["lon"]) for p in fp]
    mid_lat = sum(lats) / len(lats)
    m_per_deg_lat = 111_320.0
    m_per_deg_lon = 111_320.0 * math.cos(math.radians(mid_lat))
    width = (max(lons) - min(lons)) * m_per_deg_lon
    depth = (max(lats) - min(lats)) * m_per_deg_lat
    return max(12.0, min(width, 80.0)), max(12.0, min(depth, 80.0)), 0.0


def circuit_south_edge(racing_line: dict) -> float:
    samples = racing_line["samples"]
    return min(float(s["lat"]) for s in samples)


def load_buildings(path: Path) -> list[dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, list):
        return data
    for key in ("buildings", "features", "elements"):
        if key in data and isinstance(data[key], list):
            return data[key]
    raise ValueError(f"no building array in {path}")


def to_volume(obj: dict, idx: int) -> dict | None:
    ll = _latlon(obj)
    if not ll:
        return None
    h = _height_m(obj)
    if h < MIN_HEIGHT_M:
        return None
    lat, lon = ll
    w, d, yaw = _footprint_size(obj)
    name = obj.get("name") or obj.get("id") or f"bldg_{idx}"
    material = "Building_Glass" if h >= 90.0 else "Building_Concrete"
    return {
        "name": str(name),
        "lat": round(lat, 6),
        "lon": round(lon, 6),
        "height_m": round(h, 1),
        "width_m": round(w, 1),
        "depth_m": round(d, 1),
        "yaw_deg": yaw,
        "material": material,
        "tier": 1,
        "kind": "silhouette_volume",
        "source": "karla",
    }


def build_skyline(
    buildings: list[dict],
    south_of_lat: float,
    landmarks: list[dict] | None = None,
    max_volumes: int = MAX_VOLUMES,
) -> dict:
    vols: list[dict] = []
    for i, b in enumerate(buildings):
        v = to_volume(b, i)
        if not v:
            continue
        if v["lat"] >= south_of_lat:
            continue  # keep skyline SOUTH of the airport circuit
        vols.append(v)
    vols.sort(key=lambda x: x["height_m"], reverse=True)
    vols = vols[: max(0, max_volumes - len(landmarks or []))]
    merged = list(landmarks or []) + vols
    # de-dupe by rounded lat/lon
    seen: set[tuple[float, float]] = set()
    unique: list[dict] = []
    for v in merged:
        key = (round(v["lat"], 4), round(v["lon"], 4))
        if key in seen:
            continue
        seen.add(key)
        unique.append(v)
    return {
        "id": "cleveland_downtown_silhouette",
        "tier": 1,
        "generator": "karla_visual_kernel",
        "material_rule": "Building_Glass if height_m >= 90 else Building_Concrete",
        "notes": "Karla-generated silhouette volumes SOUTH of Burke. Not storefronts, not Cesium, not a CARLA server.",
        "south_of_lat": south_of_lat,
        "count": len(unique),
        "buildings": unique,
    }



# Offline CARLA hangar mesh candidates (no server). None of these uassets
# currently exist under Content/Carla/Static (Car / GenericMaterials / Truck only).
CARLA_HANGAR_CANDIDATES = [
    "/Game/Carla/Static/Buildings/SM_Hangar_01",
    "/Game/Carla/Static/Buildings/SM_HangarLarge",
    "/Game/Carla/Static/Props/SM_AirportHangar",
    "/Game/Carla/Static/StaticMesh/SM_Hangar",
    "/Game/Carla/Static/Architecture/SM_Hangar",
]


def _is_hangar(obj: dict) -> bool:
    for k in ("aeroway", "building", "kind", "type", "amenity", "name"):
        v = str(obj.get(k) or "").lower()
        if "hangar" in v or v in {"fbo", "t-hangar", "thangar"}:
            return True
    return False


def haversine_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    r = 6_371_000.0
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dlat = p2 - p1
    dlon = math.radians(lon2 - lon1)
    h = math.sin(dlat / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dlon / 2) ** 2
    return 2 * r * math.asin(math.sqrt(min(1.0, h)))


def to_hangar_box(obj: dict, idx: int) -> dict | None:
    """Airport hangar volume. Complements HISM city; does not replace skyline."""
    ll = _latlon(obj)
    if not ll:
        return None
    lat, lon = ll
    w, d, yaw = _footprint_size(obj)
    h = _height_m(obj)
    if h <= 0:
        h = 9.0
    h = max(6.0, min(h, 16.0))  # hangars, not terminals
    name = obj.get("name") or obj.get("id") or f"hangar_{idx}"
    return {
        "name": str(name),
        "lat": round(lat, 6),
        "lon": round(lon, 6),
        "height_m": round(h, 1),
        "width_m": round(max(16.0, min(w, 80.0)), 1),
        "depth_m": round(max(12.0, min(d, 40.0)), 1),
        "yaw_deg": round(yaw if yaw else 58.0, 1),
        "material": "Building_Concrete",
        "kind": "hangar",
        "mesh_path": CARLA_HANGAR_CANDIDATES[0],
        "prefer_carla_mesh": True,
        "complement_hism": True,
        "source": "karla_airport",
    }


def build_airport_hangars(
    buildings: list[dict],
    origin_lat: float = 41.51722,
    origin_lon: float = -81.68306,
    radius_m: float = 900.0,
    max_hangars: int = 16,
) -> dict:
    """Extract hangar-like OSM boxes near Burke. Does not touch skyline.json."""
    boxes: list[dict] = []
    for i, b in enumerate(buildings):
        if not _is_hangar(b):
            continue
        box = to_hangar_box(b, i)
        if not box:
            continue
        if haversine_m(origin_lat, origin_lon, box["lat"], box["lon"]) > radius_m:
            continue
        boxes.append(box)
        if len(boxes) >= max_hangars:
            break
    return {
        "id": "cleveland_burke_airport_hangars",
        "generator": "karla_visual_kernel.build_airport_hangars",
        "origin": {"lat": origin_lat, "lon": origin_lon},
        "radius_m": radius_m,
        "carla_required": False,
        "hangar_mesh_candidates": list(CARLA_HANGAR_CANDIDATES),
        "notes": (
            "Offline hangar suggestions near Burke. Complements Cleveland5_0KmWorld "
            "HISM city. CARLA mesh paths are LoadObject candidates; boxes if missing. "
            "No CARLA server."
        ),
        "count": len(boxes),
        "airport_boxes": boxes,
    }


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Karla skyline kernel")
    ap.add_argument("--buildings", type=Path, required=True)
    ap.add_argument("--racing-line", type=Path, required=True)
    ap.add_argument("--landmarks", type=Path, default=None, help="existing skyline.json to keep named towers")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--max", type=int, default=MAX_VOLUMES)
    ap.add_argument(
        "--airport-out",
        type=Path,
        default=None,
        help="optional hangar JSON (does not rewrite skyline)",
    )
    ap.add_argument("--origin-lat", type=float, default=41.51722)
    ap.add_argument("--origin-lon", type=float, default=-81.68306)
    args = ap.parse_args(argv)

    racing = json.loads(args.racing_line.read_text(encoding="utf-8"))
    south = circuit_south_edge(racing)
    landmarks: list[dict] = []
    if args.landmarks and args.landmarks.is_file():
        raw = json.loads(args.landmarks.read_text(encoding="utf-8"))
        landmarks = list(raw.get("buildings") or [])
        for lm in landmarks:
            lm.setdefault("tier", 0)
            lm.setdefault("kind", "silhouette_volume")
    buildings = load_buildings(args.buildings)
    sky = build_skyline(buildings, south, landmarks=landmarks, max_volumes=args.max)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(sky, indent=2) + "\n", encoding="utf-8")
    print(f"karla: wrote {len(sky['buildings'])} volumes to {args.out} (south of {south:.5f})")
    if args.airport_out:
        hang = build_airport_hangars(
            buildings,
            origin_lat=args.origin_lat,
            origin_lon=args.origin_lon,
        )
        args.airport_out.parent.mkdir(parents=True, exist_ok=True)
        args.airport_out.write_text(json.dumps(hang, indent=2) + "\n", encoding="utf-8")
        print(f"karla: wrote {hang['count']} hangars to {args.airport_out} (skyline untouched)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
