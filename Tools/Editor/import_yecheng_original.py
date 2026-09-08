"""Import the independently authored Blender mesh into a separate asset folder."""
from pathlib import Path
import json
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve().parent
VARIANT=globals().get('YECHENG_VARIANT','Original')
DEST='/Game/WindStation/Art/AnimeNPC/YeCheng'+VARIANT
task=unreal.AssetImportTask()
task.filename=str(ROOT/f'SourceAssets/Characters/YeCheng{VARIANT}/YeCheng_{VARIANT}.fbx')
task.destination_path=DEST
task.destination_name='SK_YeCheng_'+VARIANT
task.automated=True
task.replace_existing=True
task.save=True
options=unreal.FbxImportUI()
options.import_mesh=True
options.import_as_skeletal=True
options.mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH
options.automated_import_should_detect_type=False
options.import_animations=False
options.import_materials=True
options.import_textures=False
options.create_physics_asset=True
task.options=options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assets=[unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets(DEST,recursive=True,include_folder=False)]
meshes=[a for a in assets if isinstance(a,unreal.SkeletalMesh)]
assert len(meshes)==1,[a.get_path_name() for a in meshes]
mesh=meshes[0]
for a in assets:
    if isinstance(a,unreal.Material):
        a.set_editor_property('two_sided',True)
        unreal.MaterialEditingLibrary.set_material_usage(a,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
        unreal.MaterialEditingLibrary.set_material_usage(a,unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS)
        unreal.MaterialEditingLibrary.recompile_material(a)
unreal.EditorAssetLibrary.save_directory(DEST,only_if_is_dirty=True,recursive=True)
comp=unreal.new_object(unreal.SkeletalMeshComponent)
comp.set_skeletal_mesh_asset(mesh)
pose=mesh.skeleton.get_reference_pose()
report={'mesh':mesh.get_path_name(),'skeleton':mesh.skeleton.get_path_name(),'materials':[str(m.material_slot_name) for m in mesh.materials],'bones':{str(comp.get_bone_name(i)):str(pose.get_ref_bone_pose(comp.get_bone_name(i),unreal.AnimPoseSpaces.WORLD)) for i in range(comp.get_num_bones())}}
(ROOT/f'Artifacts/yecheng_{VARIANT.lower()}_import.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log(f'YECHENG_{VARIANT.upper()}_IMPORT_COMPLETE')
