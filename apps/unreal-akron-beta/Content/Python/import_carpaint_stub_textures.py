"""Import CARLA car-paint stub textures (T_flakes_d, T_flakes_n, T_dirt_01).

Run inside Unreal Editor 5.7 (Python Editor Script Plugin), or:

  UnrealEditor-Cmd.exe raceGPSAkronBeta.uproject -unattended -nopause -nullrhi ^
    -ExecutePythonScript="Content/Python/import_carpaint_stub_textures.py"

Source PNGs live next to this script's expected textures folder:
  Content/Carla/Static/GenericMaterials/00_MastersOpt/Textures/Source/*.png

Destination (matches M_CarPaint_Master_New hard refs from logs):
  /Game/Carla/Static/GenericMaterials/00_MastersOpt/Textures/T_flakes_d
  /Game/Carla/Static/GenericMaterials/00_MastersOpt/Textures/T_flakes_n
  /Game/Carla/Static/GenericMaterials/00_MastersOpt/Textures/T_dirt_01

These are PLACEHOLDERS so paint MIDs load. Not photogrammetry flakes.
"""
from __future__ import annotations

import os

import unreal

DEST = "/Game/Carla/Static/GenericMaterials/00_MastersOpt/Textures"

# (asset_name, png_filename, is_normal, srgb)
SPECS = [
    ("T_flakes_d", "T_flakes_d.png", False, True),
    ("T_flakes_n", "T_flakes_n.png", True, False),
    ("T_dirt_01", "T_dirt_01.png", False, False),  # dirt mask, linear
]


def _source_dir() -> str:
    project = unreal.Paths.project_content_dir()
    return os.path.join(
        project,
        "Carla",
        "Static",
        "GenericMaterials",
        "00_MastersOpt",
        "Textures",
        "Source",
    )


def import_stubs() -> int:
    src_dir = _source_dir()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    imported = 0
    for name, png, is_normal, srgb in SPECS:
        src = os.path.join(src_dir, png)
        if not os.path.isfile(src):
            unreal.log_error(f"[raceGPS] paint stub missing: {src}")
            continue
        task = unreal.AssetImportTask()
        task.filename = src
        task.destination_path = DEST
        task.destination_name = name
        task.replace_existing = True
        task.automated = True
        task.save = True
        task.factory = unreal.TextureFactory()
        asset_tools.import_asset_tasks([task])
        asset_path = f"{DEST}/{name}"
        tex = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not tex:
            unreal.log_error(f"[raceGPS] paint stub failed to load after import: {asset_path}")
            continue
        if is_normal:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
            lod = None
            for attr in ("TEXTUREGROUP_WORLDNORMALMAP", "WORLD_NORMAL_MAP", "WORLDNORMALMAP"):
                lod = getattr(unreal.TextureGroup, attr, None)
                if lod is not None:
                    break
            if lod is not None:
                tex.set_editor_property("lod_group", lod)
        else:
            tex.set_editor_property("srgb", bool(srgb))
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        unreal.EditorAssetLibrary.save_asset(asset_path)
        unreal.log(f"[raceGPS] paint stub imported {asset_path}")
        imported += 1
    unreal.log(f"[raceGPS] paint stubs imported {imported}/{len(SPECS)}")
    return 0 if imported == len(SPECS) else 1


if __name__ == "__main__":
    raise SystemExit(import_stubs())
