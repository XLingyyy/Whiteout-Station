"""Download and convert the pinned CC0 wind ambience used by the demo."""

from __future__ import annotations

import hashlib
import json
import sys
import urllib.request
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
OUTPUT_ROOT = REPO_ROOT / "SourceAssets" / "Freesound" / "344887"
SOURCE_URL = "https://cdn.freesound.org/previews/344/344887_4930786-lq.mp3"
SOURCE_PAGE = "https://freesound.org/people/lextrack/sounds/344887/"
SOURCE_MD5 = "59fee016ab9fa176a543cf7ca5402e98"


def md5(path: Path) -> str:
    digest = hashlib.md5()  # noqa: S324 - upstream asset integrity, not security
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    mp3_path = OUTPUT_ROOT / "344887_4930786-lq.mp3"
    if not mp3_path.exists() or md5(mp3_path) != SOURCE_MD5:
        request = urllib.request.Request(
            SOURCE_URL, headers={"User-Agent": "WhiteoutStationAssetPipeline/0.1"}
        )
        with urllib.request.urlopen(request, timeout=60) as response:
            mp3_path.write_bytes(response.read())
    if md5(mp3_path) != SOURCE_MD5:
        raise RuntimeError("Freesound preview checksum mismatch")

    try:
        import soundfile
    except ModuleNotFoundError as error:
        raise RuntimeError(
            "Install the conversion helper with: python -m pip install soundfile"
        ) from error

    audio, sample_rate = soundfile.read(mp3_path, always_2d=True, dtype="float32")
    wav_path = OUTPUT_ROOT / "S_Wind_Strong_CC0.wav"
    soundfile.write(wav_path, audio, sample_rate, subtype="PCM_16")
    manifest = {
        "title": "Strong wind",
        "author": "lextrack",
        "source_page": SOURCE_PAGE,
        "license": "CC0 1.0",
        "preview_url": SOURCE_URL,
        "preview_md5": SOURCE_MD5,
        "wav_md5": md5(wav_path),
        "sample_rate": sample_rate,
        "channels": int(audio.shape[1]),
        "frames": int(audio.shape[0]),
    }
    (OUTPUT_ROOT / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    print(f"prepared {wav_path.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
