"""Remove the legacy rescue-service marks from Ye Cheng's jacket texture.

The edit is deliberately deterministic: it preserves the original UV layout,
resolution, format, seams, and the surrounding red cloth color.
"""

from __future__ import annotations

import shutil
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[2]
TEXTURE = ROOT / "SourceAssets/MakeHuman/Characters/DoctorYeCheng/textures/rescueteam_jacket.png"
BACKUP = ROOT / ".codex_tmp/v04_jacket_backup/rescueteam_jacket.png"


def replace_mark(image: Image.Image, box: tuple[int, int, int, int], feather: float) -> None:
    crop = image.crop(box).convert("RGB")
    red_pixels = [
        pixel
        for pixel in crop.getdata()
        if pixel[0] >= 220 and pixel[1] <= 45 and pixel[2] <= 45
    ]
    if not red_pixels:
        raise RuntimeError(f"No jacket-red reference pixels in {box}")
    red_pixels.sort()
    jacket_red = red_pixels[len(red_pixels) // 2]

    mask = Image.new("L", crop.size, 0)
    inset = max(2, int(round(feather * 1.5)))
    ImageDraw.Draw(mask).rectangle(
        (inset, inset, crop.width - inset - 1, crop.height - inset - 1),
        fill=255,
    )
    mask = mask.filter(ImageFilter.GaussianBlur(feather))
    fill = Image.new("RGB", crop.size, jacket_red)
    image.paste(Image.composite(fill, crop, mask), box)


def main() -> None:
    BACKUP.parent.mkdir(parents=True, exist_ok=True)
    if not BACKUP.exists():
        shutil.copy2(TEXTURE, BACKUP)

    with Image.open(TEXTURE) as source:
        if source.size != (2048, 2048):
            raise RuntimeError(f"Unexpected jacket texture size: {source.size}")
        result = source.convert("RGB")

    # Small chest/arm label and large back emblem. Boxes stay inside the red UV
    # islands so the black seam gutters and unrelated gray/white materials survive.
    replace_mark(result, (1370, 23, 1592, 113), feather=2.5)
    replace_mark(result, (1172, 1208, 2006, 2026), feather=4.0)
    result.save(TEXTURE, format="PNG", optimize=True)
    print(f"Updated {TEXTURE}")
    print(f"Backup  {BACKUP}")


if __name__ == "__main__":
    main()
