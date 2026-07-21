"""Rebuild all v0.3 G1 UI assets and presentation materials."""

from pathlib import Path
import runpy

import unreal


TOOLS_ROOT = Path(__file__).resolve().parent


def main() -> None:
    for script_name in (
        "import_v03_ui_assets.py",
        "update_v03_string_table.py",
        "create_v03_interaction_material.py",
    ):
        unreal.log(f"WhiteoutStation v0.3: running {script_name}")
        runpy.run_path(str(TOOLS_ROOT / script_name), run_name="__main__")
    unreal.log("WhiteoutStation v0.3: G1 asset bootstrap complete")


if __name__ == "__main__":
    main()
