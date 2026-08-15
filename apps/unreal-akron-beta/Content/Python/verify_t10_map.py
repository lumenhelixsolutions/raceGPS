import unreal

def w(m):
    unreal.log_warning("[verify-t10] " + str(m))

subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
w("load: " + str(subsys.load_level("/Game/Maps/Cleveland5_0KmWorld")))
actors = unreal.EditorLevelLibrary.get_all_level_actors()

bp_gate = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Blueprints/BP_CheckpointGate")
gates = placeholders = 0
building_instances = water_instances = 0
pmc_sections = 0
pois = playerstarts = splines = 0
for a in actors:
    cls = a.get_class()
    label = a.get_actor_label()
    if bp_gate and cls == bp_gate:
        gates += 1
    elif cls.get_name() == "Actor" and label.startswith("CP_"):
        placeholders += 1
    if label == "Buildings":
        for h in a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            building_instances += h.get_instance_count()
    if label == "Water":
        for h in a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            water_instances += h.get_instance_count()
    if label == "Terrain":
        p = a.get_component_by_class(unreal.ProceduralMeshComponent)
        pmc_sections = p.get_num_sections() if p else 0
    if label.startswith("POI_"):
        pois += 1
    if cls.get_name() == "PlayerStart":
        playerstarts += 1
    if a.get_component_by_class(unreal.SplineComponent):
        splines += 1

w(f"total_actors={len(actors)}")
w(f"gates={gates} placeholders={placeholders}")
w(f"building_instances={building_instances}")
w(f"water_instances={water_instances}")
w(f"terrain_pmc_sections={pmc_sections}")
w(f"pois={pois} playerstarts={playerstarts} spline_actors={splines}")
w("done")
