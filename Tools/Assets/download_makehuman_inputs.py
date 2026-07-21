#!/usr/bin/env python3
"""Download and verify the audited MakeHuman inputs used for v0.2 NPCs."""

from __future__ import annotations

import hashlib
import shutil
import urllib.request
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / ".codex_tmp" / "makehuman"
USER_AGENT = "WhiteoutStation-v0.2-asset-pipeline"

PACKAGES = (
    {
        "name": "makehuman-community-1.3.0-windows.zip",
        "url": "https://files2.makehumancommunity.org/releases/makehuman-community-1.3.0-windows.zip",
        "sha256": "4437e431d3fcc1f882a639079e68b298ec69e7920af6ec5e5561496774ebbb91",
        "extract": CACHE / "app",
    },
    {
        "name": "suits03_cc-by.zip",
        "url": "https://files2.makehumancommunity.org/asset_packs/suits03/suits03_cc-by.zip",
        "sha256": "1ff85d72eb455751088d33b65a58896dcca14ce27f92497957a87744e77c6d34",
        "extract": CACHE / "suits03",
    },
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def download(url: str, destination: Path) -> None:
    temporary = destination.with_suffix(destination.suffix + ".part")
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=90) as response, temporary.open("wb") as output:
        shutil.copyfileobj(response, output, length=1024 * 1024)
    temporary.replace(destination)


def main() -> None:
    CACHE.mkdir(parents=True, exist_ok=True)
    for package in PACKAGES:
        archive = CACHE / package["name"]
        if not archive.exists() or sha256(archive) != package["sha256"]:
            download(package["url"], archive)
        actual = sha256(archive)
        if actual != package["sha256"]:
            raise RuntimeError(f"Checksum mismatch for {archive}: {actual}")
        extract_root: Path = package["extract"]
        if not extract_root.exists():
            with zipfile.ZipFile(archive) as source:
                source.extractall(extract_root)
        print(f"ready {archive.name} sha256={actual}")


if __name__ == "__main__":
    main()
