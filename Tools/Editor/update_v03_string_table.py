"""Import the reviewed v0.3 Chinese UI copy into the existing StringTable."""

from pathlib import Path

import unreal


def main() -> None:
    repo_root = Path(unreal.Paths.project_dir()).resolve().parent
    csv_path = repo_root / "SourceAssets" / "UI" / "WhiteoutStation_zh.csv"
    table = unreal.load_asset("/Game/WindStation/UI/ST_WhiteoutStation_zh")
    if table is None:
        raise RuntimeError("Missing /Game/WindStation/UI/ST_WhiteoutStation_zh")
    table_id = unreal.StringTableLibrary.get_table_id(table)
    if not unreal.StringTableLibrary.import_table_from_csv_file(table_id, str(csv_path)):
        raise RuntimeError(f"Unable to import {csv_path}")
    table.modify(True)
    unreal.EditorAssetLibrary.save_loaded_asset(table, False)
    unreal.log(f"WhiteoutStation v0.3: updated Chinese StringTable from {csv_path}")


if __name__ == "__main__":
    main()
