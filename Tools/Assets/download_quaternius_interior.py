"""Download the audited FBX subset of Quaternius' CC0 interior pack.

The official source is a public Google Drive folder rather than a single archive.
Keeping the selected file IDs here avoids downloading the Blend and OBJ copies.
"""

from __future__ import annotations

import hashlib
import json
import urllib.request
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
OUTPUT_ROOT = REPO_ROOT / "SourceAssets" / "Quaternius" / "UltimateHouseInterior" / "Selected"
SOURCE_PAGE = "https://quaternius.com/packs/ultimatehomeinterior.html"
SOURCE_FOLDER = "https://drive.google.com/drive/folders/1SNK9PwPi8xqqxmpU5xEZeiQjB26C1oX6"

# relative path -> (public Google Drive file id, pinned SHA-256; filled after audit)
DOWNLOADS: dict[str, tuple[str, str]] = {
    "License.txt": ("1UF65ItPpj1rIxmewMDtVnpJk_WikvnyJ", "83d8959f9fc56353ed571fbe2dc52e4bcd64508e2399501cd45ac2ce3df0bf8c"),
    "Models/Bed_Bunk.fbx": ("1rhWI7nHfufIycAspb05f3LvLtKVB2VtE", "42048987dbc4304c8d36702221a57e22676ab19142568287908f7f90969274b4"),
    "Models/Bed_Single.fbx": ("1sgmwNraONKrnuJ8ApmjCaKABl05TK9nj", "fc66e91a3f41c7a77b6a01b40a9c74408bb29cfdda1b9bc92593ec52882e211b"),
    "Models/Bookshelf.fbx": ("1Nk71nbt5Qbs57l5uuB0pNULrSsGwb_xR", "6125bb56f3cb545d82498af9bc3f72d7b98a53b0360b327b99275ea0ce8c0bca"),
    "Models/Chair_1.fbx": ("1i2qZ87oZKH8iuaRgdrV-W1rL0KWHe8k7", "1e9ac9b362e04c1bd31a973d6511ee2e94d4690743c95b326354804b9ed60616"),
    "Models/Drawer_2.fbx": ("15h1qg53vOVssDpz1TvUV8frvAUbhq_Nu", "b826f60ad7a7fb2b35aa2a37239afcc9f41188a0a79a1c8ac8e189a8ee3fa37b"),
    "Models/Kitchen_1Drawers.fbx": ("1RBhL6QYtoTLSBxg4RDW8jg2o_XXNZ1X0", "f9375028f76910a45801503338c2fb1a941a5a66f826cdf3d101fadb3a92cb1f"),
    "Models/Kitchen_2Drawers.fbx": ("1WG4hVHw5b7p0Ehb7ijCpTucEospKKyxX", "7f21cf3932163811aaa2b7fb3799087c2621afaf20cab974cdf0140d25bd1811"),
    "Models/Kitchen_Cabinet1.fbx": ("1NK8mJ-8Eg2mJx8bTR8ib-ElJYmptzRGa", "1c16eeaf7daf751c0c8872c886aec5a57a3922324cfa458cb83738a7b5a0d205"),
    "Models/Kitchen_Cabinet2.fbx": ("1Hfiz8TQaN0mDscMtIQGNk3ybEfWbOdMT", "7a43c4377cde977de45d1137c95cd085e02234aa08e631ecd3978eb10460f8ae"),
    "Models/Kitchen_CabinetSmall.fbx": ("1XTlYicBUoOBpXoHXMj4WjuV7Dbj2zZKD", "986883cd72b8ee82155ca93034c63e19facc96e7911612ccdb0f88584d338f70"),
    "Models/Kitchen_Fridge.fbx": ("19H7xQggbdDdivPE3JsfnnDMCOMPZHYIU", "e7a6c73a6188f67a99be2ad8fe6225d4c85dc6bf3110c3a4be24c2d3c110c534"),
    "Models/Kitchen_Oven_Large.fbx": ("1OgkNd5e_trTBgiqc_yBGNAvsYnDehrSD", "61e049e1e02a2d6230b4ea812f56975d1cb6f4be23e41caeaca10abcbe622993"),
    "Models/Kitchen_Sink.fbx": ("1I84whoB0PKq2Y-a4-ZbiM4vVOhbhGy32", "c20d9c1a4785890f887a781fb3befd626508e1b813333293e7c4001a35125350"),
    "Models/NightStand_1.fbx": ("1gi3yvFlKxc0NesfWmvEPNl9KOtVPVdbq", "3d6437151b8a7b62220fa3128af0cb34a71b34e1e6653010f28a6a07689f4670"),
    "Models/Plate_1.fbx": ("15uLYVMSN-WG7Emgq8h2-PwZ5hg-6oa2H", "fa739f25fdd9abbc0dfe1ba39952396b16490bc3da2c43c52d62e1cb96566027"),
    "Models/Shelf_Large.fbx": ("1Tg38yLmwbVUcV21le7Xeo_ogvWPprf-C", "3a1c3c3117c9d923f74fd7694a5d2697520ef78f19e2ba8caa9931bc07b0d83e"),
    "Models/Stool.fbx": ("1cofdkMUkCIbj1ZRh3bO4DGz9C7PRjaYU", "8bf5230240a39e2b97a3f5a05e0bc821cf91cb385dd42c9ea95a5a46395b6500"),
    "Models/Table_RoundLarge.fbx": ("1BjJYEq8Gd4jwcrQsxmgDR-MQ0G0yUvOL", "b6c452cdd0c9b5eef1585a241fd3391c16ba3a38b28442f9178ad5a2f122878c"),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(file_id: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    url = f"https://drive.usercontent.google.com/download?id={file_id}&export=download&confirm=t"
    request = urllib.request.Request(url, headers={"User-Agent": "WhiteoutStationAssetPipeline/0.2"})
    with urllib.request.urlopen(request, timeout=90) as response, destination.open("wb") as output:
        content_type = response.headers.get("Content-Type", "")
        if "text/html" in content_type:
            raise RuntimeError(f"Google Drive returned HTML instead of asset data for {file_id}")
        while chunk := response.read(1024 * 1024):
            output.write(chunk)


def main() -> None:
    records: list[dict[str, object]] = []
    for relative_path, (file_id, expected_sha) in DOWNLOADS.items():
        destination = OUTPUT_ROOT / relative_path
        if not destination.exists():
            print(f"Downloading {relative_path}")
            download(file_id, destination)
        actual_sha = sha256(destination)
        if expected_sha and actual_sha != expected_sha:
            raise RuntimeError(f"Checksum mismatch for {relative_path}: {actual_sha}")
        records.append(
            {
                "file": destination.relative_to(REPO_ROOT).as_posix(),
                "google_drive_file_id": file_id,
                "sha256": actual_sha,
                "size": destination.stat().st_size,
            }
        )
    manifest = {
        "source": SOURCE_PAGE,
        "source_folder": SOURCE_FOLDER,
        "license": "CC0-1.0",
        "files": records,
    }
    manifest_path = OUTPUT_ROOT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Wrote {manifest_path.relative_to(REPO_ROOT)} with {len(records)} audited files")


if __name__ == "__main__":
    main()
