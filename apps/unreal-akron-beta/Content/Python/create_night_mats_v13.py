"""V13 night materials: City Sample techniques ported (no Epic uassets copied).

Lessons applied from City Sample M_Window community docs:
  1) Fake interior depth (parallax / dual-layer mullion + offset warm room)
  2) AmountOff + cell hash random lit/dark (not every cell lit)
  3) InteriorExposure + InteriorTint scalars for night blend
  4) EmissiveStrength multiply for night brightness
  5) Keep V12 facade-only world Normal.Z gate (roofs stay dark)

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
        unreal.log_warning(f"[v13-mat] deleted {path}")


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
    """City Sample-inspired night windows: facade gate + AmountOff + parallax room."""
    base = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, -220)
    base.set_editor_property("parameter_name", "BaseColor")
    base.set_editor_property("default_value", unreal.LinearColor(0.024, 0.026, 0.030, 1.0))
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    emis_col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, 0)
    emis_col.set_editor_property("parameter_name", "EmissiveColor")
    # Tint multiply only (MID must NOT pre-bake strength — that crushed V12 into sheets).
    emis_col.set_editor_property("default_value", unreal.LinearColor(1.0, 0.90, 0.55, 1.0))

    intensity = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 160)
    intensity.set_editor_property("parameter_name", "EmissiveStrength")
    intensity.set_editor_property("default_value", 1.55)

    tile = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 280)
    tile.set_editor_property("parameter_name", "WindowTile")
    tile.set_editor_property("default_value", 9.0)

    amount_off = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 400)
    amount_off.set_editor_property("parameter_name", "AmountOff")
    # City Sample: fraction of windows that stay dark (~0.35–0.55).
    amount_off.set_editor_property("default_value", 0.45)

    int_tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, 520)
    int_tint.set_editor_property("parameter_name", "InteriorTint")
    int_tint.set_editor_property("default_value", unreal.LinearColor(1.0, 0.78, 0.42, 1.0))

    int_exp = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 680)
    int_exp.set_editor_property("parameter_name", "InteriorExposure")
    int_exp.set_editor_property("default_value", 1.10)

    int_depth = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 800)
    int_depth.set_editor_property("parameter_name", "InteriorDepth")
    int_depth.set_editor_property("default_value", 0.55)

    luma_var = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 920)
    luma_var.set_editor_property("parameter_name", "LumaVariation")
    luma_var.set_editor_property("default_value", 0.32)

    seed = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 1040)
    seed.set_editor_property("parameter_name", "WindowSeed")
    seed.set_editor_property("default_value", 0.0)

    nws_cls = getattr(unreal, "MaterialExpressionVertexNormalWS", None) or getattr(unreal, "MaterialExpressionPixelNormalWS", None)
    nws = mel.create_material_expression(mat, nws_cls, -900, 1160)

    wp_cls = getattr(unreal, "MaterialExpressionWorldPosition", unreal.MaterialExpressionWorldPosition)
    wp = mel.create_material_expression(mat, wp_cls, -900, 1280)

    cam_cls = getattr(unreal, "MaterialExpressionCameraPositionWS", None)
    if cam_cls is None:
        cam_cls = getattr(unreal, "MaterialExpressionViewProperty", None)
    cam = mel.create_material_expression(mat, cam_cls, -900, 1400)

    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -320, 260)
    custom.set_editor_property("code",
        "float t = max(Tile, 1.0);\n"
        "float3 N = normalize(Normal);\n"
        "float nz = abs(N.z);\n"
        "// V12 facade gate: roofs/floors stay dark.\n"
        "float facade = 1.0 - smoothstep(0.28, 0.55, nz);\n"
        "float s = 0.0042 * (t / 8.0);\n"
        "float3 P = WorldPos * s;\n"
        "float ax = abs(N.x);\n"
        "float ay = abs(N.y);\n"
        "float2 uv = (ax > ay) ? float2(P.y, P.z) : float2(P.x, P.z);\n"
        "float2 cell = floor(uv + Seed * 17.0);\n"
        "float2 f = frac(uv);\n"
        "// City Sample AmountOff: hash cell -> dark fraction.\n"
        "float amt = saturate(AmountOff);\n"
        "float n1 = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);\n"
        "float n2 = frac(sin(dot(cell, float2(39.346, 11.135))) * 23421.631);\n"
        "float lit = n1 > amt ? 1.0 : 0.0;\n"
        "// Dark mullion frame + thin crossbar (dual-layer).\n"
        "float pane = step(0.10, f.x) * step(f.x, 0.90) * step(0.12, f.y) * step(f.y, 0.88);\n"
        "float barX = abs(f.x - 0.5) < 0.032 ? 0.0 : 1.0;\n"
        "float barY = abs(f.y - 0.5) < 0.028 ? 0.0 : 1.0;\n"
        "float glass = pane * barX * barY;\n"
        "// Cheap parallax room (City Sample HDRI/parallax stand-in).\n"
        "float3 V = normalize(CameraPos - WorldPos);\n"
        "float3 T = normalize(cross(float3(0,0,1), N));\n"
        "if (dot(T,T) < 1e-4) T = float3(1,0,0);\n"
        "float3 B = normalize(cross(N, T));\n"
        "float depth = max(InteriorDepth, 0.05);\n"
        "float2 para = float2(dot(V, T), dot(V, B)) * depth;\n"
        "float2 fi = saturate((f - 0.5) * (1.0 + depth * 0.40) + 0.5 + para * 0.24);\n"
        "float wall = smoothstep(0.0, 0.20, fi.x) * smoothstep(1.0, 0.80, fi.x)\n"
        "          * smoothstep(0.0, 0.18, fi.y) * smoothstep(1.0, 0.82, fi.y);\n"
        "float ceilLamp = smoothstep(0.68, 0.96, fi.y) * 0.60 * wall;\n"
        "float room = saturate(wall * 0.50 + ceilLamp + 0.18);\n"
        "float luma = lerp(1.0 - LumaVar, 1.0 + LumaVar, n2);\n"
        "float3 tint = InteriorTint * InteriorExposure * luma;\n"
        "float glow = glass * lit * facade * room;\n"
        "return tint * glow;")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)

    def _cin(name):
        inp = unreal.CustomInput()
        inp.set_editor_property("input_name", name)
        return inp

    custom.set_editor_property("inputs", [
        _cin("Normal"), _cin("WorldPos"), _cin("CameraPos"), _cin("Tile"),
        _cin("AmountOff"), _cin("InteriorTint"), _cin("InteriorExposure"),
        _cin("InteriorDepth"), _cin("LumaVar"), _cin("Seed"),
    ])
    mel.connect_material_expressions(nws, "", custom, "Normal")
    mel.connect_material_expressions(wp, "", custom, "WorldPos")
    mel.connect_material_expressions(cam, "", custom, "CameraPos")
    mel.connect_material_expressions(tile, "", custom, "Tile")
    mel.connect_material_expressions(amount_off, "", custom, "AmountOff")
    mel.connect_material_expressions(int_tint, "RGB", custom, "InteriorTint")
    mel.connect_material_expressions(int_exp, "", custom, "InteriorExposure")
    mel.connect_material_expressions(int_depth, "", custom, "InteriorDepth")
    mel.connect_material_expressions(luma_var, "", custom, "LumaVar")
    mel.connect_material_expressions(seed, "", custom, "Seed")

    # Graph-side facade gate backup (V12).
    mask_z = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -320, 40)
    try:
        mask_z.set_editor_property("r", False)
        mask_z.set_editor_property("g", False)
        mask_z.set_editor_property("b", True)
        mask_z.set_editor_property("a", False)
    except Exception:
        pass
    mel.connect_material_expressions(nws, "", mask_z, "")
    absz = mel.create_material_expression(mat, unreal.MaterialExpressionAbs, -120, 40)
    mel.connect_material_expressions(mask_z, "", absz, "")
    one = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -120, 140)
    one.set_editor_property("r", 1.0)
    sub = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, 40, 40)
    mel.connect_material_expressions(one, "", sub, "A")
    mel.connect_material_expressions(absz, "", sub, "B")
    sat = mel.create_material_expression(mat, unreal.MaterialExpressionSaturate, 200, 40)
    mel.connect_material_expressions(sub, "", sat, "")
    pwr = mel.create_material_expression(mat, unreal.MaterialExpressionPower, 360, 40)
    pwr_exp = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, 200, 140)
    pwr_exp.set_editor_property("r", 2.2)
    mel.connect_material_expressions(sat, "", pwr, "Base")
    try:
        mel.connect_material_expressions(pwr_exp, "", pwr, "Exp")
    except Exception:
        mel.connect_material_expressions(pwr_exp, "", pwr, "Exponent")

    mul_facade = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 120, 260)
    mel.connect_material_expressions(custom, "", mul_facade, "A")
    mel.connect_material_expressions(pwr, "", mul_facade, "B")

    mul1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 320, 260)
    mel.connect_material_expressions(emis_col, "RGB", mul1, "A")
    mel.connect_material_expressions(mul_facade, "", mul1, "B")

    mul2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 520, 260)
    mel.connect_material_expressions(mul1, "", mul2, "A")
    mel.connect_material_expressions(intensity, "", mul2, "B")
    mel.connect_material_property(mul2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 1520)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.28)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 1640)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.35)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

    spec = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 1760)
    spec.set_editor_property("parameter_name", "Specular")
    spec.set_editor_property("default_value", 0.72)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    flag_ism(mat)
    mel.recompile_material(mat)


def make_car_paint(mat):
    """ClearCoat Go-Mango (carry forward V12)."""
    shading = _enum_member(unreal.MaterialShadingModel, "MSM_CLEAR_COAT", "CLEAR_COAT")
    applied_clear = False
    if shading is not None:
        for prop in ("shading_model", "material_shading_model", "shading_models"):
            try:
                mat.set_editor_property(prop, shading)
                applied_clear = True
                unreal.log_warning(f"[v13-mat] M_NightCarPaint {prop}={shading}")
                break
            except Exception as exc:
                unreal.log_warning(f"[v13-mat] set {prop} failed: {exc}")
    if not applied_clear:
        unreal.log_warning("[v13-mat] ClearCoat shading model unavailable; metallic+specular stand-in")

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
    rough.set_editor_property("default_value", 0.14)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 480)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.84)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

    spec = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 600)
    spec.set_editor_property("parameter_name", "Specular")
    spec.set_editor_property("default_value", 0.92)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    clear = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 720)
    clear.set_editor_property("parameter_name", "ClearCoat")
    clear.set_editor_property("default_value", 0.97)

    clear_r = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 840)
    clear_r.set_editor_property("parameter_name", "ClearCoatRoughness")
    clear_r.set_editor_property("default_value", 0.06)

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
            unreal.log_warning(f"[v13-mat] ClearCoat pins {amount_prop}/{rough_prop}")
            connected = True
            break
        except Exception as exc:
            unreal.log_warning(f"[v13-mat] ClearCoat pin {amount_prop} failed: {exc}")
    if not connected:
        unreal.log_warning("[v13-mat] ClearCoat pins not connected; specular stand-in remains")

    flag_ism(mat)
    try:
        mat.set_editor_property("used_with_skeletal_mesh", True)
    except Exception:
        pass
    mel.recompile_material(mat)


def make_asphalt(mat):
    """Near-field wet asphalt: lower roughness + lifted albedo near camera + lamp specular."""
    base = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -560, -80)
    base.set_editor_property("parameter_name", "BaseColor")
    base.set_editor_property("default_value", unreal.LinearColor(0.105, 0.098, 0.088, 1.0))

    rough_p = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -560, 80)
    rough_p.set_editor_property("parameter_name", "Roughness")
    rough_p.set_editor_property("default_value", 0.11)

    wp_cls = getattr(unreal, "MaterialExpressionWorldPosition", unreal.MaterialExpressionWorldPosition)
    wp = mel.create_material_expression(mat, wp_cls, -560, 220)
    cam_cls = getattr(unreal, "MaterialExpressionCameraPositionWS", None)
    if cam_cls is None:
        cam_cls = getattr(unreal, "MaterialExpressionViewProperty", None)
    cam = mel.create_material_expression(mat, cam_cls, -560, 360)

    custom_b = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -200, -40)
    custom_b.set_editor_property("code",
        "float dist = length(WorldPos.xy - CameraPos.xy);\n"
        "float nearW = saturate(1.0 - dist / 22000.0);\n"
        "float2 p = WorldPos.xy * 0.0016;\n"
        "float streaks = 0.50 + 0.50 * saturate(sin(p.x * 6.2831) * 0.5 + 0.5);\n"
        "float puddle = 0.65 + 0.35 * frac(sin(dot(floor(p * 0.40), float2(12.9, 78.2))) * 43758.5);\n"
        "float wet = saturate(nearW * streaks * puddle);\n"
        "float3 dry = BaseCol;\n"
        "float3 wetCol = dry * 1.45 + float3(0.025, 0.022, 0.018);\n"
        "return lerp(dry, wetCol, wet * 0.75);")
    custom_b.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inp_w = unreal.CustomInput(); inp_w.set_editor_property("input_name", "WorldPos")
    inp_c = unreal.CustomInput(); inp_c.set_editor_property("input_name", "CameraPos")
    inp_b = unreal.CustomInput(); inp_b.set_editor_property("input_name", "BaseCol")
    custom_b.set_editor_property("inputs", [inp_w, inp_c, inp_b])
    mel.connect_material_expressions(wp, "", custom_b, "WorldPos")
    mel.connect_material_expressions(cam, "", custom_b, "CameraPos")
    mel.connect_material_expressions(base, "RGB", custom_b, "BaseCol")
    mel.connect_material_property(custom_b, "", unreal.MaterialProperty.MP_BASE_COLOR)

    custom_r = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -200, 220)
    custom_r.set_editor_property("code",
        "float dist = length(WorldPos.xy - CameraPos.xy);\n"
        "float nearW = saturate(1.0 - dist / 22000.0);\n"
        "float2 p = WorldPos.xy * 0.0016;\n"
        "float streaks = 0.50 + 0.50 * saturate(sin(p.x * 6.2831) * 0.5 + 0.5);\n"
        "float puddle = 0.65 + 0.35 * frac(sin(dot(floor(p * 0.40), float2(12.9, 78.2))) * 43758.5);\n"
        "float wet = saturate(nearW * streaks * puddle);\n"
        "return lerp(Rough * 1.35, Rough * 0.45, wet);")
    custom_r.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inp_w2 = unreal.CustomInput(); inp_w2.set_editor_property("input_name", "WorldPos")
    inp_c2 = unreal.CustomInput(); inp_c2.set_editor_property("input_name", "CameraPos")
    inp_r = unreal.CustomInput(); inp_r.set_editor_property("input_name", "Rough")
    custom_r.set_editor_property("inputs", [inp_w2, inp_c2, inp_r])
    mel.connect_material_expressions(wp, "", custom_r, "WorldPos")
    mel.connect_material_expressions(cam, "", custom_r, "CameraPos")
    mel.connect_material_expressions(rough_p, "", custom_r, "Rough")
    mel.connect_material_property(custom_r, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -560, 520)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.10)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

    spec = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -560, 640)
    spec.set_editor_property("parameter_name", "Specular")
    spec.set_editor_property("default_value", 0.96)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    flag_ism(mat)
    mel.recompile_material(mat)


w = new_mat("/Game/Materials/M_NightWindow", "M_NightWindow")
make_window(w)
eal.save_asset("/Game/Materials/M_NightWindow")
unreal.log_warning("[v13-mat] saved M_NightWindow (City Sample AmountOff + parallax room + facade gate)")

c = new_mat("/Game/Materials/M_NightCarPaint", "M_NightCarPaint")
make_car_paint(c)
eal.save_asset("/Game/Materials/M_NightCarPaint")
unreal.log_warning("[v13-mat] saved M_NightCarPaint (ClearCoat Go-Mango)")

a = new_mat("/Game/Materials/M_NightAsphalt", "M_NightAsphalt")
make_asphalt(a)
eal.save_asset("/Game/Materials/M_NightAsphalt")
unreal.log_warning("[v13-mat] saved M_NightAsphalt (near-field wet + lamp specular)")
unreal.log_warning("[v13-mat] done")
