#!/usr/bin/env python3
"""
T10 prep: convert a citypack into a compact UE-space import bundle for the
headless city importer (tools/ue5-headless-city-import.py).

Runs OUTSIDE the editor (plain python) so the commandlet stays fast:
  - terrain: heightmap grid -> UE meter grid (east=X, north=Y, up=Z,
    absolute heights in meters above sea level)
  - buildings: footprint AABB -> instanced box transforms (x, y, z_base,
    sx, sy, h, material bucket), z_base sampled from the terrain grid
  - water: consumes the NEW water schema. Source priority:
      1. <citypack>/<city>_water.json if it has the T2 schema
         (rivers / river_polygons / lakes with points)  [T6 will bake this]
      2. else parse <city>_water_raw.osm via the compiler's read-only
         water_extractor.extract_water()
    Lakes/river polygons are clipped to the level-spec world bounds and
    emitted as plane transforms; river polylines are emitted as thin
    per-segment planes (the Cuyahoga etc. show as actual courses).
  - pois: up to 200 landmark TargetPoints

Output: generated/<city>_ueimport.json

Axis contract (matches the level-spec pipeline, remapped to UE Z-up):
  data x = east m   -> UE X
  data z = -north m -> UE Y = -data_z
  data y = up m     -> UE Z
"""

import argparse
import importlib.util
import json
import math
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

RIVER_WIDTH_M = {"river": 18.0, "canal": 12.0, "stream": 6.0}
WATER_DROP_M = 1.0          # water plane sits this far below sampled terrain
MIN_WATER_AREA_M2 = 200.0   # drop puddles smaller than this after clipping
MAX_RIVER_SEGMENTS = 30000
MAX_LANDMARK_POIS = 200
DEFAULT_BUILDING_HEIGHT_M = 7.0
LEVEL_HEIGHT_M = 3.2

# Material buckets -> index (order stable; "other" catches the rest)
MATERIAL_BUCKETS = ["concrete", "brick", "glass", "metal", "wood", "stone", "other"]


def meters_per_degree_lon(origin_lat: float) -> float:
    return 111320.0 * math.cos(math.radians(origin_lat))


def meters_per_degree_lat() -> float:
    return 110540.0


def geo_to_ue(lat, lon, origin_lat, origin_lon, mpdlon, mpdlat):
    """lat/lon -> (ue_x_m, ue_y_m) with X=east, Y=north."""
    return ((lon - origin_lon) * mpdlon, (lat - origin_lat) * mpdlat)


def load_water(citypack: Path, city_id: str):
    """Return the new-schema water dict from the baked json or the raw osm."""
    water_json = citypack / f"{city_id}_water.json"
    if water_json.exists():
        data = json.loads(water_json.read_text(encoding="utf-8"))
        if data.get("rivers") or data.get("river_polygons") or data.get("lakes"):
            print(f"[prep] water source: {water_json.name} (baked, T2 schema)")
            return data
        print(f"[prep] {water_json.name} is an empty stub; falling back to raw OSM")
    raw_osm = citypack / f"{city_id}_water_raw.osm"
    if not raw_osm.exists():
        print("[prep] no water data available")
        return {"rivers": [], "river_polygons": [], "lakes": [], "coastlines": []}
    extractor_path = REPO_ROOT / "tools" / "universal-city-compiler" / "water_extractor.py"
    spec = importlib.util.spec_from_file_location("water_extractor", extractor_path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    print(f"[prep] water source: {raw_osm.name} via water_extractor.extract_water (read-only)")
    return mod.extract_water(raw_osm)


def clip_polygon_to_rect(pts, xmin, ymin, xmax, ymax):
    """Sutherland-Hodgman clip of [(x, y), ...] against an axis-aligned rect."""
    def clip_edge(poly, inside, intersect):
        if not poly:
            return poly
        out = []
        prev = poly[-1]
        for cur in poly:
            if inside(cur):
                if not inside(prev):
                    out.append(intersect(prev, cur))
                out.append(cur)
            elif inside(prev):
                out.append(intersect(prev, cur))
            prev = cur
        return out

    def ix(p, q):  # vertical edge intersections param along segment
        return p, q

    poly = list(pts)
    # left
    poly = clip_edge(poly, lambda p: p[0] >= xmin,
                     lambda p, q: (xmin, p[1] + (q[1] - p[1]) * (xmin - p[0]) / (q[0] - p[0]) if q[0] != p[0] else p[1]))
    # right
    poly = clip_edge(poly, lambda p: p[0] <= xmax,
                     lambda p, q: (xmax, p[1] + (q[1] - p[1]) * (xmax - p[0]) / (q[0] - p[0]) if q[0] != p[0] else p[1]))
    # bottom
    poly = clip_edge(poly, lambda p: p[1] >= ymin,
                     lambda p, q: (p[0] + (q[0] - p[0]) * (ymin - p[1]) / (q[1] - p[1]) if q[1] != p[1] else p[0], ymin))
    # top
    poly = clip_edge(poly, lambda p: p[1] <= ymax,
                     lambda p, q: (p[0] + (q[0] - p[0]) * (ymax - p[1]) / (q[1] - p[1]) if q[1] != p[1] else p[0], ymax))
    return poly


def polygon_area(pts):
    if len(pts) < 3:
        return 0.0
    a = 0.0
    for i in range(len(pts)):
        x1, y1 = pts[i]
        x2, y2 = pts[(i + 1) % len(pts)]
        a += x1 * y2 - x2 * y1
    return abs(a) / 2.0


class TerrainGrid:
    """Bilinear sampler over the citypack heightmap, in UE meters (absolute)."""

    def __init__(self, heightmap_doc, origin_lat, origin_lon):
        self.rows = heightmap_doc["rows"]
        self.cols = heightmap_doc["cols"]
        b = heightmap_doc["bounds"]
        self.min_elev = heightmap_doc["min_elevation"]
        self.grid = heightmap_doc["heightmap"]
        mpdlon = meters_per_degree_lon(origin_lat)
        mpdlat = meters_per_degree_lat()
        # Row 0 = south edge, col 0 = west edge.
        self.x0 = (b["west"] - origin_lon) * mpdlon
        self.x1 = (b["east"] - origin_lon) * mpdlon
        self.y0 = (b["south"] - origin_lat) * mpdlat
        self.y1 = (b["north"] - origin_lat) * mpdlat

    def height_at(self, x, y):
        """Bilinear ABSOLUTE height (meters above sea level) at UE (x, y).

        Sprint-2 scale story: elevations are absolute so pawns/gates rest on
        the terrain at their real-world Z (~17k-26k uu for Cleveland).
        """
        fx = (x - self.x0) / (self.x1 - self.x0) * (self.cols - 1)
        fy = (y - self.y0) / (self.y1 - self.y0) * (self.rows - 1)
        fx = min(max(fx, 0.0), self.cols - 1.001)
        fy = min(max(fy, 0.0), self.rows - 1.001)
        c0, r0 = int(fx), int(fy)
        tc, tr = fx - c0, fy - r0
        g = self.grid
        h00 = g[r0][c0]
        h10 = g[r0][c0 + 1]
        h01 = g[r0 + 1][c0]
        h11 = g[r0 + 1][c0 + 1]
        h = (h00 * (1 - tc) + h10 * tc) * (1 - tr) + (h01 * (1 - tc) + h11 * tc) * tr
        return h


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--city", default="cleveland_5.0km")
    args = ap.parse_args()
    city_id = args.city

    citypack = REPO_ROOT / "citypacks" / city_id
    manifest_path = next(citypack.glob("*_semantic_manifest.json"), None)
    if not manifest_path:
        print(f"[prep] ERROR: no manifest in {citypack}")
        return 1
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    origin_lat = manifest["origin"]["lat"]
    origin_lon = manifest["origin"]["lon"]
    mpdlon = meters_per_degree_lon(origin_lat)
    mpdlat = meters_per_degree_lat()

    # Level spec: world bounds for clipping + name cross-check.
    spec_files = sorted((REPO_ROOT / "generated").glob("*_LevelSpec.json"))
    spec_doc = None
    for sf in spec_files:
        doc = json.loads(sf.read_text(encoding="utf-8"))
        if doc.get("city_id") == city_id:
            spec_doc = doc
            break
    if not spec_doc:
        print(f"[prep] ERROR: no level spec for {city_id}")
        return 1
    wb = spec_doc["world_bounds"]
    # data bounds: x east in [min_x, max_x]; data z=-north -> north in [-max_z, -min_z]
    # NOTE: spec world_bounds only cover the routes area (+500m padding), which is
    # much smaller than the citypack. Clip to the TERRAIN extent instead (the
    # heightmap bbox == citypack bbox) so the whole city is imported; fall back
    # to world_bounds when there is no heightmap.
    xmin, xmax = wb["min_x"], wb["max_x"]
    ymin, ymax = -wb["max_z"], -wb["min_z"]

    out = {"city_id": city_id, "level_name": spec_doc.get("level_name", "")}

    # ---------------- terrain ----------------
    hm_path = citypack / manifest["files"]["heightmap"]
    terrain = None
    if hm_path.exists():
        hm = json.loads(hm_path.read_text(encoding="utf-8"))
        tg = TerrainGrid(hm, origin_lat, origin_lon)
        # Widen the clip rect to the terrain (= citypack) extent.
        xmin, xmax, ymin, ymax = tg.x0, tg.x1, tg.y0, tg.y1
        terrain = {
            "rows": tg.rows, "cols": tg.cols,
            "x0": tg.x0, "y0": tg.y0, "x1": tg.x1, "y1": tg.y1,
            # absolute heights (meters above sea level)
            "heights": [list(row) for row in tg.grid],
        }
        print(f"[prep] terrain: {tg.cols}x{tg.rows} grid, relief "
              f"{hm['min_elevation']:.0f}..{hm['max_elevation']:.0f} m")
    else:
        tg = None
        print("[prep] no heightmap; terrain skipped, everything at z=0")
    out["terrain"] = terrain

    print(f"[prep] clip rect UE m: x [{xmin:.0f},{xmax:.0f}] y(north) [{ymin:.0f},{ymax:.0f}]")

    def ground_z(x, y):
        return tg.height_at(x, y) if tg else 0.0

    # ---------------- buildings ----------------
    bld_path = citypack / manifest["files"]["buildings"]
    buildings = []
    bucket_counts = {b: 0 for b in MATERIAL_BUCKETS}
    if bld_path.exists():
        doc = json.loads(bld_path.read_text(encoding="utf-8"))
        for b in doc.get("buildings", []):
            fp = b.get("footprint") or []
            if len(fp) < 3:
                continue
            xs = []
            ys = []
            for p in fp:
                ux, uy = geo_to_ue(p["lat"], p["lon"], origin_lat, origin_lon, mpdlon, mpdlat)
                xs.append(ux)
                ys.append(uy)
            cx, cy = geo_to_ue(b["lat"], b["lon"], origin_lat, origin_lon, mpdlon, mpdlat)
            sx = max(xs) - min(xs)
            sy = max(ys) - min(ys)
            if sx < 1.0 or sy < 1.0:
                continue
            h = b.get("height_meters") or (b.get("levels") or 0) * LEVEL_HEIGHT_M or DEFAULT_BUILDING_HEIGHT_M
            mat = (b.get("material") or "other").lower()
            if mat not in bucket_counts:
                mat = "other"
            bucket_counts[mat] += 1
            buildings.append([
                round(cx, 2), round(cy, 2), round(ground_z(cx, cy), 2),
                round(sx, 2), round(sy, 2), round(float(h), 2),
                MATERIAL_BUCKETS.index(mat),
            ])
    out["material_buckets"] = MATERIAL_BUCKETS
    out["buildings"] = buildings
    print(f"[prep] buildings: {len(buildings)} (buckets: "
          + ", ".join(f"{k}={v}" for k, v in bucket_counts.items() if v) + ")")

    # ---------------- water ----------------
    water = load_water(citypack, city_id)
    water_planes = []

    def add_poly_planes(entries, kind):
        added = 0
        for e in entries:
            pts = [geo_to_ue(p["lat"], p["lon"], origin_lat, origin_lon, mpdlon, mpdlat)
                   for p in e.get("points", [])]
            if len(pts) < 3:
                continue
            clipped = clip_polygon_to_rect(pts, xmin, ymin, xmax, ymax)
            if polygon_area(clipped) < MIN_WATER_AREA_M2:
                continue
            pxs = [p[0] for p in clipped]
            pys = [p[1] for p in clipped]
            cx = (min(pxs) + max(pxs)) / 2
            cy = (min(pys) + max(pys)) / 2
            water_planes.append([
                round(cx, 2), round(cy, 2), round(max(ground_z(cx, cy) - WATER_DROP_M, 0.05), 2),
                round(max(pxs) - min(pxs), 2), round(max(pys) - min(pys), 2), 0.0, kind,
            ])
            added += 1
        return added

    n_lakes = add_poly_planes(water.get("lakes", []), "lake")
    n_riverpoly = add_poly_planes(water.get("river_polygons", []), "riverpoly")

    # Rivers as thin per-segment planes so courses are visible.
    n_segments = 0
    for river in water.get("rivers", []):
        width = RIVER_WIDTH_M.get(river.get("type", "river"), 10.0)
        pts = [geo_to_ue(p["lat"], p["lon"], origin_lat, origin_lon, mpdlon, mpdlat)
               for p in river.get("points", [])]
        for i in range(len(pts) - 1):
            if n_segments >= MAX_RIVER_SEGMENTS:
                break
            x1, y1 = pts[i]
            x2, y2 = pts[i + 1]
            # skip segments fully outside the clip rect
            if (max(x1, x2) < xmin or min(x1, x2) > xmax
                    or max(y1, y2) < ymin or min(y1, y2) > ymax):
                continue
            dx, dy = x2 - x1, y2 - y1
            length = math.hypot(dx, dy)
            if length < 10.0:
                continue
            cx, cy = (x1 + x2) / 2, (y1 + y2) / 2
            water_planes.append([
                round(cx, 2), round(cy, 2), round(max(ground_z(cx, cy) - WATER_DROP_M, 0.05), 2),
                round(length + width, 2), round(width, 2),
                round(math.degrees(math.atan2(dy, dx)), 2), "river",
            ])
            n_segments += 1
    out["water_planes"] = water_planes
    print(f"[prep] water: {n_lakes} lake + {n_riverpoly} river-polygon planes, "
          f"{n_segments} river segments (from {water.get('river_count', 0)} rivers, "
          f"{water.get('lake_count', 0)} lakes, {water.get('river_polygon_count', 0)} river polygons)")

    # ---------------- POIs (landmarks only, capped) ----------------
    poi_path = citypack / manifest["files"]["pois"]
    pois = []
    if poi_path.exists():
        doc = json.loads(poi_path.read_text(encoding="utf-8"))
        for p in doc:
            if p.get("type") != "landmark":
                continue
            ux, uy = geo_to_ue(p["lat"], p["lon"], origin_lat, origin_lon, mpdlon, mpdlat)
            if not (xmin <= ux <= xmax and ymin <= uy <= ymax):
                continue
            pois.append([round(ux, 2), round(uy, 2), round(ground_z(ux, uy), 2),
                         (p.get("name") or p.get("subtype") or "poi")[:60]])
            if len(pois) >= MAX_LANDMARK_POIS:
                break
    out["pois"] = pois
    print(f"[prep] pois: {len(pois)} landmark markers")

    out_path = REPO_ROOT / "generated" / f"{city_id}_ueimport.json"
    out_path.write_text(json.dumps(out), encoding="utf-8")
    print(f"[prep] wrote {out_path} ({out_path.stat().st_size / 1e6:.1f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
