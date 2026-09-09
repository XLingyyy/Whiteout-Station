"""Capture the existing isolated lookdev scene with fixed dialogue cameras."""
from pathlib import Path
import unreal,json,math,time,traceback
ROOT=Path(unreal.Paths.project_dir()).resolve().parent
OUT=ROOT/'SourceAssets/Characters/YeChengLookdev';DEST='/Game/WindStation/Lookdev/YeChengLookdev'
versions=['baseline','candidate_02']
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorLoadingAndSavingUtils.load_map(DEST+'/LD_HeadReview')
world=unreal.EditorLevelLibrary.get_editor_world()
editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actor=next(a for a in editor.get_all_level_actors() if a.get_actor_label()=='YeCheng candidate - isolated static review')
component=actor.static_mesh_component
meshes={v:unreal.load_asset(DEST+'/'+v+'/SM_HeadReview_'+v) for v in versions}
assert all(meshes.values())
for old in editor.get_all_level_actors():
    if isinstance(old,unreal.SceneCapture2D):editor.destroy_actor(old)
target=unreal.Vector(0,0,158)
capture=editor.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(0,140,158));cap=capture.get_component_by_class(unreal.SceneCaptureComponent2D)
rt=unreal.RenderingLibrary.create_render_target2d(world,1600,900,unreal.TextureRenderTargetFormat.RTF_RGBA8)
cap.texture_target=rt;cap.capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR;cap.capture_every_frame=True;cap.always_persist_rendering_state=True;cap.capture_on_movement=False
cap.set_editor_property('show_flag_settings',[unreal.EngineShowFlagsSetting(show_flag_name='TemporalAA',enabled=True),unreal.EngineShowFlagsSetting(show_flag_name='MotionBlur',enabled=False)])
settings=cap.post_process_settings
for prop,value in [('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL),('auto_exposure_bias',-7.),('auto_exposure_apply_physical_camera_exposure',False),('motion_blur_amount',0.),('depth_of_field_fstop',0.),('bloom_intensity',0.),('vignette_intensity',0.),('scene_color_tint',unreal.LinearColor(1,1,1,1))]:
    settings.set_editor_property('override_'+prop,True);settings.set_editor_property(prop,value)
cap.post_process_settings=settings;cap.post_process_blend_weight=1
unreal.SystemLibrary.execute_console_command(world,'r.DepthOfFieldQuality 0');unreal.SystemLibrary.execute_console_command(world,'r.MotionBlurQuality 0')
unreal.SystemLibrary.execute_console_command(world,'r.EyeAdaptationQuality 0')
assert levels.save_current_level()
output=OUT/'UE';output.mkdir(exist_ok=True)
shots=[(v,n,d,f,a) for v in versions for n,d,f,a in [('dialogue_140cm_90deg',140,90,0),('portrait_140cm_55deg',140,55,0),('inspection_70cm_55deg',70,55,0),('inspection_45deg',70,55,45)]]
state={'index':0,'phase':'warmup','next':time.monotonic()+12,'last':None}
def tick(dt):
    try:
        if time.monotonic()<state['next']:return
        if state['phase']=='export':
            unreal.RenderingLibrary.export_render_target(world,rt,str(output),state['last']+'.png')
            unreal.log('LOOKDEV_CAPTURE '+state['last']);state['index']+=1;state['phase']='warmup'
        if state['index']>=len(shots):
            component.set_static_mesh(meshes['candidate_02']);levels.save_current_level()
            (output/'conditions.json').write_text(json.dumps({'shots':shots,'manual_exposure_bias':-7,'physical_camera_exposure':False,'dof':False,'motion_blur':False,'bloom':0,'character_emission':0,'scene':DEST+'/LD_HeadReview','capture':'UE SceneCapture2D, final color LDR, 1600x900','actual_dialogue_basis':'WhiteoutSettingsSubsystem default FOV 90; target visibility range 300 cm; sample distance 140 cm'},indent=2),encoding='utf-8')
            unreal.unregister_slate_post_tick_callback(handle);unreal.log('LOOKDEV_UE_REVIEW_COMPLETE');unreal.SystemLibrary.quit_editor();return
        v,n,d,f,a=shots[state['index']];component.set_static_mesh(meshes[v]);rad=math.radians(a)
        loc=unreal.Vector(d*math.sin(rad),d*math.cos(rad),158)
        capture.set_actor_location_and_rotation(loc,unreal.MathLibrary.find_look_at_rotation(loc,target),False,False);cap.fov_angle=f
        cap.capture_scene();state['last']=v+'_'+n;state['phase']='export';state['next']=time.monotonic()+3
    except Exception:
        unreal.log_error(traceback.format_exc());unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
unreal.log('LOOKDEV_UE_CAPTURE_QUEUED')
