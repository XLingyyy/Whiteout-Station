"""Validate the installed winter mesh and sample its existing dialogue motions."""
from pathlib import Path
import json
import math
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve().parent
mesh = unreal.load_asset('/Game/WindStation/Art/AnimeNPC/YeChengWinter/SK_YeCheng_Winter')
data = unreal.load_asset('/Game/WindStation/Presentation/Characters/DA_WS_YeCheng_AnimeNPC')
assert data.get_editor_property('skeletal_mesh') == mesh
records = []
for state in ['Idle','Walk','Acknowledge','Consider','Reassure','Reject','Alarmed']:
    a = unreal.load_asset('/Game/WindStation/Art/AnimeNPC/YeChengV10/AnimationsV10/AN_YeCheng_V10_'+state)
    assert a and a.get_play_length() > 0
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('optional_skeletal_mesh',mesh)
    options.set_editor_property('should_retarget',False)
    samples = []
    for fraction in [0,.25,.5,.75,1]:
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(a,a.get_play_length()*fraction,options)
        coords = []
        for bone in ['J_Bip_C_Head','J_Bip_L_Hand','J_Bip_R_Hand','J_Bip_L_Foot','J_Bip_R_Foot']:
            t = unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD).translation
            assert all(math.isfinite(v) and abs(v)<300 for v in (t.x,t.y,t.z)), (state,bone,t)
            coords.append([t.x,t.y,t.z])
        samples.append(coords)
    assert any(sample != samples[0] for sample in samples[1:]), 'Animation did not evaluate: '+state
    records.append({'animation':state,'duration':a.get_play_length(),'samples':5})
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, 'Run this audit during PIE/Simulate'
actors = unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WSInteractableActor)
matches = [a for a in actors if str(a.get_editor_property('action_id'))=='talk_ye_cheng']
assert len(matches)==1
actor=matches[0]
comp=actor.get_editor_property('character_mesh')
assert comp.get_skeletal_mesh_asset()==mesh
assert comp.get_num_bones()==160
assert len(mesh.materials)==19
report={'passed':True,'bones':160,'materials':19,'animations':records,'runtime_mesh':comp.get_skeletal_mesh_asset().get_path_name()}
(ROOT/'SourceAssets/Characters/YeChengWinter/ue_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
# Pose a live PIE instance for visual deformation review, never the saved map.
actor.set_actor_tick_enabled(False)
comp.play_animation(unreal.load_asset('/Game/WindStation/Art/AnimeNPC/YeChengV10/AnimationsV10/AN_YeCheng_V10_Reassure'),True)
unreal.log('YECHENG_WINTER_VALIDATION_COMPLETE '+json.dumps(report))
