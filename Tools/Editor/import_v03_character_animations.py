"""Reimport only the generated v0.3 NPC animations against the audited rigs."""

from __future__ import annotations

from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
CHARACTERS = (
    {
        "source_root": PROJECT_ROOT / "SourceAssets" / "MakeHuman" / "Characters" / "EngineerGuHeng",
        "destination": "/Game/WindStation/Art/Characters/Engineer",
        "token": "WS_Engineer",
    },
    {
        "source_root": PROJECT_ROOT / "SourceAssets" / "MakeHuman" / "Characters" / "DoctorYeCheng",
        "destination": "/Game/WindStation/Art/Characters/Doctor",
        "token": "WS_Doctor",
    },
)
ANIMATION_SUFFIXES = ("Idle", "Gesture", "Guarded", "Work")


def make_task(spec: dict[str, object], suffix: str) -> unreal.AssetImportTask:
    source = Path(spec["source_root"]) / f"{spec['token']}_{suffix}.fbx"
    if not source.is_file():
        raise RuntimeError(f"Missing generated animation: {source}")

    skeleton_path = f"{spec['destination']}/SK_{spec['token']}_Skeleton"
    skeleton = unreal.load_asset(skeleton_path)
    if not skeleton:
        raise RuntimeError(f"Missing audited skeleton: {skeleton_path}")

    options = unreal.FbxImportUI()
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.import_mesh = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = True
    options.automated_import_should_detect_type = False
    options.skeleton = skeleton

    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = str(spec["destination"])
    task.destination_name = f"AN_{spec['token']}_{suffix}"
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = False
    task.save = True
    task.options = options
    return task


def main() -> None:
    tasks = [
        make_task(spec, suffix)
        for spec in CHARACTERS
        for suffix in ANIMATION_SUFFIXES
    ]
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    failures = [task.destination_name for task in tasks if not task.imported_object_paths]
    if failures:
        raise RuntimeError("Animation imports produced no assets: " + ", ".join(failures))
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log("WhiteoutStation v0.3 G3: imported eight NPC animations without replacing the character meshes")


if __name__ == "__main__":
    main()
