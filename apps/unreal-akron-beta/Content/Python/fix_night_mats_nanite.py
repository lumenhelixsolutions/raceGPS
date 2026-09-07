import unreal
mel = unreal.MaterialEditingLibrary

def flag(path, **props):
    mat = unreal.EditorAssetLibrary.load_asset(path)
    if not mat:
        unreal.log_warning(f"[v10-nanite] missing {path}")
        return
    for k,v in props.items():
        try:
            mat.set_editor_property(k, v)
        except Exception as e:
            unreal.log_warning(f"[v10-nanite] {path} set {k} fail: {e}")
    try:
        mel.recompile_material(mat)
    except Exception as e:
        unreal.log_warning(f"[v10-nanite] recompile fail {path}: {e}")
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log_warning(f"[v10-nanite] saved {path}")

flag("/Game/Materials/M_NightCarPaint",
     used_with_skeletal_mesh=True,
     used_with_nanite=True,
     used_with_instanced_static_meshes=True)
flag("/Game/Materials/M_NightWindow",
     used_with_nanite=True,
     used_with_instanced_static_meshes=True,
     used_with_static_lighting=True)
# Also re-flag building mats for nanite (T10 may be nanite HISM)
for p in [
    "/Game/Materials/M_Building_brick",
    "/Game/Materials/M_Building_concrete",
    "/Game/Materials/M_Building_glass",
    "/Game/Materials/M_Building_other",
    "/Game/Materials/M_Building_stone",
    "/Game/Materials/M_Building_wood",
]:
    flag(p, used_with_nanite=True, used_with_instanced_static_meshes=True)
unreal.log_warning("[v10-nanite] done")
