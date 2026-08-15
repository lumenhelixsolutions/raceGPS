#!/usr/bin/env python3
"""
raceGPS — City runtime-readiness check (story S5).

Verifies that a given citypack + level spec pair contains every file and field
the UE5 runtime will look for, using the same resolution rules implemented in
`UAkronXodrImporter` (apps/unreal-akron-beta/Source/raceGPSAkronBeta/):

  * city selection: --city > DefaultGame.ini [RaceGPS.CitySelection] CityId
    > built-in default "akron-oh-beta-001"
  * manifest discovery: <pack>/*_semantic_manifest.json
  * file refs: flat fields (Dialect A) or files.<field> (Dialect B)
  * routes: single array file OR legacy per-route routes/ directory
  * spawns/POIs: filename refs OR embedded arrays
  * road graph / XODR: manifest ref or glob fallback
  * level spec: generated/*_LevelSpec.json whose city_id matches

Usage:
  python tools/verify-city-runtime-readiness.py                     # active city from ini
  python tools/verify-city-runtime-readiness.py --city cleveland_5.0km
  python tools/verify-city-runtime-readiness.py --city akron-oh-beta-001 --root .

Exit code 0 = runtime will find everything it needs; 1 = something is missing.
"""

import argparse
import configparser
import json
import re
import sys
from pathlib import Path

DEFAULT_CITY_ID = "akron-oh-beta-001"
PROJECT_DIR_REL = Path("apps/unreal-akron-beta")  # UE project dir, relative to repo root


class Report:
    def __init__(self):
        self.errors = []
        self.warnings = []
        self.info = []

    def ok(self, msg):
        self.info.append(f"  [OK]   {msg}")

    def warn(self, msg):
        self.warnings.append(msg)
        print(f"  [WARN] {msg}")

    def fail(self, msg):
        self.errors.append(msg)
        print(f"  [FAIL] {msg}")


def read_game_ini_city_config(project_dir: Path) -> dict:
    """Read [RaceGPS.CitySelection] from DefaultGame.ini (tolerant of UE ini quirks)."""
    ini_path = project_dir / "Config" / "DefaultGame.ini"
    out = {}
    if not ini_path.is_file():
        return out
    parser = configparser.ConfigParser(allow_no_value=True)
    parser.optionxform = str
    try:
        parser.read(ini_path, encoding="utf-8-sig")
    except configparser.Error:
        return out
    if parser.has_section("RaceGPS.CitySelection"):
        for key in ("CityId", "CitypackDir", "LevelSpecFile"):
            if parser.has_option("RaceGPS.CitySelection", key):
                value = parser.get("RaceGPS.CitySelection", key)
                if value:
                    out[key] = value
    return out


def resolve_project_relative(project_dir: Path, rel: str) -> Path:
    """Resolve a UE project-relative path like '../../citypacks/x' against the project dir."""
    return (project_dir / rel).resolve()


def manifest_file_ref(manifest: dict, field: str):
    """Flat field (Dialect A) or files.<field> (Dialect B); string refs only."""
    value = manifest.get(field)
    if isinstance(value, str) and value:
        return value
    files = manifest.get("files")
    if isinstance(files, dict):
        value = files.get(field)
        if isinstance(value, str) and value:
            return value
    return None


def load_json(path: Path):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except Exception as e:  # noqa: BLE001 - report any parse/read failure
        return e


def check_routes_source(report: Report, routes_path: Path):
    """Mirror LoadRouteSplines: array file, single object, or per-route directory."""
    if routes_path.is_file():
        data = load_json(routes_path)
        if isinstance(data, Exception):
            report.fail(f"routes file unreadable: {routes_path.name} ({data})")
            return
        if isinstance(data, dict):
            data = [data]
        if not isinstance(data, list) or not data:
            report.fail(f"routes file has no route array: {routes_path.name}")
            return
        n_bad = 0
        for route in data:
            if not isinstance(route, dict):
                n_bad += 1
                continue
            if not (route.get("route_id")):
                n_bad += 1
            if "distance_meters" not in route and "distance_m" not in route:
                n_bad += 1
            if len(route.get("points") or []) < 2:
                n_bad += 1
        if n_bad:
            report.fail(f"{routes_path.name}: {n_bad}/{len(data)} route(s) missing route_id/distance/points")
        else:
            report.ok(f"routes file {routes_path.name}: {len(data)} route(s) loadable")
    elif routes_path.is_dir():
        files = sorted(routes_path.glob("*.json"))
        if not files:
            report.fail(f"legacy routes directory has no *.json: {routes_path}")
        else:
            report.ok(f"legacy routes directory: {len(files)} per-route file(s)")
    else:
        report.fail(f"routes source not found: {routes_path}")


def check_spawn_or_poi_file(report: Report, label: str, path: Path):
    """Mirror the array-file branch of LoadSpawnPoints/LoadPOIs."""
    if not path.is_file():
        report.fail(f"{label} file not found: {path}")
        return
    data = load_json(path)
    if isinstance(data, Exception):
        report.fail(f"{label} file unreadable: {path.name} ({data})")
        return
    if isinstance(data, dict):
        data = data.get(label)  # tolerate {"spawn_points": [...]} / {"pois": [...]}
    if not isinstance(data, list):
        report.fail(f"{label} file is not an array: {path.name}")
        return
    n_bad = sum(
        1 for item in data
        if not isinstance(item, dict) or "id" not in item or "lat" not in item or "lon" not in item
    )
    if n_bad:
        report.fail(f"{path.name}: {n_bad}/{len(data)} {label} entries missing id/lat/lon")
    else:
        report.ok(f"{label} file {path.name}: {len(data)} entries loadable")


def main():
    parser = argparse.ArgumentParser(description="raceGPS city runtime-readiness check")
    parser.add_argument("--city", help="City id (overrides DefaultGame.ini); default comes from ini or Akron")
    parser.add_argument("--root", default=str(Path(__file__).resolve().parent.parent),
                        help="Repo root (default: parent of tools/)")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    project_dir = root / PROJECT_DIR_REL
    ini = read_game_ini_city_config(project_dir)

    city_id = args.city or ini.get("CityId") or DEFAULT_CITY_ID
    source = "--city" if args.city else ("DefaultGame.ini" if ini.get("CityId") else "built-in default")

    report = Report()
    print(f"raceGPS runtime-readiness check")
    print(f"  city_id:  {city_id} (from {source})")
    print(f"  root:     {root}")
    print()

    # --- citypack dir + manifest -------------------------------------------------
    if ini.get("CitypackDir"):
        pack_dir = resolve_project_relative(project_dir, ini["CitypackDir"])
    else:
        pack_dir = root / "citypacks" / city_id

    if not pack_dir.is_dir():
        report.fail(f"citypack directory not found: {pack_dir}")
        print_summary(report)
        sys.exit(1)
    report.ok(f"citypack directory: {pack_dir}")

    manifests = sorted(pack_dir.glob("*_semantic_manifest.json"))
    if not manifests:
        report.fail(f"no *_semantic_manifest.json in {pack_dir}")
        print_summary(report)
        sys.exit(1)
    manifest_path = manifests[0]
    report.ok(f"manifest: {manifest_path.name}")

    manifest = load_json(manifest_path)
    if isinstance(manifest, Exception) or not isinstance(manifest, dict):
        report.fail(f"manifest unreadable: {manifest_path.name}")
        print_summary(report)
        sys.exit(1)

    manifest_city = manifest.get("city_id")
    if manifest_city and manifest_city != city_id:
        report.warn(f"manifest city_id '{manifest_city}' != selected city '{city_id}'")

    display_name = manifest.get("display_name") or manifest.get("name")
    if display_name:
        report.ok(f"display name: {display_name} ({'display_name' if manifest.get('display_name') else 'name'})")
    else:
        report.warn("manifest has neither display_name (Dialect A) nor name (Dialect B)")

    # --- origin (LoadManifest) ---------------------------------------------------
    origin = manifest.get("origin") or {}
    bounds = manifest.get("bounds") or {}
    if "lat" in origin and "lon" in origin:
        report.ok(f"origin: ({origin['lat']}, {origin['lon']})")
    elif "lat_min" in bounds and "lon_min" in bounds:
        report.ok("origin via legacy bounds.lat_min/lon_min")
    elif "south" in bounds and "west" in bounds:
        report.ok(f"origin via bounds corner: ({bounds['south']}, {bounds['west']})")
    else:
        report.fail("no origin.lat/lon and no usable bounds corner — runtime keeps caller default origin")

    # --- routes ------------------------------------------------------------------
    routes_ref = manifest_file_ref(manifest, "routes")
    if routes_ref:
        check_routes_source(report, pack_dir / routes_ref)
    elif (pack_dir / "routes").is_dir():
        check_routes_source(report, pack_dir / "routes")
    else:
        report.fail("no routes reference (flat or files.routes) and no routes/ directory")

    # --- spawn points / POIs ------------------------------------------------------
    for label in ("spawn_points", "pois"):
        ref = manifest_file_ref(manifest, label)
        if ref:
            check_spawn_or_poi_file(report, label, pack_dir / ref)
        elif isinstance(manifest.get(label), list):
            report.ok(f"{label}: embedded array ({len(manifest[label])} entries)")
        else:
            report.fail(f"no {label} file reference and no embedded array")

    # --- road graph ---------------------------------------------------------------
    rg_ref = manifest_file_ref(manifest, "road_graph")
    if rg_ref and (pack_dir / rg_ref).is_file():
        report.ok(f"road graph: {rg_ref}")
    else:
        matches = sorted(pack_dir.glob("*_road_graph.json"))
        if matches:
            report.ok(f"road graph (glob): {matches[0].name}")
        else:
            report.fail("no road_graph reference and no *_road_graph.json (XODR fallback will fail)")

    # --- XODR ----------------------------------------------------------------------
    xodr_ref = manifest.get("opendrive_file")
    if xodr_ref and (pack_dir / xodr_ref).is_file():
        report.ok(f"XODR: {xodr_ref}")
    else:
        matches = sorted(pack_dir.glob("*.xodr"))
        if matches:
            report.ok(f"XODR (glob): {matches[0].name}")
        else:
            report.warn("no XODR file — runtime will use the road-graph JSON fallback")

    # --- buildings (optional) ------------------------------------------------------
    bld_ref = manifest_file_ref(manifest, "buildings")
    if bld_ref and (pack_dir / bld_ref).is_file():
        report.ok(f"buildings: {bld_ref}")
    else:
        matches = sorted(pack_dir.glob("*_buildings.json"))
        if matches:
            report.ok(f"buildings (glob): {matches[0].name}")
        else:
            report.warn("no buildings file (optional; BuildingMeshGenerator gets nothing)")

    # --- level spec ------------------------------------------------------------------
    if ini.get("LevelSpecFile"):
        spec_path = resolve_project_relative(project_dir, ini["LevelSpecFile"])
        spec_candidates = [spec_path] if spec_path.is_file() else []
    else:
        generated = root / "generated"
        spec_candidates = []
        for spec in sorted(generated.glob("*_LevelSpec.json")):
            data = load_json(spec)
            if isinstance(data, dict) and data.get("city_id") == city_id:
                spec_candidates.append(spec)

    if not spec_candidates:
        report.fail(f"no generated/*_LevelSpec.json with city_id '{city_id}'")
    else:
        spec_path = spec_candidates[0]
        spec = load_json(spec_path)
        level_name = spec.get("level_name") if isinstance(spec, dict) else None
        report.ok(f"level spec: {spec_path.name} (level_name: {level_name or 'MISSING'})")
        if isinstance(spec, dict):
            if not level_name:
                report.fail(f"{spec_path.name}: missing level_name (menu cannot open the level)")
            for key in ("spawn_points", "routes"):
                if not isinstance(spec.get(key), list) or not spec[key]:
                    report.warn(f"{spec_path.name}: '{key}' empty or missing")
            # --- baked map sanity (cheap binary sniff; T10 black-map regression) ---
            if level_name:
                safe_name = re.sub(r"[^A-Za-z0-9_]", "_", level_name)
                umap = (project_dir / "Content" / "Maps" / f"{safe_name}.umap")
                if not umap.is_file():
                    report.warn(f"baked map missing: {umap.relative_to(root)} "
                                "(run tools/ue5-headless-city-import.py)")
                else:
                    blob = umap.read_bytes()
                    missing_rig = [name for name in
                                   (b"DirectionalLight", b"SkyAtmosphere", b"SkyLight")
                                   if name not in blob]
                    if missing_rig:
                        report.fail(f"{umap.name}: no lighting actors baked in "
                                    f"(missing {', '.join(n.decode() for n in missing_rig)}) "
                                    "— map will render black; run tools/ue5-headless-lighting-rig.py")
                    else:
                        report.ok(f"baked map lighting rig present: {umap.name}")

    print_summary(report)
    sys.exit(1 if report.errors else 0)


def print_summary(report: Report):
    print()
    for line in report.info:
        print(line)
    print()
    print(f"Result: {len(report.errors)} error(s), {len(report.warnings)} warning(s)")
    if report.errors:
        print("FAILED — the runtime will not fully load this city.")
    else:
        print("PASSED — the runtime has everything it looks for.")


if __name__ == "__main__":
    main()
