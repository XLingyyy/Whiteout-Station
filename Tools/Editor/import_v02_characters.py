"""Import the audited MakeHuman FBX characters as UE skeletal assets."""

from __future__ import annotations

from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent

CHARACTERS = (
    {
        "source": PROJECT_ROOT / "SourceAssets" / "MakeHuman" / "Characters" / "EngineerGuHeng" / "WS_Engineer.fbx",
        "destination": "/Game/WindStation/Art/Characters/Engineer",
        "name": "SK_WS_Engineer",
    },
    {
        "source": PROJECT_ROOT / "SourceAssets" / "MakeHuman" / "Characters" / "DoctorYeCheng" / "WS_Doctor.fbx",
        "destination": "/Game/WindStation/Art/Characters/Doctor",
        "name": "SK_WS_Doctor",
    },
)

ANIMATION_SUFFIXES = ("Idle", "Gesture", "Guarded", "Work")


def make_task(spec: dict[str, object]) -> unreal.AssetImportTask:
    source = Path(spec["source"])
    if not source.is_file():
        raise RuntimeError(f"Missing audited character FBX: {source}")

    options = unreal.FbxImportUI()
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.import_materials = True
    options.import_textures = True
    options.import_animations = False
    options.create_physics_asset = True
    options.automated_import_should_detect_type = False
    skeletal = options.skeletal_mesh_import_data
    skeletal.import_mesh_lods = False
    skeletal.convert_scene = True
    skeletal.force_front_x_axis = False

    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = str(spec["destination"])
    task.destination_name = str(spec["name"])
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = False
    task.save = True
    task.options = options
    return task


def make_animation_task(spec: dict[str, object], suffix: str) -> unreal.AssetImportTask:
    source = Path(spec["source"]).with_name(f"{Path(spec['source']).stem}_{suffix}.fbx")
    if not source.is_file():
        raise RuntimeError(f"Missing generated character animation: {source}")

    skeleton_path = f"{spec['destination']}/{spec['name']}_Skeleton"
    skeleton = unreal.load_asset(skeleton_path)
    if not skeleton:
        raise RuntimeError(f"Missing imported skeleton: {skeleton_path}")

    options = unreal.FbxImportUI()
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.import_mesh = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = True
    options.automated_import_should_detect_type = False
    options.skeleton = skeleton

    character_token = str(spec["name"]).removeprefix("SK_")
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = str(spec["destination"])
    task.destination_name = f"AN_{character_token}_{suffix}"
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = False
    task.save = True
    task.options = options
    return task


def create_animation_blueprint(spec: dict[str, object]) -> None:
    skeleton_path = f"{spec['destination']}/{spec['name']}_Skeleton"
    skeleton = unreal.load_asset(skeleton_path)
    character_token = str(spec["name"]).removeprefix("SK_")
    asset_path = f"{spec['destination']}/ABP_{character_token}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return
    factory_class = unreal.load_class(None, "/Script/UnrealEd.AnimBlueprintFactory")
    anim_blueprint_class = unreal.load_class(None, "/Script/Engine.AnimBlueprint")
    if not factory_class or not anim_blueprint_class:
        raise RuntimeError("UE AnimBlueprint factory classes are unavailable")
    factory = unreal.new_object(factory_class)
    factory.set_editor_property("target_skeleton", skeleton)
    factory.set_editor_property("parent_class", unreal.AnimInstance.static_class())
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        f"ABP_{character_token}",
        str(spec["destination"]),
        anim_blueprint_class,
        factory,
    )
    if not created:
        raise RuntimeError(f"Failed to create Animation Blueprint: {asset_path}")
    unreal.BlueprintEditorLibrary.compile_blueprint(created)
    unreal.log(f"WhiteoutStation v0.2: created Animation Blueprint {asset_path}")


def main() -> None:
    tasks = [make_task(spec) for spec in CHARACTERS]
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    failures = []
    for spec, task in zip(CHARACTERS, tasks):
        imported = list(task.imported_object_paths)
        if not imported:
            failures.append(str(spec["name"]))
        unreal.log(f"WhiteoutStation v0.2: imported {spec['name']} -> {imported}")
    if failures:
        raise RuntimeError("Character imports produced no assets: " + ", ".join(failures))
    animation_tasks = [
        make_animation_task(spec, suffix)
        for spec in CHARACTERS
        for suffix in ANIMATION_SUFFIXES
    ]
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(animation_tasks)
    animation_failures = [
        task.destination_name for task in animation_tasks if not task.imported_object_paths
    ]
    if animation_failures:
        raise RuntimeError(
            "Character animation imports produced no assets: "
            + ", ".join(animation_failures)
        )
    for spec in CHARACTERS:
        create_animation_blueprint(spec)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)


if __name__ == "__main__":
    main()
