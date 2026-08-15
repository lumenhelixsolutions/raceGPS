import unreal
def w(m): unreal.log_warning("[spawn-check] " + str(m))
subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
for map_name in ("/Game/Maps/Cleveland5_0KmWorld", "/Game/Maps/Akron5_0KmWorld"):
    subsys.load_level(map_name)
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        if a.get_actor_label().startswith("SP_"):
            loc = a.get_actor_location()
            w(f"{map_name} {a.get_actor_label()} ({loc.x:.1f},{loc.y:.1f},{loc.z:.1f})")
