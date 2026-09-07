"""V12 night materials: facade-only emissive windows, ClearCoat paint, wet asphalt.

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
        unreal.log_warning(f"[v12-mat] deleted {path}")


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


def _enum_member(enum_cls, *names):
    for n in names:
        if hasattr(enum_cls, n):
            return getattr(enum_cls, n)
    return None


def make_window(mat):
    """Dark facade + world-aligned window mask, GLOW ONLY on vertical walls.

    World-normal gate: abs(Normal.Z) high (roofs/floors) => emissive 0.
    World-XY/Z projection so HISM/Karla boxes get window grids even with bad UVs.
    """
    base = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -720, -200)
    base.set_editor_property("parameter_name", "BaseColor")
    base.set_editor_property("default_value", unreal.LinearColor(0.028, 0.030, 0.034, 1.0))
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    emis_col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -720, 20)
    emis_col.set_editor_property("parameter_name", "EmissiveColor")
    # Horizon skyline punch without blowing a lit ceiling (roofs gated to 0).
    emis_col.set_editor_property("default_value", unreal.LinearColor(1.15, 0.92, 0.48, 1.0))

    intensity = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -720, 180)
    intensity.set_editor_property("parameter_name", "EmissiveStrength")
    intensity.set_editor_property("default_value", 0.85)

    tile = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -720, 300)
    tile.set_editor_property("parameter_name", "WindowTile")
    tile.set_editor_property("default_value", 8.0)

    # Vertex normal is stable on HISMs; pixel normal used as graph-side backup multiply.
    nws_cls = getattr(unreal, "MaterialExpressionVertexNormalWS", None) or getattr(unreal, "MaterialExpressionPixelNormalWS", None)
    nws = mel.create_material_expression(mat, nws_cls, -720, 420)

    wp_cls = getattr(unreal, "MaterialExpressionWorldPosition", unreal.MaterialExpressionWorldPosition)
    wp = mel.create_material_expression(mat, wp_cls, -720, 540)

    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -280, 220)
    custom.set_editor_property("code",
        "float t = max(Tile, 1.0);\n"
        "float3 N = normalize(Normal);\n"
        "float nz = abs(N.z);\n"
        "// Vertical walls only. Roofs/floors (nz~1) stay dark.\n"
        "float facade = 1.0 - smoothstep(0.30, 0.58, nz);\n"
        "// World-meter window grid (Tile=8 => ~2.4m cells).\n"
        "float s = 0.0042 * (t / 8.0);\n"
        "float3 P = WorldPos * s;\n"
        "float ax = abs(N.x);\n"
        "float ay = abs(N.y);\n"
        "float2 uv = (ax > ay) ? float2(P.y, P.z) : float2(P.x, P.z);\n"
        "float2 cell = floor(uv);\n"
        "float2 f = frac(uv);\n"
        "float pane = step(0.16, f.x) * step(f.x, 0.84) * step(0.24, f.y) * step(f.y, 0.80);\n"
        "float n = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);\n"
        "float lit = n > 0.40 ? 1.0 : 0.0;\n"
        "return pane * lit * facade;")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inp_n = unreal.CustomInput()
    inp_n.set_editor_property("input_name", "Normal")
    inp_w = unreal.CustomInput()
    inp_w.set_editor_property("input_name", "WorldPos")
    inp_t = unreal.CustomInput()
    inp_t.set_editor_property("input_name", "Tile")
    custom.set_editor_property("inputs", [inp_n, inp_w, inp_t])
    mel.connect_material_expressions(nws, "", custom, "Normal")
    mel.connect_material_expressions(wp, "", custom, "WorldPos")
    mel.connect_material_expressions(tile, "", custom, "Tile")

    # Graph-side facade gate (backup if Custom Normal pin is ignored).
    mask_z = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -280, 40)
    try:
        mask_z.set_editor_property("r", False)
        mask_z.set_editor_property("g", False)
        mask_z.set_editor_property("b", True)
        mask_z.set_editor_property("a", False)
    except Exception:
        pass
    mel.connect_material_expressions(nws, "", mask_z, "")
    absz = mel.create_material_expression(mat, unreal.MaterialExpressionAbs, -80, 40)
    mel.connect_material_expressions(mask_z, "", absz, "")
    one = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -80, 140)
    one.set_editor_property("r", 1.0)
    sub = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, 80, 40)
    mel.connect_material_expressions(one, "", sub, "A")
    mel.connect_material_expressions(absz, "", sub, "B")
    sat = mel.create_material_expression(mat, unreal.MaterialExpressionSaturate, 240, 40)
    mel.connect_material_expressions(sub, "", sat, "")
    # Power keeps only near-vertical (low |Nz|) glowing; roofs ~0.
    pwr = mel.create_material_expression(mat, unreal.MaterialExpressionPower, 400, 40)
    pwr_exp = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, 240, 140)
    pwr_exp.set_editor_property("r", 2.2)
    mel.connect_material_expressions(sat, "", pwr, "Base")
    try:
        mel.connect_material_expressions(pwr_exp, "", pwr, "Exp")
    except Exception:
        mel.connect_material_expressions(pwr_exp, "", pwr, "Exponent")

    mul_mask = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 80, 220)
    mel.connect_material_expressions(custom, "", mul_mask, "A")
    mel.connect_material_expressions(pwr, "", mul_mask, "B")

    mul1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 280, 220)
    mel.connect_material_expressions(emis_col, "RGB", mul1, "A")
    mel.connect_material_expressions(mul_mask, "", mul1, "B")

    mul2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 480, 220)
    mel.connect_material_expressions(mul1, "", mul2, "A")
    mel.connect_material_expressions(intensity, "", mul2, "B")
    mel.connect_material_property(mul2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -720, 680)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.38)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -720, 800)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.22)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

    flag_ism(mat)
    mel.recompile_material(mat)


def make_car_paint(mat):
    """ClearCoat shading model when the domain allows; else metallic+specular."""
    shading = _enum_member(unreal.MaterialShadingModel, "MSM_CLEAR_COAT", "CLEAR_COAT")
    applied_clear = False
    if shading is not None:
        for prop in ("shading_model", "material_shading_model", "shading_models"):
            try:
                mat.set_editor_property(prop, shading)
                applied_clear = True
                unreal.log_warning(f"[v12-mat] M_NightCarPaint {prop}={shading}")
                break
            except Exception as exc:
                unreal.log_warning(f"[v12-mat] set {prop} failed: {exc}")
    if not applied_clear:
        unreal.log_warning("[v12-mat] ClearCoat shading model unavailable; metallic+specular stand-in")

    base = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -480, -120)
    base.set_editor_property("parameter_name", "Base_color")
    base.set_editor_property("default_value", unreal.LinearColor(1.0, 0.42, 0.08, 1.0))
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    flakes = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -480, 40)
    flakes.set_editor_property("parameter_name", "Base_color_flakes")
    flakes.set_editor_property("default_value", unreal.LinearColor(1.0, 0.50, 0.12, 1.0))

    emis = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -480, 200)
    emis.set_editor_property("parameter_name", "EmissiveColor")
    emis.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
    mel.connect_material_property(emis, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 360)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.16)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 480)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.82)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

    spec = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 600)
    spec.set_editor_property("parameter_name", "Specular")
    spec.set_editor_property("default_value", 0.88)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    clear = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 720)
    clear.set_editor_property("parameter_name", "ClearCoat")
    clear.set_editor_property("default_value", 0.95)

    clear_r = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 840)
    clear_r.set_editor_property("parameter_name", "ClearCoatRoughness")
    clear_r.set_editor_property("default_value", 0.08)

    # ClearCoat amount/roughness live on CustomData0/1 (or dedicated MP if present).
    connected = False
    for amount_prop, rough_prop in (
        ("MP_CUSTOM_DATA_0", "MP_CUSTOM_DATA_1"),
        ("MP_CUSTOM_DATA0", "MP_CUSTOM_DATA1"),
        ("MP_CLEAR_COAT", "MP_CLEAR_COAT_ROUGHNESS"),
    ):
        a = _enum_member(unreal.MaterialProperty, amount_prop)
        r = _enum_member(unreal.MaterialProperty, rough_prop)
        if a is None or r is None:
            continue
        try:
            mel.connect_material_property(clear, "", a)
            mel.connect_material_property(clear_r, "", r)
            unreal.log_warning(f"[v12-mat] ClearCoat pins {amount_prop}/{rough_prop}")
            connected = True
            break
        except Exception as exc:
            unreal.log_warning(f"[v12-mat] ClearCoat pin {amount_prop} failed: {exc}")
    if not connected:
        unreal.log_warning("[v12-mat] ClearCoat pins not connected; specular stand-in remains")

    flag_ism(mat)
    try:
        mat.set_editor_property("used_with_skeletal_mesh", True)
    except Exception:
        pass
    mel.recompile_material(mat)


def make_asphalt(mat):
    """Readable wet night asphalt: lifted albedo + low roughness + world streaks."""
    base = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -520, -80)
    base.set_editor_property("parameter_name", "BaseColor")
    base.set_editor_property("default_value", unreal.LinearColor(0.078, 0.074, 0.068, 1.0))
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    rough_p = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -520, 80)
    rough_p.set_editor_property("parameter_name", "Roughness")
    rough_p.set_editor_property("default_value", 0.18)

    wp_cls = getattr(unreal, "MaterialExpressionWorldPosition", unreal.MaterialExpressionWorldPosition)
    wp = mel.create_material_expression(mat, wp_cls, -520, 220)
    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -200, 180)
    custom.set_editor_property("code",
        "float2 p = WorldPos.xy * 0.0018;\n"
        "float streaks = 0.55 + 0.45 * saturate(sin(p.x * 6.2831) * 0.5 + 0.5);\n"
        "float puddle = 0.70 + 0.30 * frac(sin(dot(floor(p * 0.35), float2(12.9, 78.2))) * 43758.5);\n"
        "return lerp(0.10, 1.0, streaks * puddle);")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inp_w = unreal.CustomInput()
    inp_w.set_editor_property("input_name", "WorldPos")
    custom.set_editor_property("inputs", [inp_w])
    mel.connect_material_expressions(wp, "", custom, "WorldPos")

    mul_r = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 40, 80)
    mel.connect_material_expressions(rough_p, "", mul_r, "A")
    mel.connect_material_expressions(custom, "", mul_r, "B")
    mel.connect_material_property(mul_r, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -520, 360)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.08)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

    spec = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -520, 480)
    spec.set_editor_property("parameter_name", "Specular")
    spec.set_editor_property("default_value", 0.90)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    flag_ism(mat)
    mel.recompile_material(mat)


w = new_mat("/Game/Materials/M_NightWindow", "M_NightWindow")
make_window(w)
eal.save_asset("/Game/Materials/M_NightWindow")
unreal.log_warning("[v12-mat] saved M_NightWindow (facade-only world-normal emissive + world UV windows)")

c = new_mat("/Game/Materials/M_NightCarPaint", "M_NightCarPaint")
make_car_paint(c)
eal.save_asset("/Game/Materials/M_NightCarPaint")
unreal.log_warning("[v12-mat] saved M_NightCarPaint (ClearCoat attempt, zero emissive default, Go-Mango)")

a = new_mat("/Game/Materials/M_NightAsphalt", "M_NightAsphalt")
make_asphalt(a)
eal.save_asset("/Game/Materials/M_NightAsphalt")
unreal.log_warning("[v12-mat] saved M_NightAsphalt (lifted wet ground)")
unreal.log_warning("[v12-mat] done")