import unreal


CHARACTERS = (
    {
        "name": "GuHeng",
        "source_mesh": "/Game/WindStation/Art/Characters/Engineer/SK_WS_Engineer.SK_WS_Engineer",
        "source_animations": (
            "/Game/WindStation/Art/Characters/Engineer/AN_WS_Engineer_Idle.AN_WS_Engineer_Idle",
            "/Game/WindStation/Art/Characters/Engineer/AN_WS_Engineer_Gesture.AN_WS_Engineer_Gesture",
            "/Game/WindStation/Art/Characters/Engineer/AN_WS_Engineer_Guarded.AN_WS_Engineer_Guarded",
            "/Game/WindStation/Art/Characters/Engineer/AN_WS_Engineer_Work.AN_WS_Engineer_Work",
        ),
        "target_mesh": "/Game/WindStation/Art/AnimeNPC/GuHeng/SK_Male_AvatarSample_C.SK_Male_AvatarSample_C",
        "retargeter": "/Game/WindStation/Art/AnimeNPC/GuHeng/RTG_UE4_Male_AvatarSample_C.RTG_UE4_Male_AvatarSample_C",
        "search": "AN_WS_Engineer",
        "replace": "AN_GuHeng",
        "target_path": "/Game/WindStation/Art/AnimeNPC/GuHeng/Animations",
    },
    {
        "name": "YeCheng",
        "source_mesh": "/Game/WindStation/Art/Characters/Doctor/SK_WS_Doctor.SK_WS_Doctor",
        "source_animations": (
            "/Game/WindStation/Art/Characters/Doctor/AN_WS_Doctor_Idle.AN_WS_Doctor_Idle",
            "/Game/WindStation/Art/Characters/Doctor/AN_WS_Doctor_Gesture.AN_WS_Doctor_Gesture",
            "/Game/WindStation/Art/Characters/Doctor/AN_WS_Doctor_Guarded.AN_WS_Doctor_Guarded",
            "/Game/WindStation/Art/Characters/Doctor/AN_WS_Doctor_Work.AN_WS_Doctor_Work",
        ),
        "target_mesh": "/Game/WindStation/Art/AnimeNPC/YeChengV10/SK_YeCheng_NoanoaHair_RefinedFace_v10.SK_YeCheng_NoanoaHair_RefinedFace_v10",
        "retargeter": "/Game/WindStation/Art/AnimeNPC/YeChengV10/RTG_UE4_YeCheng_NoanoaHair_RefinedFace_v10.RTG_UE4_YeCheng_NoanoaHair_RefinedFace_v10",
        "search": "AN_WS_Doctor",
        "replace": "AN_YeCheng_V10",
        "target_path": "/Game/WindStation/Art/AnimeNPC/YeChengV10/Animations",
    },
)


def load_required(path: str):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError(f"Required asset is missing: {path}")
    return asset


for character in CHARACTERS:
    inputs = unreal.IKRetargetBatchOperationInputs()
    source_assets = [
        unreal.EditorAssetLibrary.find_asset_data(path)
        for path in character["source_animations"]
    ]
    if any(not data.is_valid() for data in source_assets):
        raise RuntimeError(f"Animation asset data is missing for {character['name']}")

    inputs.set_editor_property("assets_to_retarget", source_assets)
    inputs.set_editor_property("source_mesh", load_required(character["source_mesh"]))
    inputs.set_editor_property("target_mesh", load_required(character["target_mesh"]))
    inputs.set_editor_property("ik_retarget_asset", load_required(character["retargeter"]))
    inputs.set_editor_property("search", character["search"])
    inputs.set_editor_property("replace", character["replace"])
    inputs.set_editor_property("target_path", character["target_path"])
    inputs.set_editor_property("include_referenced_assets", False)
    inputs.set_editor_property("overwrite_existing_files", True)

    results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    unreal.log(
        f"WS_ANIME_NPC_RETARGET character={character['name']} "
        f"assets={[result.package_name for result in results]}"
    )
    if len(results) != len(source_assets):
        raise RuntimeError(
            f"Expected {len(source_assets)} retargeted animations for {character['name']}, got {len(results)}"
        )

unreal.EditorAssetLibrary.save_directory(
    "/Game/WindStation/Art/AnimeNPC",
    only_if_is_dirty=False,
    recursive=True,
)
unreal.log("WS_ANIME_NPC_RETARGET_COMPLETE")
