"""Import the selected Quaternius CC0 interior FBX files for v0.2 zones."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


REPO_ROOT = Path(unreal.Paths.project_dir()).resolve().parent
SELECTED_ROOT = REPO_ROOT / "SourceAssets" / "Quaternius" / "UltimateHouseInterior" / "Selected"
MANIFEST_PATH = SELECTED_ROOT / "manifest.json"
MESH_ROOT = "/Game/WindStation/Art/Environment/Quaternius/Interior"


def import_static_mesh(source_file: Path) -> None:
    object_name = source_file.stem
    asset_path = f"{MESH_ROOT}/{object_name}"
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
    task.set_editor_property("destination_path", MESH_ROOT)
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
        raise RuntimeError("Quaternius interior manifest contains no FBX files")

    unreal.EditorAssetLibrary.make_directory(MESH_ROOT)
    for source_file in model_files:
        import_static_mesh(source_file)
        unreal.log(f"WhiteoutStation v0.2: imported interior {source_file.name}")
    unreal.log(f"WhiteoutStation v0.2: imported {len(model_files)} interior meshes")


if __name__ == "__main__":
    main()
