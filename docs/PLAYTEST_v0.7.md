# Whiteout Station v0.7 - Playtest 记录

日期：2026-07-28

构建：`20260728T150103Z-0e96ad1b-release`

## 测试目标

确认玩家能够在 Shipping 包中理解开场与操作，完成探索、交互、阶段意向、
自由输入、行动确认和结算，并观察 AI 驱动的 NPC 移动与反应。

## 基础流程

1. 启动 `Windows/WhiteoutStation.exe`。
2. 使用 Space 或鼠标左键逐句推进七句黑幕开场。
3. 检查进入关卡后左侧状态面板、空心圆准心和目标提示。
4. 使用 `H` 查看生存手册，使用 `E` 查看证据板。
5. 瞄准行动点，确认准心切换为手形；使用 `F` 预览，`Q` 切换方案，
   再次使用 `F` 确认。
6. 与顾衡或叶澄对话，先选择当前阶段开放的意向，再输入自由文本。
7. 观察 NPC 回复、移动和反应动画。
8. 使用 Enter 进入结算并查看结局动画、结果页和重新开始入口。

## 自动真实输入记录

### survival_controls

- 输入：鼠标左键、Space、H、Esc、E、W、F、Q、F、Enter、Enter；
- 已执行行动：`distribute_food`；
- 结局：`SurvivalWait`；
- 得分：43.64；
- 剩余 AP：7；
- 模型调用：0；
- 结果：PASS。

该路径验证了逐句开场、手册、证据板、移动、目标聚焦、行动预览、方案切换、
确认、结算和结果页。

### dialogue_free_text

- 输入：鼠标左键、Space、F、Tab、Enter、Unicode 中文文本、Enter、Esc、
  Enter、Enter；
- 已执行行动：`talk_gu_heng`；
- 结局：`SurvivalWait`；
- 得分：46.92；
- 剩余 AP：7；
- 模型调用：0；
- 结果：PASS。

该路径验证了分阶段意向、自由文本输入、NPC 回复、键盘焦点、退出对话和结算。

## 路线覆盖

Shipping 自动路线覆盖四类结局：

- `TaskSuccess`：医疗路线、技术路线；
- `CostUncontrolled`：快速路线、代价路线；
- `SurvivalWait`：等待路线；
- `TotalCollapse`：崩溃路线。

在线、显式离线、缺少密钥和端点不可达四种 AI 状态均完成预期闭环。

## NPC 表演检查

Loopback 模型分别让顾衡、叶澄执行 `step_closer` 和 `acknowledge`。两名 NPC
均移动 85 cm，行走阶段与反应阶段有独立截图，人物模型保持用户替换后的版本。

证据：

- `Validation/ShippingSmoke/loopback_performance_guheng_Walk.png`
- `Validation/ShippingSmoke/loopback_performance_guheng_Acknowledge.png`
- `Validation/ShippingSmoke/loopback_performance_yecheng_Walk.png`
- `Validation/ShippingSmoke/loopback_performance_yecheng_Acknowledge.png`

## 视觉检查

- 左侧面板未发现文字越界；
- 空心圆和手形准心位于屏幕中心；
- 交互文字与墨刷背景对齐；
- 对话意向、文本输入、回复和退出状态连续；
- 黑幕开场必须由玩家输入推进，不会自动跳过；
- 结局动画和结果页均可到达。

## 尚待真人样本验证

- 首次游玩者能否在不阅读外部文档的情况下理解 AP、资源和人物状态；
- 不同玩家对七句开场节奏、字体停留时间和整体叙事清晰度的评价；
- 不同输入法、键盘布局、显示缩放和超宽屏下的体验；
- 长时间在线对话的自然度、重复率和服务延迟。
