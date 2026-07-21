"""Convert the interaction overlay to a thin white Fresnel edge for v0.3."""

import unreal


PATH = "/Game/WindStation/Art/Materials/M_WS_InteractionOverlay"


def scalar(material, name: str, value: float, x: int, y: int):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, x, y
    )
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def main() -> None:
    material = unreal.load_asset(PATH)
    if material is None:
        raise RuntimeError(f"Missing interaction material: {PATH}")

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    white = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -620, -150
    )
    white.set_editor_property("parameter_name", "OutlineColor")
    white.set_editor_property("default_value", unreal.LinearColor(0.92, 0.95, 1.0, 1.0))
    exponent = scalar(material, "EdgeExponent", 6.0, -620, -10)
    strength = scalar(material, "OutlineStrength", 1.35, -620, 110)
    opacity = scalar(material, "OutlineOpacity", 0.62, -620, 230)
    fresnel = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionFresnel, -390, -10
    )
    emissive_scale = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -175, -105
    )
    emissive = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 35, -105
    )
    edge_opacity = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -155, 185
    )

    unreal.MaterialEditingLibrary.connect_material_expressions(exponent, "", fresnel, "ExponentIn")
    unreal.MaterialEditingLibrary.connect_material_expressions(fresnel, "", emissive_scale, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(strength, "", emissive_scale, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(white, "", emissive, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(emissive_scale, "", emissive, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(fresnel, "", edge_opacity, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(opacity, "", edge_opacity, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        edge_opacity, "", unreal.MaterialProperty.MP_OPACITY
    )
    unreal.MaterialEditingLibrary.set_material_usage(
        material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    unreal.log("WhiteoutStation v0.3: interaction material is now edge-only white Fresnel")


if __name__ == "__main__":
    main()
