"""Retarget the six v0.7 performance animations to the user-provided NPC meshes."""

from __future__ import annotations

import unreal


ANIMATION_SUFFIXES = ("Walk", "Acknowledge", "Consider", "Reassure", "Reject", "Alarmed")
CHARACTERS = (
    {
        "name": "GuHeng",
        "source_mesh": "/Game/WindStation/Art/Characters/Engineer/SK_WS_Engineer.SK_WS_Engineer",
        "source_root": "/Game/WindStation/Art/Characters/Engineer/AnimationsV07",
        "source_token": "WS_Engineer",
        "target_mesh": "/Game/WindStation/Art/AnimeNPC/GuHeng/SK_Male_AvatarSample_C.SK_Male_AvatarSample_C",
        "retargeter": "/Game/WindStation/Art/AnimeNPC/GuHeng/RTG_UE4_Male_AvatarSample_C.RTG_UE4_Male_AvatarSample_C",
        "search": "AN_WS_Engineer",
        "replace": "AN_GuHeng",
        "target_path": "/Game/WindStation/Art/AnimeNPC/GuHeng/AnimationsV07",
    },
    {
        "name": "YeCheng",
        "source_mesh": "/Game/WindStation/Art/Characters/Doctor/SK_WS_Doctor.SK_WS_Doctor",
        "source_root": "/Game/WindStation/Art/Characters/Doctor/AnimationsV07",
        "source_token": "WS_Doctor",
        "target_mesh": "/Game/WindStation/Art/AnimeNPC/YeChengV10/SK_YeCheng_NoanoaHair_RefinedFace_v10.SK_YeCheng_NoanoaHair_RefinedFace_v10",
        "retargeter": "/Game/WindStation/Art/AnimeNPC/YeChengV10/RTG_UE4_YeCheng_NoanoaHair_RefinedFace_v10.RTG_UE4_YeCheng_NoanoaHair_RefinedFace_v10",
        "search": "AN_WS_Doctor",
        "replace": "AN_YeCheng_V10",
        "target_path": "/Game/WindStation/Art/AnimeNPC/YeChengV10/AnimationsV07",
    },
)


def load_required(path: str):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError(f"Required asset is missing: {path}")
    return asset


def main() -> None:
    for character in CHARACTERS:
        source_paths = [
            f"{character['source_root']}/AN_{character['source_token']}_{suffix}"
            f".AN_{character['source_token']}_{suffix}"
            for suffix in ANIMATION_SUFFIXES
        ]
        source_assets = [
            unreal.EditorAssetLibrary.find_asset_data(path)
            for path in source_paths
        ]
        if any(not data.is_valid() for data in source_assets):
            raise RuntimeError(f"Animation asset data is missing for {character['name']}")

        inputs = unreal.IKRetargetBatchOperationInputs()
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
            f"WS_V07_ANIMATION_RETARGET character={character['name']} "
            f"assets={[result.package_name for result in results]}"
        )
        if len(results) != len(source_assets):
            raise RuntimeError(
                f"Expected {len(source_assets)} retargeted animations for "
                f"{character['name']}, got {len(results)}"
            )

        unreal.EditorAssetLibrary.save_directory(
            character["target_path"],
            only_if_is_dirty=True,
            recursive=True,
        )
    unreal.log("WS_V07_ANIMATION_RETARGET_COMPLETE assets=12")


if __name__ == "__main__":
    main()
