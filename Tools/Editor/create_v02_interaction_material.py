"""Create the translucent emissive overlay used by all 13 interaction hotspots."""

import unreal


ROOT = "/Game/WindStation/Art/Materials"
NAME = "M_WS_InteractionOverlay"
PATH = f"{ROOT}/{NAME}"


def main() -> None:
    unreal.EditorAssetLibrary.make_directory(ROOT)
    material = unreal.load_asset(PATH)
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            NAME, ROOT, unreal.Material, unreal.MaterialFactoryNew()
        )
    if material is None:
        raise RuntimeError(f"Unable to create {PATH}")

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    tint = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -420, -60
    )
    tint.set_editor_property("parameter_name", "HighlightColor")
    tint.set_editor_property("default_value", unreal.LinearColor(0.08, 0.72, 1.0, 1.0))
    strength = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -420, 80
    )
    strength.set_editor_property("parameter_name", "HighlightStrength")
    strength.set_editor_property("default_value", 2.8)
    multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -170, -20
    )
    opacity = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -170, 135
    )
    opacity.set_editor_property("parameter_name", "OverlayOpacity")
    opacity.set_editor_property("default_value", 0.26)

    unreal.MaterialEditingLibrary.connect_material_expressions(tint, "", multiply, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(strength, "", multiply, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity, "", unreal.MaterialProperty.MP_OPACITY
    )
    unreal.MaterialEditingLibrary.set_material_usage(
        material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    unreal.log(f"WhiteoutStation v0.2: created interaction overlay {PATH}")


if __name__ == "__main__":
    main()
