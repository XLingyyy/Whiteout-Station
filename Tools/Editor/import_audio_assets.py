"""Import the pinned CC0 ambience and configure it as a looping SoundWave."""

from pathlib import Path

import unreal


repo_root = Path(unreal.Paths.project_dir()).resolve().parent
source_file = repo_root / "SourceAssets" / "Freesound" / "344887" / "S_Wind_Strong_CC0.wav"
destination_root = "/Game/WindStation/Audio/Ambience"
asset_path = f"{destination_root}/S_WindStrong_CC0"

unreal.EditorAssetLibrary.make_directory(destination_root)
if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
    task = unreal.AssetImportTask()
    task.filename = str(source_file)
    task.destination_path = destination_root
    task.destination_name = "S_WindStrong_CC0"
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if not task.imported_object_paths:
        raise RuntimeError(f"Unreal did not import {source_file}")

sound_wave = unreal.load_asset(asset_path)
if sound_wave is None:
    raise RuntimeError(f"Unable to load {asset_path}")
sound_wave.set_editor_property("looping", True)
unreal.EditorAssetLibrary.save_loaded_asset(sound_wave)
unreal.log("WhiteoutStation assets: imported looping CC0 wind ambience")
