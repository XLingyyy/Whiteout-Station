# GATE G0 v0.4 — 输入导航与证据板交互

日期：2026-07-22
结论：**PASS**

## 12 条按键路径

| # | 路径 | 结果 | 证据 |
|---:|---|---|---|
| 1 | 游戏态 ESC → 暂停 | PASS | Shipping `01_game_esc_pause.png` |
| 2 | 暂停 ESC → 游戏 | PASS | Shipping `02_pause_esc_game.png` |
| 3 | 暂停 → 设置 → ESC → 暂停 | PASS | Dev 双分辨率设置/暂停基线 + `HandleBackRequested` 审计 |
| 4 | 设置再 ESC → 游戏（经暂停） | PASS | 设置→暂停状态机审计 + Shipping 路径 2 |
| 5 | E → 证据板，鼠标可用且人物不转 | PASS | Shipping `05_e_evidence.png` + Dev 过滤/详情连拍 |
| 6 | 证据板 ESC → 游戏、鼠标隐藏 | PASS | Shipping `06_evidence_esc_game.png` |
| 7 | 证据板 E → 游戏 | PASS | Shipping `07_evidence_e_game.png` |
| 8 | 对话 ESC → 游戏，不叠暂停 | PASS | Dev `UI_dialogue_*` 基线 + `Dialogue → CancelDialogue` 审计 |
| 9 | 行动预览 ESC → 仅关闭预览 | PASS | Dev `UI_preview` 基线 + `Preview → HideActionPreview` 审计 |
| 10 | 证据板 ESC 不叠暂停 | PASS | Shipping `05_e_evidence.png` → `06_evidence_esc_game.png` |
| 11 | 结算 ESC → 暂停 | PASS | Shipping `11_results.png` → `11_results_esc_pause.png` |
| 12 | 开场 ESC → 跳过开场并进入暂停 | PASS | Shipping `12_opening_esc_pause.png` |

Shipping 自动烟测直接启动最终归档、按进程绑定游戏窗口，并用 Win32 `PrintWindow` 抓取真实窗口内容，避免多显示器遮挡污染。脚本与来源标注见 `Tools/Release/run_v04_navigation_smoke.py` 和 `docs/evidence_v0.4/g0_navigation/navigation_smoke.json`。路径 3、4、8、9 的自动鼠标/前置场景不具备同等确定性，因此以 Dev 实机基线和同一状态机代码审计补证，未伪装成 Shipping 自动输入。

## V4-02 / V4-03

- 证据板打开时切换到 UI 输入、显示鼠标并禁止游戏视角；ESC/E 关闭后恢复 GameOnly 输入。
- 全部、文件、物品、目击、对话五个过滤按钮及卡片详情均有 1280×720 / 1920×1080 连拍。
- 证据板、暂停、设置、对话与结算等全屏层打开时主 HUD 和准心隐藏，不再叠层。
- 所有返回行为汇入单一 `HandleBackRequested()`；没有只能用鼠标关闭的面板。

证据目录：`docs/evidence_v0.4/g0_navigation/`、`docs/baseline_v0.4/`。
