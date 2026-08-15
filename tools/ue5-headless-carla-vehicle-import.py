#!/usr/bin/env python3
"""
Headless importer for the CARLA Dodge Charger 2024 hero vehicle (story T5).

Run via the PythonScript commandlet (same pattern as ue5-headless-map-import.py):

    UnrealEditor-Cmd.exe "<uproject>" -run=pythonscript ^
        -script="tools/ue5-headless-carla-vehicle-import.py" ^
        -unattended -nop4 -nullrhi

Prerequisites: the raw CARLA .uasset files must already be staged under
Content/Carla/... (see tools/carla-content-fetch.py). The CARLA packages
reference each other via /Game/Carla/... paths, so the Content/Carla tree
must keep its original layout.

What it does:
  1. Loads SK_DodgeCharger2024 + its physics asset and prints skeleton bone
     count and physics body count (verification).
  2. Creates /Game/Vehicles/DodgeCharger2024/BP_DodgeCharger2024, a Blueprint
     child of AChaosVehiclePawn, assigns the CARLA skeletal mesh to the CDO's
     Mesh component and configures 4 Chaos wheel setups whose bone names are
     discovered from the physics asset. Arcade handling (bEnableArcadeHandling,
     LateralGripMultiplier, ...) comes from AChaosVehiclePawn class defaults —
     no C++ changes.

Asset license: CARLA content is CC-BY 4.0 — see Content/Vehicles/CARLA-ATTRIBUTION.txt.
"""

import unreal

CAR_PATH = "/Game/Carla/Static/Car/4Wheeled/DodgeCharger2024"
SK_PATH = f"{CAR_PATH}/SK_DodgeCharger2024"
PHYS_PATH = f"{CAR_PATH}/Phys_DodgeCharger2024"
BP_PATH = "/Game/Vehicles/DodgeCharger2024"
BP_NAME = "BP_DodgeCharger2024"

WHEEL_SLOTS = ("front_left", "front_right", "rear_left", "rear_right")


def log(msg):
    # unreal.log is filtered out in the pythonscript commandlet; warnings pass.
    unreal.log_warning(f"[raceGPS:T5] {msg}")


def err(msg):
    unreal.log_error(f"[raceGPS:T5] {msg}")


def _norm(name: str) -> str:
    return name.lower().replace(" ", "")


def export_t3d(asset, dest_path: str) -> str:
    """Export an asset to T3D text and return the text (ground-truth parse)."""
    task = unreal.AssetExportTask()
    task.set_editor_property("object", asset)
    task.set_editor_property("filename", dest_path)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_identical", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("exporter", unreal.ObjectExporterT3D())
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError(f"T3D export failed for {asset.get_path_name()}")
    with open(dest_path, "r", encoding="utf-8", errors="replace") as fh:
        return fh.read()


def physics_bodies_from_t3d(t3d: str):
    """Return [(body_object_name, bone_name)] from a physics-asset T3D dump.

    UE T3D has a declaration pass (Class=..., empty body) and a definition
    pass (no Class=..., holds BoneName); parse the definition blocks.
    """
    import re

    body_names = set(re.findall(
        r"Begin Object Class=[^ \"]*BodySetup[^ \"]* Name=\"([^\"]+)\"", t3d))
    bodies = []
    for m in re.finditer(r"Begin Object Name=\"([^\"]+)\"(.*?)End Object", t3d, re.S):
        if m.group(1) not in body_names:
            continue
        bone = re.search(r"BoneName=\"?([A-Za-z0-9_]+)", m.group(2))
        bodies.append((m.group(1), bone.group(1) if bone else None))
    return bodies


def discover_wheel_bones(body_bones):
    """Map [(body_name, bone_name)] -> {slot: bone_name} for the 4 wheels."""
    slots = {}
    for _body, bone in body_bones:
        if not bone:
            continue
        nb = _norm(bone)
        if "wheel" not in nb:
            continue
        side = "left" if ("_l" in nb or "left" in nb) else "right"
        axle = "front" if ("_f" in nb or "front" in nb) else "rear"
        slots[f"{axle}_{side}"] = bone
    return slots


def set_skeletal_mesh(mesh_comp, sk_asset):
    for prop in ("skeletal_mesh_asset", "skeletal_mesh"):
        try:
            mesh_comp.set_editor_property(prop, sk_asset)
            return prop
        except Exception:
            continue
    raise RuntimeError("no writable skeletal-mesh property on Mesh component")


def main() -> int:
    eal = unreal.EditorAssetLibrary

    # --- 1. Load + verify CARLA assets --------------------------------------
    sk = eal.load_asset(SK_PATH)
    if not sk:
        err(f"failed to load skeletal mesh {SK_PATH}")
        return 1
    phys = eal.load_asset(PHYS_PATH)
    if not phys:
        err(f"failed to load physics asset {PHYS_PATH}")
        return 1

    bone_count = -1
    skeleton = sk.get_editor_property("skeleton")
    try:
        bone_count = len(skeleton.get_editor_property("bone_tree"))
    except Exception:
        log("bone_tree not readable via python; continuing")

    # Physics bodies: 5.7 does not expose SkeletalBodySetups to python, so
    # parse the T3D export of the loaded asset (ground truth from the editor).
    import tempfile, os
    t3d_path = os.path.join(tempfile.gettempdir(), "T5_Phys_DodgeCharger2024.t3d")
    bodies = physics_bodies_from_t3d(export_t3d(phys, t3d_path))
    body_count = len(bodies)
    log(f"VERIFY skeletal_mesh={SK_PATH} bones={bone_count} "
        f"physics_bodies={body_count}")
    log(f"physics bodies: {bodies}")
    log(f"mesh physics asset: {sk.get_editor_property('physics_asset')}")

    wheel_bones = discover_wheel_bones(bodies)
    log(f"discovered wheel bones: {wheel_bones}")
    missing = [s for s in WHEEL_SLOTS if s not in wheel_bones]
    if missing:
        err(f"could not map wheel bodies for slots: {missing}")
        return 1

    # --- 2. Create the hero Blueprint ---------------------------------------
    pawn_class = unreal.load_class(None, "/Script/raceGPSAkronBeta.ChaosVehiclePawn")
    if not pawn_class:
        err("AChaosVehiclePawn class not found — build the module first")
        return 1

    full_bp_path = f"{BP_PATH}/{BP_NAME}"
    if eal.does_asset_exist(full_bp_path):
        log(f"deleting existing {full_bp_path} for a clean re-import")
        if not eal.delete_asset(full_bp_path):
            err(f"failed to delete {full_bp_path}")
            return 1

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", pawn_class)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    bp = asset_tools.create_asset(BP_NAME, BP_PATH, unreal.Blueprint, factory)
    if not bp:
        err("Blueprint creation failed")
        return 1

    gen_class = bp.generated_class()
    cdo = unreal.get_default_object(gen_class)

    mesh_comp = cdo.get_editor_property("mesh")
    used_prop = set_skeletal_mesh(mesh_comp, sk)
    log(f"assigned skeletal mesh via '{used_prop}'")

    # 4 Chaos wheel setups using the CARLA skeleton's wheel bones.
    wheel_class = unreal.load_class(None, "/Script/ChaosVehicles.ChaosVehicleWheel")
    if not wheel_class:
        err("UChaosVehicleWheel class not found (ChaosVehicles plugin)")
        return 1
    move_comp = cdo.get_editor_property("vehicle_movement_component")
    setups = []
    for slot in WHEEL_SLOTS:  # FL, FR, RL, RR
        setup = unreal.ChaosWheelSetup()
        setup.set_editor_property("wheel_class", wheel_class)
        setup.set_editor_property("bone_name", wheel_bones[slot])
        setups.append(setup)
    move_comp.set_editor_property("wheel_setups", setups)

    # Read back arcade handling defaults inherited from AChaosVehiclePawn.
    # (UE python strips the leading 'b' from bool UPROPERTY names.)
    def read(*names):
        for n in names:
            try:
                return cdo.get_editor_property(n)
            except Exception:
                continue
        return "<?>"
    log(f"arcade defaults: enabled={read('enable_arcade_handling', 'b_enable_arcade_handling')} "
        f"lat_grip={read('lateral_grip_multiplier')} "
        f"drift_retention={read('drift_grip_retention')}")

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not eal.save_asset(full_bp_path, only_if_is_dirty=False):
        err(f"failed to save {full_bp_path}")
        return 1

    log(f"VERIFY blueprint={full_bp_path} parent={pawn_class.get_name()} "
        f"wheel_setups={len(setups)} saved=ok")
    return 0


rc = main()
unreal.log(f"[raceGPS:T5] carla vehicle import exit code: {rc}")
if rc != 0:
    raise SystemExit(rc)
