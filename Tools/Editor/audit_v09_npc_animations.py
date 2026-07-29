"""Fail-closed structural and pose-safety audit for v0.9 NPC animations."""

from __future__ import annotations

import json
import math
from pathlib import Path

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
        "root": "/Game/WindStation/Art/AnimeNPC/GuHeng/AnimationsV09",
        "token": "GuHeng",
    },
    {
        "name": "YeCheng",
        "mesh": (
            "/Game/WindStation/Art/AnimeNPC/YeChengV10/"
            "SK_YeCheng_NoanoaHair_RefinedFace_v10"
        ),
        "root": (
            "/Game/WindStation/Art/AnimeNPC/YeChengV10/AnimationsV09"
        ),
        "token": "YeCheng_V10",
    },
)

HIPS = "J_Bip_C_Hips"
SPINE = "J_Bip_C_Spine"
CHEST = "J_Bip_C_Chest"
UPPER_CHEST = "J_Bip_C_UpperChest"
NECK = "J_Bip_C_Neck"
HEAD = "J_Bip_C_Head"
L_HAND = "J_Bip_L_Hand"
R_HAND = "J_Bip_R_Hand"
ALLOWED_TRACKS = {
    HIPS,
    SPINE,
    CHEST,
    UPPER_CHEST,
    NECK,
    HEAD,
    "J_Bip_L_Shoulder",
    "J_Bip_R_Shoulder",
    "J_Bip_L_UpperArm",
    "J_Bip_R_UpperArm",
    "J_Bip_L_LowerArm",
    "J_Bip_R_LowerArm",
    L_HAND,
    R_HAND,
    "J_Bip_L_UpperLeg",
    "J_Bip_R_UpperLeg",
    "J_Bip_L_LowerLeg",
    "J_Bip_R_LowerLeg",
    "J_Bip_L_Foot",
    "J_Bip_R_Foot",
}
ALLOWED_TRACKS = {name.lower() for name in ALLOWED_TRACKS}
TORSO_LIMITS = {
    HIPS: 2.1,
    SPINE: 2.1,
    CHEST: 2.1,
    UPPER_CHEST: 2.1,
    NECK: 4.0,
    HEAD: 8.0,
}
SAMPLE_COUNT = 17


def finite(values) -> bool:
    return all(math.isfinite(value) for value in values)


def quaternion_dot(left, right) -> float:
    return abs(
        left.x * right.x
        + left.y * right.y
        + left.z * right.z
        + left.w * right.w
    )


def angular_distance(left, right) -> float:
    return math.degrees(
        2.0
        * math.acos(max(-1.0, min(1.0, quaternion_dot(left, right))))
    )


def evaluated_pose(animation, mesh, time: float):
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("optional_skeletal_mesh", mesh)
    options.set_editor_property("should_retarget", False)
    options.set_editor_property("retrieve_additive_as_full_pose", True)
    return unreal.AnimPoseExtensions.get_anim_pose_at_time(
        animation,
        time,
        options,
    )


def world_location(pose, bone_name: str) -> unreal.Vector:
    return unreal.AnimPoseExtensions.get_bone_pose(
        pose,
        bone_name,
        unreal.AnimPoseSpaces.WORLD,
    ).translation


def main() -> None:
    failures: list[str] = []
    audited = 0
    records: list[dict[str, object]] = []
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
                            f"{label}:{bone_name}:{sample_name}: "
                            f"translation drift"
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

            minimum_hand_separation = float("inf")
            minimum_lateral_separation = float("inf")
            maximum_torso_deviation = {
                bone_name: 0.0 for bone_name in TORSO_LIMITS
            }
            for sample_index in range(SAMPLE_COUNT):
                sample_time = duration * sample_index / (SAMPLE_COUNT - 1)
                pose = evaluated_pose(animation, mesh, sample_time)
                left_hand = world_location(pose, L_HAND)
                right_hand = world_location(pose, R_HAND)
                hand_separation = (right_hand - left_hand).length()
                lateral_separation = right_hand.x - left_hand.x
                minimum_hand_separation = min(
                    minimum_hand_separation,
                    hand_separation,
                )
                minimum_lateral_separation = min(
                    minimum_lateral_separation,
                    lateral_separation,
                )
                if hand_separation < 44.0 or lateral_separation < 43.0:
                    failures.append(
                        f"{label}:sample={sample_index}: unsafe hand "
                        f"separation total={hand_separation:.2f} "
                        f"lateral={lateral_separation:.2f}"
                    )
                    break
                if left_hand.x >= -18.0 or right_hand.x <= 18.0:
                    failures.append(
                        f"{label}:sample={sample_index}: hand entered torso "
                        f"corridor left={left_hand.x:.2f} "
                        f"right={right_hand.x:.2f}"
                    )
                    break
                for bone_name, limit in TORSO_LIMITS.items():
                    reference = reference_pose.get_ref_bone_pose(
                        bone_name,
                        unreal.AnimPoseSpaces.LOCAL,
                    )
                    current = unreal.AnimationLibrary.get_bone_pose_for_time(
                        animation,
                        bone_name,
                        sample_time,
                        False,
                    )
                    deviation = angular_distance(
                        current.rotation,
                        reference.rotation,
                    )
                    maximum_torso_deviation[bone_name] = max(
                        maximum_torso_deviation[bone_name],
                        deviation,
                    )
                    if deviation > limit:
                        failures.append(
                            f"{label}:sample={sample_index}:{bone_name}: "
                            f"deviation={deviation:.2f} limit={limit:.2f}"
                        )
                        break

            audited += 1
            records.append(
                {
                    "character": character["name"],
                    "animation": suffix,
                    "duration_seconds": duration,
                    "track_count": len(tracks),
                    "minimum_hand_separation_cm": minimum_hand_separation,
                    "minimum_lateral_hand_separation_cm": (
                        minimum_lateral_separation
                    ),
                    "maximum_torso_deviation_degrees": (
                        maximum_torso_deviation
                    ),
                }
            )
            unreal.log(
                f"WS_V09_ANIMATION_AUDIT character={character['name']} "
                f"animation={suffix} min_hands="
                f"{minimum_hand_separation:.2f} min_lateral="
                f"{minimum_lateral_separation:.2f}"
            )

    expected_assets = len(CHARACTERS) * len(EXPECTED)
    if audited != expected_assets:
        failures.append(
            f"asset count {audited}, expected {expected_assets}"
        )
    report = {
        "schema": "whiteout.v0.9.npc-animation-audit.v1",
        "engine_version": unreal.SystemLibrary.get_engine_version(),
        "passed": not failures,
        "audited_assets": audited,
        "expected_assets": expected_assets,
        "failures": failures,
        "records": records,
    }
    output = (
        Path(
            unreal.Paths.convert_relative_path_to_full(
                unreal.Paths.project_dir()
            )
        ).parent
        / "docs"
        / "evidence_v0.9"
        / "npc_animation_audit.json"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(report, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    if failures:
        for failure in failures:
            unreal.log_error(f"WS_V09_ANIMATION_AUDIT_ERROR {failure}")
        raise RuntimeError(
            f"v0.9 NPC animation audit failed: {len(failures)}"
        )
    unreal.log(
        f"WS_V09_ANIMATION_AUDIT_COMPLETE result=PASS "
        f"assets={audited} report={output}"
    )


if __name__ == "__main__":
    main()
