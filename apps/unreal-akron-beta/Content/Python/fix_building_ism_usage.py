"""Editor Python: force T10 building materials to support HISM + add emissive defaults."""
import unreal

mats = [
    "/Game/Materials/M_Building_brick",
    "/Game/Materials/M_Building_concrete",
    "/Game/Materials/M_Building_glass",
    "/Game/Materials/M_Building_other",
    "/Game/Materials/M_Building_stone",
    "/Game/Materials/M_Building_wood",
    "/Game/Materials/M_Water_Blue",
]

changed = 0
for path in mats:
    mat = unreal.EditorAssetLibrary.load_asset(path)
    if not mat:
        unreal.log_warning(f"[v8-ism] missing {path}")
        continue
    # UMaterial / MaterialInstanceConstant
    try:
        # Set usage flags via MaterialEditingLibrary when available
        if hasattr(mat, "set_editor_property"):
            try:
                mat.set_editor_property("used_with_instanced_static_meshes", True)
            except Exception:
                pass
            try:
                mat.set_editor_property("b_used_with_instanced_static_meshes", True)
            except Exception:
                pass
        unreal.MaterialEditingLibrary.recompile_material(mat) if hasattr(unreal, "MaterialEditingLibrary") else None
        unreal.EditorAssetLibrary.save_asset(path)
        changed += 1
        unreal.log_warning(f"[v8-ism] flagged+saved {path}")
    except Exception as e:
        unreal.log_warning(f"[v8-ism] fail {path}: {e}")

unreal.log_warning(f"[v8-ism] done changed={changed}")
