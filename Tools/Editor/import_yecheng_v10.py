import unreal


SOURCE_VRM = (
    r"G:\MyWorkSpace\AnimeNPCKit\Models\Custom"
    r"\YeCheng_NoanoaHair_RefinedFace_v10.vrm"
)
DESTINATION = "/Game/WindStation/Art/AnimeNPC/YeChengV10"


existing_assets = []
if unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
    existing_assets = unreal.EditorAssetLibrary.list_assets(
        DESTINATION, recursive=True, include_folder=False
    )
    unexpected_existing = [
        path
        for path in existing_assets
        if "VrmAssetList" not in path
    ]
    if unexpected_existing:
        raise RuntimeError(
            f"Refusing to overwrite complete V10 assets: {unexpected_existing}"
        )
else:
    unreal.EditorAssetLibrary.make_directory(DESTINATION)

task = unreal.AssetImportTask()
task.set_editor_property("filename", SOURCE_VRM)
task.set_editor_property("destination_path", DESTINATION)
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", bool(existing_assets))
task.set_editor_property("save", True)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
unreal.EditorAssetLibrary.save_directory(
    DESTINATION, only_if_is_dirty=False, recursive=True
)
imported = unreal.EditorAssetLibrary.list_assets(
    DESTINATION, recursive=True, include_folder=False
)
skeletal_meshes = []
for asset_path in imported:
    asset = unreal.load_asset(asset_path)
    if isinstance(asset, unreal.SkeletalMesh):
        skeletal_meshes.append(asset.get_path_name())

if len(skeletal_meshes) != 1:
    raise RuntimeError(
        f"Expected one imported skeletal mesh, got {skeletal_meshes}"
    )

unreal.log(
    "WS_YECHENG_V10_IMPORT_COMPLETE "
    f"mesh={skeletal_meshes[0]} imported_count={len(imported)}"
)
