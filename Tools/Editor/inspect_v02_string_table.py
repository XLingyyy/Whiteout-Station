"""Report selected StringTable entries for v0.2 localization QA."""

import unreal


ASSET_PATH = "/Game/WindStation/UI/ST_WhiteoutStation_zh"


def main() -> None:
    table = unreal.load_asset(ASSET_PATH)
    if not table:
        raise RuntimeError(f"Missing {ASSET_PATH}")
    table_id = unreal.StringTableLibrary.get_table_id(table)
    methods = [name for name in dir(unreal.StringTableLibrary) if "string" in name.lower() or "table" in name.lower()]
    unreal.log(f"WS_STRING_TABLE_METHODS={methods}")
    keys = unreal.StringTableLibrary.get_keys_from_string_table(table_id)
    unreal.log(f"WS_STRING_TABLE_KEY_COUNT={len(keys)}")
    for key in ("ui_initial_message", "ui_help", "ui_top_format", "ui_preview_body_format"):
        value = unreal.StringTableLibrary.get_table_entry_source_string(table_id, key)
        registered = unreal.StringTableLibrary.is_registered_table_entry(table_id, key)
        unreal.log(f"WS_STRING_TABLE_ENTRY {key} registered={registered} value={value!r}")


if __name__ == "__main__":
    main()
