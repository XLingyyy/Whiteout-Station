from __future__ import annotations

import struct
from collections import Counter
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
BASELINE_ROOT = REPO_ROOT / "docs" / "baseline_v0.4"
REFERENCE_ROOT = REPO_ROOT / "docs" / "reference_v0.3" / "feedback_after"

RESOLUTIONS = {
    "1280x720": (1280, 720),
    "1920x1080": (1920, 1080),
}

REFERENCE_VIEWS = {
    "After_01_HUD.png": "UI_hud",
    "After_02_Pause.png": "UI_pause",
    "After_03_Settings.png": "UI_settings_default",
    "After_04_EvidenceBoard.png": "UI_evidence_detail",
    "After_05_FocusObject.png": "UI_focus_near",
    "After_06_FocusNPC.png": "UI_focus_npc",
    "After_07_Dialogue.png": "UI_dialogue_gu_default",
    "After_08_GuHeng.png": "UI_character_gu_front",
    "After_09_YeCheng.png": "UI_character_ye_front",
}


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()[:24]
    if len(data) != 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"NOT PNG: {path}")
    return struct.unpack(">II", data[16:24])


def view_name(path: Path, resolution: str) -> str:
    suffix = f"_{resolution}.png"
    if not path.name.endswith(suffix):
        raise SystemExit(f"INVALID BASELINE NAME: {path.name}")
    return path.name[: -len(suffix)]


def main() -> None:
    files = sorted(BASELINE_ROOT.glob("*.png"))
    by_resolution: dict[str, set[str]] = {key: set() for key in RESOLUTIONS}

    for path in files:
        matched = False
        for label, expected_size in RESOLUTIONS.items():
            if path.name.endswith(f"_{label}.png"):
                matched = True
                actual_size = png_size(path)
                if actual_size != expected_size:
                    raise SystemExit(
                        f"SIZE MISMATCH: {path.name} expected={expected_size} actual={actual_size}"
                    )
                by_resolution[label].add(view_name(path, label))
                break
        if not matched:
            raise SystemExit(f"UNCLASSIFIED BASELINE: {path.name}")

    low = by_resolution["1280x720"]
    high = by_resolution["1920x1080"]
    if low != high:
        raise SystemExit(
            f"PAIR MISMATCH: only_1280={sorted(low - high)} only_1920={sorted(high - low)}"
        )

    if len(low) != 70 or len(files) != 140:
        raise SystemExit(f"BASELINE COUNT MISMATCH: views={len(low)} files={len(files)}")

    for reference_name, baseline_view in REFERENCE_VIEWS.items():
        reference_path = REFERENCE_ROOT / reference_name
        if not reference_path.is_file():
            raise SystemExit(f"MISSING REFERENCE: {reference_path}")
        if baseline_view not in low:
            raise SystemExit(f"MISSING FEEDBACK VIEW: {baseline_view}")

    categories = Counter(
        "scene" if name.startswith("UI_scene_")
        else "character" if name.startswith("UI_character_")
        else "lookat" if name.startswith("UI_lookat_")
        else "lighting" if name.startswith("UI_lighting_")
        else "ui"
        for name in low
    )

    print("V0.4 BASELINE: PASS")
    print(f"views={len(low)} files={len(files)} resolutions=70+70")
    print("categories=" + " ".join(f"{key}:{categories[key]}" for key in sorted(categories)))
    print("feedback_after=9/9")


if __name__ == "__main__":
    main()
