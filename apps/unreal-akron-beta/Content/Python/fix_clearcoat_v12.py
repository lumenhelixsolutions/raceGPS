import unreal
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

names = [x for x in dir(unreal.MaterialProperty) if not x.startswith("_")]
unreal.log_warning("[v12-cc] MaterialProperty: " + ", ".join(names))

mat = eal.load_asset("/Game/Materials/M_NightCarPaint")
if not mat:
    unreal.log_error("[v12-cc] M_NightCarPaint missing")
else:
    # Find existing ClearCoat scalar params by iterating expressions.
    exprs = list(mel.get_material_expressions(mat) or [])
    by_name = {}
    for e in exprs:
        try:
            pn = e.get_editor_property("parameter_name")
            if pn:
                by_name[str(pn)] = e
        except Exception:
            pass
    unreal.log_warning("[v12-cc] params " + ", ".join(sorted(by_name.keys())))
    clear = by_name.get("ClearCoat")
    clear_r = by_name.get("ClearCoatRoughness")
    for amount_name, rough_name in (
        ("CUSTOM_DATA_0", "CUSTOM_DATA_1"),
        ("MP_CUSTOM_DATA_0", "MP_CUSTOM_DATA_1"),
        ("CUSTOM_DATA0", "CUSTOM_DATA1"),
        ("CLEAR_COAT", "CLEAR_COAT_ROUGHNESS"),
        ("MP_CLEAR_COAT", "MP_CLEAR_COAT_ROUGHNESS"),
    ):
        a = getattr(unreal.MaterialProperty, amount_name, None)
        r = getattr(unreal.MaterialProperty, rough_name, None)
        if a is None or r is None or clear is None or clear_r is None:
            continue
        try:
            mel.connect_material_property(clear, "", a)
            mel.connect_material_property(clear_r, "", r)
            unreal.log_warning("[v12-cc] connected %s / %s" % (amount_name, rough_name))
            break
        except Exception as exc:
            unreal.log_warning("[v12-cc] %s failed: %s" % (amount_name, exc))
    try:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    except Exception as exc:
        unreal.log_warning("[v12-cc] shading_model set failed: %s" % exc)
    mel.recompile_material(mat)
    eal.save_asset("/Game/Materials/M_NightCarPaint")
    unreal.log_warning("[v12-cc] saved M_NightCarPaint")
