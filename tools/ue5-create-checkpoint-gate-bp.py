#!/usr/bin/env python3
"""
Headless creation of /Game/Blueprints/BP_CheckpointGate.

The C++ ACheckpointGate builds its full gate visual in its constructor
(engine Cylinder posts + Cube arch beam on an OverlapBox root), so the
Blueprint only needs to exist parented to the native class — no component
edits required. Exposes ensure_checkpoint_gate_bp() so the headless map
importer can call it before spawning checkpoint actors; also runnable
standalone via the pythonscript commandlet.
"""

import unreal

BP_PACKAGE_PATH = "/Game/Blueprints/BP_CheckpointGate"
BP_NAME = "BP_CheckpointGate"
BP_FOLDER = "/Game/Blueprints"
NATIVE_CLASS_PATH = "/Script/raceGPSAkronBeta.CheckpointGate"


def ensure_checkpoint_gate_bp():
    """Create BP_CheckpointGate parented to ACheckpointGate if missing.

    Returns the Blueprint class object, or None on failure.
    """
    existing = unreal.EditorAssetLibrary.load_blueprint_class(BP_PACKAGE_PATH)
    if existing:
        unreal.log(f"[raceGPS] {BP_PACKAGE_PATH} already exists, reusing")
        return existing

    native_class = unreal.load_class(None, NATIVE_CLASS_PATH)
    if not native_class:
        unreal.log_error(f"[raceGPS] Native class not found: {NATIVE_CLASS_PATH}")
        return None

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", native_class)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = asset_tools.create_asset(BP_NAME, BP_FOLDER, unreal.Blueprint, factory)
    if not asset:
        unreal.log_error(f"[raceGPS] Failed to create {BP_PACKAGE_PATH}")
        return None

    if not unreal.EditorAssetLibrary.save_asset(BP_PACKAGE_PATH, only_if_is_dirty=False):
        unreal.log_error(f"[raceGPS] Failed to save {BP_PACKAGE_PATH}")
        return None

    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_PACKAGE_PATH)
    unreal.log(f"[raceGPS] Created {BP_PACKAGE_PATH} (parent={NATIVE_CLASS_PATH}, class_loaded={bp_class is not None})")
    return bp_class


if __name__ == "__main__":
    result = ensure_checkpoint_gate_bp()
    if result is None:
        raise SystemExit(1)
