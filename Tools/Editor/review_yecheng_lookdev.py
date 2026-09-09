"""Isolated static head review. Never writes the production map or character data."""
from pathlib import Path
import unreal,json,math,time,traceback
ROOT=Path(unreal.Paths.project_dir()).resolve().parent
OUT=ROOT/'SourceAssets/Characters/YeChengLookdev';DEST='/Game/WindStation/Lookdev/YeChengLookdev'
versions=['baseline','candidate_02'];meshes={}
assettools=unreal.AssetToolsHelpers.get_asset_tools();editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorAssetLibrary.make_directory(DEST)
assert levels.new_level(DEST+'/LD_HeadReview',False)
world=unreal.EditorLevelLibrary.get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',unreal.GameModeBase)
for version in versions:
    src=OUT/'UEExport'/version
    task=unreal.AssetImportTask();task.filename=str(src/'HeadReview.fbx');task.destination_path=DEST+'/'+version
    task.destination_name='SM_HeadReview_'+version;task.automated=True;task.replace_existing=True;task.save=True
    options=unreal.FbxImportUI();options.import_as_skeletal=False;options.import_mesh=True;options.import_materials=False;options.import_textures=False
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.generate_lightmap_u_vs=False
    task.options=options;assettools.import_asset_tasks([task]);mesh=unreal.load_asset(task.imported_object_paths[0]);meshes[version]=mesh
    manifest=json.loads((src/'materials.json').read_text(encoding='utf-8'))
    for i,slot in enumerate(mesh.static_materials):
        name=str(slot.material_slot_name);props=manifest[name]
        mat=assettools.create_asset('M_'+name,DEST+'/'+version+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
        assert mat
        mat.set_editor_property('two_sided',True)
        if props['texture']:
            tt=unreal.AssetImportTask();tt.filename=str(src/props['texture']);tt.destination_path=DEST+'/'+version+'/Textures';tt.automated=True;tt.replace_existing=True;tt.save=True
            assettools.import_asset_tasks([tt]);tex=unreal.load_asset(tt.imported_object_paths[0]);tex.set_editor_property('never_stream',True);unreal.EditorAssetLibrary.save_loaded_asset(tex)
            color=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionTextureSample);color.texture=tex;output='RGB'
        else:
            color=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);color.constant=unreal.LinearColor(*props['color'],1);output=''
        unreal.MaterialEditingLibrary.connect_material_property(color,output,unreal.MaterialProperty.MP_BASE_COLOR)
        r=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant);r.r=props['roughness']
        unreal.MaterialEditingLibrary.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS)
        unreal.MaterialEditingLibrary.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat);mesh.set_material(i,mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)

actor=editor.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,0));actor.set_actor_label('YeCheng candidate - isolated static review')
component=actor.static_mesh_component;component.set_static_mesh(meshes['candidate_02'])
target=unreal.Vector(0,0,158)
for pos,power,size in [((-68,127.5,221),1000,102),((76.5,59.5,170),450,102),((0,-68,195.5),600,85)]:
    loc=unreal.Vector(*pos);light=editor.spawn_actor_from_class(unreal.RectLight,loc,unreal.MathLibrary.find_look_at_rotation(loc,target))
    c=light.get_component_by_class(unreal.RectLightComponent);c.set_intensity(power);c.set_editor_property('source_width',size);c.set_editor_property('source_height',size)
background=assettools.create_asset('M_Background',DEST,unreal.Material,unreal.MaterialFactoryNew());background.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
color=unreal.MaterialEditingLibrary.create_material_expression(background,unreal.MaterialExpressionConstant3Vector);color.constant=unreal.LinearColor(.08,.08,.08,1)
unreal.MaterialEditingLibrary.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);unreal.MaterialEditingLibrary.recompile_material(background);unreal.EditorAssetLibrary.save_loaded_asset(background)
wall=editor.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,-180,150));wall.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));wall.set_actor_scale3d(unreal.Vector(8,.02,8));wall.static_mesh_component.set_material(0,background)
assert levels.save_current_level()
import runpy
runpy.run_path(str(ROOT/'Tools/Editor/capture_yecheng_lookdev.py'))
