"""Download the pinned Poly Haven CC0 texture inputs used by the demo."""

from __future__ import annotations

import hashlib
import json
import sys
import urllib.parse
import urllib.request
from pathlib import Path


ASSETS = {
    "snow_02": {"Diffuse": "Diffuse", "Normal": "nor_dx", "Roughness": "Rough"},
    "rusty_metal_05": {"Diffuse": "Diffuse", "Normal": "nor_dx", "Roughness": "Rough"},
    "concrete": {"Diffuse": "Diffuse", "Normal": "nor_dx", "Roughness": "Rough"},
    "blue_metal_plate": {"Diffuse": "Diffuse", "Normal": "nor_dx", "Roughness": "Rough"},
    "fabric_pattern_05": {"Diffuse": "col_01", "Normal": "nor_dx", "Roughness": "Rough"},
}
RESOLUTION = "1k"
FORMAT = "jpg"
USER_AGENT = "WhiteoutStationAssetPipeline/0.1 (+https://polyhaven.com)"
REPO_ROOT = Path(__file__).resolve().parents[2]
OUTPUT_ROOT = REPO_ROOT / "SourceAssets" / "PolyHaven"


def fetch_json(url: str) -> dict:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def file_md5(path: Path) -> str:
    digest = hashlib.md5()  # noqa: S324 - upstream asset integrity, not security
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(url: str, destination: Path, expected_md5: str) -> None:
    if destination.exists() and file_md5(destination) == expected_md5:
        print(f"verified {destination.relative_to(REPO_ROOT)}")
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_suffix(destination.suffix + ".part")
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=90) as response, partial.open("wb") as output:
        while chunk := response.read(1024 * 1024):
            output.write(chunk)
    actual_md5 = file_md5(partial)
    if actual_md5 != expected_md5:
        partial.unlink(missing_ok=True)
        raise RuntimeError(f"checksum mismatch for {destination.name}: {actual_md5} != {expected_md5}")
    partial.replace(destination)
    print(f"downloaded {destination.relative_to(REPO_ROOT)}")


def main() -> int:
    manifest: dict[str, dict[str, dict[str, str | int]]] = {}
    for asset_id, channels in ASSETS.items():
        files = fetch_json(f"https://api.polyhaven.com/files/{asset_id}")
        manifest[asset_id] = {}
        for semantic, channel in channels.items():
            entry = files[channel][RESOLUTION][FORMAT]
            filename = Path(urllib.parse.urlparse(entry["url"]).path).name
            destination = OUTPUT_ROOT / asset_id / filename
            download(entry["url"], destination, entry["md5"])
            manifest[asset_id][semantic] = {
                "source_channel": channel,
                "file": str(destination.relative_to(REPO_ROOT)).replace("\\", "/"),
                "url": entry["url"],
                "md5": entry["md5"],
                "size": entry["size"],
            }
    manifest_path = OUTPUT_ROOT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {manifest_path.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
