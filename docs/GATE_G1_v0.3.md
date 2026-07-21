# G1 UI 风格系统验收记录

验收日期：2026-07-22

## 结论

G1 通过。v0.3 已将 HUD、ESC 菜单、证据板、焦点信息卡、toast、行动预览、拒绝提示、开场与结算迁移到统一的低遮挡蓝黑局部背衬、墨刷提示与白/灰文字体系。橙色仅用于 AP、警告与当前交互目标；交互设备使用白色边缘高亮，不再采用 v0.2 的蓝色整块发光。

## V3-04 / V3-05 规范与资产

- `docs/UI_STYLE_v0.3.md` 固化色值、透明度、字号、间距、描边、橙色纪律和 5 张反馈 UI 参考图索引。
- `docs/FEEDBACK_SUMMARY_v0.3.md` 是可入库的脱敏反馈摘要；原始 DOCX 仍由 `.gitignore` 隔离。
- `SourceAssets/UI/v0.3/` 保留墨刷透明源、3 张原创角色肖像、44 个 256 px 透明单线图标、4 张色键母版、逐图 SHA-256 清单与 prompt 摘要。
- `Tools/Assets/process_v03_ui_atlas.py` 可确定性重建图标；`Tools/Editor/bootstrap_v03_g1_assets.py` 可重建 48 个 UI Texture 资产、中文 StringTable 与白色边缘高亮材质。
- 生成工具、日期、用途与许可口径已登记到 `SourceAssets/ASSET_LICENSES.md`。

## V3-06 至 V3-10 运行态验收

- HUD：左上时间/AP/阶段/暴雪状态，左侧目标与储备，右侧三张肖像状态卡，底部反馈与控制提示；数值仍来自既有状态，等级化文案未改规则。
- ESC：继续、操作说明、重开、退出可用；存档、读取、设置、主菜单如实禁用。显示鼠标并切换 `UIOnly`，关闭恢复 `GameOnly`。
- 证据板：左侧五类过滤计数、双列卡片、证据类型图标、重要性标记和 `n/18` 收集进度均由当前状态生成；不提供空壳页。
- 焦点：近/中/远三组运行态连拍展示橙色菱形、名称、AP 成本、F 提示与白色边缘高亮；不可执行态单独使用红色警示。
- 其余界面：预览六要素、拒绝原因与改变条件、toast、开场目标、危机和结算信息均保留。

## 截图基线

`docs/baseline_v0.3/` 当前保存 26 张 G1 基线：1280×720 与 1920×1080 各 13 张，覆盖：

- `UI_hud_*`、`UI_pause_*`、`UI_evidence_*`；
- `UI_focus_{near,mid,far}_*` 与 `UI_focus_blocked_*`；
- `UI_preview_*`、`UI_reject_generator_*`；
- `UI_toast_{commit,promise}_*`；
- `UI_opening_objective_*`、`UI_results_task_*`。

两档分辨率均使用 `-ForceRes -RenderOffscreen` 实机渲染，文件名记录实际 Viewport 尺寸。未发现 HUD/菜单/证据板裁切；焦点卡和 toast 已禁止自动折行。

## 门禁结果

| 检查 | 结果 |
|---|---|
| Editor Development 编译 | PASS |
| Python 规则回归 | 17 / 17 PASS |
| UE Automation `WhiteoutStation` | 6 / 6 PASS |
| 规则冻结 | 5 / 5 MATCH |
| 中文 StringTable | 243 条、199 个当前引用键，PASS |
| 无模型 AutoRoute `medical` | TaskSuccess，76.64 |
| 无模型 AutoRoute `technical` | TaskSuccess，71.90 |
| 无模型 AutoRoute `quick` | TaskSuccess，72.06 |
| 当前索引密钥扫描 | PASS |
| 分支/远端/标签历史密钥扫描 | PASS |
| UI 资产导入/内容验证 | 48 / 48，0 error |

本门禁只改表现、捕获与编辑器资产脚本；`Content/Rules`、`Tools/Rules`、`Source/*/State` 均未修改。
