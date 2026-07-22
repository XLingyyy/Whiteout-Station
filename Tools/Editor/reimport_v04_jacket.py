"""Reimport the deterministic v0.4 Ye Cheng jacket texture in Unreal."""

from __future__ import annotations

from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
SOURCE = PROJECT_ROOT / "SourceAssets/MakeHuman/Characters/DoctorYeCheng/textures/rescueteam_jacket.png"
DESTINATION = "/Game/WindStation/Art/Characters/Doctor"
ASSET_NAME = "rescueteam_jacket"


def main() -> None:
    if not SOURCE.is_file():
        raise RuntimeError(f"Missing source texture: {SOURCE}")
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE)
    task.destination_path = DESTINATION
    task.destination_name = ASSET_NAME
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = False
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    expected = f"{DESTINATION}/{ASSET_NAME}"
    if not unreal.EditorAssetLibrary.does_asset_exist(expected):
        raise RuntimeError(f"Reimport did not produce {expected}")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log(f"WhiteoutStation v0.4: reimported {expected} from {SOURCE}")


if __name__ == "__main__":
    main()
