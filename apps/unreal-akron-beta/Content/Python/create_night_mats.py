"""Create simple night car-paint + window emissive materials with ISM usage."""
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary

def ensure_material(path, name, make_fn):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mat = unreal.EditorAssetLibrary.load_asset(path)
        unreal.log_warning(f"[v10-mat] exists {path}")
        return mat
    mat = asset_tools.create_asset(name, "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    make_fn(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log_warning(f"[v10-mat] created {path}")
    return mat

def make_car_paint(mat):
    # Base color param
    base = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, -100)
    base.set_editor_property("parameter_name", "Base_color")
    base.set_editor_property("default_value", unreal.LinearColor(1.0, 0.42, 0.08, 1.0))
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    flakes = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 40)
    flakes.set_editor_property("parameter_name", "Base_color_flakes")
    flakes.set_editor_property("default_value", unreal.LinearColor(1.0, 0.42, 0.08, 1.0))

    emis = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 200)
    emis.set_editor_property("parameter_name", "EmissiveColor")
    emis.set_editor_property("default_value", unreal.LinearColor(0.8, 0.2, 0.02, 1.0))
    mel.connect_material_property(emis, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 360)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.2)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 480)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.75)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

    mat.set_editor_property("two_sided", False)
    try:
        mat.set_editor_property("used_with_skeletal_mesh", True)
    except Exception:
        pass
    mel.recompile_material(mat)

def make_window(mat):
    base = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, -80)
    base.set_editor_property("parameter_name", "BaseColor")
    base.set_editor_property("default_value", unreal.LinearColor(0.03, 0.04, 0.06, 1.0))
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    emis = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 120)
    emis.set_editor_property("parameter_name", "EmissiveColor")
    emis.set_editor_property("default_value", unreal.LinearColor(8.0, 6.0, 3.0, 1.0))
    mel.connect_material_property(emis, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 300)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.35)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    try:
        mat.set_editor_property("used_with_instanced_static_meshes", True)
    except Exception:
        pass
    try:
        mat.set_editor_property("used_with_static_lighting", True)
    except Exception:
        pass
    mel.recompile_material(mat)

ensure_material("/Game/Materials/M_NightCarPaint", "M_NightCarPaint", make_car_paint)
ensure_material("/Game/Materials/M_NightWindow", "M_NightWindow", make_window)
unreal.log_warning("[v10-mat] done")
