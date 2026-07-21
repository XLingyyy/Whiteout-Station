"""Run every editor-side asset step required by the v0.2 G4 gate."""

from pathlib import Path
import runpy

import unreal


TOOLS_ROOT = Path(__file__).resolve().parent


def main() -> None:
    for script_name in (
        "create_v02_presentation_assets.py",
        "import_v02_audio.py",
        "create_v02_interaction_material.py",
    ):
        unreal.log(f"WhiteoutStation v0.2: running {script_name}")
        runpy.run_path(str(TOOLS_ROOT / script_name), run_name="__main__")
    stale_asset = "/Game/WindStation/Art/VFX/NS_WS_Blizzard"
    if unreal.EditorAssetLibrary.does_asset_exist(stale_asset):
        if not unreal.EditorAssetLibrary.delete_asset(stale_asset):
            raise RuntimeError(f"Unable to delete stale asset {stale_asset}")
    unreal.log("WhiteoutStation v0.2: G4 asset bootstrap complete")


if __name__ == "__main__":
    main()
