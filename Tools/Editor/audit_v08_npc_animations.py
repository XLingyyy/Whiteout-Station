"""Fail-closed audit for the exact-skeleton v0.8 NPC animation set."""

from __future__ import annotations

import math

import unreal


EXPECTED = {
    "Idle": 2.4,
    "Walk": 1.0,
    "Acknowledge": 40 / 30,
    "Consider": 50 / 30,
    "Reassure": 46 / 30,
    "Reject": 38 / 30,
    "Alarmed": 34 / 30,
}
CHARACTERS = (
    {
        "name": "GuHeng",
        "mesh": "/Game/WindStation/Art/AnimeNPC/GuHeng/SK_Male_AvatarSample_C",
        "root": "/Game/WindStation/Art/AnimeNPC/GuHeng/AnimationsV08",
        "token": "GuHeng",
    },
    {
        "name": "YeCheng",
        "mesh": "/Game/WindStation/Art/AnimeNPC/YeChengV10/SK_YeCheng_NoanoaHair_RefinedFace_v10",
        "root": "/Game/WindStation/Art/AnimeNPC/YeChengV10/AnimationsV08",
        "token": "YeCheng_V10",
    },
)
ALLOWED_TRACKS = {
    "J_Bip_C_Hips",
    "J_Bip_C_Spine",
    "J_Bip_C_Chest",
    "J_Bip_C_UpperChest",
    "J_Bip_C_Neck",
    "J_Bip_C_Head",
    "J_Bip_L_Shoulder",
    "J_Bip_R_Shoulder",
    "J_Bip_L_UpperArm",
    "J_Bip_R_UpperArm",
    "J_Bip_L_LowerArm",
    "J_Bip_R_LowerArm",
    "J_Bip_L_Hand",
    "J_Bip_R_Hand",
    "J_Bip_L_UpperLeg",
    "J_Bip_R_UpperLeg",
    "J_Bip_L_LowerLeg",
    "J_Bip_R_LowerLeg",
    "J_Bip_L_Foot",
    "J_Bip_R_Foot",
}
ALLOWED_TRACKS = {name.lower() for name in ALLOWED_TRACKS}


def finite(values) -> bool:
    return all(math.isfinite(value) for value in values)


def quaternion_dot(left, right) -> float:
    return abs(
        left.x * right.x
        + left.y * right.y
        + left.z * right.z
        + left.w * right.w
    )


def main() -> None:
    failures: list[str] = []
    audited = 0
    for character in CHARACTERS:
        mesh = unreal.load_asset(character["mesh"])
        if not mesh:
            failures.append(f"{character['name']}: target mesh missing")
            continue
        skeleton = mesh.get_editor_property("skeleton")
        reference_pose = skeleton.get_reference_pose()
        for suffix, expected_duration in EXPECTED.items():
            asset_path = (
                f"{character['root']}/AN_{character['token']}_{suffix}"
            )
            animation = unreal.load_asset(asset_path)
            label = f"{character['name']}:{suffix}"
            if not isinstance(animation, unreal.AnimSequence):
                failures.append(f"{label}: missing AnimSequence")
                continue
            if animation.get_editor_property("skeleton") != skeleton:
                failures.append(f"{label}: wrong skeleton")
            duration = animation.get_play_length()
            if abs(duration - expected_duration) > 0.04:
                failures.append(
                    f"{label}: duration {duration:.3f}, expected "
                    f"{expected_duration:.3f}"
                )
            tracks = {
                str(name)
                for name in unreal.AnimationLibrary.get_animation_track_names(
                    animation
                )
            }
            unexpected = sorted(tracks - ALLOWED_TRACKS)
            if unexpected:
                failures.append(f"{label}: unexpected tracks {unexpected}")
            if not tracks:
                failures.append(f"{label}: contains no tracks")
            for bone_name in sorted(tracks):
                reference = reference_pose.get_ref_bone_pose(
                    bone_name,
                    unreal.AnimPoseSpaces.LOCAL,
                )
                start = unreal.AnimationLibrary.get_bone_pose_for_time(
                    animation,
                    bone_name,
                    0.0,
                    False,
                )
                end = unreal.AnimationLibrary.get_bone_pose_for_time(
                    animation,
                    bone_name,
                    duration,
                    False,
                )
                for sample_name, transform in (("start", start), ("end", end)):
                    translation = transform.translation
                    scale = transform.scale3d
                    rotation = transform.rotation
                    if not finite(
                        (
                            translation.x,
                            translation.y,
                            translation.z,
                            scale.x,
                            scale.y,
                            scale.z,
                            rotation.x,
                            rotation.y,
                            rotation.z,
                            rotation.w,
                        )
                    ):
                        failures.append(
                            f"{label}:{bone_name}:{sample_name}: non-finite key"
                        )
                    if (translation - reference.translation).length() > 0.02:
                        failures.append(
                            f"{label}:{bone_name}:{sample_name}: translation drift"
                        )
                    if (scale - reference.scale3d).length() > 0.002:
                        failures.append(
                            f"{label}:{bone_name}:{sample_name}: scale drift"
                        )
                if suffix in ("Idle", "Walk"):
                    if quaternion_dot(start.rotation, end.rotation) < 0.999:
                        failures.append(
                            f"{label}:{bone_name}: loop seam does not close"
                        )
            audited += 1
            unreal.log(
                f"WS_V08_ANIMATION_AUDIT character={character['name']} "
                f"animation={suffix} duration={duration:.3f} "
                f"tracks={len(tracks)}"
            )

    expected_assets = len(CHARACTERS) * len(EXPECTED)
    if audited != expected_assets:
        failures.append(
            f"asset count {audited}, expected {expected_assets}"
        )
    if failures:
        for failure in failures:
            unreal.log_error(f"WS_V08_ANIMATION_AUDIT_ERROR {failure}")
        raise RuntimeError(
            f"v0.8 NPC animation audit failed: {len(failures)}"
        )
    unreal.log(
        f"WS_V08_ANIMATION_AUDIT_COMPLETE result=PASS assets={audited}"
    )


if __name__ == "__main__":
    main()
