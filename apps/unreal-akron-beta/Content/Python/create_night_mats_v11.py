"""V11 night materials: window-grid façade, clearcoat car paint, wet asphalt.

Rebuilds /Game/Materials/M_NightWindow, M_NightCarPaint, M_NightAsphalt.
Safe to re-run (deletes prior asset then recreates).
"""
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def wipe(path: str):
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
        unreal.log_warning(f"[v11-mat] deleted {path}")


def new_mat(path: str, name: str):
    wipe(path)
    mat = asset_tools.create_asset(name, "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    return mat


def flag_ism(mat):
    for prop in ("used_with_instanced_static_meshes", "b_used_with_instanced_static_meshes",
                 "used_with_static_lighting", "used_with_nanite"):
        try:
            mat.set_editor_property(prop, True)
        except Exception:
            pass
    try:
        mat.set_editor_property("used_with_skeletal_mesh", True)
    except Exception:
        pass


def make_window(mat):
    """Dark façade + tiled emissive window mask (Custom HLSL)."""
    base = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, -160)
    base.set_editor_property("parameter_name", "BaseColor")
    base.set_editor_property("default_value", unreal.LinearColor(0.03, 0.035, 0.045, 1.0))
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    emis_col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 40)
    emis_col.set_editor_property("parameter_name", "EmissiveColor")
    # Cap default — MID multiplies further; keep bloom from blowing the sky.
    emis_col.set_editor_property("default_value", unreal.LinearColor(1.35, 1.05, 0.55, 1.0))

    intensity = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 200)
    intensity.set_editor_property("parameter_name", "EmissiveStrength")
    intensity.set_editor_property("default_value", 1.0)

    tile = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 320)
    tile.set_editor_property("parameter_name", "WindowTile")
    tile.set_editor_property("default_value", 8.0)

    texcoord = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -600, 440)

    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -280, 200)
    custom.set_editor_property("code",
        "float t = max(Tile, 1.0);\n"
        "float2 uv = float2(UV.x, UV.y) * t;\n"
        "float2 cell = floor(uv);\n"
        "float2 f = frac(uv);\n"
        "float pane = step(0.12, f.x) * step(f.x, 0.88) * step(0.18, f.y) * step(f.y, 0.82);\n"
        "float n = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);\n"
        "float lit = n > 0.38 ? 1.0 : 0.0;\n"
        "return pane * lit;")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    # inputs
    try:
        inputs = custom.get_editor_property("inputs")
    except Exception:
        inputs = []
    # Rebuild inputs list
    inp_uv = unreal.CustomInput()
    inp_uv.set_editor_property("input_name", "UV")
    inp_tile = unreal.CustomInput()
    inp_tile.set_editor_property("input_name", "Tile")
    custom.set_editor_property("inputs", [inp_uv, inp_tile])
    mel.connect_material_expressions(texcoord, "", custom, "UV")
    mel.connect_material_expressions(tile, "", custom, "Tile")

    mul1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -40, 80)
    mel.connect_material_expressions(emis_col, "RGB", mul1, "A")
    mel.connect_material_expressions(custom, "", mul1, "B")

    mul2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 160, 80)
    mel.connect_material_expressions(mul1, "", mul2, "A")
    mel.connect_material_expressions(intensity, "", mul2, "B")
    mel.connect_material_property(mul2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 560)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.35)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 680)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.25)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

    flag_ism(mat)
    mel.recompile_material(mat)


def make_car_paint(mat):
    """Lit clearcoat-ish paint: BaseColor + Metallic/Roughness, near-zero emissive."""
    base = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -480, -120)
    base.set_editor_property("parameter_name", "Base_color")
    base.set_editor_property("default_value", unreal.LinearColor(1.0, 0.42, 0.08, 1.0))
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    flakes = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -480, 40)
    flakes.set_editor_property("parameter_name", "Base_color_flakes")
    flakes.set_editor_property("default_value", unreal.LinearColor(1.0, 0.50, 0.12, 1.0))

    # Keep pin so runtime MID can zero it; default near-black (not neon).
    emis = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -480, 200)
    emis.set_editor_property("parameter_name", "EmissiveColor")
    emis.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
    mel.connect_material_property(emis, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 360)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.18)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 480)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.78)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

    # Specular as stand-in for clearcoat punch when ClearCoat shading model unavailable.
    spec = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 600)
    spec.set_editor_property("parameter_name", "Specular")
    spec.set_editor_property("default_value", 0.85)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    clear = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 720)
    clear.set_editor_property("parameter_name", "ClearCoat")
    clear.set_editor_property("default_value", 0.95)

    flag_ism(mat)
    try:
        mat.set_editor_property("used_with_skeletal_mesh", True)
    except Exception:
        pass
    mel.recompile_material(mat)


def make_asphalt(mat):
    """Dark wet night asphalt with specular sheen."""
    base = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -480, -80)
    base.set_editor_property("parameter_name", "BaseColor")
    base.set_editor_property("default_value", unreal.LinearColor(0.035, 0.036, 0.040, 1.0))
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 120)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.28)  # wetter = lower
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 240)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.05)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

    spec = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 360)
    spec.set_editor_property("parameter_name", "Specular")
    spec.set_editor_property("default_value", 0.72)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    # Tiny cool specular glint as emissive-free wet cue (no emissive).
    flag_ism(mat)
    mel.recompile_material(mat)


w = new_mat("/Game/Materials/M_NightWindow", "M_NightWindow")
make_window(w)
eal.save_asset("/Game/Materials/M_NightWindow")
unreal.log_warning("[v11-mat] saved M_NightWindow (tiled window mask)")

c = new_mat("/Game/Materials/M_NightCarPaint", "M_NightCarPaint")
make_car_paint(c)
eal.save_asset("/Game/Materials/M_NightCarPaint")
unreal.log_warning("[v11-mat] saved M_NightCarPaint (clearcoat, zero emissive default)")

a = new_mat("/Game/Materials/M_NightAsphalt", "M_NightAsphalt")
make_asphalt(a)
eal.save_asset("/Game/Materials/M_NightAsphalt")
unreal.log_warning("[v11-mat] saved M_NightAsphalt (wet night ground)")
unreal.log_warning("[v11-mat] done")
