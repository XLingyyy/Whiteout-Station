import unreal


IMPORTS = (
    (
        r"G:\MyWorkSpace\AnimeNPCKit\Models\ReadyPair_Recommended\Male_AvatarSample_C.vrm",
        "/Game/WindStation/Art/AnimeNPC/GuHeng",
    ),
    (
        r"G:\MyWorkSpace\AnimeNPCKit\Models\Custom\YeCheng_NoanoaHair_RefinedFace_v10.vrm",
        "/Game/WindStation/Art/AnimeNPC/YeChengV10",
    ),
)


def import_vrm(filename: str, destination_path: str) -> None:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", destination_path)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = list(task.get_editor_property("imported_object_paths"))
    unreal.log(f"WS_ANIME_NPC_IMPORT destination={destination_path} imported={imported}")
    if not imported:
        raise RuntimeError(f"VRM import produced no assets for {filename}")


for source_file, asset_path in IMPORTS:
    import_vrm(source_file, asset_path)

unreal.EditorAssetLibrary.save_directory("/Game/WindStation/Art/AnimeNPC", only_if_is_dirty=False, recursive=True)
unreal.log("WS_ANIME_NPC_IMPORT_COMPLETE")
