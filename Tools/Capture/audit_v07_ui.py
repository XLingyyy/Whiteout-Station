#!/usr/bin/env python3
"""Fail closed when the captured v0.7 crosshair is not visually centered."""

from __future__ import annotations

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / "docs" / "baseline_v0.7"
CAPTURES = (
    ("UI_hud_1280x720.png", "circle"),
    ("UI_focus_near_1280x720.png", "hand"),
    ("UI_hud_1920x1080.png", "circle"),
    ("UI_focus_near_1920x1080.png", "hand"),
)
MAX_CENTER_ERROR_PX = 1.5


def icon_pixels(image: Image.Image, kind: str) -> list[tuple[int, int]]:
    width, height = image.size
    center_x, center_y = width // 2, height // 2
    pixels: list[tuple[int, int]] = []
    for y in range(center_y - 35, center_y + 35):
        for x in range(center_x - 35, center_x + 35):
            red, green, blue = image.getpixel((x, y))
            if kind == "hand":
                matches = (
                    red > 140
                    and 85 < green < 200
                    and blue < 110
                    and red > green * 1.15
                )
            else:
                matches = (
                    red > 155
                    and green > 155
                    and blue > 155
                    and max(red, green, blue) - min(red, green, blue) < 25
                )
            if matches:
                pixels.append((x, y))
    return pixels


def main() -> int:
    failures: list[str] = []
    for filename, kind in CAPTURES:
        path = BASELINE / filename
        if not path.is_file():
            failures.append(f"{filename}: missing")
            continue
        image = Image.open(path).convert("RGB")
        pixels = icon_pixels(image, kind)
        if not pixels:
            failures.append(f"{filename}: {kind} pixels not found")
            continue
        xs = [point[0] for point in pixels]
        ys = [point[1] for point in pixels]
        icon_center = ((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2)
        screen_center = (image.width / 2, image.height / 2)
        error = (
            icon_center[0] - screen_center[0],
            icon_center[1] - screen_center[1],
        )
        print(
            f"{filename}: kind={kind} "
            f"center_error_x={error[0]:.1f}px center_error_y={error[1]:.1f}px"
        )
        if abs(error[0]) > MAX_CENTER_ERROR_PX or abs(error[1]) > MAX_CENTER_ERROR_PX:
            failures.append(
                f"{filename}: center error {error} exceeds {MAX_CENTER_ERROR_PX}px"
            )
    if failures:
        for failure in failures:
            print(f"UI_AUDIT_ERROR {failure}")
        print(f"UI AUDIT v0.7: FAIL ({len(failures)} error(s))")
        return 1
    print("UI AUDIT v0.7: PASS (4/4 centered states)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
