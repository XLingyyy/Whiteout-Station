# G1 中文交互与信息设计验收记录

验收日期：2026-07-21

## 结论

G1 通过。玩家界面已迁移到中文 StringTable 与原生 UMG 宿主；双分辨率 HUD、行动预览/确认、拒绝引导、显式对话菜单、证据板和四类结算页均已完成运行态截图验收。

## 实现与视觉证据

- `/Game/WindStation/UI/ST_WhiteoutStation_zh`：**G1 门禁时点**为 223 个中文条目、207 个代码引用键；v0.2 后续演出与发布阶段补充后，**封版口径**为 236 个条目、210 个引用键。两组数字对应不同门禁快照；运行日志中缺失条目与 StringTable 警告均为 0。
- `/Game/WindStation/Presentation/DA_WS_UIDesign`：Noto Sans CJK 字体、色板、字号与间距令牌。
- `/Game/WindStation/World/Dev_TestMap`：隔离的控件测试地图；`baseline_v0.2/UI_components_1280x720.png` 展示字体层级、状态标签、三种进度条、按钮与 toast。
- `baseline_v0.2/UI_opening_*.png`：开场在首屏说明撤离目标、8 点行动力、按键与风险。
- `baseline_v0.2/UI_hud_*.png`：阶段、暴雪倒计时、8 格 AP、三步目标、资源、三人等级状态与底部操作提示。
- `baseline_v0.2/UI_preview_*.png`：名称、AP 成本、执行者、资源成本、风险、预期结果和 F 二次确认全部可见。
- `baseline_v0.2/UI_reject_generator_*.png`、`UI_reject_medical_*.png`、`UI_reject_relay_*.png`：三种不同拒绝原因与可改变条件。
- `baseline_v0.2/UI_dialogue_*.png`：询问、质疑、三类承诺和安抚六条路径显式可见。
- `baseline_v0.2/UI_evidence_*.png`：3 条系统证据、角色说法、已证实事实与 1 条进行中承诺。
- `baseline_v0.2/UI_results_{task,survival,cost,collapse}_*.png`：四类结局；完整滚动时间线、五维真实进度条、归因、三人最终状态与改进建议。
- 所有上述页面均各有 1280×720 与 960×540 版本，共 26 张 UI 基线；未发现重叠、裁切、内部枚举或原始 ID 外露。

## 交互核对

- `AWhiteoutHUD` 只负责创建、转发与更新 `UWhiteoutHUDWidget`，实际表现由 UMG 控件树承担。
- 注视热点只打开行动预览；只有再次按 F 才提交事务并扣除 AP，移开注视会取消预览。
- `EWSReasonCode` 的全部枚举值都有中文原因映射；下一步提示提供具体条件或统一的可行动检查建议。
- 证据板、对话菜单、暂停页与结算页不调用行动提交，不扣 AP。
- 对话菜单不再依赖 Q 循环记忆；当前选择以 `▶` 常驻标记，承诺状态进入证据板追踪。

## 回归证据

| 检查 | 结果 |
|---|---|
| `python -X utf8 -m unittest discover -s Tools/Rules -p "test_*.py" -v` | 17/17 通过 |
| `python -X utf8 Tools/Release/validate_v02_rule_freeze.py` | 5 个冻结文件校验通过 |
| `python -X utf8 Tools/Release/validate_v02_strings.py` | 通过：223 条目，207 个引用键 |
| UE Automation `WhiteoutStation` | 6/6 通过：DialogueBoundary、ModelBudget、APFlow、KnowledgeAndAgentValidation、Routes、Transactions |
| 运行态 `-WhiteoutAutoRoute=medical` | 成功，TaskSuccess，76.64 |
| 运行态 `-WhiteoutAutoRoute=technical` | 成功，TaskSuccess，71.90 |
| 运行态 `-WhiteoutAutoRoute=quick` | 成功，TaskSuccess，72.06 |
| `WhiteoutStationEditor Win64 Development` | 编译通过 |
| UI 捕获运行日志 | 缺失字符串 0、StringTable 警告 0、崩溃/未处理异常 0 |

> 口径修订（v0.3 / V3-24）：本表中的 223/207 保留为 G1 当日可复现结果；跨文档比较 v0.2 最终版本时统一采用 QA 封版的 236/210，详见 `DOCUMENTATION_RECONCILIATION_v0.3.md`。
