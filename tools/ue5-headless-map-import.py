#!/usr/bin/env python3
"""
Headless UE5 map generator for a level-spec JSON.

Run via the PythonScript commandlet (requires PythonScriptPlugin +
EditorScriptingUtilities enabled in the uproject):

    UnrealEditor-Cmd.exe "<uproject>" -run=pythonscript ^
        -script="tools/ue5-headless-map-import.py" ^
        -unattended -nop4 -nullrhi

It creates (or recreates) /Game/Maps/<level_name> from the level spec,
populates it with the same actor pass as tools/ue5-import-level-spec.py
(spawn points, route splines, checkpoint gates, sun rotation, reflection
captures, traffic volumes), then saves the .umap asset.

Spec selection: edit SPEC_REL below, or set the RACEGPS_LEVEL_SPEC env var
to an absolute spec path before launching the commandlet.
"""

import importlib.util
import os
import sys
from pathlib import Path

import unreal

SCRIPT_PATH = Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent

SPEC_REL = "generated/Cleveland5.0KmWorld_LevelSpec.json"
IMPORTER_REL = "tools/ue5-import-level-spec.py"


def _load_importer():
    importer_path = REPO_ROOT / IMPORTER_REL
    spec = importlib.util.spec_from_file_location("level_spec_importer", importer_path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    if not mod.HAS_UNREAL:
        raise RuntimeError("Importer did not detect the unreal module")
    return mod


def main() -> int:
    spec_path = Path(os.environ.get("RACEGPS_LEVEL_SPEC", str(REPO_ROOT / SPEC_REL)))
    if not spec_path.exists():
        unreal.log_error(f"[raceGPS] Level spec not found: {spec_path}")
        return 1

    import json

    spec_doc = json.loads(spec_path.read_text(encoding="utf-8"))
    level_name = spec_doc.get("level_name") or spec_path.stem.replace("_LevelSpec", "")
    # UE package names cannot contain '.' (or spaces); sanitize for the asset
    # path while keeping the spec's level_name untouched (runtime contract).
    import re

    asset_name = re.sub(r"[^A-Za-z0-9_]", "_", level_name)
    if asset_name != level_name:
        unreal.log_warning(
            f"[raceGPS] level_name '{level_name}' is not a legal UE package name; "
            f"saving map asset as '{asset_name}'"
        )
    package_path = f"/Game/Maps/{asset_name}"

    unreal.log(f"[raceGPS] Creating level {package_path} from {spec_path.name}")

    # Start from a blank level so reruns are idempotent. Use the Level Editor
    # Subsystem (EditorLevelLibrary is deprecated and its save path fails
    # headlessly on untitled worlds).
    if unreal.EditorAssetLibrary.does_asset_exist(package_path):
        unreal.log(f"[raceGPS] Deleting existing {package_path} for a clean re-import")
        if not unreal.EditorAssetLibrary.delete_asset(package_path):
            unreal.log_error(f"[raceGPS] Failed to delete existing {package_path}")
            return 1
    level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_subsys.new_level(package_path):
        unreal.log_error(f"[raceGPS] Failed to create level {package_path}")
        return 1

    # Reuse the interactive importer's actor pass with the editor (unreal) live.
    importer = _load_importer()
    sys.argv = [str(importer.__file__), "--spec", str(spec_path)]
    rc = importer.main()
    if rc != 0:
        unreal.log_error(f"[raceGPS] Importer returned {rc}")
        return rc

    saved = level_subsys.save_current_level()
    if not saved:
        # Fallback: save the world package directly through the asset library.
        saved = unreal.EditorAssetLibrary.save_asset(package_path, only_if_is_dirty=False)
    if not saved:
        unreal.log_error(f"[raceGPS] save failed for {package_path}")
        return 1

    actor_count = len(unreal.EditorLevelLibrary.get_all_level_actors())
    unreal.log(f"[raceGPS] Saved {package_path} with {actor_count} actor(s)")
    return 0


rc = main()
unreal.log(f"[raceGPS] headless map import exit code: {rc}")
if rc != 0:
    # Non-zero process exit so CI/build wrappers notice the failure.
    raise SystemExit(rc)
