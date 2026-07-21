"""Write the deterministic v0.3 G3 NPC mesh/material/animation audit."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


CHARACTERS = (
    {
        "label": "GuHeng",
        "root": "/Game/WindStation/Art/Characters/Engineer",
        "mesh": "SK_WS_Engineer",
        "token": "WS_Engineer",
        "minimum_material_slots": 6,
    },
    {
        "label": "YeCheng",
        "root": "/Game/WindStation/Art/Characters/Doctor",
        "mesh": "SK_WS_Doctor",
        "token": "WS_Doctor",
        "minimum_material_slots": 8,
    },
)
ANIMATION_SUFFIXES = ("Idle", "Gesture", "Guarded", "Work")
EYE_MATERIAL = "/Game/WindStation/Art/Materials/M_WS_Eye"


def audit_character(spec: dict[str, object]) -> dict[str, object]:
    mesh_path = f"{spec['root']}/{spec['mesh']}"
    mesh = unreal.load_asset(mesh_path)
    if not mesh:
        return {"label": spec["label"], "passed": False, "error": f"missing {mesh_path}"}

    bounds = mesh.get_bounds()
    materials = mesh.get_editor_property("materials")
    material_rows = []
    eye_slot_present = False
    all_materials_present = True
    for index, slot in enumerate(materials):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        interface = slot.get_editor_property("material_interface")
        eye_slot_present = eye_slot_present or slot_name == "high-poly"
        all_materials_present = all_materials_present and interface is not None
        material_rows.append(
            {
                "index": index,
                "slot": slot_name,
                "asset": interface.get_path_name() if interface else None,
            }
        )

    animations = {}
    for suffix in ANIMATION_SUFFIXES:
        path = f"{spec['root']}/AN_{spec['token']}_{suffix}"
        animation = unreal.load_asset(path)
        animations[suffix] = animation.get_path_name() if animation else None

    skeleton = mesh.get_editor_property("skeleton")
    passed = bool(
        skeleton
        and len(materials) >= int(spec["minimum_material_slots"])
        and all_materials_present
        and eye_slot_present
        and all(animations.values())
        and bounds.box_extent.z > 700.0
    )
    return {
        "label": spec["label"],
        "passed": passed,
        "mesh": mesh.get_path_name(),
        "skeleton": skeleton.get_path_name() if skeleton else None,
        "bounds_extent_z": round(float(bounds.box_extent.z), 3),
        "material_slots": material_rows,
        "all_materials_present": all_materials_present,
        "eye_slot_present": eye_slot_present,
        "animations": animations,
    }


def main() -> None:
    eye_material = unreal.load_asset(EYE_MATERIAL)
    eye_usage = bool(
        eye_material and eye_material.get_editor_property("used_with_skeletal_mesh")
    )
    characters = [audit_character(spec) for spec in CHARACTERS]
    report = {
        "passed": bool(eye_material and eye_usage and all(row["passed"] for row in characters)),
        "shared_eye_material": eye_material.get_path_name() if eye_material else None,
        "shared_eye_material_skeletal_usage": eye_usage,
        "characters": characters,
    }
    output = Path(unreal.Paths.project_saved_dir()) / "Automation" / "v03-g3-character-audit.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log(f"WhiteoutStation v0.3 G3: character audit passed={report['passed']} output={output}")
    if not report["passed"]:
        raise RuntimeError("v0.3 G3 character audit failed")


if __name__ == "__main__":
    main()
