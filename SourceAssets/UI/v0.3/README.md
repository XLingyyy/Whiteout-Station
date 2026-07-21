# v0.3 generated UI source assets

Generated on 2026-07-22 with Codex built-in `image_gen`, then copied into this project. These are original project assets; no third-party logo, trademark or stock image was requested.

## Deliverables

- `T_UI_InkBrush.png`: alpha ink-brush nine-slice source; `*_Chroma.png` preserves the generated green-screen master.
- `P_PlayerSilhouette.png`, `P_GuHeng.png`, `P_YeCheng.png`: 3:4 portrait-card sources.
- `Icons/*.png`: 44 normalized transparent 256×256 icons.
- `Atlas_*.png`: alpha category masters; `*_Chroma.png` preserves raw generated masters.
- `manifest_*.json`: atlas cell order and per-icon SHA-256.

## Prompt summaries

- Brush: one restrained blue-black dry-brush panel, dense stretchable center, irregular bristle edge, flat chroma-key backdrop.
- Portraits: grounded cinematic polar-station personnel photography; cold side light, subtle warm fill, realistic skin/fabric, no glamour treatment.
- Icons: coherent cold-white monoline industrial-survival symbols, rounded caps/joins, equal grid cells, legible at 24 px, flat chroma-key backdrop.

Exact subject order is encoded in the four manifest files and the invocation history of `Tools/Assets/process_v03_ui_atlas.py`. Chroma removal used the installed imagegen skill helper with border auto-key, soft matte and despill. Icons were normalized with:

```powershell
python Tools/Assets/process_v03_ui_atlas.py <alpha-atlas> --columns <n> --rows <n> --names <ordered-names>
```

Import into Unreal with `Tools/Editor/import_v03_ui_assets.py`. Runtime copies are stored under `/Game/WindStation/UI/v03` using UI compression, no mipmaps and no texture streaming.
