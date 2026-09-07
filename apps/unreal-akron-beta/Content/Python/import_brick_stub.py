import unreal
from pathlib import Path

src = r"C:/projects/racegps/apps/unreal-akron-beta/Content/Carla/Static/GenericMaterials/Brick/Textures/Source/T_Brick_03_d.png"
dest = "/Game/Carla/Static/GenericMaterials/Brick/Textures/T_Brick_03_d"
task = unreal.AssetImportTask()
task.filename = src
task.destination_path = "/Game/Carla/Static/GenericMaterials/Brick/Textures"
task.destination_name = "T_Brick_03_d"
task.automated = True
task.save = True
task.replace_existing = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
unreal.log_warning(f"[v8-brick] imported {dest}")
