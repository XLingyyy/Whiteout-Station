"""Import the pinned Chinese UI fonts as Unreal font-face assets."""

from __future__ import annotations

from pathlib import Path

import unreal


REPO_ROOT = Path(unreal.Paths.project_dir()).resolve().parent
FONT_SOURCE_ROOT = REPO_ROOT / "SourceAssets" / "Fonts" / "NotoSansSC"
FONT_ASSET_ROOT = "/Game/WindStation/UI/Fonts"


def import_font(source_file: Path, asset_name: str) -> None:
    asset_path = f"{FONT_ASSET_ROOT}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log(f"WhiteoutStation v0.2: verified existing {asset_path}")
        return
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_file))
    task.set_editor_property("destination_path", FONT_ASSET_ROOT)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if not task.get_editor_property("imported_object_paths"):
        raise RuntimeError(f"Unreal did not import {source_file}")
    unreal.log(f"WhiteoutStation v0.2: imported {asset_name}")


def main() -> None:
    unreal.EditorAssetLibrary.make_directory(FONT_ASSET_ROOT)
    import_font(FONT_SOURCE_ROOT / "NotoSansSC-Regular.otf", "FF_NotoSansSC_Regular")
    import_font(FONT_SOURCE_ROOT / "NotoSansSC-Bold.otf", "FF_NotoSansSC_Bold")
    unreal.EditorAssetLibrary.save_directory(FONT_ASSET_ROOT, only_if_is_dirty=False, recursive=True)


if __name__ == "__main__":
    main()
