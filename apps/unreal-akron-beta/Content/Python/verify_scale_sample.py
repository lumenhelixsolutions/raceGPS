import unreal

def w(m):
    unreal.log_warning("[scale-check] " + str(m))

subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
for map_name in ("/Game/Maps/Cleveland5_0KmWorld", "/Game/Maps/Akron5_0KmWorld"):
    subsys.load_level(map_name)
    sp_done = False
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        label = a.get_actor_label()
        if label == "Buildings":
            h = a.get_component_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
            n = h.get_instance_count()
            # tallest instance by z-scale
            best = None
            for i in range(n):
                t = h.get_instance_transform(i, True)
                if best is None or t.scale3d.z > best.scale3d.z:
                    best = t
            w(f"{map_name} instances={n} tallest: loc=({best.translation.x:.0f},{best.translation.y:.0f},{best.translation.z:.0f}) scale=({best.scale3d.x:.1f},{best.scale3d.y:.1f},{best.scale3d.z:.1f})")
        if label.startswith("SP_") and not sp_done:
            loc = a.get_actor_location()
            w(f"{map_name} {label} at ({loc.x:.0f},{loc.y:.0f},{loc.z:.0f})")
            sp_done = True
