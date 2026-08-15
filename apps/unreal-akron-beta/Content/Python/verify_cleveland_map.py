import unreal
subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ok = subsys.load_level("/Game/Maps/Cleveland5_0KmWorld")
unreal.log_warning(f"[verify] load_level -> {ok}")
actors = unreal.EditorLevelLibrary.get_all_level_actors()
unreal.log_warning(f"[verify] actor count: {len(actors)}")
from collections import Counter
counts = Counter(a.get_class().get_name() for a in actors)
for name, n in sorted(counts.items()):
    unreal.log_warning(f"[verify]   {name}: {n}")
splines = [a for a in actors if a.get_component_by_class(unreal.SplineComponent)]
for a in splines:
    comp = a.get_component_by_class(unreal.SplineComponent)
    unreal.log_warning(f"[verify]   spline actor {a.get_actor_label()}: {comp.get_number_of_spline_points()} pts")
