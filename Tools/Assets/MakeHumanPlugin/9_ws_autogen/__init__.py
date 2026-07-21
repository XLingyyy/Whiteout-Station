"""One-shot MakeHuman exporter used by the Whiteout Station asset pipeline.

Copy this folder into MakeHuman's plugins directory, set the WS_MH_* environment
variables documented in Tools/Assets/generate_makehuman_characters.ps1, then
launch MakeHuman.  The plugin configures one audited character, exports a binary
FBX with the game-engine rig, and exits the application.
"""

from __future__ import annotations

import os
import inspect
import importlib
import math
import traceback

import animation
import gui3d
import log
import material
import mh
import numpy as np
import skeleton
import transformations as tm
from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QApplication


def _system_data(*parts: str) -> str:
    return mh.getSysDataPath(os.path.join(*parts))


def _external_clothes(root: str, name: str) -> str:
    return os.path.join(root, "clothes", name, name + ".mhclo")


def _assign_game_rig(human) -> None:
    rig_path = _system_data("rigs", "game_engine.mhskel")
    reference_rig = human.getBaseSkeleton()
    rig = skeleton.load(rig_path, human.meshData)
    rig.autoBuildWeightReferences(reference_rig)
    rig.getVertexWeights(reference_rig.getVertexWeights(), force_remap=False)
    rig.addReferencePlanes(reference_rig)
    human.setSkeleton(rig)


def _install_ascii_animation_export_patch() -> None:
    """Enable the dormant ASCII FBX animation writer in MakeHuman 1.3.

    The upstream exporter already contains the complete animation writer but
    hard-codes ``useAnim = False``.  This narrowly scoped runtime patch keeps
    the installed application untouched and supplies the action dictionary it
    expects from the currently active AnimationTrack.
    """
    module = importlib.import_module("9_export_fbx.mh2fbx")
    if getattr(module, "_whiteout_animation_patch", False):
        return

    def build_action(human, rig):
        track = human.getActiveAnimation()
        if track is None or track.nFrames < 2:
            return None
        frames = [track.getAtFramePos(frame, noBake=True) for frame in range(track.nFrames)]
        result = {}
        for bone_index, bone in enumerate(rig.getBones()):
            result[bone.name] = [
                tm.quaternion_from_matrix(frame[bone_index]) for frame in frames
            ]
        return result

    source = inspect.getsource(module.exportFbx)
    original = """    useAnim = False
    if useAnim:
        # TODO allow exporting poseunits
        action = None
    else:
        action = None
"""
    replacement = """    action = _whiteout_build_action(human, skel)
    useAnim = action is not None
"""
    if original not in source:
        raise RuntimeError("MakeHuman 1.3 FBX animation patch point changed")
    namespace = {}
    module._whiteout_build_action = build_action
    source = source.replace(original, replacement)
    # The dormant code path also contains an obsolete duplicate argument.
    source = source.replace(
        "fbx_anim.writeLinks(fp, action, config, config)",
        "fbx_anim.writeLinks(fp, action, config)",
    )
    exec(source, module.__dict__, namespace)
    module.exportFbx = namespace["exportFbx"]
    takes_source = inspect.getsource(module.fbx_anim.writeTakes).replace(
        "action.values()[0]", "next(iter(action.values()))"
    )
    takes_namespace = {}
    exec(takes_source, module.fbx_anim.__dict__, takes_namespace)
    module.fbx_anim.writeTakes = takes_namespace["writeTakes"]
    # FBX time ticks at 46,186,158,000 Hz.  The upstream dormant value is
    # slightly off-grid; use an exact 30 fps step so Interchange accepts it.
    module.fbx_anim.TimeStep = 1_539_538_600
    module._whiteout_animation_patch = True


def _euler_matrix(x_degrees: float, y_degrees: float, z_degrees: float):
    return tm.euler_matrix(
        math.radians(x_degrees),
        math.radians(y_degrees),
        math.radians(z_degrees),
        axes="sxyz",
    )[:3, :4]


def _animation_track(rig, name: str, style: str) -> animation.AnimationTrack:
    """Build a short, looping local-space performance for the game-engine rig."""
    bones = rig.getBones()
    bone_indices = {bone.name: index for index, bone in enumerate(bones)}
    frame_count = 48
    frame_rate = 24.0
    data = np.zeros((frame_count * len(bones), 3, 4), dtype=np.float32)
    identity = np.identity(4, dtype=np.float32)[:3, :4]
    data[:] = identity

    for frame in range(frame_count):
        phase = (frame / frame_count) * math.tau
        pose = {
            # The game-engine rig's local Z axis lowers the arms from its T-pose.
            # Rotating local X instead pitches the hands toward the camera.
            "upperarm_l": (0.0, 0.0, -78.0),
            "upperarm_r": (0.0, 0.0, 78.0),
            "lowerarm_l": (0.0, 0.0, 0.0),
            "lowerarm_r": (0.0, 0.0, 0.0),
            "spine_02": (1.2 * math.sin(phase), 0.8 * math.sin(phase), 0.0),
            "neck_01": (-0.8 * math.sin(phase), 0.0, 0.0),
        }
        if style == "gesture":
            wave = 0.5 - 0.5 * math.cos(phase)
            pose.update(
                {
                    "upperarm_l": (-70.0 + 42.0 * wave, 8.0, 8.0 * wave),
                    "upperarm_r": (70.0, 0.0, 0.0),
                    "lowerarm_l": (-72.0 * wave, 0.0, 20.0),
                    "hand_l": (0.0, 18.0 * math.sin(phase), 0.0),
                    "spine_03": (0.0, 5.0 * math.sin(phase), 0.0),
                }
            )
        elif style == "guarded":
            pulse = 2.0 * math.sin(phase)
            pose.update(
                {
                    "upperarm_l": (-48.0, 10.0, 10.0 + pulse),
                    "upperarm_r": (48.0, -10.0, -10.0 - pulse),
                    "lowerarm_l": (-78.0, 0.0, 24.0),
                    "lowerarm_r": (78.0, 0.0, -24.0),
                    "spine_03": (-3.0, 0.0, 0.0),
                    "head": (4.0, 0.0, 0.0),
                }
            )
        elif style == "work":
            reach = 4.0 * math.sin(phase)
            pose.update(
                {
                    "spine_01": (9.0, 0.0, 0.0),
                    "spine_02": (5.0, 0.0, 0.0),
                    "neck_01": (-8.0, 0.0, 0.0),
                    "upperarm_l": (-46.0 + reach, 0.0, 12.0),
                    "upperarm_r": (46.0 - reach, 0.0, -12.0),
                    "lowerarm_l": (-72.0, 0.0, 12.0),
                    "lowerarm_r": (72.0, 0.0, -12.0),
                    "hand_r": (0.0, 0.0, 8.0 * math.sin(phase)),
                }
            )
        for bone_name, angles in pose.items():
            bone_index = bone_indices.get(bone_name)
            if bone_index is not None:
                data[frame * len(bones) + bone_index] = _euler_matrix(*angles)

    track = animation.AnimationTrack(name, data, frame_count, frame_rate)
    track.interpolationType = animation.INTERPOLATION["LINEAR"]
    track.loop = True
    return track


def _export_animations(app, exporter, output_path: str, character: str) -> None:
    _install_ascii_animation_export_patch()
    human = app.selectedHuman
    rig = human.getSkeleton()
    prefix = "WS_Engineer" if character == "engineer" else "WS_Doctor"
    for suffix, style in (
        ("Idle", "idle"),
        ("Gesture", "gesture"),
        ("Guarded", "guarded"),
        ("Work", "work"),
    ):
        human.removeAnimations()
        track = _animation_track(rig, prefix + "_" + suffix, style)
        human.addAnimation(track)
        human.setActiveAnimation(track.name)
        human.setPosed(False)
        exporter.binary.setSelected(False)
        animation_path = os.path.join(os.path.dirname(output_path), prefix + "_" + suffix + ".fbx")
        app.mhapi.exports.exportAsFBX(animation_path, useExportsDir=False)
        log.message("WHITEOUT_AUTOGEN_ANIMATION %s", animation_path)
    human.removeAnimations()


def _configure_engineer(app, suits_root: str) -> None:
    human = app.selectedHuman
    human.setGender(1.0)
    human.setAgeYears(41)
    human.setWeight(0.56)
    human.setMuscle(0.57)
    human.setHeight(0.56)
    human.setAsian(1.0)
    human.applyAllTargets()
    human.material = material.fromFile(
        _system_data("skins", "middleage_asian_male", "middleage_asian_male.mhmat")
    )
    app.mhapi.assets.equipHair(_system_data("hair", "short04", "short04.mhpxy"))
    app.mhapi.assets.equipEyebrows(
        _system_data("eyebrows", "eyebrow007", "eyebrow007.mhpxy")
    )
    app.mhapi.assets.equipClothes(
        _external_clothes(suits_root, "elvs_male_coveralls_1")
    )
    app.mhapi.assets.equipClothes(
        _system_data("clothes", "shoes03", "shoes03.mhpxy")
    )


def _configure_doctor(app, suits_root: str) -> None:
    human = app.selectedHuman
    human.setGender(0.0)
    human.setAgeYears(31)
    human.setWeight(0.47)
    human.setMuscle(0.43)
    human.setHeight(0.49)
    human.setAsian(1.0)
    human.applyAllTargets()
    human.material = material.fromFile(
        _system_data("skins", "young_asian_female", "young_asian_female.mhmat")
    )
    app.mhapi.assets.equipHair(_system_data("hair", "bob01", "bob01.mhpxy"))
    app.mhapi.assets.equipEyebrows(
        _system_data("eyebrows", "eyebrow005", "eyebrow005.mhpxy")
    )
    app.mhapi.assets.equipEyelashes(
        _system_data("eyelashes", "eyelashes02", "eyelashes02.mhpxy")
    )
    app.mhapi.assets.equipClothes(
        _external_clothes(suits_root, "elvs_emt_uniform_jacket_female")
    )
    app.mhapi.assets.equipClothes(
        _external_clothes(suits_root, "elvs_emt_uniform_pants_female")
    )
    app.mhapi.assets.equipClothes(
        _system_data("clothes", "shoes03", "shoes03.mhpxy")
    )


def _export_character(app) -> None:
    character = os.environ.get("WS_MH_CHARACTER", "").strip().lower()
    output_path = os.environ.get("WS_MH_OUTPUT", "").strip()
    suits_root = os.environ.get("WS_MH_SUITS_ROOT", "").strip()
    try:
        if character not in {"engineer", "doctor"}:
            raise ValueError("WS_MH_CHARACTER must be engineer or doctor")
        if not output_path:
            raise ValueError("WS_MH_OUTPUT is required")
        if not os.path.isdir(suits_root):
            raise ValueError("WS_MH_SUITS_ROOT does not exist: " + suits_root)

        output_path = os.path.abspath(output_path)
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        app.mhapi.assets.unequipAllClothes()
        if character == "engineer":
            _configure_engineer(app, suits_root)
        else:
            _configure_doctor(app, suits_root)
        _assign_game_rig(app.selectedHuman)

        exporter = app.mhapi.exports.getFBXExporter()
        if exporter is None:
            raise RuntimeError("MakeHuman FBX exporter is not available")
        exporter.binary.setSelected(True)
        exporter.feetOnGround.setSelected(True)
        app.mhapi.exports.exportAsFBX(output_path, useExportsDir=False)
        _export_animations(app, exporter, output_path, character)
        log.message("WHITEOUT_AUTOGEN_SUCCESS %s %s", character, output_path)
    except Exception:
        log.error("WHITEOUT_AUTOGEN_FAILED\n%s", traceback.format_exc())
    finally:
        QTimer.singleShot(500, QApplication.quit)


def load(app) -> None:
    if os.environ.get("WS_MH_AUTOGEN", "") == "1":
        # The Windows community build finishes plugin loading before it reports
        # "Loading done".  Waiting for the initial macro-target pass avoids
        # re-entering MakeHuman's nested progress tracker during startup.
        QTimer.singleShot(9000, lambda: _export_character(gui3d.app))


def unload(app) -> None:
    pass
