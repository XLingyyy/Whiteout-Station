"""Download the pinned OFL Noto Sans SC faces used by the Chinese UI."""

from __future__ import annotations

import hashlib
import json
import sys
import urllib.request
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
OUTPUT_ROOT = REPO_ROOT / "SourceAssets" / "Fonts" / "NotoSansSC"
COMMIT = "f8d157532fbfaeda587e826d4cd5b21a49186f7c"
BASE_URL = f"https://raw.githubusercontent.com/notofonts/noto-cjk/{COMMIT}/Sans"
FILES = {
    "NotoSansSC-Regular.otf": f"{BASE_URL}/SubsetOTF/SC/NotoSansSC-Regular.otf",
    "NotoSansSC-Bold.otf": f"{BASE_URL}/SubsetOTF/SC/NotoSansSC-Bold.otf",
    "LICENSE.txt": f"{BASE_URL}/LICENSE",
}
USER_AGENT = "WhiteoutStationAssetPipeline/0.2"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    records: list[dict[str, str | int]] = []
    for filename, url in FILES.items():
        destination = OUTPUT_ROOT / filename
        request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
        with urllib.request.urlopen(request, timeout=90) as response:
            payload = response.read()
        if not payload:
            raise RuntimeError(f"empty download: {url}")
        if not destination.exists() or destination.read_bytes() != payload:
            destination.write_bytes(payload)
            print(f"downloaded {destination.relative_to(REPO_ROOT)}")
        else:
            print(f"verified {destination.relative_to(REPO_ROOT)}")
        records.append(
            {
                "file": destination.relative_to(REPO_ROOT).as_posix(),
                "url": url,
                "sha256": sha256(destination),
                "size": destination.stat().st_size,
            }
        )
    manifest = {
        "source": "https://github.com/notofonts/noto-cjk",
        "commit": COMMIT,
        "license": "OFL-1.1",
        "files": records,
    }
    manifest_path = OUTPUT_ROOT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {manifest_path.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
