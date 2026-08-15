#!/usr/bin/env python3
"""
T10 headless FULL-city import (terrain + buildings + water + POIs + spec actors).

Run via:
    UnrealEditor-Cmd.exe "<uproject>" -run=pythonscript ^
        -script="tools/ue5-headless-city-import.py" -unattended -nop4 -nullrhi

Consumes generated/<city>_ueimport.json produced by tools/ue5-city-import-prep.py
(plain-python preprocessing) and drives the staged, resumable import below.
Re-running is idempotent: a progress file tracks the stage; interrupted runs
resume where they stopped (building instancing is batched across runs).

Stages:
  spec      recreate /Game/Maps/<level>, ensure BP_CheckpointGate, run the
            level-spec actor pass (spawns, routes, gates, captures, volumes)
  terrain   heightmap grid -> ProceduralMeshComponent on a
            BuildingMeshGenerator actor (its PMC root serializes with the map)
  water     one BP with a HISM of engine Plane meshes: lake/river-polygon
            planes + thin per-segment river planes, clipped in prep
  buildings one HISM per material bucket (engine Cube), AABB boxes at
            terrain-sampled base heights; batched (25k/run) with resume
  pois      landmark TargetPoints (<=200)

Coordinate contract: bundle is already UE-space meters (X=east, Y=north,
Z=up, terrain-relative); this script only scales m -> cm.
"""

import importlib.util
import json
import math
import os
import re
import sys
from pathlib import Path

import unreal

SCRIPT_PATH = Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent

CITY_ID = os.environ.get("RACEGPS_CITY_ID", "cleveland_5.0km")
BUNDLE_REL = f"generated/{CITY_ID}_ueimport.json"
PROGRESS_REL = f"generated/{CITY_ID}_ueimport_progress.json"


def find_spec_for_city(city_id: str) -> Path:
    """Locate generated/*_LevelSpec.json matching the city_id."""
    for sf in sorted((REPO_ROOT / "generated").glob("*_LevelSpec.json")):
        try:
            doc = json.loads(sf.read_text(encoding="utf-8"))
        except Exception:
            continue
        if doc.get("city_id") == city_id:
            return sf
    raise RuntimeError(f"no level spec found for city {city_id}")

BUILDING_BATCH = 25000
CUBE_SIZE_CM = 100.0   # /Engine/BasicShapes/Cube
PLANE_SIZE_CM = 100.0  # /Engine/BasicShapes/Plane

BUCKET_COLORS = {
    "concrete": (0.62, 0.60, 0.57, 1.0),
    "brick": (0.55, 0.25, 0.18, 1.0),
    "glass": (0.35, 0.55, 0.65, 1.0),
    "metal": (0.45, 0.47, 0.50, 1.0),
    "wood": (0.45, 0.33, 0.20, 1.0),
    "stone": (0.50, 0.48, 0.44, 1.0),
    "other": (0.58, 0.56, 0.52, 1.0),
}


def log(msg):
    unreal.log_warning(f"[city-import] {msg}")


def load_module(name, rel_path):
    spec = importlib.util.spec_from_file_location(name, REPO_ROOT / rel_path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def sanitize_level_name(level_name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", level_name)


def read_progress():
    p = REPO_ROOT / PROGRESS_REL
    if p.exists():
        return json.loads(p.read_text(encoding="utf-8"))
    return {"stage": "spec", "building_index": 0}


def write_progress(prog):
    (REPO_ROOT / PROGRESS_REL).write_text(json.dumps(prog), encoding="utf-8")


def make_color_material(name, rgba, translucent=False):
    """Create (or reuse) a flat-color material asset; None on failure."""
    package = f"/Game/Materials/{name}"
    existing = unreal.EditorAssetLibrary.load_asset(package)
    if existing:
        return existing
    try:
        at = unreal.AssetToolsHelpers.get_asset_tools()
        mat = at.create_asset(name, "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
        if not mat:
            return None
        expr = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
        expr.set_editor_property("constant", unreal.LinearColor(*rgba))
        unreal.MaterialEditingLibrary.connect_material_property(
            expr, "", unreal.MaterialProperty.MP_BASE_COLOR)
        if translucent:
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
            alpha = unreal.MaterialEditingLibrary.create_material_expression(
                mat, unreal.MaterialExpressionConstant, -400, 200)
            alpha.set_editor_property("r", rgba[3])
            unreal.MaterialEditingLibrary.connect_material_property(
                alpha, "", unreal.MaterialProperty.MP_OPACITY)
        unreal.MaterialEditingLibrary.recompile_material(mat)
        unreal.EditorAssetLibrary.save_asset(package)
        return mat
    except Exception as e:
        log(f"material {name} failed ({type(e).__name__}: {e}); using default")
        return None


def create_hism_bp(name, num_comps):
    """Create (or reuse) a BP with num_comps HISMC subobjects. Returns class."""
    package = f"/Game/Blueprints/{name}"
    cls = unreal.EditorAssetLibrary.load_blueprint_class(package)
    if cls:
        return cls
    at = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor)
    bp = at.create_asset(name, "/Game/Blueprints", unreal.Blueprint, factory)
    if not bp:
        raise RuntimeError(f"create_asset failed for {package}")
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = list(sds.k2_gather_subobject_data_for_blueprint(bp))
    for _ in range(num_comps):
        params = unreal.AddNewSubobjectParams()
        params.set_editor_property("parent_handle", handles[0])
        params.set_editor_property("new_class", unreal.HierarchicalInstancedStaticMeshComponent)
        params.set_editor_property("blueprint_context", bp)
        new_handle, fail = sds.add_new_subobject(params)
        if str(fail):  # fail is a Text; empty Text is still truthy as an object
            raise RuntimeError(f"add_new_subobject failed for {package}: {fail}")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_asset(package)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(package)
    if not cls:
        raise RuntimeError(f"load_blueprint_class failed for {package} right after creation")
    return cls


def find_actor_by_label(label):
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        if a.get_actor_label() == label:
            return a
    return None


# ---------------------------------------------------------------- stages

def _terrain_height_cm(terrain, x_cm, y_cm):
    """Bilinear-sample the terrain bundle at a world position (cm).

    Bundle fields x0/y0/x1/y1/heights are meters (absolute elevation);
    world X/Y here are cm (1 uu = 1 cm).
    """
    rows, cols = terrain["rows"], terrain["cols"]
    heights = terrain["heights"]
    x0, y0 = terrain["x0"] * 100.0, terrain["y0"] * 100.0
    dx = (terrain["x1"] - terrain["x0"]) / (cols - 1) * 100.0
    dy = (terrain["y1"] - terrain["y0"]) / (rows - 1) * 100.0
    fc = (x_cm - x0) / dx
    fr = (y_cm - y0) / dy
    fc = min(max(fc, 0.0), cols - 1.001)
    fr = min(max(fr, 0.0), rows - 1.001)
    c0, r0 = int(fc), int(fr)
    tc, tr = fc - c0, fr - r0
    h00 = heights[r0][c0] * 100.0
    h10 = heights[r0][c0 + 1] * 100.0
    h01 = heights[r0 + 1][c0] * 100.0
    h11 = heights[r0 + 1][c0 + 1] * 100.0
    return (h00 * (1 - tc) * (1 - tr) + h10 * tc * (1 - tr)
            + h01 * (1 - tc) * tr + h11 * tc * tr)


def _lift_spec_actors(bundle):
    """Lift spec-imported actors (PlayerStarts, gates, splines, volumes) to
    absolute terrain height. Spec y is 0 (flat), so without this every baked
    spawn/gate would sit 17km+ under the absolute-elevation terrain."""
    terrain = bundle.get("terrain")
    if not terrain:
        log("no terrain in bundle; spec actors left at spec heights")
        return
    # per-label-prefix Z offset above ground (cm): gates use box half-height
    lifts = (("SP_", 100.0), ("CP_", 400.0), ("ReflCapture_", 5000.0),
             ("TrafficVol_", 2000.0))
    lifted = 0
    spline_pts = 0
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        label = actor.get_actor_label()
        loc = actor.get_actor_location()
        for prefix, off in lifts:
            if label.startswith(prefix):
                ground = _terrain_height_cm(terrain, loc.x, loc.y)
                actor.set_actor_location(
                    unreal.Vector(loc.x, loc.y, ground + off), False, False)
                lifted += 1
                break
        else:
            if label.startswith("RouteSpline_"):
                splines = actor.get_components_by_class(unreal.SplineComponent)
                for sp in splines:
                    n = sp.get_number_of_spline_points()
                    for i in range(n):
                        p = sp.get_location_at_spline_point(
                            i, unreal.SplineCoordinateSpace.WORLD)
                        ground = _terrain_height_cm(terrain, p.x, p.y)
                        sp.set_location_at_spline_point(
                            i, unreal.Vector(p.x, p.y, ground + 50.0),
                            unreal.SplineCoordinateSpace.WORLD, True)
                        spline_pts += 1
    log(f"spec lift: {lifted} actors, {spline_pts} spline points raised to terrain")


def stage_spec(bundle, spec_path, package_path):
    level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(package_path):
        if not unreal.EditorAssetLibrary.delete_asset(package_path):
            raise RuntimeError(f"failed to delete {package_path}")
    if not level_subsys.new_level(package_path):
        raise RuntimeError(f"new_level failed for {package_path}")

    gate_bp = load_module("checkpoint_gate_bp", "tools/ue5-create-checkpoint-gate-bp.py") \
        .ensure_checkpoint_gate_bp()
    log(f"gate BP: {'ok' if gate_bp else 'MISSING (placeholders will be used)'}")

    importer = load_module("level_spec_importer", "tools/ue5-import-level-spec.py")
    sys.argv = [str(REPO_ROOT / "tools" / "ue5-import-level-spec.py"), "--spec", str(spec_path)]
    rc = importer.main()
    if rc != 0:
        raise RuntimeError(f"spec importer returned {rc}")
    _lift_spec_actors(bundle)
    if not level_subsys.save_current_level():
        raise RuntimeError("save after spec stage failed")
    log(f"spec stage done -> {package_path}")


def stage_terrain(bundle):
    level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level_subsys.load_level(f"/Game/Maps/{sanitize_level_name(bundle['level_name'])}")
    terrain = bundle.get("terrain")
    if not terrain:
        log("no terrain in bundle; skipping")
        return
    actor = find_actor_by_label("Terrain")
    if actor:
        unreal.EditorLevelLibrary.destroy_actor(actor)
    bmg_class = unreal.load_class(None, "/Script/raceGPSAkronBeta.BuildingMeshGenerator")
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(bmg_class, unreal.Vector(0, 0, 0))
    actor.set_actor_label("Terrain")
    pmc = actor.get_component_by_class(unreal.ProceduralMeshComponent)

    rows, cols = terrain["rows"], terrain["cols"]
    heights = terrain["heights"]
    dx = (terrain["x1"] - terrain["x0"]) / (cols - 1) * 100.0  # cm
    dy = (terrain["y1"] - terrain["y0"]) / (rows - 1) * 100.0
    x0, y0 = terrain["x0"] * 100.0, terrain["y0"] * 100.0

    verts = []
    for r in range(rows):
        for c in range(cols):
            verts.append(unreal.Vector(x0 + c * dx, y0 + r * dy, heights[r][c] * 100.0))
    tris = []

    def idx(r, c):
        return r * cols + c

    for r in range(rows - 1):
        for c in range(cols - 1):
            a, b, cc, d = idx(r, c), idx(r, c + 1), idx(r + 1, c), idx(r + 1, c + 1)
            tris += [a, cc, b, b, cc, d]   # up-facing
            tris += [a, b, cc, b, d, cc]   # down-facing (safety: double-sided)
    normals = [unreal.Vector(0, 0, 1)] * len(verts)
    uvs = [unreal.Vector2D(0, 0)] * len(verts)
    pmc.create_mesh_section(0, verts, tris, normals, uvs, [], [], True)
    mat = make_color_material("M_Terrain_Green", (0.23, 0.35, 0.18, 1.0))
    if mat:
        pmc.set_material(0, mat)
    if not level_subsys.save_current_level():
        raise RuntimeError("save after terrain stage failed")
    log(f"terrain stage done: {cols}x{rows} grid, {len(tris) // 3} tris")


def stage_water(bundle):
    level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    map_name = sanitize_level_name(bundle["level_name"])
    level_subsys.load_level(f"/Game/Maps/{map_name}")
    planes = bundle.get("water_planes", [])
    if not planes:
        log("no water in bundle; skipping")
        return
    old = find_actor_by_label("Water")
    if old:
        unreal.EditorLevelLibrary.destroy_actor(old)
    cls = create_hism_bp("BP_CityWater", 1)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0))
    actor.set_actor_label("Water")
    hism = actor.get_component_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
    plane = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane")
    hism.set_static_mesh(plane)
    mat = make_color_material("M_Water_Blue", (0.05, 0.25, 0.55, 0.75), translucent=True)
    if mat:
        hism.set_material(0, mat)
    n = 0
    for x, y, z, sx, sy, yaw, kind in planes:
        t = unreal.Transform(
            location=unreal.Vector(x * 100.0, y * 100.0, z * 100.0),
            rotation=unreal.Rotator(0, yaw, 0),
            scale=unreal.Vector(sx, sy, 1.0))
        hism.add_instance(t)
        n += 1
    if not level_subsys.save_current_level():
        raise RuntimeError("save after water stage failed")
    log(f"water stage done: {n} plane instances (lakes/river polygons/river segments)")


def stage_buildings(bundle, prog):
    level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    map_name = sanitize_level_name(bundle["level_name"])
    level_subsys.load_level(f"/Game/Maps/{map_name}")
    buildings = bundle.get("buildings", [])
    buckets = bundle.get("material_buckets", ["other"])
    # buckets actually in use, in stable order
    used = sorted({b[6] for b in buildings})
    if not buildings:
        log("no buildings in bundle; skipping")
        return

    start = prog.get("building_index", 0)
    actor = find_actor_by_label("Buildings")
    if actor is not None:
        # Stale actor from an older/failed run with a mismatched BP (e.g. the
        # pre-T6 shared BP_CityBuildings) -> drop it and rebuild from scratch.
        existing = actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
        if len(existing) != len(used):
            log(f"existing Buildings actor has {len(existing)} HISMs, expected {len(used)}; recreating")
            unreal.EditorLevelLibrary.destroy_actor(actor)
            actor = None
            start = 0
    if actor is None:
        # Per-city BP: the HISM count/order depends on which material buckets
        # the city's buildings use, so a shared BP breaks cross-city.
        bp_name = f"BP_CityBuildings_{sanitize_level_name(CITY_ID)}"
        cls = create_hism_bp(bp_name, len(used))
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0))
        actor.set_actor_label("Buildings")
        cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
        hisms = actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
        for hism, bucket_idx in zip(hisms, used):
            hism.set_static_mesh(cube)
            mat = make_color_material(f"M_Building_{buckets[bucket_idx]}",
                                      BUCKET_COLORS.get(buckets[bucket_idx], (0.6, 0.6, 0.6, 1.0)))
            if mat:
                hism.set_material(0, mat)
        start = 0
    hisms = actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
    if len(hisms) != len(used):
        raise RuntimeError(f"Buildings actor has {len(hisms)} HISMs, expected {len(used)}")
    bucket_of_hism = {i: used[i] for i in range(len(used))}

    end = min(start + BUILDING_BATCH, len(buildings))
    added = 0
    for i in range(start, end):
        x, y, z, sx, sy, h, bucket = buildings[i]
        hism = hisms[used.index(bucket)]
        t = unreal.Transform(
            location=unreal.Vector(x * 100.0, y * 100.0, (z + h / 2.0) * 100.0),
            rotation=unreal.Rotator(0, 0, 0),
            scale=unreal.Vector(sx, sy, h))
        hism.add_instance(t)
        added += 1
    prog["building_index"] = end
    write_progress(prog)
    if not level_subsys.save_current_level():
        raise RuntimeError("save after buildings stage failed")
    total = sum(h.get_instance_count() for h in hisms)
    log(f"buildings: added {added} (progress {end}/{len(buildings)}, HISM total {total})")
    return end >= len(buildings)


def stage_pois(bundle):
    level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    map_name = sanitize_level_name(bundle["level_name"])
    level_subsys.load_level(f"/Game/Maps/{map_name}")
    pois = bundle.get("pois", [])
    n = 0
    for x, y, z, name in pois:
        a = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.TargetPoint, unreal.Vector(x * 100.0, y * 100.0, (z + 30.0) * 100.0))
        if a:
            a.set_actor_label(f"POI_{name}")
            n += 1
    if not level_subsys.save_current_level():
        raise RuntimeError("save after pois stage failed")
    log(f"pois stage done: {n} landmark TargetPoints")


def main() -> int:
    bundle_path = REPO_ROOT / BUNDLE_REL
    if not bundle_path.exists():
        log(f"ERROR: bundle missing: {bundle_path} (run tools/ue5-city-import-prep.py first)")
        return 1
    bundle = json.loads(bundle_path.read_text(encoding="utf-8"))
    spec_path = Path(os.environ.get("RACEGPS_LEVEL_SPEC", "")) if os.environ.get("RACEGPS_LEVEL_SPEC") else find_spec_for_city(CITY_ID)
    map_name = sanitize_level_name(bundle.get("level_name") or f"{CITY_ID}World")
    package_path = f"/Game/Maps/{map_name}"

    prog = read_progress()
    stage = prog.get("stage", "spec")
    log(f"city={CITY_ID} map={package_path} resume stage={stage} building_index={prog.get('building_index', 0)}")

    if stage == "spec":
        stage_spec(bundle, spec_path, package_path)
        prog["stage"] = "terrain"
        write_progress(prog)
        stage = "terrain"
    if stage == "terrain":
        stage_terrain(bundle)
        prog["stage"] = "water"
        write_progress(prog)
        stage = "water"
    if stage == "water":
        stage_water(bundle)
        prog["stage"] = "buildings"
        write_progress(prog)
        stage = "buildings"
    if stage == "buildings":
        done = stage_buildings(bundle, prog)
        if not done:
            log("building batch incomplete; re-run the commandlet to continue")
            return 2  # resumable
        prog["stage"] = "pois"
        write_progress(prog)
        stage = "pois"
    if stage == "pois":
        stage_pois(bundle)
        prog["stage"] = "lighting"
        write_progress(prog)
        stage = "lighting"
    if stage == "lighting":
        # Never ship a black map: idempotent daytime rig (sun/sky/skylight/fog).
        level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        level_subsys.load_level(package_path)
        load_module("lighting_rig", "tools/ue5-headless-lighting-rig.py") \
            .ensure_lighting_rig(level_subsys)
        if not level_subsys.save_current_level():
            raise RuntimeError("save after lighting stage failed")
        prog["stage"] = "done"
        write_progress(prog)

    log("ALL STAGES COMPLETE")
    return 0


rc = main()
log(f"exit code: {rc}")
if rc == 1:
    raise SystemExit(rc)
