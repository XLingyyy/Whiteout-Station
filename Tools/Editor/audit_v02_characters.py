import unreal


CHARACTERS = {
    "Engineer": "/Game/WindStation/Art/Characters/Engineer/SK_WS_Engineer",
    "Doctor": "/Game/WindStation/Art/Characters/Doctor/SK_WS_Doctor",
}


def describe(label: str, asset_path: str) -> None:
    mesh = unreal.load_asset(asset_path)
    if not mesh:
        raise RuntimeError(f"Missing skeletal mesh: {asset_path}")

    bounds = mesh.get_bounds()
    skeleton = mesh.get_editor_property("skeleton")
    materials = mesh.get_editor_property("materials")
    unreal.log(
        "WS_CHARACTER_AUDIT "
        f"label={label} class={mesh.get_class().get_name()} "
        f"skeleton={skeleton.get_path_name() if skeleton else 'None'} "
        f"origin={bounds.origin} extent={bounds.box_extent} radius={bounds.sphere_radius:.2f} "
        f"materials={len(materials)}"
    )
    for index, material_slot in enumerate(materials):
        interface = material_slot.get_editor_property("material_interface")
        unreal.log(
            "WS_CHARACTER_MATERIAL "
            f"label={label} index={index} slot={material_slot.get_editor_property('material_slot_name')} "
            f"asset={interface.get_path_name() if interface else 'None'}"
        )


for character_label, character_asset in CHARACTERS.items():
    describe(character_label, character_asset)

unreal.log("WS_CHARACTER_AUDIT_COMPLETE")
