"""Import pinned CC0 textures and build the runtime station materials."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


REPO_ROOT = Path(unreal.Paths.project_dir()).resolve().parent
MANIFEST_PATH = REPO_ROOT / "SourceAssets" / "PolyHaven" / "manifest.json"
TEXTURE_ROOT = "/Game/WindStation/Art/Textures"
MATERIAL_ROOT = "/Game/WindStation/Art/Materials"


def import_texture(source_file: Path, destination_name: str, is_normal: bool, is_linear: bool):
    destination_path = f"{TEXTURE_ROOT}/{destination_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(destination_path):
        texture = unreal.load_asset(destination_path)
    else:
        task = unreal.AssetImportTask()
        task.filename = str(source_file)
        task.destination_path = TEXTURE_ROOT
        task.destination_name = destination_name
        task.automated = True
        task.replace_existing = True
        task.save = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        if not task.imported_object_paths:
            raise RuntimeError(f"Unreal did not import {source_file}")
        texture = unreal.load_asset(task.imported_object_paths[0])
    if texture is None:
        raise RuntimeError(f"Unable to load imported texture {destination_path}")
    texture.set_editor_property("srgb", not is_linear and not is_normal)
    if is_normal:
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def make_material(name: str, textures: dict[str, object], metallic: float, tiling: float):
    asset_path = f"{MATERIAL_ROOT}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        material = unreal.load_asset(asset_path)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
        )
    if material is None:
        raise RuntimeError(f"Unable to create {asset_path}")

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    coordinates = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -620, 0
    )
    coordinates.set_editor_property("u_tiling", tiling)
    coordinates.set_editor_property("v_tiling", tiling)

    diffuse = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -360, -160
    )
    diffuse.set_editor_property("texture", textures["Diffuse"])
    normal = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -360, 40
    )
    normal.set_editor_property("texture", textures["Normal"])
    rough = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -360, 220
    )
    rough.set_editor_property("texture", textures["Roughness"])
    metallic_value = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -120, 340
    )
    metallic_value.set_editor_property("r", metallic)

    for sample in (diffuse, normal, rough):
        unreal.MaterialEditingLibrary.connect_material_expressions(coordinates, "", sample, "UVs")
    unreal.MaterialEditingLibrary.connect_material_property(
        diffuse, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        normal, "RGB", unreal.MaterialProperty.MP_NORMAL
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, "R", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        metallic_value, "", unreal.MaterialProperty.MP_METALLIC
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def main() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    unreal.EditorAssetLibrary.make_directory(TEXTURE_ROOT)
    unreal.EditorAssetLibrary.make_directory(MATERIAL_ROOT)
    material_specs = {
        "snow_02": ("M_WS_Snow", 0.0, 2.2),
        "rusty_metal_05": ("M_WS_RustedMetal", 0.82, 2.8),
        "concrete": ("M_WS_Concrete", 0.0, 2.6),
        "blue_metal_plate": ("M_WS_PaintedMetal", 0.38, 2.0),
        "fabric_pattern_05": ("M_WS_WinterFabric", 0.0, 3.2),
    }
    for asset_id, (material_name, metallic, tiling) in material_specs.items():
        imported = {}
        for channel, entry in manifest[asset_id].items():
            source_file = REPO_ROOT / entry["file"]
            suffix = {"Diffuse": "D", "Normal": "N", "Roughness": "R"}[channel]
            imported[channel] = import_texture(
                source_file,
                f"T_{asset_id}_{suffix}",
                is_normal=channel == "Normal",
                is_linear=channel == "Roughness",
            )
        make_material(material_name, imported, metallic, tiling)
        unreal.log(f"WhiteoutStation assets: built {material_name}")


if __name__ == "__main__":
    main()
