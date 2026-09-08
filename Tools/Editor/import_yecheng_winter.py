"""Import the Blender winter outfit without overwriting existing NPC assets."""
from pathlib import Path
import json
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve().parent
DEST = '/Game/WindStation/Art/AnimeNPC/YeChengWinter'
if unreal.EditorAssetLibrary.does_directory_exist(DEST):
    raise RuntimeError('Winter destination already exists; inspect before reimporting')
task = unreal.AssetImportTask()
task.filename = str(ROOT / 'SourceAssets/Characters/YeChengWinter/YeCheng_Winter.vrm')
task.destination_path = DEST
task.automated = True
task.replace_existing = False
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assets = [unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True, include_folder=False)]
meshes = [a for a in assets if isinstance(a, unreal.SkeletalMesh)]
assert len(meshes) == 1, [a.get_path_name() for a in meshes]
mesh = meshes[0]
unreal.EditorAssetLibrary.save_directory(DEST, only_if_is_dirty=True, recursive=True)
report = {'mesh': mesh.get_path_name(), 'skeleton': mesh.skeleton.get_path_name(), 'materials': [str(m.material_slot_name) for m in mesh.materials], 'assets': len(assets)}
(ROOT / 'Artifacts/YeChengReference/import_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('YECHENG_WINTER_IMPORT_COMPLETE ' + json.dumps(report))
