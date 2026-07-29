"""Capture a read-only rendering and animation audit for the two v0.8 NPCs."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


MAP_PATH = "/Game/WindStation/World/MVP_StationMap"
ACTION_IDS = ("talk_gu_heng", "talk_ye_cheng")
REPORT_PATH = (
    Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))
    / "Diagnostics"
    / "v08_npc_render_audit.json"
)


def asset_path(value) -> str:
    return value.get_path_name() if value is not None else ""


def read_property(value, name: str, fallback=None):
    try:
        return value.get_editor_property(name)
    except Exception:
        return fallback


def transform_record(transform: unreal.Transform) -> dict[str, list[float]]:
    translation = transform.translation
    rotation = transform.rotation.rotator()
    scale = transform.scale3d
    return {
        "translation": [translation.x, translation.y, translation.z],
        "rotation": [rotation.roll, rotation.pitch, rotation.yaw],
        "scale": [scale.x, scale.y, scale.z],
    }


def material_record(slot) -> dict[str, object]:
    material = read_property(slot, "material_interface")
    base_material = material.get_base_material() if material else None
    return {
        "slot": str(read_property(slot, "material_slot_name", "")),
        "material": asset_path(material),
        "base_material": asset_path(base_material),
        "blend_mode": str(read_property(base_material, "blend_mode", "")),
        "two_sided": bool(read_property(base_material, "two_sided", False)),
    }


def animation_record(animation) -> dict[str, object]:
    if animation is None:
        return {}
    return {
        "asset": asset_path(animation),
        "skeleton": asset_path(read_property(animation, "skeleton")),
        "duration": animation.get_play_length(),
    }


def main() -> None:
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    if world is None:
        raise RuntimeError(f"Unable to load {MAP_PATH}")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = {}
    for actor in actor_subsystem.get_all_level_actors():
        if not isinstance(actor, unreal.WSInteractableActor):
            continue
        action_id = str(actor.get_editor_property("action_id"))
        if action_id in ACTION_IDS:
            actors[action_id] = actor

    report: dict[str, object] = {
        "schema": "whiteout.v0.8.npc-render-audit.v1",
        "engine_version": unreal.SystemLibrary.get_engine_version(),
        "map": MAP_PATH,
        "characters": {},
    }
    for action_id in ACTION_IDS:
        actor = actors.get(action_id)
        if actor is None:
            raise RuntimeError(f"Missing map actor for {action_id}")
        data_asset = actor.get_editor_property("character_asset")
        component = actor.get_editor_property("character_mesh")
        mesh = component.get_skeletal_mesh_asset()
        if data_asset is None or mesh is None:
            raise RuntimeError(f"Missing character data for {action_id}")

        skeleton = mesh.get_editor_property("skeleton")
        current_bones: dict[str, object] = {}
        for bone_name in (
            "J_Bip_C_Head",
            "J_Adj_L_FaceEye",
            "J_Adj_R_FaceEye",
            "J_Bip_L_UpperArm",
            "J_Bip_R_UpperArm",
            "J_Bip_L_LowerArm",
            "J_Bip_R_LowerArm",
            "J_Bip_L_Hand",
            "J_Bip_R_Hand",
        ):
            bone_index = component.get_bone_index(bone_name)
            if bone_index >= 0:
                bone_record: dict[str, object] = {"index": bone_index}
                try:
                    bone_record["component_transform"] = transform_record(
                        component.get_socket_transform(
                            bone_name,
                            unreal.RelativeTransformSpace.RTS_COMPONENT,
                        )
                    )
                except Exception as error:
                    bone_record["transform_error"] = str(error)
                current_bones[bone_name] = bone_record

        animations = data_asset.get_editor_property("animations")
        override_materials = [
            asset_path(material)
            for material in component.get_editor_property("override_materials")
        ]
        report["characters"][action_id] = {
            "actor": actor.get_path_name(),
            "data_asset": asset_path(data_asset),
            "mesh": asset_path(mesh),
            "skeleton": asset_path(skeleton),
            "eye_override_slot": str(
                data_asset.get_editor_property("eye_material_slot_name")
            ),
            "eye_override_material": asset_path(
                data_asset.get_editor_property("eye_material")
            ),
            "mesh_materials": [
                material_record(slot)
                for slot in mesh.get_editor_property("materials")
            ],
            "component_override_materials": override_materials,
            "current_bones": current_bones,
            "animations": {
                name: animation_record(animations.get_editor_property(name))
                for name in ("idle", "gesture", "guarded", "work")
            },
            "v07_animations": {
                name: animation_record(actor.get_editor_property(name))
                for name in (
                    "walk_animation",
                    "acknowledge_animation",
                    "consider_animation",
                    "reassure_animation",
                    "reject_animation",
                    "alarmed_animation",
                )
            },
        }

    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(
        json.dumps(report, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    unreal.log(f"WS_V08_NPC_RENDER_AUDIT_COMPLETE report={REPORT_PATH}")


if __name__ == "__main__":
    main()
