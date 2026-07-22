# Third-party asset record

All files listed here are source inputs for the Whiteout Station v0.1/v0.2/v0.3 demo. Imported Unreal assets remain subject to the same license.

## Whiteout Station v0.3 AI-generated UI — original project output

| Assets | Generator / date | Project use | Terms / provenance |
|---|---|---|---|
| 44 monoline menu, state, resource, evidence, dialogue, quick-menu and key-hint icons | Codex built-in `image_gen`, 2026-07-22 | all v0.3 runtime UI iconography | original output generated for this repository; subject to OpenAI service terms |
| `T_UI_InkBrush` | Codex built-in `image_gen`, 2026-07-22 | interaction card, toast and compact notice nine-slice | original output; chroma master and deterministic alpha post-process retained |
| 顾衡、叶澄写实肖像与玩家剪影 | Codex built-in `image_gen`, 2026-07-22 | HUD and dialogue NPC cards | original fictional-character output; feedback mock used only as identity/presentation reference |

Prompt summaries, processing commands and deliverable inventory are recorded in `UI/v0.3/README.md`; atlas cell order and per-icon hashes are in `UI/v0.3/manifest_*.json`. Raw chroma masters are retained for audit. Generated outputs contain no requested third-party logos, trademarks or stock imagery.

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

## Whiteout Station v0.2 generated audio — original procedural / CC0 derivative

| Assets | Authorship / license | Project use |
|---|---|---|
| `S_WindIndoor_CC0_Derivative.wav` | Procedural derivative of lextrack sound 344887, CC0 1.0 | looping muffled indoor wind bed |
| Generator loop, three surface footsteps, four UI cues, crisis stinger, radio reply and four ending beds | Original procedural audio authored for this repository | machinery, foley, interaction feedback and ending presentation |

All 15 WAV files are generated deterministically by `python -X utf8 Tools/Assets/generate_v02_audio.py` with seed `20260721`. `GeneratedAudio/manifest.json` records the exact source-wind hash, generator version, per-file SHA-256, sample rate, channel count, duration, loop setting and provenance. No trained-model output or unlisted third-party sample is present in these files.

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

## Unreal Engine 5.8 First Person Horror template — Unreal Engine EULA

| Asset | Author | Project use | Source |
|---|---|---|---|
| `NS_DustMote` and `M_DustMote` | Epic Games | soft translucent snow-sprite material and optional mist layer for the exterior blizzard system | UE 5.8 installation: `Templates/TP_FirstPerson/Content/Variant_Horror/Blueprints/Light/Assets` |
| `NS_Confetti` and `M_GoldConfetti` | Epic Games | audited emitter-stack source for the windblown streak layer; the runtime duplicate replaces the gold material, radial velocity and timing with the white dust material and linear storm motion | UE 5.8 installation: `Templates/TP_UEIntro_BP/Content/DemoTemplate/Templates/Confetti` |

This is official Unreal Engine template content supplied with the installed engine, not a CC0 third-party download. It is used only as part of this Unreal Engine Product under the Unreal Engine EULA. Project duplicates are stored under `/Game/WindStation/Art/VFX`; the audited source packages retain their original template paths so package references remain deterministic.

## MakeHuman v0.2 characters — CC0 and CC-BY

| Asset | Author | License | Project use | Source |
|---|---|---|---|---|
| MakeHuman system mesh, `game_engine` rig, Asian skins, facial parts, hair and shoes | MakeHuman Community contributors | CC0 | base meshes, skeletons and system materials for 顾衡 / 叶澄 | https://static.makehumancommunity.org/assets/assetpacks/makehuman_system_assets.html |
| `elvs_male_coveralls_1` | Elvaerwyn | CC-BY | 顾衡 navy engineer coveralls | https://static.makehumancommunity.org/assets/assetpacks/suits03.html |
| `elvs_emt_uniform_jacket_female`, `elvs_emt_uniform_pants_female` | Elvaerwyn + Punkduck | CC BY 4.0 | 叶澄 insulated emergency-medical uniform | https://static.makehumancommunity.org/assets/assetpacks/suits03.html |

The MakeHuman application is used only as an offline exporter; its AGPL code is neither linked nor redistributed. Exact download URLs and SHA-256 values are pinned in `Tools/Assets/download_makehuman_inputs.py`; the generated FBX files, textures and detailed attribution are stored under `SourceAssets/MakeHuman/`.

v0.4 (2026-07-22): 叶澄夹克贴图已在原 CC BY 4.0 资产基础上做去徽章化修改；移除救援组织文字与红十字圆徽，不新增标识或文字，修改脚本为 `Tools/Assets/process_v04_jacket.py`。
