import sys
import unreal

subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

def count(map_path, tag):
    ok = subsys.load_level(map_path)
    if not ok:
        unreal.log_warning(f"[{tag}] FAILED to load {map_path}")
        return
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    gate_native = unreal.load_class(None, "/Script/raceGPSAkronBeta.CheckpointGate")
    bp_class = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Blueprints/BP_CheckpointGate")
    gates, placeholders = 0, 0
    for a in actors:
        cls = a.get_class()
        if bp_class and cls == bp_class:
            gates += 1
        elif cls.get_name() == "Actor" and a.get_actor_label().startswith("CP_"):
            placeholders += 1
    unreal.log_warning(f"[{tag}] {map_path}: total={len(actors)} bp_gates={gates} placeholder_checkpoints={placeholders} bp_class_loaded={bp_class is not None}")

count("/Game/Maps/Cleveland5_0KmWorld", "cleveland")
count("/Game/Maps/AkronWorld", "akron")
