"""Recreate the existing performances on the newly authored reference skeleton."""
from pathlib import Path
import runpy
import json
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve().parent
DEST='/Game/WindStation/Art/AnimeNPC/YeChengOriginal'
source=runpy.run_path(str(ROOT/'Tools/Editor/generate_v10_npc_animations.py'))
mesh=unreal.load_asset(DEST+'/SK_YeCheng_Original')
assert mesh
skel=mesh.skeleton
reference=skel.get_reference_pose()
results={}
for suffix,performance in source['PERFORMANCES'].items():
    name='AN_YeCheng_Original_'+suffix
    path=DEST+'/Animations/'+name
    animation=unreal.load_asset(path)
    if animation is None:
        factory=unreal.AnimSequenceFactory()
        factory.set_editor_property('target_skeleton',skel)
        factory.set_editor_property('preview_skeletal_mesh',mesh)
        animation=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,DEST+'/Animations',unreal.AnimSequence,factory)
    frames=round(performance.duration*30)
    samples=[performance.evaluator(i/frames) for i in range(frames+1)]
    controller=animation.controller
    controller.open_bracket('Adapt Ye Cheng performance to original skeleton',False)
    try:
        controller.set_frame_rate(unreal.FrameRate(numerator=30,denominator=1),False)
        controller.set_number_of_frames(unreal.FrameNumber(value=frames),False)
        for name in sorted({b for s in samples for b in s}):
            local=reference.get_ref_bone_pose(name,unreal.AnimPoseSpaces.LOCAL)
            world=reference.get_ref_bone_pose(name,unreal.AnimPoseSpaces.WORLD)
            q=world.rotation
            inverse=unreal.Quat(-q.x,-q.y,-q.z,q.w)
            controller.add_bone_track(name,False)
            rotations=[]
            for sample in samples:
                delta=source['euler_quaternion'](sample.get(name,(0,0,0)))
                rotations.append(local.rotation*inverse*delta*world.rotation)
            assert controller.set_bone_track_keys(name,[local.translation]*(frames+1),rotations,[local.scale3d]*(frames+1),False)
    finally:
        controller.close_bracket(False)
    unreal.EditorAssetLibrary.save_loaded_asset(animation)
    options=unreal.AnimPoseEvaluationOptions()
    options.optional_skeletal_mesh=mesh
    options.should_retarget=False
    snapshots=[]
    for t in [0,.25,.5,.75,1]:
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(animation,animation.sequence_length*t,options)
        sample={}
        for name in ['J_Bip_C_Head','J_Bip_L_Hand','J_Bip_R_Hand','J_Bip_L_Foot','J_Bip_R_Foot']:
            p=unreal.AnimPoseExtensions.get_bone_pose(pose,name,unreal.AnimPoseSpaces.WORLD).translation
            sample[name]=[p.x,p.y,p.z]
            assert all(abs(v)<300 for v in sample[name]),(suffix,name,sample[name])
        snapshots.append(sample)
    results[suffix]={'length':animation.sequence_length,'samples':snapshots}
(ROOT/'SourceAssets/Characters/YeChengOriginal/animation_validation.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
unreal.log('YECHENG_ORIGINAL_ANIMATIONS_COMPLETE')
