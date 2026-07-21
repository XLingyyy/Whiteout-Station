"""Create the v0.2 presentation DataAssets and Chinese StringTable."""

from __future__ import annotations

from pathlib import Path

import unreal


REPO_ROOT = Path(unreal.Paths.project_dir()).resolve().parent
ASSET_ROOT = "/Game/WindStation/Presentation"
UI_ROOT = "/Game/WindStation/UI"
CSV_PATH = REPO_ROOT / "SourceAssets" / "UI" / "WhiteoutStation_zh.csv"


def ensure_data_asset(name: str, asset_class, refresh_properties: tuple[str, ...] = ()) -> object:
    asset_path = f"{ASSET_ROOT}/{name}"
    asset = unreal.load_asset(asset_path)
    if not asset:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", asset_class)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, ASSET_ROOT, asset_class, factory
        )
    if not asset:
        raise RuntimeError(f"Unable to create {asset_path}")
    if refresh_properties:
        defaults = unreal.get_default_object(asset_class)
        for property_name in refresh_properties:
            asset.set_editor_property(property_name, defaults.get_editor_property(property_name))
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log(f"WhiteoutStation v0.2: populated {asset_path}")
    return asset


def ensure_string_table() -> object:
    asset_path = f"{UI_ROOT}/ST_WhiteoutStation_zh"
    string_table = unreal.load_asset(asset_path)
    if not string_table:
        string_table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "ST_WhiteoutStation_zh",
            UI_ROOT,
            unreal.StringTable,
            unreal.StringTableFactory(),
        )
    if not string_table:
        raise RuntimeError(f"Unable to create {asset_path}")
    table_id = unreal.StringTableLibrary.get_table_id(string_table)
    if not unreal.StringTableLibrary.import_table_from_csv_file(table_id, str(CSV_PATH)):
        raise RuntimeError(f"Unable to import {CSV_PATH} into {table_id}")
    unreal.EditorAssetLibrary.save_loaded_asset(string_table)
    unreal.log(f"WhiteoutStation v0.2: populated {asset_path} as table {table_id}")
    return string_table


def main() -> None:
    unreal.EditorAssetLibrary.make_directory(ASSET_ROOT)
    unreal.EditorAssetLibrary.make_directory(UI_ROOT)
    ensure_data_asset("DA_WS_StationAssembly", unreal.WSStationAssemblyData, ("placements", "lights"))
    ensure_data_asset("DA_WS_UIDesign", unreal.WSUIDesignData)
    ensure_data_asset("DA_WS_RejectionCopy", unreal.WSReasonPresentationData, ("reasons",))
    ensure_string_table()


if __name__ == "__main__":
    main()
