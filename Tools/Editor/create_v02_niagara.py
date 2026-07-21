"""Duplicate the audited UE template Niagara system into the project VFX namespace."""

from __future__ import annotations

import unreal


SOURCE_MIST = "/Game/Variant_Horror/Blueprints/Light/Assets/NS_DustMote"
TARGET_MIST = "/Game/WindStation/Art/VFX/NS_WS_BlizzardMist"
SOURCE_STREAKS = "/Game/DemoTemplate/Templates/Confetti/NS_Confetti"
TARGET_STREAKS = "/Game/WindStation/Art/VFX/NS_WS_BlizzardStreaks"
CONFETTI_MATERIAL = "/Game/DemoTemplate/Templates/Confetti/M_GoldConfetti"


def main() -> None:
    unreal.EditorAssetLibrary.make_directory("/Game/WindStation/Art/VFX")
    pairs = ((SOURCE_MIST, TARGET_MIST), (SOURCE_STREAKS, TARGET_STREAKS))
    for source, target in pairs:
        if not unreal.EditorAssetLibrary.does_asset_exist(source):
            raise RuntimeError(f"Missing audited template source: {source}")
        asset = unreal.load_asset(target) if unreal.EditorAssetLibrary.does_asset_exist(target) else None
        if not asset:
            asset = unreal.EditorAssetLibrary.duplicate_asset(source, target)
        if not asset:
            raise RuntimeError(f"Unable to create {target}")
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        unreal.log(f"WhiteoutStation v0.2: Niagara blizzard layer ready at {target}")

    material = unreal.load_asset(CONFETTI_MATERIAL)
    if not material:
        raise RuntimeError(f"Missing audited Niagara material: {CONFETTI_MATERIAL}")
    color_parameters = [
        expression
        for expression in unreal.MaterialEditingLibrary.get_material_expressions(material)
        if expression.get_class().get_name() == "MaterialExpressionVectorParameter"
    ]
    if not color_parameters:
        raise RuntimeError("Confetti material no longer contains a color parameter")
    color_parameters[0].set_editor_property(
        "default_value", unreal.LinearColor(0.78, 0.90, 1.0, 1.0)
    )
    material.set_editor_property("two_sided", True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    unreal.log("WhiteoutStation v0.2: Niagara streak material recolored to windblown snow")


if __name__ == "__main__":
    main()
