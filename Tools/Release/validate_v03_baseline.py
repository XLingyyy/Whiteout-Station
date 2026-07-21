from __future__ import annotations

import struct
from collections import Counter
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
BASELINE_ROOT = REPO_ROOT / "docs" / "baseline_v0.3"
REFERENCE_ROOT = REPO_ROOT / "docs" / "reference_v0.3" / "feedback"

RESOLUTIONS = {
    "1280x720": (1280, 720),
    "1920x1080": (1920, 1080),
}

REFERENCE_VIEWS = {
    "UI_01_ESC_Menu.png": "UI_pause",
    "UI_02_DialogueWheel.png": "UI_dialogue_gu_wheel",
    "UI_03_EvidenceBoard.png": "UI_evidence",
    "UI_04_HUD.jpeg": "UI_hud",
    "UI_05_FocusCard.png": "UI_focus_near",
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

    if len(low) != 46 or len(files) != 92:
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

    print("V0.3 BASELINE: PASS")
    print(f"views={len(low)} files={len(files)} resolutions=46+46")
    print("categories=" + " ".join(f"{key}:{categories[key]}" for key in sorted(categories)))
    print("feedback_views=5/5")


if __name__ == "__main__":
    main()
