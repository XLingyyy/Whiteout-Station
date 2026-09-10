"""Import audited v1.7 real captures and create the runtime tutorial data asset."""
from pathlib import Path
import json
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve().parent
TEXTURES = '/Game/WindStation/UI/v17/Tutorial/Textures'
DATA = '/Game/WindStation/UI/v17/Tutorial/Data'

def main():
    manifest = json.loads((ROOT/'docs/QA/v1.7_tutorial_assets.json').read_text(encoding='utf-8-sig'))
    tasks=[]
    for capture in manifest['captures']:
        if capture['resource_status'] != 'Approved': raise RuntimeError('Unapproved tutorial capture')
        task=unreal.AssetImportTask()
        task.filename=str(ROOT/'Artifacts/v1.7-evidence'/capture['cropped_file'])
        task.destination_path=TEXTURES
        task.destination_name=capture['capture_id']
        task.automated=True
        task.replace_existing=True
        task.save=True
        tasks.append(task)
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    tools.import_asset_tasks(tasks)
    for task in tasks:
        if not task.imported_object_paths: raise RuntimeError(f'Import failed: {task.filename}')
        texture=unreal.load_asset(task.imported_object_paths[0])
        texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property('srgb',True)
        texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
    asset=unreal.load_asset(DATA+'/DA_Tutorial_A')
    if not asset:
        factory=unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.WSTutorialData)
        asset=tools.create_asset('DA_Tutorial_A',DATA,unreal.WSTutorialData,factory)
    pages=asset.get_editor_property('pages')
    if len(pages)!=6: raise RuntimeError('Tutorial defaults must contain offline/online plus four common pages')
    for page in pages:
        page.set_editor_property('resource_status',unreal.WSTutorialResourceStatus.APPROVED)
    asset.set_editor_property('pages',pages)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log('V17 tutorial import complete: 7 textures and content asset')

if __name__=='__main__': main()
