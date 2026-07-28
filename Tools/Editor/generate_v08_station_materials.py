"""Create tiled v0.8 floor and wall materials without modifying the saved level."""

from __future__ import annotations

import unreal


MATERIAL_ROOT = "/Game/WindStation/Art/Materials"
TEXTURE_ROOT = "/Game/WindStation/Art/Textures"


def load_required(path: str) -> unreal.Object:
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError(f"required asset is missing: {path}")
    return asset


def create_or_reset_material(name: str) -> unreal.Material:
    asset_path = f"{MATERIAL_ROOT}/{name}"
    material = (
        unreal.EditorAssetLibrary.load_asset(asset_path)
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path)
        else None
    )
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name,
            MATERIAL_ROOT,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"asset is not a Material: {asset_path}")
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    return material


def add_texture_sample(
    material: unreal.Material,
    texture: unreal.Texture,
    texcoord: unreal.MaterialExpressionTextureCoordinate,
    x: int,
    y: int,
) -> unreal.MaterialExpressionTextureSample:
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSample,
        x,
        y,
    )
    sample.set_editor_property("texture", texture)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        texcoord,
        "",
        sample,
        "UVs",
    )
    return sample


def build_material(
    name: str,
    texture_prefix: str,
    tiling: float,
    tint: unreal.LinearColor,
    metallic: float,
) -> None:
    material = create_or_reset_material(name)
    material.set_editor_property("two_sided", False)

    texcoord = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureCoordinate,
        -900,
        0,
    )
    texcoord.set_editor_property("u_tiling", tiling)
    texcoord.set_editor_property("v_tiling", tiling)

    diffuse = add_texture_sample(
        material,
        load_required(f"{TEXTURE_ROOT}/{texture_prefix}_D"),
        texcoord,
        -640,
        -220,
    )
    normal = add_texture_sample(
        material,
        load_required(f"{TEXTURE_ROOT}/{texture_prefix}_N"),
        texcoord,
        -640,
        80,
    )
    roughness = add_texture_sample(
        material,
        load_required(f"{TEXTURE_ROOT}/{texture_prefix}_R"),
        texcoord,
        -640,
        360,
    )

    color_tint = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant3Vector,
        -370,
        -70,
    )
    color_tint.set_editor_property("constant", tint)
    color_multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionMultiply,
        -150,
        -180,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        diffuse,
        "RGB",
        color_multiply,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        color_tint,
        "",
        color_multiply,
        "B",
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        color_multiply,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        normal,
        "RGB",
        unreal.MaterialProperty.MP_NORMAL,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness,
        "R",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )

    metallic_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -130,
        520,
    )
    metallic_node.set_editor_property("r", metallic)
    unreal.MaterialEditingLibrary.connect_material_property(
        metallic_node,
        "",
        unreal.MaterialProperty.MP_METALLIC,
    )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    unreal.log(f"V08 station material generated: {material.get_path_name()}")


def main() -> None:
    build_material(
        "M_WS_FloorDeck_V08",
        "T_blue_metal_plate",
        10.0,
        unreal.LinearColor(0.46, 0.55, 0.62, 1.0),
        0.28,
    )
    build_material(
        "M_WS_WallPanel_V08",
        "T_concrete",
        7.0,
        unreal.LinearColor(0.62, 0.70, 0.75, 1.0),
        0.04,
    )


if __name__ == "__main__":
    main()
