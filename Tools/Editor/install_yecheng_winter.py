"""Bind the winter mesh to existing animations and the placed Ye Cheng actor."""
from pathlib import Path
import json
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve().parent
mesh = unreal.load_asset('/Game/WindStation/Art/AnimeNPC/YeChengWinter/SK_YeCheng_Winter')
old = unreal.load_asset('/Game/WindStation/Art/AnimeNPC/YeChengV10/SK_YeCheng_NoanoaHair_RefinedFace_v10')
data = unreal.load_asset('/Game/WindStation/Presentation/Characters/DA_WS_YeCheng_AnimeNPC')
assert mesh and old and data
components = []
for asset in (old, mesh):
    c = unreal.new_object(unreal.SkeletalMeshComponent)
    c.set_skeletal_mesh_asset(asset)
    components.append(c)
names = [[str(c.get_bone_name(i)) for i in range(c.get_num_bones())] for c in components]
assert names[0] == names[1], 'Imported bone order differs from animation skeleton'
old_pose = old.skeleton.get_reference_pose()
new_pose = mesh.skeleton.get_reference_pose()
max_error = 0.0
for name in names[0]:
    a = old_pose.get_ref_bone_pose(name, unreal.AnimPoseSpaces.LOCAL)
    b = new_pose.get_ref_bone_pose(name, unreal.AnimPoseSpaces.LOCAL)
    max_error = max(max_error, (a.translation-b.translation).length())
assert max_error < .01, max_error
mesh.skeleton.set_editor_property('compatible_skeletons', [old.skeleton])
unreal.EditorAssetLibrary.save_loaded_asset(mesh.skeleton)
unreal.EditorAssetLibrary.save_directory('/Game/WindStation/Art/AnimeNPC/YeChengWinter', only_if_is_dirty=True, recursive=True)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
def transform_values(a):
    p, r, s = a.get_actor_location(), a.get_actor_rotation(), a.get_actor_scale3d()
    return (p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z)
before = {a.get_path_name(): transform_values(a) for a in actors}
matches = [a for a in actors if isinstance(a,unreal.WSInteractableActor) and str(a.get_editor_property('action_id')) == 'talk_ye_cheng']
assert len(matches) == 1
actor = matches[0]
comp = actor.get_editor_property('character_mesh')
prior_mesh = comp.get_skeletal_mesh_asset().get_path_name()
data.modify()
data.set_editor_property('skeletal_mesh', mesh)
actor.modify()
comp.modify()
comp.set_skeletal_mesh_asset(mesh)
comp.set_editor_property('override_materials', [])
assert comp.get_skeletal_mesh_asset() == mesh
assert all(transform_values(a) == before[a.get_path_name()] for a in actors)
unreal.EditorAssetLibrary.save_loaded_asset(data)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
report = {'old_mesh':prior_mesh,'new_mesh':mesh.get_path_name(),'bones':len(names[0]),'max_reference_translation_error_cm':max_error,'actor_transforms_unchanged':len(actors),'actor':actor.get_path_name(),'actor_location':str(actor.get_actor_location()),'actor_rotation':str(actor.get_actor_rotation()),'component_transform':str(comp.get_world_transform())}
(ROOT/'Artifacts/YeChengReference/install_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([actor])
unreal.log('YECHENG_WINTER_INSTALL_COMPLETE '+json.dumps(report))
