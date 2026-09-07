import unreal
mel = unreal.MaterialEditingLibrary
mat = unreal.EditorAssetLibrary.load_asset("/Game/Materials/M_NightWindow")
# Find EmissiveColor vector param expression and lower default
for expr in mel.get_material_expressions(mat):
    try:
        name = str(expr.get_editor_property("parameter_name"))
    except Exception:
        continue
    if name == "EmissiveColor":
        expr.set_editor_property("default_value", unreal.LinearColor(1.6, 1.2, 0.55, 1.0))
        unreal.log_warning("[v10.1] lowered NightWindow EmissiveColor default")
    if name == "BaseColor":
        expr.set_editor_property("default_value", unreal.LinearColor(0.02, 0.025, 0.04, 1.0))
mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset("/Game/Materials/M_NightWindow")
# Car paint emissive default moderate orange
mat2 = unreal.EditorAssetLibrary.load_asset("/Game/Materials/M_NightCarPaint")
for expr in mel.get_material_expressions(mat2):
    try:
        name = str(expr.get_editor_property("parameter_name"))
    except Exception:
        continue
    if name == "EmissiveColor":
        expr.set_editor_property("default_value", unreal.LinearColor(1.2, 0.35, 0.04, 1.0))
mel.recompile_material(mat2)
unreal.EditorAssetLibrary.save_asset("/Game/Materials/M_NightCarPaint")
unreal.log_warning("[v10.1] mats saved")
