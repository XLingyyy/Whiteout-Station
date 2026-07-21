"""Create the v0.2 UI component test map from the deterministic station map."""

import unreal


SOURCE = "/Game/WindStation/World/MVP_StationMap"
TARGET = "/Game/WindStation/World/Dev_TestMap"


def main() -> None:
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        if not unreal.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET):
            raise RuntimeError(f"Unable to duplicate {SOURCE} to {TARGET}")
    if not unreal.EditorAssetLibrary.save_asset(TARGET, False):
        raise RuntimeError(f"Unable to save {TARGET}")
    unreal.log(f"WhiteoutStation v0.2: UI component test map ready at {TARGET}")


if __name__ == "__main__":
    main()
