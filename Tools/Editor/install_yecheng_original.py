"""Assign original materials and replace Ye Cheng while preserving actor layout."""
from pathlib import Path
import json
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve().parent
VARIANT=globals().get('YECHENG_VARIANT','Original')
DEST='/Game/WindStation/Art/AnimeNPC/YeCheng'+VARIANT
mesh=unreal.load_asset(DEST+'/SK_YeCheng_'+VARIANT)
palette=json.loads((ROOT/f'SourceAssets/Characters/YeCheng{VARIANT}/materials.json').read_text(encoding='utf-8'))
slots=list(mesh.materials)
for slot in slots:
    token=str(slot.material_slot_name)
    props=palette[token]
    path=DEST+'/Materials/M_'+token
    mat=unreal.load_asset(path)
    rebuild=mat is None or VARIANT=='HighDetail'
    if mat is None:
        mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_'+token,DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    if rebuild:
        unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
        if props.get('texture'):
            texture_task=unreal.AssetImportTask()
            texture_task.filename=str(ROOT/f'SourceAssets/Characters/YeCheng{VARIANT}'/props['texture'])
            texture_task.destination_path=DEST+'/Textures'
            texture_task.automated=True
            texture_task.replace_existing=True
            texture_task.save=True
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([texture_task])
            texture=unreal.load_asset(texture_task.imported_object_paths[0])
            texture.set_editor_property('never_stream',True)
            unreal.EditorAssetLibrary.save_loaded_asset(texture)
            color=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionTextureSample)
            color.set_editor_property('texture',texture)
            unreal.MaterialEditingLibrary.connect_material_property(color,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
        else:
            color=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector)
            color.set_editor_property('constant',unreal.LinearColor(*props['color'],1))
            unreal.MaterialEditingLibrary.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
        for key,prop in [('roughness',unreal.MaterialProperty.MP_ROUGHNESS),('metallic',unreal.MaterialProperty.MP_METALLIC)]:
            node=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant)
            node.set_editor_property('r',props[key])
            unreal.MaterialEditingLibrary.connect_material_property(node,'',prop)
        if props.get('emissive_strength'):
            multiply=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionMultiply)
            multiply.set_editor_property('const_b',props['emissive_strength'])
            unreal.MaterialEditingLibrary.connect_material_expressions(color,'RGB' if props.get('texture') else '',multiply,'A')
            unreal.MaterialEditingLibrary.connect_material_property(multiply,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property('two_sided',True)
    unreal.MaterialEditingLibrary.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    unreal.MaterialEditingLibrary.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    slot.material_interface=mat
mesh.set_editor_property('materials',slots)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
data=unreal.load_asset('/Game/WindStation/Presentation/Characters/DA_WS_YeCheng_AnimeNPC')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
def transform(a):
    p,r,s=a.get_actor_location(),a.get_actor_rotation(),a.get_actor_scale3d()
    return [p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z]
before={a.get_path_name():transform(a) for a in actors}
matches=[a for a in actors if isinstance(a,unreal.WSInteractableActor) and str(a.get_editor_property('action_id'))=='talk_ye_cheng']
assert len(matches)==1
actor=matches[0];component=actor.get_editor_property('character_mesh')
actor.modify();component.modify();data.modify()
data.set_editor_property('skeletal_mesh',mesh)
component.set_skeletal_mesh_asset(mesh)
component.set_editor_property('override_materials',[])
component.set_editor_property('relative_rotation',unreal.Rotator(yaw=0))
data.set_editor_property('mesh_rotation',unreal.Rotator(yaw=0))
idle=unreal.load_asset(DEST+'/Animations/AN_YeCheng_'+VARIANT+'_Idle')
assert idle
component.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
component.play_animation(idle,True)
assert all(transform(a)==before[a.get_path_name()] for a in actors)
unreal.EditorAssetLibrary.save_loaded_asset(data)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
report={'mesh':mesh.get_path_name(),'actor_transforms_preserved':len(actors),'component_yaw':0,'actor':actor.get_path_name()}
(ROOT/f'SourceAssets/Characters/YeCheng{VARIANT}/install_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log(f'YECHENG_{VARIANT.upper()}_INSTALL_COMPLETE')
