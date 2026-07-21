"""Import the selected CC0 Quaternius station meshes for the v0.2 assembly."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


REPO_ROOT = Path(unreal.Paths.project_dir()).resolve().parent
SELECTED_ROOT = (
    REPO_ROOT / "SourceAssets" / "Quaternius" / "ModularSciFiMegaKit" / "Selected"
)
MANIFEST_PATH = SELECTED_ROOT / "manifest.json"
MESH_ROOT = "/Game/WindStation/Art/Environment/Quaternius"


def import_static_mesh(source_file: Path, destination_path: str) -> None:
    object_name = source_file.stem
    asset_path = f"{destination_path}/{object_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log(f"WhiteoutStation v0.2: verified existing {asset_path}")
        return

    options = unreal.FbxImportUI()
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    static_options = options.get_editor_property("static_mesh_import_data")
    static_options.set_editor_property("combine_meshes", True)
    static_options.set_editor_property("generate_lightmap_u_vs", True)
    static_options.set_editor_property("auto_generate_collision", True)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_file))
    task.set_editor_property("destination_path", destination_path)
    task.set_editor_property("destination_name", object_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if not task.get_editor_property("imported_object_paths"):
        raise RuntimeError(f"Unreal did not import {source_file}")


def main() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8-sig"))
    model_files = [
        REPO_ROOT / entry["file"]
        for entry in manifest["files"]
        if str(entry["file"]).lower().endswith(".fbx")
    ]
    if not model_files:
        raise RuntimeError("Quaternius selected manifest contains no FBX files")

    for source_file in model_files:
        relative = source_file.relative_to(SELECTED_ROOT / "Models")
        destination = f"{MESH_ROOT}/{relative.parent.as_posix()}"
        unreal.EditorAssetLibrary.make_directory(destination)
        import_static_mesh(source_file, destination)
        unreal.log(f"WhiteoutStation v0.2: imported {relative.as_posix()}")

    unreal.EditorAssetLibrary.save_directory(MESH_ROOT, only_if_is_dirty=False, recursive=True)
    unreal.log(f"WhiteoutStation v0.2: imported {len(model_files)} environment meshes")


if __name__ == "__main__":
    main()
