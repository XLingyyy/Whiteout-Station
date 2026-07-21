"""Import v0.3 generated UI textures, portraits and icon set."""

from __future__ import annotations

from pathlib import Path

import unreal


REPO_ROOT = Path(unreal.Paths.project_dir()).resolve().parent
SOURCE_ROOT = REPO_ROOT / "SourceAssets" / "UI" / "v0.3"
ASSET_ROOT = "/Game/WindStation/UI/v03"


def import_texture(source_file: Path, destination_root: str, asset_name: str) -> object:
    if not source_file.is_file():
        raise RuntimeError(f"Missing v0.3 UI source {source_file}")
    unreal.EditorAssetLibrary.make_directory(destination_root)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_file))
    task.set_editor_property("destination_path", destination_root)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset_path = f"{destination_root}/{asset_name}"
    texture = unreal.load_asset(asset_path)
    if texture is None:
        raise RuntimeError(f"Unable to import {source_file} as {asset_path}")
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    unreal.log(f"WhiteoutStation v0.3: imported {asset_path}")
    return texture


def main() -> None:
    import_texture(SOURCE_ROOT / "T_UI_InkBrush.png", f"{ASSET_ROOT}/Textures", "T_UI_InkBrush")
    for portrait in ("P_PlayerSilhouette", "P_GuHeng", "P_YeCheng"):
        import_texture(SOURCE_ROOT / f"{portrait}.png", f"{ASSET_ROOT}/Portraits", portrait)
    for icon_file in sorted((SOURCE_ROOT / "Icons").glob("*.png")):
        import_texture(icon_file, f"{ASSET_ROOT}/Icons", icon_file.stem)
    unreal.EditorAssetLibrary.save_directory(ASSET_ROOT, only_if_is_dirty=False, recursive=True)


if __name__ == "__main__":
    main()
