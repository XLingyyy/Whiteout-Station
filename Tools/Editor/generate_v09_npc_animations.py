"""Generate v0.9 NPC performances on each character's exact skeleton.

The v0.8 clips inherited a legacy idle whose spine and chest were already bent
15-32 degrees and whose hands sat only 29-32 cm apart.  These clips use the
imported reference rotations for the torso/head, retain only the useful
same-skeleton limb setup, and open the relaxed arm silhouette enough for the
characters' wide sleeves to clear the body.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import unreal


FPS = 30
CHARACTERS = (
    {
        "name": "GuHeng",
        "mesh": "/Game/WindStation/Art/AnimeNPC/GuHeng/SK_Male_AvatarSample_C",
        "legacy_idle": (
            "/Game/WindStation/Art/AnimeNPC/GuHeng/Animations/AN_GuHeng_Idle"
        ),
        "root": "/Game/WindStation/Art/AnimeNPC/GuHeng/AnimationsV09",
        "token": "GuHeng",
    },
    {
        "name": "YeCheng",
        "mesh": (
            "/Game/WindStation/Art/AnimeNPC/YeChengV10/"
            "SK_YeCheng_NoanoaHair_RefinedFace_v10"
        ),
        "legacy_idle": (
            "/Game/WindStation/Art/AnimeNPC/YeChengV10/Animations/"
            "AN_YeCheng_V10_Idle"
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
L_SHOULDER = "J_Bip_L_Shoulder"
R_SHOULDER = "J_Bip_R_Shoulder"
L_UPPER_ARM = "J_Bip_L_UpperArm"
R_UPPER_ARM = "J_Bip_R_UpperArm"
L_LOWER_ARM = "J_Bip_L_LowerArm"
R_LOWER_ARM = "J_Bip_R_LowerArm"
L_HAND = "J_Bip_L_Hand"
R_HAND = "J_Bip_R_Hand"
L_UPPER_LEG = "J_Bip_L_UpperLeg"
R_UPPER_LEG = "J_Bip_R_UpperLeg"
L_LOWER_LEG = "J_Bip_L_LowerLeg"
R_LOWER_LEG = "J_Bip_R_LowerLeg"
L_FOOT = "J_Bip_L_Foot"
R_FOOT = "J_Bip_R_Foot"

LEGACY_BASE_BONES = {
    L_SHOULDER,
    R_SHOULDER,
    L_UPPER_ARM,
    R_UPPER_ARM,
    L_LOWER_ARM,
    R_LOWER_ARM,
    L_HAND,
    R_HAND,
    L_UPPER_LEG,
    R_UPPER_LEG,
    L_LOWER_LEG,
    R_LOWER_LEG,
    L_FOOT,
    R_FOOT,
}

# Rotator triples are (roll, pitch, yaw). These mirrored offsets move both
# forearms beside the torso while avoiding axial upper-arm twist.
NEUTRAL = {
    HIPS: (0.0, 0.0, 0.0),
    SPINE: (0.0, 0.0, 0.0),
    CHEST: (0.0, 0.0, 0.0),
    UPPER_CHEST: (0.0, 0.0, 0.0),
    NECK: (0.0, 0.0, 0.0),
    HEAD: (0.0, 0.0, 0.0),
    L_SHOULDER: (0.0, -7.0, 0.0),
    R_SHOULDER: (0.0, 7.0, 0.0),
    L_UPPER_ARM: (0.0, -7.0, -7.0),
    R_UPPER_ARM: (0.0, 7.0, 7.0),
    L_LOWER_ARM: (0.0, 5.0, 0.0),
    R_LOWER_ARM: (0.0, -5.0, 0.0),
    L_HAND: (0.0, 0.0, 0.0),
    R_HAND: (0.0, 0.0, 0.0),
}


@dataclass(frozen=True)
class Performance:
    duration: float
    evaluator: object


def wave(t: float, cycles: float = 1.0, phase: float = 0.0) -> float:
    return math.sin((t * cycles + phase) * math.tau)


def pulse(t: float, center: float, radius: float) -> float:
    distance = abs(t - center)
    return max(0.0, 1.0 - distance / radius) if radius > 0.0 else 0.0


def pose(
    **overrides: tuple[float, float, float],
) -> dict[str, tuple[float, float, float]]:
    result = dict(NEUTRAL)
    result.update(overrides)
    return result


def idle(t: float) -> dict[str, tuple[float, float, float]]:
    breath = wave(t)
    return pose(
        **{
            SPINE: (0.30 * breath, 0.0, 0.0),
            CHEST: (0.20 * breath, 0.0, 0.0),
            UPPER_CHEST: (0.25 * breath, 0.0, 0.0),
            HEAD: (-0.18 * breath, 0.0, 0.0),
        }
    )


def walk(t: float) -> dict[str, tuple[float, float, float]]:
    stride = wave(t)
    lift_left = max(0.0, stride)
    lift_right = max(0.0, -stride)
    result = pose(
        **{
            HIPS: (0.0, 0.0, 1.0 * wave(t, 2.0)),
            SPINE: (0.0, 0.0, -0.8 * stride),
            CHEST: (0.0, 0.0, 0.6 * stride),
            L_UPPER_ARM: (0.0, -7.0, -7.0 + 3.0 * stride),
            R_UPPER_ARM: (0.0, 7.0, 7.0 + 3.0 * stride),
            L_UPPER_LEG: (12.0 * stride, 0.0, 0.0),
            R_UPPER_LEG: (-12.0 * stride, 0.0, 0.0),
            L_LOWER_LEG: (-14.0 * lift_left, 0.0, 0.0),
            R_LOWER_LEG: (-14.0 * lift_right, 0.0, 0.0),
            L_FOOT: (6.0 * lift_left, 0.0, 0.0),
            R_FOOT: (6.0 * lift_right, 0.0, 0.0),
        }
    )
    return result


def acknowledge(t: float) -> dict[str, tuple[float, float, float]]:
    nod = pulse(t, 0.38, 0.25) - 0.55 * pulse(t, 0.68, 0.20)
    return pose(
        **{
            HEAD: (5.0 * nod, 0.0, 0.0),
            NECK: (1.5 * nod, 0.0, 0.0),
            UPPER_CHEST: (0.7 * nod, 0.0, 0.0),
        }
    )


def consider(t: float) -> dict[str, tuple[float, float, float]]:
    amount = pulse(t, 0.52, 0.38)
    return pose(
        **{
            HEAD: (1.5 * amount, 0.0, 5.0 * amount),
            NECK: (0.0, 0.0, 1.5 * amount),
            UPPER_CHEST: (0.0, 0.0, -0.8 * amount),
            R_SHOULDER: (0.0, 5.5 * amount + 7.0, 0.0),
            R_UPPER_ARM: (0.0, 7.0 + 2.5 * amount, 7.0),
        }
    )


def reassure(t: float) -> dict[str, tuple[float, float, float]]:
    amount = pulse(t, 0.50, 0.42)
    return pose(
        **{
            HEAD: (-1.5 * amount, 0.0, 0.0),
            UPPER_CHEST: (-0.8 * amount, 0.0, 0.0),
            L_SHOULDER: (0.0, -7.0 - 3.0 * amount, 0.0),
            R_SHOULDER: (0.0, 7.0 + 3.0 * amount, 0.0),
            L_UPPER_ARM: (0.0, -7.0 - 3.0 * amount, -7.0),
            R_UPPER_ARM: (0.0, 7.0 + 3.0 * amount, 7.0),
        }
    )


def reject(t: float) -> dict[str, tuple[float, float, float]]:
    shake = wave(t, 2.0) * math.sin(math.pi * t)
    brace = pulse(t, 0.5, 0.45)
    return pose(
        **{
            HEAD: (0.0, 0.0, 5.5 * shake),
            NECK: (0.0, 0.0, 1.5 * shake),
            L_UPPER_ARM: (0.0, -7.0 - 1.5 * brace, -7.0),
            R_UPPER_ARM: (0.0, 7.0 + 1.5 * brace, 7.0),
        }
    )


def alarmed(t: float) -> dict[str, tuple[float, float, float]]:
    startle = pulse(t, 0.30, 0.22)
    settle = pulse(t, 0.62, 0.32)
    amount = max(startle, 0.55 * settle)
    return pose(
        **{
            HEAD: (-3.0 * amount, 0.0, 0.0),
            NECK: (-1.5 * amount, 0.0, 0.0),
            UPPER_CHEST: (-1.5 * amount, 0.0, 0.0),
            L_SHOULDER: (0.0, -7.0 + 2.0 * amount, 0.0),
            R_SHOULDER: (0.0, 7.0 - 2.0 * amount, 0.0),
            L_UPPER_ARM: (0.0, -7.0 - 2.0 * amount, -7.0),
            R_UPPER_ARM: (0.0, 7.0 + 2.0 * amount, 7.0),
        }
    )


PERFORMANCES = {
    "Idle": Performance(2.4, idle),
    "Walk": Performance(1.0, walk),
    "Acknowledge": Performance(1.35, acknowledge),
    "Consider": Performance(1.65, consider),
    "Reassure": Performance(1.55, reassure),
    "Reject": Performance(1.25, reject),
    "Alarmed": Performance(1.15, alarmed),
}


def require_asset(path: str):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError(f"Required asset is missing: {path}")
    return asset


def euler_quaternion(euler: tuple[float, float, float]) -> unreal.Quat:
    return unreal.Rotator(
        roll=euler[0],
        pitch=euler[1],
        yaw=euler[2],
    ).quaternion()


def author_animation(
    character: dict[str, str],
    mesh,
    skeleton,
    reference_pose,
    legacy_idle,
    suffix: str,
    performance: Performance,
) -> None:
    asset_name = f"AN_{character['token']}_{suffix}"
    asset_path = f"{character['root']}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        if not unreal.EditorAssetLibrary.delete_asset(asset_path):
            raise RuntimeError(f"Unable to replace generated animation: {asset_path}")

    factory = unreal.AnimSequenceFactory()
    factory.set_editor_property("target_skeleton", skeleton)
    factory.set_editor_property("preview_skeletal_mesh", mesh)
    animation = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        character["root"],
        unreal.AnimSequence,
        factory,
    )
    if not animation:
        raise RuntimeError(f"Unable to create animation: {asset_path}")

    frame_count = max(1, round(performance.duration * FPS))
    samples = [
        performance.evaluator(index / frame_count)
        for index in range(frame_count + 1)
    ]
    bone_names = sorted({bone for sample in samples for bone in sample})
    controller = animation.controller
    controller.open_bracket(f"Generate {asset_name}", False)
    try:
        controller.set_frame_rate(
            unreal.FrameRate(numerator=FPS, denominator=1),
            False,
        )
        controller.set_number_of_frames(
            unreal.FrameNumber(value=frame_count),
            False,
        )
        for bone_name in bone_names:
            reference = reference_pose.get_ref_bone_pose(
                bone_name,
                unreal.AnimPoseSpaces.LOCAL,
            )
            legacy = unreal.AnimationLibrary.get_bone_pose_for_time(
                legacy_idle,
                bone_name,
                0.0,
                False,
            )
            base_rotation = (
                legacy.rotation
                if bone_name in LEGACY_BASE_BONES
                else reference.rotation
            )
            controller.add_bone_track(bone_name, False)
            translations = [
                unreal.Vector(
                    reference.translation.x,
                    reference.translation.y,
                    reference.translation.z,
                )
                for _sample in samples
            ]
            rotations = [
                base_rotation
                * euler_quaternion(sample.get(bone_name, (0.0, 0.0, 0.0)))
                for sample in samples
            ]
            scales = [
                unreal.Vector(
                    reference.scale3d.x,
                    reference.scale3d.y,
                    reference.scale3d.z,
                )
                for _sample in samples
            ]
            if not controller.set_bone_track_keys(
                bone_name,
                translations,
                rotations,
                scales,
                False,
            ):
                raise RuntimeError(
                    f"Unable to set {bone_name} keys on {asset_path}"
                )
    finally:
        controller.close_bracket(False)

    animation.set_editor_property("rate_scale", 1.0)
    if not unreal.EditorAssetLibrary.save_asset(
        asset_path,
        only_if_is_dirty=False,
    ):
        raise RuntimeError(f"Unable to save generated animation: {asset_path}")
    unreal.log(
        f"WS_V09_ANIMATION_GENERATED character={character['name']} "
        f"animation={suffix} frames={frame_count} tracks={len(bone_names)}"
    )


def main() -> None:
    for character in CHARACTERS:
        mesh = require_asset(character["mesh"])
        skeleton = mesh.get_editor_property("skeleton")
        if skeleton is None:
            raise RuntimeError(f"Missing skeleton for {character['name']}")
        legacy_idle = require_asset(character["legacy_idle"])
        if legacy_idle.get_editor_property("skeleton") != skeleton:
            raise RuntimeError(
                f"Legacy limb source uses the wrong skeleton for "
                f"{character['name']}"
            )
        reference_pose = skeleton.get_reference_pose()
        for suffix, performance in PERFORMANCES.items():
            author_animation(
                character,
                mesh,
                skeleton,
                reference_pose,
                legacy_idle,
                suffix,
                performance,
            )
        unreal.EditorAssetLibrary.save_directory(
            character["root"],
            only_if_is_dirty=False,
            recursive=True,
        )
    unreal.log(
        f"WS_V09_ANIMATION_GENERATION_COMPLETE assets="
        f"{len(CHARACTERS) * len(PERFORMANCES)}"
    )


if __name__ == "__main__":
    main()
