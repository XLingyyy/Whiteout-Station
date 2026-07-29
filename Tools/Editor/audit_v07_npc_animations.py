"""Fail closed when any v0.7 NPC performance animation is missing or misbound."""

from __future__ import annotations

import unreal


ANIMATION_SUFFIXES = ("Walk", "Acknowledge", "Consider", "Reassure", "Reject", "Alarmed")
CHARACTERS = (
    {
        "name": "GuHeng",
        "mesh": "/Game/WindStation/Art/AnimeNPC/GuHeng/SK_Male_AvatarSample_C",
        "animation_root": "/Game/WindStation/Art/AnimeNPC/GuHeng/AnimationsV07",
        "token": "GuHeng",
    },
    {
        "name": "YeCheng",
        "mesh": "/Game/WindStation/Art/AnimeNPC/YeChengV10/SK_YeCheng_NoanoaHair_RefinedFace_v10",
        "animation_root": "/Game/WindStation/Art/AnimeNPC/YeChengV10/AnimationsV07",
        "token": "YeCheng_V10",
    },
)


def main() -> None:
    failures: list[str] = []
    audited = 0
    for character in CHARACTERS:
        mesh = unreal.load_asset(character["mesh"])
        if not mesh:
            failures.append(f"{character['name']}: target mesh missing")
            continue
        target_skeleton = mesh.get_editor_property("skeleton")
        for suffix in ANIMATION_SUFFIXES:
            path = (
                f"{character['animation_root']}/"
                f"AN_{character['token']}_{suffix}"
            )
            animation = unreal.load_asset(path)
            if not animation:
                failures.append(f"{character['name']}:{suffix}: asset missing")
                continue
            if not isinstance(animation, unreal.AnimSequence):
                failures.append(f"{character['name']}:{suffix}: not AnimSequence")
                continue
            animation_skeleton = animation.get_editor_property("skeleton")
            if animation_skeleton != target_skeleton:
                failures.append(f"{character['name']}:{suffix}: wrong skeleton")
            play_length = animation.get_play_length()
            if play_length < 0.5 or play_length > 3.0:
                failures.append(
                    f"{character['name']}:{suffix}: invalid duration {play_length:.3f}"
                )
            audited += 1
            unreal.log(
                f"WS_V07_ANIMATION_AUDIT character={character['name']} "
                f"animation={suffix} duration={play_length:.3f}"
            )

    if failures:
        for failure in failures:
            unreal.log_error(f"WS_V07_ANIMATION_AUDIT_ERROR {failure}")
        raise RuntimeError(f"v0.7 NPC animation audit failed: {len(failures)}")
    if audited != 12:
        raise RuntimeError(f"v0.7 NPC animation audit expected 12 assets, got {audited}")
    unreal.log("WS_V07_ANIMATION_AUDIT_COMPLETE result=PASS assets=12")


if __name__ == "__main__":
    main()
