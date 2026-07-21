import unreal


MATERIAL_PATH = "/Game/Variant_Horror/Blueprints/Light/Assets/Materials/M_DustMote"
material = unreal.load_asset(MATERIAL_PATH)
if not material:
    raise RuntimeError(f"Missing material {MATERIAL_PATH}")

unreal.log(
    "WS_VFX_MATERIAL "
    f"path={material.get_path_name()} blend={material.get_editor_property('blend_mode')} "
    f"two_sided={material.get_editor_property('two_sided')}"
)
unreal.log(
    "WS_VFX_MATERIAL_API "
    + ",".join(
        name
        for name in dir(unreal.MaterialEditingLibrary)
        if "express" in name.lower() or "node" in name.lower()
    )
)
unreal.log(
    "WS_VFX_MATERIAL_OBJECT_API "
    + ",".join(
        name
        for name in dir(material)
        if "express" in name.lower() or "editor" in name.lower()
    )
)
for index, expression in enumerate(
    unreal.MaterialEditingLibrary.get_material_expressions(material)
):
    properties = []
    for property_name in (
        "parameter_name",
        "default_value",
        "constant",
        "texture",
        "desc",
    ):
        try:
            properties.append(
                f"{property_name}={expression.get_editor_property(property_name)}"
            )
        except Exception:
            pass
    unreal.log(
        "WS_VFX_EXPRESSION "
        f"index={index} class={expression.get_class().get_name()} "
        + " ".join(properties)
    )

unreal.log("WS_VFX_MATERIAL_AUDIT_COMPLETE")
