"""Split a generated v0.3 icon atlas into normalized transparent 256 px icons."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image


def normalize_icon(cell: Image.Image, size: int = 256, inner: int = 196) -> Image.Image:
    rgba = cell.convert("RGBA")
    alpha = rgba.getchannel("A")
    bbox = alpha.point(lambda value: 255 if value >= 16 else 0).getbbox()
    if not bbox:
        raise ValueError("atlas cell has no visible pixels")
    visible = rgba.crop(bbox)
    visible.thumbnail((inner, inner), Image.Resampling.LANCZOS)
    output = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    x = (size - visible.width) // 2
    y = (size - visible.height) // 2
    output.alpha_composite(visible, (x, y))
    return output


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--columns", type=int, required=True)
    parser.add_argument("--rows", type=int, required=True)
    parser.add_argument("--names", required=True, help="comma-separated filenames without extension")
    parser.add_argument("--out-dir", type=Path, default=Path("SourceAssets/UI/v0.3/Icons"))
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()

    names = [name.strip() for name in args.names.split(",") if name.strip()]
    if len(names) > args.columns * args.rows:
        raise ValueError("more names than atlas cells")
    source = Image.open(args.input).convert("RGBA")
    cell_width = source.width // args.columns
    cell_height = source.height // args.rows
    args.out_dir.mkdir(parents=True, exist_ok=True)
    records: list[dict[str, object]] = []
    for index, name in enumerate(names):
        row, column = divmod(index, args.columns)
        bounds = (
            column * cell_width,
            row * cell_height,
            source.width if column == args.columns - 1 else (column + 1) * cell_width,
            source.height if row == args.rows - 1 else (row + 1) * cell_height,
        )
        icon = normalize_icon(source.crop(bounds))
        output_path = args.out_dir / f"{name}.png"
        icon.save(output_path, optimize=True)
        digest = hashlib.sha256(output_path.read_bytes()).hexdigest()
        records.append({"name": name, "source_cell": index, "sha256": digest})
        print(f"wrote {output_path.as_posix()} sha256={digest}")
    if args.manifest:
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_text(
            json.dumps(
                {
                    "source": args.input.as_posix(),
                    "columns": args.columns,
                    "rows": args.rows,
                    "icons": records,
                },
                ensure_ascii=False,
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )


if __name__ == "__main__":
    main()
