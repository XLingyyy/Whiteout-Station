"""Verify and extract the pinned CC0 Quaternius environment inputs."""

from __future__ import annotations

import hashlib
import json
import sys
import zipfile
from pathlib import Path, PurePosixPath


REPO_ROOT = Path(__file__).resolve().parents[2]
PACK_ROOT = REPO_ROOT / "SourceAssets" / "Quaternius" / "ModularSciFiMegaKit"
ARCHIVE = PACK_ROOT / "ModularSciFiMegaKit_Standard.zip"
EXTRACT_ROOT = PACK_ROOT / "Selected"
EXPECTED_SHA256 = "6fae60cf5189e44dff0bd91097f094a765acc6d57d64a85a0cc0dd56e03035e3"

SELECTED_MODELS = {
    "Columns/Column_MetalSupport.fbx",
    "Columns/Column_Pipes.fbx",
    "Platforms/Door_DarkMetal.fbx",
    "Platforms/Door_Frame_SquareTall.fbx",
    "Platforms/Platform_Metal.fbx",
    "Platforms/Platform_Rails_4.fbx",
    "Platforms/Platform_Stairs_4.fbx",
    "Props/Prop_AccessPoint.fbx",
    "Props/Prop_Barrel_Large.fbx",
    "Props/Prop_Cable_1.fbx",
    "Props/Prop_Cable_3.fbx",
    "Props/Prop_Computer.fbx",
    "Props/Prop_Crate3.fbx",
    "Props/Prop_Crate4.fbx",
    "Props/Prop_Fan_Small.fbx",
    "Props/Prop_Light_Corner.fbx",
    "Props/Prop_Light_Wide.fbx",
    "Props/Prop_PipeHolder.fbx",
    "Props/Prop_Vent_Big.fbx",
    "Props/Prop_Vent_Wide.fbx",
    "Walls/BottomMetal_Straight.fbx",
    "Walls/TopCables_Straight.fbx",
    "Walls/TopWindow_Straight.fbx",
    "Walls/WallAstra_Straight.fbx",
    "Walls/WallAstra_Straight_Window.fbx",
    "Walls/WallBand_Straight.fbx",
    "Walls/WallWindow_Straight.fbx",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def archive_relative(name: str) -> str | None:
    parts = PurePosixPath(name).parts
    if len(parts) < 3:
        return None
    if parts[1] == "FBX":
        return str(PurePosixPath(*parts[2:]))
    return None


def main() -> int:
    if not ARCHIVE.is_file():
        raise SystemExit(f"missing archive: {ARCHIVE}")
    actual_sha256 = sha256(ARCHIVE)
    if actual_sha256 != EXPECTED_SHA256:
        raise SystemExit(f"archive checksum mismatch: {actual_sha256}")

    extracted: list[dict[str, str | int]] = []
    found_models: set[str] = set()
    with zipfile.ZipFile(ARCHIVE) as archive:
        for info in archive.infolist():
            parts = PurePosixPath(info.filename).parts
            relative_model = archive_relative(info.filename)
            is_model = relative_model in SELECTED_MODELS
            is_texture = len(parts) >= 3 and parts[1] == "Textures" and not info.is_dir()
            is_license = len(parts) == 2 and parts[1] == "License_Standard.txt"
            if not (is_model or is_texture or is_license):
                continue
            if is_model:
                destination = EXTRACT_ROOT / "Models" / relative_model
                found_models.add(relative_model)
            elif is_texture:
                destination = EXTRACT_ROOT / "Textures" / PurePosixPath(*parts[2:])
            else:
                destination = EXTRACT_ROOT / "License_Standard.txt"
            destination.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(info) as source, destination.open("wb") as output:
                output.write(source.read())
            extracted.append(
                {
                    "file": destination.relative_to(REPO_ROOT).as_posix(),
                    "sha256": sha256(destination),
                    "size": destination.stat().st_size,
                }
            )

    missing = sorted(SELECTED_MODELS - found_models)
    if missing:
        raise SystemExit(f"archive is missing selected models: {missing}")

    manifest = {
        "source": "https://quaternius.com/packs/modularscifimegakit.html",
        "license": "CC0-1.0",
        "archive": ARCHIVE.relative_to(REPO_ROOT).as_posix(),
        "archive_sha256": actual_sha256,
        "files": sorted(extracted, key=lambda item: str(item["file"])),
    }
    manifest_path = EXTRACT_ROOT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"verified {ARCHIVE.relative_to(REPO_ROOT)}")
    print(f"extracted {len(extracted)} files to {EXTRACT_ROOT.relative_to(REPO_ROOT)}")
    print(f"wrote {manifest_path.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
