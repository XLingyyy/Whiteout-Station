# Third-party asset record

All files listed here are source inputs for the Whiteout Station v0.1/v0.2 demo. Imported Unreal assets remain subject to the same license.

## Poly Haven textures — CC0 1.0

Retrieved 2026-07-21 from Poly Haven. Poly Haven publishes these assets under the CC0 public-domain dedication; attribution is not required, but retained here for provenance.

| Asset | Author | Project use | Source |
|---|---|---|---|
| Snow 02 | Rob Tuytel | exterior snow floor | https://polyhaven.com/a/snow_02 |
| Rusty Metal 05 | Amal Kumar | generator, cabinet and heater surfaces | https://polyhaven.com/a/rusty_metal_05 |
| Concrete | Rob Tuytel | station floor, wall and partition surfaces | https://polyhaven.com/a/concrete |
| Blue Metal Plate | Rob Tuytel | painted station metal and control-room accents | https://polyhaven.com/a/blue_metal_plate |
| Fabric Pattern 05 | Rob Tuytel | winter clothing and soft interior surfaces | https://polyhaven.com/a/fabric_pattern_05 |

License: https://polyhaven.com/license (CC0 1.0 Universal). Exact downloaded URLs, checksums and sizes are recorded in `PolyHaven/manifest.json`; downloads are reproducible with `python -X utf8 Tools/Assets/download_polyhaven.py`.

## Freesound ambience — CC0 1.0

| Asset | Author | Project use | Source |
|---|---|---|---|
| Strong wind (sound 344887) | lextrack | looping exterior blizzard bed | https://freesound.org/people/lextrack/sounds/344887/ |

The Freesound source page marks sound 344887 as Creative Commons Zero. The tracked WAV is a PCM conversion of Freesound's low-quality MP3 preview; no creative changes were made. URL, checksums and audio properties are in `Freesound/344887/manifest.json`. Reproduce with `python -X utf8 Tools/Assets/download_freesound_audio.py` after installing the `soundfile` conversion helper.

## Quaternius Modular Sci-Fi MegaKit Standard — CC0 1.0

| Asset | Author | Project use | Source |
|---|---|---|---|
| Modular Sci-Fi MegaKit (Standard free edition) | Quaternius | station walls, platforms, doors, structural columns and industrial props | https://quaternius.com/packs/modularscifimegakit.html |

The archive's `License_Standard.txt` declares CC0 1.0 Universal. The pinned archive SHA-256 and each selected extracted input are recorded by `python -X utf8 Tools/Assets/prepare_quaternius_megakit.py` in `Quaternius/ModularSciFiMegaKit/Selected/manifest.json`.

## Quaternius Ultimate House Interior Pack — CC0 1.0

| Asset | Author | Project use | Source |
|---|---|---|---|
| Ultimate House Interior Pack (audited FBX subset) | Quaternius | medical bed and cabinet, bunk beds, kitchen units, shelves, seating and small living props | https://quaternius.com/packs/ultimatehomeinterior.html |

The official public Google Drive folder contains Blend, FBX and OBJ copies. `python -X utf8 Tools/Assets/download_quaternius_interior.py` downloads only 18 selected FBX files plus the original CC0 `License.txt`; its manifest pins every public file ID, size and SHA-256 value.

## Noto Sans SC — SIL Open Font License 1.1

| Asset | Author | Project use | Source |
|---|---|---|---|
| Noto Sans SC Regular and Bold | Noto project contributors | all Simplified Chinese interface and world labels | https://github.com/notofonts/noto-cjk |

The original OFL 1.1 license is retained at `Fonts/NotoSansSC/LICENSE.txt`. The source revision, exact URLs, sizes and SHA-256 values are pinned by `python -X utf8 Tools/Assets/download_noto_cjk.py` in `Fonts/NotoSansSC/manifest.json`.
