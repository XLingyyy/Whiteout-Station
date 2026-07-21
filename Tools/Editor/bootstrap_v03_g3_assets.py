"""Build v0.3 G3 character materials and refresh the station assembly asset."""

from __future__ import annotations

import unreal


MATERIAL_ROOT = "/Game/WindStation/Art/Materials"
EYE_TEXTURE = "/Game/WindStation/Art/Characters/Engineer/brown_eye"
ASSEMBLY_ASSET = "/Game/WindStation/Presentation/DA_WS_StationAssembly"


def build_eye_material() -> object:
    unreal.EditorAssetLibrary.make_directory(MATERIAL_ROOT)
    asset_path = f"{MATERIAL_ROOT}/M_WS_Eye"
    material = unreal.load_asset(asset_path)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_WS_Eye", MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
        )
    if not material:
        raise RuntimeError(f"Unable to create {asset_path}")

    # The material is applied at runtime to the characters' shared eye slot.
    # Persist the usage bit up front so packaged/game launches never substitute
    # the default material while waiting for an editor-side recompile.
    material.set_editor_property("used_with_skeletal_mesh", True)

    eye_texture = unreal.load_asset(EYE_TEXTURE)
    if not eye_texture:
        raise RuntimeError(f"Unable to load {EYE_TEXTURE}")

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    texture_sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -340, -80
    )
    texture_sample.set_editor_property("texture", eye_texture)
    # Dim the sclera as well as the iris so the stylized mesh never reads as
    # emissive white under the station's task lights.
    eye_level = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -340, 80
    )
    eye_level.set_editor_property("r", 0.28)
    multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -120, -80
    )
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -120, 120
    )
    roughness.set_editor_property("r", 1.0)
    specular = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -120, 220
    )
    specular.set_editor_property("r", 0.0)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        texture_sample, "RGB", multiply, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(eye_level, "", multiply, "B")
    unreal.MaterialEditingLibrary.connect_material_property(
        multiply, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        specular, "", unreal.MaterialProperty.MP_SPECULAR
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    unreal.log(f"WhiteoutStation v0.3 G3: built {asset_path}")
    return material


def refresh_station_assembly() -> object:
    asset = unreal.load_asset(ASSEMBLY_ASSET)
    if not asset:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.WSStationAssemblyData)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "DA_WS_StationAssembly",
            "/Game/WindStation/Presentation",
            unreal.WSStationAssemblyData,
            factory,
        )
    if not asset:
        raise RuntimeError(f"Unable to load or create {ASSEMBLY_ASSET}")

    defaults = unreal.get_default_object(unreal.WSStationAssemblyData)
    asset.set_editor_property("placements", defaults.get_editor_property("placements"))
    asset.set_editor_property("lights", defaults.get_editor_property("lights"))
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    unreal.log(f"WhiteoutStation v0.3 G3: refreshed {ASSEMBLY_ASSET}")
    return asset


def main() -> None:
    build_eye_material()
    refresh_station_assembly()
    unreal.log("WhiteoutStation v0.3: G3 asset bootstrap complete")


if __name__ == "__main__":
    main()
